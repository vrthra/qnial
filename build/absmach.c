/*==============================================================

  MODULE ABSMACH.C

  COPYRIGHT NIAL Systems Limited  1983-2005


  This module implements the abstract array machine model, consisting
  of a heap that holds array objects, a stack of array values, and
  a hash table that supports unique representations of phrases and faults.
  It also includes utility routines related to these and routines
  that support a buffer area used to gather C character or integer
  arrays.

  All the above areas are allocated in one memory array to reduce
  interference with other processes.

  ================================================================*/


/* Q'Nial file that selects features and loads the xxxspecs.h file */

#include "switches.h"

/* standard library header files */

#define IOLIB
#define LIMITLIB
#define STDLIB
#define STLIB
#define SIGLIB
#define SJLIB
#define MALLOCLIB
#define MATHLIB
#define CLIB
#include "sysincl.h"

/* Q'Nial header files */

#include "coreif_p.h"        /* for messages and IOContext */
 /* Must be first include because of conflict with */
 /* with windows.h */

#include "qniallim.h"
#include "lib_main.h"
#include "absmach.h"
#include "basics.h"
#include "if.h"

#include "ops.h"             /* for append  used in nial_extend (move to
                              * parse) */
#include "mainlp.h"          /* for exit cover */
#include "eval.h"            /* for fault triggering */
#include "fileio.h"          /* for nprintf and related messages */
#include "utils.h"           /* for cnvtup */


static nialptr reserve(nialint n);
static void release(nialptr x);
static nialint hash(char *s);
static void allocate_stack(void);
static void reset_absmach(void);
static void allocate_heap(nialint initialmemsize);
static void deallocate_heap(void);
static void setup_heap(void);
static void remove_atom(nialptr x);
static void rehash(nialint tblsize);
static void allocate_Cbuffer(void);

/*
static char * my_realloc(char *x,nialint newsize,nialint oldsize);
*/

static void allocate_atomtbl(void);
static void leavewithsavequery(int rc);

/* atom table variables */
static int  inrehash = false;/* variable to prevent reentry to rehash during
                              * a heap recovery */
static int  firsttry = true; /* flag to monitor heap expansion tries in
                              * checkavailspace */

/* C buffer variables */
static nialint Cbuffersize;  /* current size of the C buffer */



/* the following routine is used in place of a macro because several
   compilers were confused by it in macro form. It does enough work
   that the routine call overhead is not excessive. */
   
/* routine to store bit b at position i in a packed boolean array */

void
store_bool(nialptr x, nialint i, int b)
{
  nialint     it = (i) / boolsPW;
  nialint    *wordaddr = pfirstint(x) + it;
  nialint     pos = (i) % boolsPW;
  nialint     oldword = *wordaddr;

#ifdef OLD_BOOL_ORDER
  nialint     newword = (oldword & ~(1 << pos)) | (b << pos);

#else
  nialint     tmp = 31 - pos;
  nialint     newword = (oldword & ~(1 << tmp)) | (b << tmp);

#endif

  *wordaddr = newword;
}

/* Abstract Machine initialization */

void
setup_abstract_machine(nialint initialmemsize)
{
  allocate_heap(initialmemsize);  /* do heap first since the other areas are
                                   * allocated from it. */
  allocate_stack();
  atomtblsize = dfatomtblsize;
  allocate_atomtbl();
  allocate_Cbuffer();
}


#ifdef CALLDLL
extern int DLLcleanup(void);
#endif

void
clear_abstract_machine()
{
#ifdef CALLDLL
  /* This must be called whenever we load a workspace, because it is the only
   * opportunity to empty the DLL table and to call FreeLibrary on the loaded
   * entries. */
  DLLcleanup();
#endif
  deallocate_heap();         /* clears all since others are within it. */
  free(Cbuffer);
  Cbuffer = NULL;
}

/*  Heap management routines.  */


/* routine to allocate the heap as a contiguous block of nialptrs or words
   (32 bit integers) */

static void
allocate_heap(nialint initialmemsize)
{
  memsize = initialmemsize;
  mem = (nialint *) malloc(memsize * sizeof(nialint));
  if (mem == NULL) {         /* malloc failed */
    nprintf(OF_NORMAL, "System limitation: unable to allocate heap of %d words\n",
            memsize);
    /* exit using longjmp directly since the mem has not been allocated */
    longjmp(init_buf, NC_MEM_ERR_F);
  }
  setup_heap();
}

/* routine to free the heap memory area */

static void
deallocate_heap()
{
  free(mem);
}

/* routine to set up the heap as two free blocks:
 * the first is the free list header block. It has less than the
 * minimal space (minsize - 2) so it can never be * allocated.
 * the second has all the rest of the space */

static void
setup_heap()
{
  nialint     flhsize;

  freelisthdr = 0;
  flhsize = minsize - 2;
  if (flhsize % 2 != 0)      /* current settings ensure minsize is even. */
    flhsize++;
  membase = freelisthdr + flhsize;

  /* set up free list header block */
  blksize(freelisthdr) = flhsize;
  set_freetag(freelisthdr);
  fwdlink(freelisthdr) = membase;

  /* the backward link for the header block is not used */
  reset_endinfo(freelisthdr);/* set as not free to prevent a merge */

  /* set up the large block */
  blksize(membase) = memsize - membase;
  set_freetag(membase);
  fwdlink(membase) = TERMINATOR;
  bcklink(membase) = freelisthdr;
  set_endinfo(membase);      /* mark end as free with ptr to block front */
}

/* routine to expand the heap if required and allowed.
   n is the request size that triggered the expansion.
   An attempt to expand the heap is made if the expansion flag is on.
   The amount used is 20% of current size unless this is more than
   50% of the original size. Extra is added if this does not leave
   a minimum heap size.
   If expansion is not allowed, a small expansion is tried to ensure that
   space needs of the clean up process can be met. If this fails then
   the session is terminated, otherwise an infinite loop can be caused.

   The expansion is done with a realloc with the hope that the current
   area can be extended in place without copying on some operating systems.
   If the realloc fails then control jumps to top level with a warning.
   If expansions continue to be called, and a recovery routine is
   present then a limit is placed on the number of recoveries attempted
   to avoid an infinite recovery loop.
   */

void
expand_heap(nialint n)
{
  nialint    *newmem,
              memincr,
              mspace;
  nialptr     newblk;
  static int  donottry = false; /* used to prevent the small expansion done
                                 * to permit recovery to be done repeatedly
                                 * if expansion not turned on. */


  if (donottry) {
    nprintf(OF_MESSAGE, "Out of memory. Cannot continue.\n");
    leavewithsavequery(NC_MEM_EXPAND_NOT_TRIED_F);
  }

  if (!expansion) {     /* expansion is not allowed (+size paramter was set */
                              
    memincr = MINHEAPSPACE + 100; /* do a slight expansion to allow cleanup */
    donottry = true;
  }
  else {
    memincr = memsize / 5;   /* grow area by 20% of its current size */
    memincr = (memincr > dfmemsize / 2 ? dfmemsize / 2 : memincr);
    /* but not more than 1/2 the dfmemsize to prevent too large chunks as
     * memsize grows */
    if (memincr < n + MINHEAPSPACE) /* or by request size + extra
                                     * MINHEAPSPACE if more is needed */
      memincr = n + MINHEAPSPACE;
    if (memincr % 2 != 0)
      memincr++;             /* ensure even sized memarea */
    nprintf(OF_MESSAGE_LOG, "expanding heap to %d words\n", memsize + memincr);
  }
  mspace = (memsize + memincr) * (sizeof(nialint));
  newmem = (nialint *) my_realloc((char *) mem, mspace, memsize * (sizeof(nialint)));

  if (newmem == NULL) {      /* the realloc has failed */
    nprintf(OF_NORMAL_LOG, "heap expansion failed\n");
    if (interpreter_running) {  /* safe to issue a warning and recover */
      donottry = true;       /* to ensure we do not try to expand again */
/*
printf("calling exit_cover when expansion failed\n");
*/
      exit_cover(NC_MEM_EXPAND_FAILED_W);
    }
    else {                   /* called from NCInitNial. force a termination */
      nprintf(OF_NORMAL_LOG, "Warning: Not enough memory for the workspace\n");
      if (instartup)
        longjmp(init_buf, NC_MEM_EXPAND_NOT_TRIED_F);
      else
        longjmp(error_env, NC_MEM_EXPAND_NOT_TRIED_F);
    }
  }

  /* reset mem to the new area */
  if (mem != newmem) {       /* the heap has moved. Change mem and reset all
                              * explicit pointers */
    mem = newmem;
    reset_absmach();
  }
  /* new block of size memincr starts at old memsize */
  newblk = memsize;
  blksize(newblk) = memincr;

  memsize = memsize + memincr;  /* adjust memsize */
  release(arrayptr(newblk)); /* to link it into the chain, possibly merging
                              * it with an existing free block at the top of
                              * mem */
  if (!expansion) {          /* behave as if expansion not done */
    nprintf(OF_NORMAL_LOG, "Warning: heap cannot expand\n");
  /* go through recover */
    if (interpreter_running) {  /* safe to issue a warning and recover */
/*
printf("calling exit_cover when expansion not allowed\n");
*/
      exit_cover(NC_MEM_EXPAND_FAILED_W);
    }
    else {                   /* called from NCInitNial. force a termination */
      nprintf(OF_NORMAL_LOG, "Warning: Not enough memory for the workspace\n");
      if (instartup)
        longjmp(init_buf, NC_MEM_EXPAND_NOT_TRIED_F);
      else
        longjmp(error_env, NC_MEM_EXPAND_NOT_TRIED_F);
    }
  }
}

/* routine to reset the explicit global C pointers into the heap area. */

void
reset_absmach(void)
{
  /* stack variables */
  stkarea = pfirstitem(stkareabase);
  stklim = tally(stkareabase);
  /* atom table variables */
  atomtbl = pfirstitem(atomtblbase);
  atomtblsize = tally(atomtblbase);
}


/* routine to reserve n words of space in the heap.
   reserve allocates a block of appropriate size from the free list
   if possible.
   The size may be bigger than required to avoid memory chunks
   too small to be blocks.
   It is only called from new_create_array.

   The space is allocated on a first fit basis from a free list
   that is doublly linked. If the space left in a chosen block is
   too small the entire block is allocated, otherwise the left over
   space is placed back in the free list in the same place.
  Putting it there helps keep the free list with the largest free block
   near the end of the list which reduces fragmentation.
*/

static      nialptr
reserve(nialint n)
{
  register nialptr freeptr,
              nextfree;
  register nialint k;

  if (n % 2 != 0)            /* ensure request is of even size for alignment */
    n++;
/* check for stack / heap clash */
  if (CSTACKFULL)
    longjmp(error_env, NC_C_STACK_OVERFLOW_W);

retry:                       /* come back here after expanding heap */
#ifdef DEBUG
  chkfl();                   /* debugging test to check that free list is OK */
#endif

  /* search the free list for the first block that fits */
  freeptr = freelisthdr;
  nextfree = fwdlink(freeptr);
  while ((nextfree != TERMINATOR) && (blksize(nextfree) < n)) {
    freeptr = nextfree;
    nextfree = fwdlink(freeptr);
  }
/*
  The above loop can terminate if either blksize(nextfree) >= n , in which
  case there is space, or if nextfree == TERMINATOR, in which case there is
  no suitable free block.
*/
  if (nextfree == TERMINATOR) { /* no block large enough *//* e xpand the
                                 * heap by enough to accomodate the requested
                                 * block */
    expand_heap(n);
    checksignal(NC_CS_NORMAL);
    goto retry;              /* we can assume that expand_heap has worked
                              * since it will jump to top level if it hasn't */
  }
  /* nextfree is a block large enough to accomodate the request */

  k = blksize(nextfree) - n; /* size of unneeded part of nextfree block */
  if (k < minsize) {         /* use all of nextfree block *//* r emove
                              * nextfree block from free chain */
    nialptr     next = fwdlink(nextfree);

    fwdlink(freeptr) = next;
    if (next != TERMINATOR)
      bcklink(next) = freeptr;
  }
  else {                     /* set up the partial block in the chain
                              * replacing nextfree */
    register nialptr newnextfree = nextfree + n,
                next = fwdlink(nextfree);

    fwdlink(newnextfree) = next;
    bcklink(newnextfree) = bcklink(nextfree);
    fwdlink(freeptr) = newnextfree;
    if (next != TERMINATOR)
      bcklink(next) = newnextfree;
    blksize(newnextfree) = k;
    set_freetag(newnextfree);
    set_endinfo(newnextfree);
    blksize(nextfree) = n;
  }
#ifdef DEBUG
  if (nextfree <= 0 || nextfree >= memsize) {
    nprintf(OF_DEBUG, "nextfree is %d \n", nextfree);
    nprintf(OF_DEBUG, "*** wild array pointer in reserve ***");
    nabort(NC_ABORT_F);
  }
/* MAJ: The following code is used in finding unfreed blocks
  when debugging space leaks.
  if (nextfree+hdrsize==34276)
  { nprintf(OF_DEBUG_LOG,"reserving space for array 34276\n");
  }
  if (debug)
  nprintf(OF_DEBUG,"reserving block %d of size %d\n",nextfree,n);
*/
#endif
  reset_freetag(nextfree);   /* zeros the refcnt also */
  reset_endinfo(nextfree);   /* marks the block to be allocated */
#ifdef DEBUG
  if (nextfree % 2 != 0) {   /* check that block is on even index boundary */
    nprintf(OF_DEBUG, "odd block start in reserve %d\n", nextfree);
    nabort(NC_ABORT_F);
  }
#endif
  return (arrayptr(nextfree));
}

/* routine to release a block, placing it in the free block
   possibly merging it with a block before and/or after it in memory
   if one or both are free.

It uses the allocation information in the preceding and following blocks
to determine when a merge is possible. See absmach.h for a more complete
explanation.

See the note below about where to place a block merged at both ends.
The choice is crucial to the behaviour of the memory allocator.
*/

static void
release(nialptr x)
/* returns the block at x to the free list */
{
  register nialptr p,
              q,
              next;
  register nialint n,
              sz;

  x = blockptr(x);

#ifdef DEBUG
  if (x <= 0 || x >= memsize) {
    nprintf(OF_DEBUG_LOG, "** invalid release **\n");
    nabort(NC_ABORT_F);
  }
  n = blksize(x);
/* MAJ: the following code is used in finding unfreed blocks
   when debugging space leaks.
  if (x+hdrsize==9598)
  {
    nprintf(OF_DEBUG_LOG,"releasing space for array 9598\n");
  }
if (debug)
nprintf(OF_DEBUG_LOG,"releasing block %d of size %d\n",x,n);
*/
#else
  n = blksize(x);
#endif

  if (isfree(x + n)) {       /* the following block is free */
    q = x + n;
    if (isprevfree(x)) {     /* merging three blocks, two in the chain
                              * already */
      p = prevblk(x);
      /* the choice below to replace the following block in the chain with
       * the merged block and relinquishing the preceding block is crucial to
       * the success of the memory allocator. It has the salutary effect of
       * keeping the large free block at the end of the chain which results
       * in significantly less fragmentation. The combined effect produces
       * good paging behaviour because of patterns of use of objects in Nial. */

      /* adjust header for p */
      sz = blksize(p) + n + blksize(q);
      blksize(p) = sz;

      /* remove old links for p from the chain */
      next = fwdlink(p);
      fwdlink(bcklink(p)) = next;
      if (next != TERMINATOR)
        bcklink(next) = bcklink(p);

      /* attach p in q's place in the chain */
      next = fwdlink(q);
      fwdlink(p) = next;
      bcklink(p) = bcklink(q);
      fwdlink(bcklink(p)) = p;
      if (next != TERMINATOR)
        bcklink(next) = p;

      /* set up trailer for extended p */
      set_endinfo(p);

#ifdef DEBUG
      x = p;                 /* so tests at end makes sense */
#endif
    }
    else {                   /* merge block x with following block (already
                              * in the chain) */
      /* set up header for x */
      sz = blksize(q) + n;
      blksize(x) = sz;

      /* change the links to replace q in the chain with x */
      next = fwdlink(q);
      fwdlink(x) = next;
      bcklink(x) = bcklink(q);
      fwdlink(bcklink(x)) = x;
      if (next != TERMINATOR)
        bcklink(next) = x;
      set_freetag(x);

      /* adjust trailer for extended x */
      set_endinfo(x);
    }
  }
  else
    /* test if the block before x is free */
  if (isprevfree(x)) {       /* merge block x with previous block (already in
                              * the chain) */
    p = prevblk(x);

    /* adjust header for p */
    sz = blksize(p) + n;
    blksize(p) = sz;

    /* set up trailer for extended p */
    set_endinfo(p);

#ifdef DEBUG
    x = p;                   /* so tests at end makes sense */
#endif
  }
  else {
    q = freelisthdr;         /* add the block to the beginning of the free
                              * list */
    next = fwdlink(q);
    fwdlink(x) = next;
    bcklink(x) = q;
    fwdlink(q) = x;
    if (next != TERMINATOR)
      bcklink(next) = x;
    set_freetag(x);
    set_endinfo(x);
  }

#ifdef DEBUG
  if (x == fwdlink(x) || x == bcklink(x)) {
    nprintf(OF_DEBUG, "release: bad free list at %d fwd %d bkwd %d\n", x, fwdlink(x),
            bcklink(x));
    nabort(NC_ABORT_F);
  }
  if (blksize(x) < minsize) {
    nprintf(OF_DEBUG_LOG, "release: invalid block size %d", blksize(x));
    nabort(NC_ABORT_F);
  }
  chkfl();
#endif
}

/* routine to used to test whether an array is free and if so release
   its space.
   freeit is called in two ways.
   When FREEUPMACRO is set it is only called with refcnt = 0. If
   it is not set freeit must test the refcnt.

   For an array containing references to other arrays, their reference
   counts are reduced and freeit called recursively. For phrases and faults
   the corresponding entry in the hash table is also removed. */


void
freeit(nialptr x)
/* frees space used for an array representation */
{
  int         k;

#ifdef DEBUG
/* temp debug stuff MAJ, leave to be set as needed.
   Use the debugger, and set a breakpoint on the print statment.
   THen the stack will tell us where the bad freeup occurs.
  if (blockptr(x)==34385)
  {
         nprintf(OF_DEBUG_LOG,"calling freeit on block 34385\n"); }
  if (x==7480)
  {
         nprintf(OF_DEBUG_LOG,"calling freeit on array 7480\n"); }
*/
/* compute block address from array address. This is repeated here to
        make debug tests work */
  if (!allocated(blockptr(x))) {
    nprintf(OF_DEBUG, "**** array already free ****\n");
    wb(blockptr(x));
    nabort(NC_ABORT_F);
  }
  if (blockptr(x) <= 0 || blockptr(x) >= memsize) {
    nprintf(OF_DEBUG, "** invalid array pointer in freeit \n");
    wb(blockptr(x));
    nabort(NC_ABORT_F);
  }
  if (x <= 0) {
    nprintf(OF_DEBUG, "hitting negative entry in freeit\n");
    nabort(NC_ABORT_F);
  }
#endif

/* check for stack / heap clash */
  if (CSTACKFULL)
    longjmp(error_env, NC_C_STACK_OVERFLOW_W);

#ifndef FREEUPMACRO
  /* MAJ: the refcnt test is already true if freeit has been called
     from the macro. */
  if (refcnt(x) > 0)
    return;
#endif


  k = kind(x);
  if (k == atype) {          /* items are arrays to be freed if possible */
    nialptr     it;
    nialptr    *topx = pfirstitem(x); /* safe: no expansion done */
    nialint     i,
                tlx = tally(x);

#ifdef V4AT
    if (tlx == 0)
      tlx = 1;
#endif
    for (i = 0; i < tlx; i++) {
      it = *topx++;
#ifdef DEBUG
      if (it == 0) {
        nprintf(OF_DEBUG, "invalid item of 0 in an atype, aborting\n");
        nabort(NC_ABORT_F);
      }
#endif
      if (it != invalidptr &&/* unused portions of atype arrays */
          refcnt(it) > 0) {  /* it could possibly be 0 due to aborted freeit
                              * done earlier. The abort can occur if freeit
                              * is freeing a very deep array and the C stack
                              * gets used up */
        decrrefcnt(it);
        if (refcnt(it) == 0)
          freeit(it);
      }
    }
  }
  else if (k == phrasetype || k == faulttype) /* Adjust hash table address */
    remove_atom(x);

  release(x);
}


/* routine to clear the heap of temporary arrays on a jump
   to top level. It walks block by block through memory freeing up
   unreferenced arrays ie. arrays with refcnt = 0;
*/

void
clearheap()
{
  nialint     next;

  next = membase;
  do {
    if (CSTACKFULL)
      longjmp(error_env, NC_C_STACK_OVERFLOW_W);
    if (allocated(next) && refcnt(arrayptr(next)) == 0) {
      freeup(arrayptr(next));
    }
    next = next + blksize(next);
  }
  while (next < memsize);
}



/*-------- array creation routines --------*/

/* routine to create the container in the heap for a new array.
   For arrays of kind "atype", the data part is initialized with
   "invalidptr" to indicate unallocated elements.
   The data for the array is placed in the container by the calling routine.

  interface for array container creation
  k - kind of array
  v - valence
  len - length for phrases and faults (unused for other kinds)
  extents - ptr to list of C ints of length v giving the shape vector

  In order not to put valence limitations in the system the shape
  is provided as a C pointer. In many cases this is a pointer into
  the heap and hence the address could become invalid if the call
  to reserve moves the heap. This is a problem if valence is > 1 since
  the shape has to be copied into the new container. To get around this
  in such cases the shape is copied to a C vector. For large valence a
  temporary one is allcated to avoid an arbitrary limit on valence.
*/

#define VALENCE_LIMIT 10

static nialint holder[VALENCE_LIMIT];
static nialint *shapevec = holder;

nialptr
new_create_array(int k, int v, nialint len, nialint * extents)
{
  nialptr     z;
  nialint     i,
             *sp;
  nialint     tly,
              t = 0,
              m,
              n = 0,
              n1 = 0;

  /* initialize n and n1 so the case doesn't complain below */

  if (v == 0 && homotype(k)) {  /* container for an atom of fixed size set
                                 * values directly for efficiency */
    /* fixed sized atoms use minimum size block */
    m = minsize;
    tly = 1;
    n1 = 2;                  /* effective length of character atom */
  }
  else {
    /* compute block size needed */

    tly = 1;
    if (v == 1)
      tly = extents[0];
    else if (v > 1) {        /* compute tally while saving the shape */
      if (v > VALENCE_LIMIT) {  /* use allocated space for shape vec */
        nialint    *tempvec = (nialint *) malloc(v * sizeof(nialint));

        if (tempvec == NULL)
          exit_cover(NC_VALENCE_OVERFLOW_CREATE_W);
        shapevec = tempvec;
      }
      for (i = 0; i < v; i++) {
        int         len_i = extents[i];

        shapevec[i] = len_i;
        tly *= len_i;
        if (tly < 0) {       /* an integer overflow has occured */
#ifdef DEBUG
          nprintf(OF_DEBUG, "integer overflow in size for array creation: valence %d\n", v);
          for (i = 0; i < v; i++)
            nprintf(OF_DEBUG, " i %d extent %d\n", i, extents[i]);
          nabort(NC_ABORT_F);
#else
          exit_cover(NC_INTEGER_OVERFLOW_CREATE_W);
#endif
        }
      }
    }
#ifdef V4AT
    if (tly == 0) 
    { /* make all empties have kind atype */
      k = atype;
      t = 1;   /* space for the archetype */
    }
/* compute number of item places, phrases and faults use len  */
    else
    if (k == phrasetype || k == faulttype)
      t = len;
    else
      t = tly;
#else
    if (tly == 0) 
    { /* make all empties have kind atype */
      k = atype;
      if (v == 1 && !doingclearws)  /* force all empty lists to be Null */
        return (Null);
    }
/* compute number of item places, phrases and faults use len  */
    if (k == phrasetype || k == faulttype)
      t = len;
    else
      t = tly;
#endif

/* compute number of words (nialptrs or nialints) in data part */
    switch (k) {
      case atype:
          n = t * WParray;
          break;
      case booltype:
          n = t / boolsPW + ((t % boolsPW) == 0 ? 0 : 1);
          break;
      case phrasetype:
      case faulttype:
      case chartype:
          n1 = t + 1;        /* leave room for the terminating null */
          n = n1 / charsPW + ((n1 % charsPW) == 0 ? 0 : 1);
          break;
      case inttype:
          n = t * WPint;
          break;
      case realtype:         /* need to worry about size blow up here */
          {                  /* the overflow test depends on doubles having
                              * more precision than ints */
            double      p = (double) t;

            p = p * WPreal;
            if (p + v + minsize > LARGEINT)
              exit_cover(NC_INTEGER_OVERFLOW_CREATE_W);
            n = floor(p);
            break;
          }
    }
    m = hdrsize + n + v;
    if (v == 0)
      m++;                   /* space for endtag if nonatomic single */
    if (m < minsize)
      m = minsize;
  }

  z = reserve(m);
  set_kind(z, k);
  set_valence(z, v);
  set_sorted(z, false);
  tally(z) = tly;
  /* the refcnt will have been set to zero by reserve when it resets the
   * freetag field */
  if (k == chartype)
    store_char(z, n1 - 1, '\0');  /* insert the null terminating char */
  else if (k == atype) {
    /* fill up array with invalidptr's so it can be freed even if only partly
     * used because of a user interrupt. MAJ: We may not need this if we
     * restrict where checksignal is called. */
    nialptr    *it;

    it = pfirstitem(z);
    for (i = 0; i < t; i++)
      *it++ = invalidptr;    /* freeit knows not to use these ones */
  }
  else if (k == booltype) {
    /* fill boolean arrays with 0 to allow word-by-word & and | . MAJ: This
     * may be unnecessary. */
    if (v == 0)
      n = 2;

    for (i = 0; i < n; i++)
      *(pfirstint(z) + i) = 0;
  }
  /* put shape in place */
  if (v > 0) {
    sp = shpptr(z, v);
    if (v == 1)
      sp[0] = tly;
    else {
      for (i = 0; i < v; i++)
        sp[i] = shapevec[i];
      if (v > VALENCE_LIMIT) {  /* restore shape holder */
        free(shapevec);
        shapevec = holder;
      }
    }
  }

  return z;
}


/* routines to create atoms of each type */

nialptr
createint(nialint x)
{
  if (x >= 0 && x < NOINTS)
    return (intvals[x]);
  else {
    nialint     dummy;       /* need empty extents list */
    nialptr     z = new_create_array(inttype, 0, 1, &dummy);

    store_int(z, 0, x);
    return (z);
  }
}


nialptr
createbool(int x)
{
  if (x == false)
    return (False_val);
  else
    return (True_val);
}


nialptr
createchar(char c)
{
  nialptr     z;
  nialint     dummy;         /* needed for empty extents list */

  z = new_create_array(chartype, 0, 1, &dummy);
  store_char(z, 0, c);
  return (z);
}

nialptr
createreal(double x)
{
  nialptr     z;
  nialint     dummy;         /* needed for empty extents list */

  if (x == 0.)
    return (Zeror);
  z = new_create_array(realtype, 0, 1, &dummy);
  store_real(z, 0, x);
  return (z);
}


/************************* atom table routines ********************/

/* The atom table is an object table used to hold pointers to phrases
   or faults. Access to it is through a hash function applied to
   the type and the string. Collisions are handled by adjusting the
   hash by an amount that is relatively prime with the size
   of the hash table. On removing an entry, one has to leave a fake
   entry if it appears in the middle of a chain. The variable atomcnt
   keeps track of how many locations are in use. If the table becomes
   overly full it is expanded and the atoms are rehashed into the new
   table.
   The variable atomcnt keeps track of how many cells are either in use
   or held.
*/

/* routine to create either a phrase or a fault. Returns current heap
   entry if the atom already exists. Uses the hash table to determine
   if it already exists.
*/

nialptr
createatom(unsigned k, char *s)
{
  nialptr     z;
  nialint     hashv,
              posn,
              posn_to_use = 0,
              dummy;
  int         match = false, /* indicates that the atom has been found */
              cont = true,   /* control for loop */
              hashadjusted = false;


  hashv = hash(s);
  posn = hashv;
  while (cont) {
    z = atomtbl[posn];
    if (z == vacant) {       /* can store the atom reference here */
      posn_to_use = posn;
      cont = false;
    }
    else {                   /* z is held or occupied */
      if (z == held) {       /* held signifies location that has been erased.
                              * First one encountered should be used if new
                              * atom */
        posn_to_use = posn;
      }
      else {                 /* probe for a match */
        match = strcmp(pfirstchar(z), s) == 0 && kind(z) == k;
        cont = !match;
      }
    }
    if (cont) {              /* advance through the heap cyclically in steps
                              * of linadj until a vacant entry or a match
                              * occurs */
      posn = (posn + linadj) % atomtblsize;
      cont = posn != hashv;  /* stop if hashv encountered again */
      hashadjusted = true;
    }
  }
  if (match)
    return z;                /* the atom has been found. Return its array
                              * reference */

  if (!match && hashadjusted && posn == hashv) {  /* The entire atom table
                                                   * was searched and no
                                                   * vacant or held location
                                                   * was found. Normally we
                                                   * will rehash before this
                                                   * happens. Will only get
                                                   * here if the atom table
                                                   * cannot be expanded */
#ifdef DEBUG
    nprintf(OF_DEBUG, "system limitation: atom table is full\n");
    nprintf(OF_DEBUG, "atomcnt %ld maxnoatoms %d\n", atomcnt, atomtblsize/2);
    nprintf(OF_DEBUG, "linadj %ld atomtblsize %d\n", linadj, atomtblsize);
    nprintf(OF_DEBUG, "match %d posn %d hashv %d\n", match, posn, hashv);
    nabort(NC_ABORT_F);
#else
    nprintf(OF_DEBUG, "system limitation: atom table is full\n");
    nprintf(OF_DEBUG, "atomcnt %ld maxnoatoms %d\n", atomcnt, atomtblsize/2);
    /* we don't expect this to happen */
    exit_cover(NC_ATOM_TABLE_EXPAND_ERR_F);
#endif
  }

  /* Create and record new atom. */
  z = new_create_array(k, 0, strlen(s), &dummy);
  if (s)
    strcpy(pfirstchar(z), s);/* assumes s is null terminated */
  else
    exit_cover(NC_INVALID_ATOM_F);

  if (atomtbl[posn_to_use] == vacant)
    atomcnt++;               /* one more used space in atomtbl if pos was
                              * vacant, not held */
  atomtbl[posn_to_use] = z;

  if (atomcnt > atomtblsize/2 && !inrehash)
    /* table is getting full, expand it */
  {
    rehash(atomtblsize + dfatomtblsize);  /* increase size by initial size */
  }
  return (z);
}

/* routine to allocate the atom table space within the heap */

static void
allocate_atomtbl(void)
{
  nialint     i;

  /* ensure size is not a multiple of linadj which must be prime */
  atomtblsize = dfatomtblsize;
  while (atomtblsize % linadj == 0)
    atomtblsize = atomtblsize + 1;

  /* allocate the atom table in the heap and set its refcnt to 1 so it won't
   * be  cleared */
  atomtblbase = new_create_array(atype, 1, 0, &atomtblsize);
  incrrefcnt(atomtblbase);

  atomtbl = pfirstitem(atomtblbase);  /* convert to C pointer */

  /* set maximum so that if the atomtbl is more than half full it is expanded
   * to keep the number of collisions small */


  /* initialize atom table */
  for (i = 0; i < atomtblsize; i++)
    atomtbl[i] = vacant;
  atomcnt = 0;

}



/* routine to remove the entry for a phrase or fault from the atom table.
   Because collisions are handled by stepping forward in the atom table
   it is necessary to leave an indication that the posn had been in
   use if the cell at the next step is occupied by an atom or is also
   flagged as held. If the posn can be marked vacant, then all cells
   in steps above it that are marked as held can be changed to vacant.

   The variable atomcnt counts both used and held locations. If it gets
   too high the atom table is expanded to reduce the likelihood of
   collisions.
 */

static void
remove_atom(nialptr x)
{
  char       *s;
  nialint     hashv,
              posn,
              nextposn;
  nialptr     newentry;

  s = pfirstchar(x);         /* safe: no expansion possible */
  hashv = hash(s);
  posn = hashv;
  while (atomtbl[posn] != x) {
#ifdef DEBUG
    if (atomtbl[posn] == vacant) {
      nprintf(OF_DEBUG, "error in atomtbl on removing %s\n", s);
      nprintf(OF_DEBUG, "at %d\n", posn);
      nabort(NC_ABORT_F);
    }
#endif
    posn = (posn + linadj) % atomtblsize;
  }

  /* if position past posn is occupied, mark posn as held rather than vacant */
  nextposn = (posn + linadj) % atomtblsize;
  newentry = (atomtbl[nextposn] == vacant ? vacant : held);
  atomtbl[posn] = newentry;

  /* step backwards removing helds if the entry has become vacant */
  if (newentry == vacant) {
    atomcnt--;               /* one less used location in atomtable */
    posn = ((posn + atomtblsize) - linadj) % atomtblsize; /* assumes
                                                           * linadj<atomtblsize */
    while (atomtbl[posn] == held) {
      atomtbl[posn] = vacant;/* replace held with vacant */
      atomcnt--;             /* one less used location in atomtable */
      posn = ((posn + atomtblsize) - linadj) % atomtblsize;
    }
  }
}


/* The hash function for the atom table.
   From Aho, Ullman, Sethi compiler book.
*/

static      nialint
hash(char *s)
{
  unsigned    z = 0,
              g;
  char       *p;

  for (p = s; *p != '\0'; p++) {
    z = (z << 4) + (*p);
    g = z & 0xf0000000;
    if (g) {
      z = z ^ (g >> 24);
      z = z ^ g;
    }
  }
  return z % atomtblsize;
}


/* routine to expand the atom table. Allocates new space and
   then moves each atom hashing it to a new location */

static void
rehash(nialint tblsize)
{
  nialptr     t;
  nialint     i,
              oldsize,
              newhashv;
  nialptr    *oldatomtbl,
             *newarea,
              newareabase;

  nprintf(OF_MESSAGE_LOG, "extending atom table to %d\n", tblsize);

  /* ensure size is not a multiple of linadj which must be prime */
  while (tblsize % linadj == 0)
    tblsize = tblsize + 1;

  /* allocate new area */
  inrehash = true;           /* flag to prevent re-entry to rehash in
                              * recovery from an expand heap failure */
  newareabase = new_create_array(atype, 1, 0, &tblsize);
  inrehash = false;
  incrrefcnt(newareabase);
  newarea = pfirstitem(newareabase);
  oldatomtbl = atomtbl;      /* Safe since atomtbl will have been updated in
                              * case heap has moved */
  atomtbl = newarea;
  oldsize = atomtblsize;
  atomtblsize = tblsize;     /* this must be changed for hash to work */

  /* initialize new atom table */
  for (i = 0; i < atomtblsize; i++)
    atomtbl[i] = vacant;

  /* loop through old table moving the entries */
  for (i = 0; i < oldsize; i++) {
    t = oldatomtbl[i];
    /* find new hash location */
    if (t != held && t != vacant) {
      newhashv = hash(pfirstchar(t));
      while (atomtbl[newhashv] != vacant) /* assumes linadj is a prime */
        newhashv = (newhashv + linadj) % atomtblsize;
      atomtbl[newhashv] = t;
    }
  }
  decrrefcnt(atomtblbase);
  release(atomtblbase);
  atomtblbase = newareabase;
}

/* routine to make a phrase */

nialptr
makephrase(char *s)
{
  return (createatom(phrasetype, s));
}

/* routine to make a fault.
   Faults are atomic values in Nial that are used to hold special
   values and to signal error conditions.
   The interpreter has modal behaviour with respect to faults.
   If the flag triggering is on, then an interruption is signaled
   when a fault is created. The flag can be turned on and off under
   program control. The special values (?noexpr, ?eof, etc.) are
   created in the initialization and do not cause an interrupt when
   encountered.

   If a fault is created while at top level it does not cause an
   interruption; its value is returned.

   The triggering mechanism is not available if the debugging flag
   is turned off. The debugging flag is only settable on startup.
 */

nialptr
makefault(char *f)
{
  if (triggered && debugging_on)
    process_faultbreak(f);
  return createatom(faulttype, f);
}


/* routine to implement the operation phrase.
 * phrase converts any atom to a phrase. */

/* Initial size of static Duplication buffer */
#define CDupBuf_dsz 512

/* static buffer that should be fine for most things */
static char CDupBuf_d[CDupBuf_dsz];

/* This will be used (via malloc) if we exceed CDupBuf_dsz */
static char *CDupBuf = CDupBuf_d;


static int  CDupBuf_sz = 0;


#define CDupBufdup(instr) { int instr_sz = strlen(instr); \
                            if (instr_sz+1 > CDupBuf_dsz) \
                                   { CDupBuf = (char *) malloc(CDupBuf_sz = (instr_sz+1)); \
                            if (!CDupBuf) exit_cover(NC_MEM_ERR_F);\
                            } strcpy(CDupBuf,instr); }

#define CDupBufdone() { if (CDupBuf_sz) { free(CDupBuf); \
    CDupBuf = CDupBuf_d; CDupBuf_sz = 0; } }



/* routine to implement the operation phrase.
   Phrase converts an atom or string into a phrase.
   We duplicate a string argument to protect against heap movement */

void
iphrase(void)
{
  nialptr     x,
              z;

  if (kind(top) == phrasetype)
    return;                  /* already a phrase */

  /* allow any atom to be converted to a phrase */
  if (atomic(top)) {
    int         savedecor = decor;

    decor = false;
    isketch();
    ilist();
    decor = savedecor;
  }
  x = apop();
  if (tally(x) == 0)         /* special case since an empty is of atype */
    z = createatom(phrasetype, "");

  else if (kind(x) == chartype) {
    CDupBufdup(pfirstchar(x))
      z = createatom(phrasetype, CDupBuf);
    CDupBufdone()
  }

  else
    z = makefault("?type error in phrase");

  apush(z);
  freeup(x);
}

/* routine to implement the operation fault.
   Fault leaves a fault unchanged and converts a non-empty
   string or a phrase into a fault. We duplicate a string
   or phrase argument to protect against heap movement */

void
ifault(void)
{
  nialptr     x;

  if (kind(top) == faulttype)
    return;                  /* already a fault */
  x = apop();
  /* CHRIS May 8, 96 To fix Test Suite error */
  if (tally(x) == 0) {       /* could be an empty string */
    apush(makefault("?empty fault not allowed"));
  }
  else if (kind(x) == chartype || kind(x) == phrasetype) {
    nialptr     z;

    CDupBufdup(pfirstchar(x))
      z = makefault(CDupBuf);
    CDupBufdone()
      apush(z);
  }
  else
    apush(makefault("?type error in fault"));
  freeup(x);
}

/* routine to implement the operation string.
   String converts any atom to its corresponding display form */

void
istring(void)
{
  if (atomic(top)) {
    if (kind(top) == chartype)
      ilist();
    else if (kind(top) >= phrasetype) {
      nialptr     x = apop();

      mkstring(pfirstchar(x));
      freeup(x);
    }
    else {                   /* use list sketch A */
      int         savedecor = decor;

      decor = false;
      isketch();
      ilist();
      decor = savedecor;
    }
  }
  else {
    if ((kind(top) == chartype && valence(top) == 1) || top == Null)
      return;                /* argument is a string */
    freeup(apop());
    buildfault("argument to string must be an atom or string");
  }
}



/*---------Nial stack routines------------*/


/* routine to allocate space for the stack */

static void
allocate_stack(void)
{
  stklim = dfstklim;
  stkareabase = new_create_array(atype, 1, 0, &stklim);
  incrrefcnt(stkareabase);
  stkarea = pfirstitem(stkareabase);
  topstack = -1;
}


/* routine to expand the Nial stack when it is full. */

void
growstack(void)
{
  nialptr     newstackbase,
             *newstack;
  nialint     newstklim = stklim + dfstklim;

  if (isatty(0))
    nprintf(OF_MESSAGE_LOG, "stack increasing to %d\n", stklim + dfstklim);

  /* grow stack by initial size */
  /* this new_create_array is safe because the stkarea pointer is updated if
   * the heap expands. */
  newstackbase = new_create_array(atype, 1, 0, &newstklim);
  incrrefcnt(newstackbase);
  newstack = pfirstitem(newstackbase);  /* safe: allocation already done */
  /* stkarea will have been updated if heap has moved */

  /* copy using memcpy to leave refcnts untouched */
  memcpy(newstack, stkarea, stklim * sizeof(nialptr));
  decrrefcnt(stkareabase);
  release(stkareabase);
  stklim = newstklim;
  stkareabase = newstackbase;
  stkarea = newstack;
}

/* routine to call when stack is empty. Jumps to top level.
   It pretends to return a result for use in a conditional expr */

nialptr
stackempty(void)
{
#ifdef DEBUG
  nabort(NC_STACK_EMPTY_W);
#else
  exit_cover(NC_STACK_EMPTY_W);
#endif
  return Null;
}


#ifndef STACKMACROS          /* ( */

/* stack routines used in DEBUG mode. */

void
apush(nialptr x)
{
#ifdef DEBUG
  if (x <= 0 || x >= memsize) {
    nprintf(OF_DEBUG, "invalid value in gpush  %d\n", x);
    nabort(NC_ABORT_F);
  }
  if (!allocated(blockptr(x))) {
    nprintf(OF_DEBUG, "free space being pushed block\n");
    wb(blockptr(x));
    nabort(NC_ABORT_F);
  }
#endif
  {
    nialint     nts = topstack + 1;

    if (nts == stklim)
      growstack();
    stkarea[nts] = x;        /* doing the store directly so we have to
                              * increment the refcount */
    incrrefcnt(x);
    topstack = nts;
  }
}


nialptr
apop()
{
  nialptr     x;

  if (topstack == -1)
    stackempty();
  x = stkarea[topstack];
#ifdef DEBUG
  if (x <= 0 || x >= memsize) {
    nprintf(OF_DEBUG, "invalid value in gpop x %d \n", x);
    nabort(NC_ABORT_F);
  }
  if (!allocated(blockptr(x))) {
    nprintf(OF_DEBUG, "free space being popped block %d \n", blockptr(x));
    wb(blockptr(x));
    nabort(NC_ABORT_F);
  }
#endif
  stkarea[topstack] = invalidptr;
  topstack--;
  decrrefcnt(x);
  return (x);
}

#endif             /* not STACKMACROS ) */


/* routine to pop n entries off the stack into a list. It is
   made into a homogeneous array if possible */

void
mklist(nialint n)
{
  nialptr     z,
             *endz;
  nialint     i;

  if (n == 0) {              /* empty list */
    apush(Null);
  }
  else {
    if (homotype(kind(top)) && valence(top) == 0) { /* check if the items are
                                                     * all of the same type */
      int         k = kind(top);
      int         ishomo = true;

      i = 1;
      while (ishomo && i < n) {
        nialptr     it = stkarea[topstack - i];

        ishomo = kind(it) == k && valence(it) == 0;
        i++;
      }
      if (ishomo) {          /* create an array of that type and fill it from
                              * the end */
        z = new_create_array(k, 1, 0, &n);
        for (i = 0; i < n; i++) {
          nialptr     it = apop();

          copy1(z, n - i - 1, it, 0);
          freeup(it);
        }
        apush(z);
        return;
      }
    }
    /* create an atype array and fill it by popping the items */
    z = new_create_array(atype, 1, 0, &n);
    endz = pfirstitem(z) + (n - 1);
    for (i = 0; i < n; i++)
      *endz-- = fastpop();   /* avoids decrementing and incrementing refcnts */
    apush(z);
  }
}


/* routine to clear the stack on jumps to top level */

void
clearstack()
{
  while (topstack != -1)
    freeup(apop());
}

/* routine to implement the expression status.
   Values are reported in words */

void
istatus(void)
{
  nialptr     p,
              z;
  nialint     seven = 7,
              maxx,
              total,
              cnt;

  /* scan the free list to compute space free, freelist size, and largest
   * available block */
  p = fwdlink(freelisthdr);
  maxx = total = cnt = 0;
  while (p != TERMINATOR) {
    total = total + blksize(p);
    if (blksize(p) > maxx)
      maxx = blksize(p);
    p = fwdlink(p);
    cnt++;
  }

  /* create the result container and fill */
  z = new_create_array(inttype, 1, 0, &seven);
  store_int(z, 0, total);
  store_int(z, 1, maxx);
  store_int(z, 2, cnt);
  store_int(z, 3, memsize);
  store_int(z, 4, stklim);
  store_int(z, 5, atomtblsize);
  store_int(z, 6, Cbuffersize);
  apush(z);
}

/*--------routines to support filling of array containers------*/

/* routine to copy a portion of an array to another of the same kind.
  copies cnt values from x at sx to z at sz. */


void
copy(nialptr z, nialint sz, nialptr x, nialint sx, nialint cnt)
{
  int         kx = kind(x);

#ifdef DEBUG
  nialint     tx = tally(x),
              tz = tally(z);

#ifdef V4AT
  int         kz = kind(z);
  if ( kx != kz ||
      sz < 0 || (sz + cnt > tz && tz != 0 && sz + cnt > 1) || 
      sx < 0 || (sx + cnt > tx && tx != 0 && sx + cnt > 1)) {
#else
  if (sz < 0 || sz + cnt > tz || sx < 0 || sx + cnt > tx) {
#endif
    nprintf(OF_DEBUG, "invalid copy sz=%d,sx=%d,tz=%d,tx=%d,cnt=%d\n", sz, sx, tz, tx, cnt);
    nabort(NC_ABORT_F);
  }
  if (kx == phrasetype || kx == faulttype) {
    nprintf(OF_DEBUG, "attempt to use copy on atomtbl type \n");
    nabort(NC_ABORT_F);
  }
#endif


  if (kx != booltype || (sz % boolsPW == 0 && sx % boolsPW == 0
                         && cnt % boolsPW == 0)) {  /* copy can be done by
                                                     * byte moves using
                                                     * memcpy */
    void       *startx = NULL,
               *startz = NULL;
    int         nobytes = 0;

    /* compute the byte count and starting addresses */
    switch (kx) {
      case atype:
          nobytes = cnt * sizeof(int);
          startx = (pfirstitem(x)) + sx;
          startz = (pfirstitem(z)) + sz;
          break;
      case inttype:
          nobytes = cnt * sizeof(int);
          startx = (pfirstint(x)) + sx;
          startz = (pfirstint(z)) + sz;
          break;
      case chartype:
          nobytes = cnt * sizeof(char);
          startx = (pfirstchar(x)) + sx;
          startz = (pfirstchar(z)) + sz;
          break;
      case realtype:
          nobytes = cnt * sizeof(double);
          startx = (pfirstreal(x)) + sx;
          startz = (pfirstreal(z)) + sz;
          break;
      case booltype:
          nobytes = cnt / boolsPW * sizeof(int);
          startx = (pfirstint(x)) + sx / boolsPW;
          startz = (pfirstint(z)) + sz / boolsPW;
          break;
    }

    /* do the move */
    memcpy(startz, startx, nobytes);

    if (kx == atype) {       /* update reference counts for copied items */
      int         i;

      for (i = 0; i < cnt; i++)
        incrrefcnt(fetch_array(z, sz + i));
    }
    return;
  }
  else {                     /* boolean array copy */
    nialint    *startx,
               *startz;
    nialint     i,
                wds;

    if (sz % boolsPW == 0 && sx % boolsPW == 0) { /* both areas start on work
                                                   * boundary */
      wds = cnt / boolsPW;
      startx = pfirstint(x) + sx / boolsPW;
      startz = pfirstint(z) + sz / boolsPW;

      /* copy as words until last partial word */
      for (i = 0; i < wds; i++)
        *startz++ = *startx++;

      /* copy the partial word bit by bit */
      for (i = wds * boolsPW; i < cnt; i++) {
        int         it = fetch_bool(x, sx + i);

        store_bool(z, sz + i, it);
      }
    }
    else {                   /* the blocks are not aligned on word
                              * boundaries. Use bit by bit copy. */
      for (i = 0; i < cnt; i++) {
        int         it = fetch_bool(x, sx + i);

        store_bool(z, sz + i, it);
      }
    }
  }
}


/* routine to copy 1 item from one array to another of the
   same homogeneous kind or of atype. */


#ifdef DEBUG


void
copy1(nialptr z, nialint sz, nialptr x, nialint sx)
{
  int         kx = kind(x);

#ifdef DEBUG
  if (kx == phrasetype || kx == faulttype) {
    nprintf(OF_DEBUG, "attempt to use copy on atomtbl type \n");
    nabort(NC_ABORT_F);
  }
#endif
  switch (kx) {
    case atype:
        {
          nialptr     xi = fetch_array(x, sx);

          store_array(z, sz, xi);
          break;
        }
    case chartype:
        store_char(z, sz, fetch_char(x, sx));
        break;
    case booltype:
        store_bool(z, sz, fetch_bool(x, sx));
        break;
    case inttype:
        store_int(z, sz, fetch_int(x, sx));
        break;
    case realtype:
        store_real(z, sz, fetch_real(x, sx));
        break;
  }
}

#endif

 /* routine used to fetch a value which must be an array. It creates a
  * temporay container if x is not atomic and the kind is a homogeneous type.
  * All users of this routine must clean up the temporary if it is not stored
  * as part of an array or pushed on the stack. */

nialptr
fetchasarray(nialptr x, nialint i)
{
  if (atomic(x))
    return x;
  switch (kind(x)) {
    case atype:
        return fetch_array(x, i);
    case inttype:
        return createint(fetch_int(x, i));
    case booltype:
        return createbool((int) fetch_bool(x, i));
    case chartype:
        return createchar(fetch_char(x, i));
    case realtype:
        return createreal(fetch_real(x, i));
#ifdef DEBUG
    default:
        nprintf(OF_DEBUG, "wrong type in fetchasarray  %d\n", kind(x));
        nabort(NC_ABORT_F);
#endif
  }
  return (~1);               /* to make lint shut up */
}


#ifndef FETCHARRAYMACRO

/* routine to fetch an array from an atype array. This is replaced
   by a macro in the production version to get adequate performance.
*/

nialptr
fetch_array(nialptr x, nialint i)
{
#ifdef DEBUG
  if (x <= 0 || x >= memsize) {
    nprintf(OF_DEBUG, "x is int %d  \n", x);
    nprintf(OF_DEBUG, "*** wild array pointer in fetch ***\n");
    nabort(NC_ABORT_F);
    return (Nullexpr);
  }
  if (kind(x) != atype) {
    nprintf(OF_DEBUG, "kind is %d\n", kind(x));
    nprintf(OF_DEBUG, " *** wrong type in fetch ***\n");
    nabort(NC_ABORT_F);
    return (Nullexpr);
  }
#ifdef V4AT
  if (i < 0 || (i >= tally(x) && tally(x) > 0)) {
#else
  if (i < 0 || i >= tally(x)) {
#endif
    nprintf(OF_DEBUG, "*** out of range fetch ***\n");
    nabort(NC_ABORT_F);
  }
  {
    nialptr     z = *(pfirstitem(x) + i);

    if (z <= 0 || z >= memsize) {
      nprintf(OF_DEBUG, "*** invalid value fetched *** %d\n",z);
      nabort(NC_ABORT_F);
    }
    return (z);
  }
#else
  return *(pfirstitem(x) + i);
#endif
}

#endif


/* general store routine into an atype array.
   It assumes the array is new and not overwriting an
   existing array value. Caller should use replace_array
   if this is not true. This version is used in debug mode.
   A macro version  is used in the non-debug version.
   */

#ifndef STOREARRAYMACRO

void
store_array(nialptr x, nialint i, nialptr z)
{
#ifdef DEBUG
  /* checks in here to catch bugs. */
  if (x <= 1 || x >= memsize) {
    nprintf(OF_DEBUG, "*** storing into invalid array ***\n");
    nabort(NC_ABORT_F);
  }
  if (kind(x) != atype) {
    nprintf(OF_DEBUG, " *** wrong type in store ***\n");
    nabort(NC_ABORT_F);
  }
#ifdef V4AT
  if (i < 0 || (i >= tally(x) && tally(x) > 0)) {
#else
  if (i < 0 || i >= tally(x)) {
#endif
    nprintf(OF_DEBUG, "*** out of range store ***\n");
    nabort(NC_ABORT_F);
  }
  if (z <= 1 || z >= memsize) {
    nprintf(OF_DEBUG, "*** invalid array being stored ***\n");
    nabort(NC_ABORT_F);
  }
#endif
  incrrefcnt(z);
  *(pfirstitem(x) + i) = z;
}

#endif

/* routine to replace an array item in an atype array */

void
replace_array(nialptr z, nialint i, nialptr x)
{
  nialptr     y;             /* order of steps important in case x is y or an
                              * item of y */

  y = fetch_array(z, i);
  store_array(z, i, x);
  decrrefcnt(y);
  freeup(y);
}

/* routine to explode a homogeneous array into an atype one. The
  allocated array may be larger to accomodate its use in hitch and
  append. For convenience the original array is freed.
      x - the array to be exploded
      v - its valence
      t - the tally of the result container
      s - start filling the result here
      e - stop converting items of x prior to this value

*/

nialptr
explode(nialptr x, int v, nialint t, nialint s, nialint e)
{
  nialptr     z,
              xi;
  nialint     i;

  if (v == 1)                /* allow for expanding explode for val==1 */
    z = new_create_array(atype, v, 0, &t);
  else
    z = new_create_array(atype, v, 0, shpptr(x, v));
  for (i = 0; i < e; i++) {  /* DO NOT pointerize this loop. An allocate in
                              * reserve due to the fetchasarray can change
                              * the address of mem[z] */
    xi = fetchasarray(x, i);
    store_array(z, i + s, xi);
  }
  freeup(x);

  return (z);
}

/* routine to collapse an atype array to homogeneous. This is called
   if the test in homotest succeeds.  */

nialptr
implode(nialptr x)
/* maps an array of atoms of one type to a homo array. */
{
  nialptr     z,
             *topx;          /* do not initialize this until after
                              * create_array since the heap might move. */
  nialint     i,
              tallyx = tally(x);
  int         kx = kind(fetch_array(x, 0));
  int         v = valence(x);

  z = new_create_array(kx, v, 0, shpptr(x, v));
  topx = pfirstitem(x);
  for (i = 0; i < tallyx; i++) {
    nialptr     it = *topx++;

    copy1(z, i, it, 0);
  }
  freeup(x);

  return (z);
}

#ifdef DEBUG

/* routines to increment and decrement reference counts n a debug
   version. These are macros in a production version. */


void
incrrefcnt(nialptr x)
{
  nialint     rc;

  if (x <= 0) {
    nprintf(OF_DEBUG, "incrementing bad block \n");
    nabort(NC_ABORT_F);
  }
  rc = refcnt(x);

  rc++;
  set_refcnt(x, rc);
}


void
decrrefcnt(nialptr x)
{
  nialint     rc;

  rc = refcnt(x);
  if (x <= 0) {
    nprintf(OF_DEBUG, "decrementing bad block \n");
    nabort(NC_ABORT_F);
  }
  if (rc == 0) {
    nprintf(OF_DEBUG, "**** attempt to decrrefcnt below 0 \n");
    nabort(NC_ABORT_F);
  }
  else
    rc--;
  set_refcnt(x, rc);
}

#endif

/*------------ utility routines ------------- */

/* routine to select an item of the shape of an array. */

nialint
pickshape(nialptr x, nialint i)
{
  return (*(shpptr(x, valence(x)) + i));
}


/* routine to test if the shapes of two arrays are the same */

int
equalshape(nialptr x, nialptr y)
{
  int         i,
              vx = valence(x),
              vy = valence(y);
  nialint    *shxp,
             *shyp;

  shxp = shpptr(x, vx);      /* storing these shape ptrs is safe since no
                              * heap entries are created in the loop below */
  shyp = shpptr(y, vy);
  if (vx != vy)
    return false;
  for (i = 0; i < vx; i++)
    if (*shxp++ != *shyp++)
      return false;
  return true;
}


/* duplicates mkstring without the apush */
/* routine to converts the C string s into a Nial list
   of characters. Leaves the list on the stack. */

nialptr
makestring(char *s)
{
  nialint     n = strlen(s);

  if (n == 0) {
    return (Null);
  }
  else {
    nialptr     z;

    CDupBufdup(s);
    z = new_create_array(chartype, 1, 0, &n);

    strcpy(pfirstchar(z), CDupBuf);
    CDupBufdone();
    return (z);
  }
}


/* routine to converts the C string s into a Nial list
   of characters. Leaves the result on the stack.
   Since s can point at the heap, we duplicate s before allocating .*/

void
mkstring(char *s)
{
  nialint     n = strlen(s);

  if (n == 0) {
    apush(Null);
  }
  else {
    nialptr     z;

    CDupBufdup(s);
    z = new_create_array(chartype, 1, 0, &n);
    strcpy(pfirstchar(z), CDupBuf);
    CDupBufdone();
    apush(z);
  }
}

/* routine to make a Nial string from a character array that may
   contain nulls; thus we avoid use of strlen and strcpy.
   Since s can point to the heap, we duplicate it first.  */

void
mknstring(char *s, nialint n)
{
  if (n == 0) {
    apush(Null);
  }
  else {
    nialptr     z;

    /* "n" version of CdupBufdup done in line */
    if (n > CDupBuf_dsz) {
      CDupBuf = (char *) malloc(CDupBuf_sz = n);
      if (!CDupBuf)
        exit_cover(NC_MEM_ERR_F);
    }
    strncpy(CDupBuf, s, n);
    z = new_create_array(chartype, 1, 0, &n);
    strncpy(pfirstchar(z), CDupBuf, n);
    CDupBufdone()
      apush(z);
  }
}

/* routine that does an append and returns the new array. used in parse. */

nialptr
nial_extend(nialptr x, nialptr y)
{
  append(x, y);
  return (apop());
}

/* routine to build a fault and put it on the stack.
   This routine adds the '?' at the front.
   All uses could be replaced with apush(makefault(...)),
   but the ? would ave to be put into the string */

void
buildfault(char *msgptr)
{
  gcharbuf[0] = '?';         /* put in leading ? */
  strcpy(&gcharbuf[1], msgptr);
  apush(makefault(gcharbuf));
}

/* routine that is called when an invalid condition occurs during
   execution of the abstract machine. In DEBUG mode it creates a core dump
   by calling abort. In debugging work, setting a break point on nabort
   helps isolate where a crash is coming from.
*/

#ifdef DEBUG
void
nabort(int flag)
{
  nprintf(OF_DEBUG_LOG, ">>(nabort):fatal error encountered. system aborting\n");
  nprintf(OF_DEBUG_LOG, ">>with message: %s\n", NC_ErrorMessage(flag));
#ifdef UNIXSYS
  /* use kill instead of abort to get core dumped in UNIX version.  */
  nprintf(OF_DEBUG_LOG, "kill value %d \n", kill(getpid(), SIGSEGV));
#else
  if (instartup)
    longjmp(init_buf, flag);
  else
    longjmp(error_env, flag);
#endif
}

#endif


/* routine to compare a Nial phrase with a symbol in upper case */

int
equalsymbol(nialptr x, char *s)
{
  return (STRCASECMP(pfirstchar(x), s) == 0);
}

/* returns true if the array must be converted to a homogeneous array */

int
homotest(nialptr x)
{
  int         z = false;
  nialint     t = tally(x);

  if (kind(x) == atype && t > 0)     /* empties are not converted */
  { nialptr    *ptr = pfirstitem(x);  /* use of ptr is safe since no heap
                                       * creations in the loop. */
    nialint     i;
    nialptr     val = *ptr++;
    int         k = kind(val);

    /* test if the first item is atomic and of a homotype */
    z = homotype(k) && atomic(val);
    /* see if all the rest of the items are atoms of the same type */
    i = 1;
    while (z && i++ < t) {
      val = *ptr++;
      z = (k == kind(val) && atomic(val));
    }
  }
  return z;
}

/* routines to support the C buffer. */
 /* C buffer variables */

static void
allocate_Cbuffer(void)
{
  Cbuffersize = INITCBUFFERSIZE;
#ifdef DEBUG
  chkfl();
#endif
  Cbuffer = malloc(Cbuffersize);
  startCbuffer = &Cbuffer[0];
  ptrCbuffer = startCbuffer;
  finCbuffer = startCbuffer + Cbuffersize;
}


void
extendCbuffer(nialint n)
{
  int         pos = ptrCbuffer - startCbuffer;
  char       *newCbuf;
  nialint     requestsize = Cbuffersize + (n <
                                     INITCBUFFERSIZE ? INITCBUFFERSIZE : n);

#ifdef DEBUG
  nprintf(OF_DEBUG_LOG, "extending Cbuffer to %u\n", requestsize);
#endif
  newCbuf = realloc(Cbuffer, requestsize);
  if (newCbuf == NULL)
    exit_cover(NC_CBUFFER_ALLOCATION_FAILED_W);
  Cbuffer = newCbuf;
  startCbuffer = &Cbuffer[0];
  Cbuffersize = requestsize; /* change this after it succeeds */
  ptrCbuffer = startCbuffer + pos;
  finCbuffer = startCbuffer + Cbuffersize;
}




/* routine to check that there is enough free space in the heap
   to be able to proceed safely in the main loop. If not it
   attempts one expand_heap
   if expansion is allowed and expansion has not failed already.
   */


nialint
checkavailspace()
{
  nialptr     start,
              p;
  nialint     total;

#ifdef DEBUG
  int         cnt = 1;

#endif

  start = freelisthdr;
  p = fwdlink(start);
  total = blksize(start);
#ifdef DEBUG
  if (debug)
    nprintf(OF_DEBUG, " in checkavailspace: freeblock fhl %d at %d has size %d\n", cnt, start, blksize(start));
#endif
  while (p != TERMINATOR) {
    total = total + blksize(p);
    p = fwdlink(p);
  }
  if (total < MINHEAPSPACE) {

    if (firsttry && expansion) {
      firsttry = false;
      expand_heap(MINHEAPSPACE);
      firsttry = true;       /* if expand_heap fails it will not return here */
    }
    else {
      nprintf(OF_MESSAGE_LOG, "inadequate space to continue work\n");
      leavewithsavequery(NC_MEM_EXPAND_NOT_TRIED_F);
    }
  }
  return total;
}

#ifdef DEBUG

/* code to sweep all memory blocks looking for
  allocated temporary arrays. This is used in debug mode to
  discover when temporaries are left at top level. */

void
checkfortemps()
{
  nialptr     start,
              p;

  start = freelisthdr;
  p = start + blksize(start);
  while (p < memsize) {
    if (allocated(p) && refcnt(arrayptr(p)) == 0) {
      nprintf(OF_DEBUG, "temporary array %d at block %d\n", arrayptr(p), p);
      wa(arrayptr(p));
      printarray(arrayptr(p));
    }
    p = p + blksize(p);
  }
  memchk();
}

#endif             /* DEBUG */

/* routine to end a Nial session, but query the user whether to save
   the workspace in "continue.nws". This is used only on an emergency
   exit due to lack of space.
*/

static void
leavewithsavequery(int rc)
{
  char        ch;


  nprintf(OF_MESSAGE_LOG, "workspace is %d words\n", memsize);
  nprintf(OF_NORMAL_LOG, "Do you wish to save the continue workspace (y/n)? : ");
  if (NC_readchar(&ch)) {
    if (ch == 'y') {
      continueflag = true;
      nprintf(OF_MESSAGE_LOG, "workspace saved in continue.nws\n");
      leavenial(rc);
      return;
    }
  }
  continueflag = false;
  nprintf(OF_MESSAGE_LOG, "workspace not saved\n");
  leavenial(rc);
}


