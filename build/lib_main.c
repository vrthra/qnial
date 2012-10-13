/*=============================================================

MODULE  LIB_MAIN.C

  COPYRIGHT NIAL Systems Limited  1983-2005

Does the initialization of the Q'Nial abstract machine and calls
initialization routines for many aspects of the interpreter.

================================================================*/

/* Q'Nial file that selects features and loads the xxxspecs.h file */

#include "switches.h"

/* standard library header files */

#define SIGLIB
#define IOLIB
#define STLIB
#define MATHLIB
#define STDLIB
#define SJLIB
#include "sysincl.h"

/* Q'Nial header files */

#include "coreif_p.h"        /* for messages and other things */
#include "qniallim.h"        /* must go before lib_main.h */
#include "lib_main.h"
#include "absmach.h"
#include "basics.h"
#include "if.h"

#include "version.h"

#include "mainlp.h"          /* for exit_cover */
#include "fileio.h"          /* for nprintf etc. */
#include "roles.h"           /* role codes */
#include "symtab.h"          /* symbol table macros */
#include "blders.h"          /* for b_basic in initialization */
#include "wsmanage.h"        /* for wsload */
#include "parse.h"           /* parse tree node tags */
#include "lexical.h"         /* for BLANK */


/* prototypes for local routines */

static void sysinit(void);
static void init_res_words(void);
static void do_init(char *name, nialptr * globevar);
static void init_fncodes(void);
static void set_cseq(char *cseq);
static void banner(char *b);
static void signon(void);
static void makeinitial(void);

/* Declare the global struct defined in lib_main.c that holds global
   variables shared between modules.
*/

struct global_vars1 G1;


/* Declare the global struct that contains values that are saved in a 
   workspace dump. A struct is used so that only a single read and write
   is needed to load or save the information. 
*/

struct global_vars G;

static nialint nmcnt,
            bincnt;          /* globals used to assign indices to basic names
                                across multiple calls to init_bnames. */

static char qcr[] =
"Copyright (c) NIAL Systems Limited";

/* routine to do the initialization */

int
do_init_work()
{
  /* call initialization code for various features. These routines are
   * setting global C data for later use. */

  setup_nialroot(nialrootname);

  /* invert the collating sequence */
  set_cseq(collatingseq);    /* given in specs.h file for each version */

  initfpsignal();            /* initializes signal handler for floating point
                                exceptions. */

  /* print the version and copywright banners */

  signon();

#ifdef TIMEHOOK
  inittime();                /* initialize time for session */
#endif


  /* set up the heap, stack and buffer areas */

  setup_abstract_machine(initmemsize);

  /* now load or create the starting workspace */

  if (!nialboot(loadsw, false, true)) /* workspace setup */
    return NC_WS_OPEN_FAILED_S;

  instartup = false;         /* startup process finished. It is now safe to
                                use exit_cover */
  return 0;

}


/* routine to implement Clearws primitive expression */

void
iclearws(void)
{
  longjmp(error_env, NC_CLEARWS_N);
}


/* routine to implement Restart primitive expression */

void
irestart(void)
{ 
  longjmp(error_env, NC_RESTART_N);
}


/* routine to load or create the starting workspace and to
   initalize workspace variables used by the interpreter.
   The first parameter indicates whether the initial ws is to
   be the clearws.nws or a named one, the second indicates whether
   this is a restart or the original boot.

   The result indicates whether the boot succeeded. This is only needed
   in calls from NC_Init_Nial to avoid the longjmp used for error handling
   when called from NC_CommandInterpret.
*/

int
nialboot(int wsgiven, int clearsw, int firsttime)
{
  FILE       *wsport = OPENFAILED;  /* starting value */

  if (!firsttime) {          /* reinitialize the abstract machine */
    clear_abstract_machine();
    setup_abstract_machine(initmemsize);
  }
  if (wsgiven) {     /* requesting the user named workspace */
    strcpy(gcharbuf, loadfnm);
    check_ext(gcharbuf, ".nws",FORCE_EXTENSION);  
         /* user can give abbreviated name. check_ext extends it if needed */
    wsport = openfile(gcharbuf, 'r', 'b');
    if (wsport == OPENFAILED) {
      nprintf(OF_NORMAL_LOG, "failed to open workspace %s\n", gcharbuf);
      if (instartup)
        return false;
      exit_cover(NC_WS_OPEN_FAILED_S);
    }
  }
  else {
    /* if first time through or if a continue found on an earlier startup and
       this is a restart, then look for the continue workspace in the local
       directory */
    if (!clearsw && (firsttime || continuefound)) {
      strcpy(gcharbuf, "continue.nws");
      wsport = openfile(gcharbuf, 'r', 'b');
      continuefound = !(wsport == OPENFAILED);
    }
    /* if continue.nws not found or this is a clearws request find or make
     * the clearws. */
    if (clearsw || !continuefound) {  /* look for the clear workspace in the
                                       * local directory */
      strcpy(gcharbuf, "clearws.nws");
      wsport = openfile(gcharbuf, 'r', 'b');
      if (wsport == OPENFAILED) { /* look for the clearws in the root directory */
        strcpy(gcharbuf, nialrootname); /* this contains the separator */
        strcat(gcharbuf, "clearws.nws");
        wsport = openfile(gcharbuf, 'r', 'b');
        if (wsport == OPENFAILED) { /* create the inital workspace */
          if (!quiet)
            nprintf(OF_NORMAL_LOG, "making the initial workspace\n");
          makeinitial();
#ifdef DEBUG
          memchk();
          if (debug)
            nprintf(OF_DEBUG, "initial workspace loaded -  memchk done\n");
#endif
          goto finishboot;
        }
      }
    }
    else if (wsport == OPENFAILED) {  /* don't expect to get here */
      nprintf(OF_NORMAL_LOG, "failed to open a starting workspace\n");
      if (instartup)
        return false;
      exit_cover(NC_WS_OPEN_FAILED_S);
    }
  }
  /* load the workspace that has been opened */
  wsload(wsport, false);
  if (!quiet)
    nprintf(OF_NORMAL_LOG, "starting with workspace %s\n", gcharbuf);

#ifdef DEBUG
  memchk();
  if (debug)
    nprintf(OF_DEBUG, "initial workspace loaded -  memchk done\n");
#endif

finishboot:
  /* start the file system housekeeping */
  startfilesystem();

  /* record that the interpreter is running for a restart */
  interpreter_running = true;

  return true;
}

/* routine to construct the banner */

static void
banner(char *b)
{
  strcpy(b, "Q'Nial (");
  strcat(b, special);
  strcat(b, " Edition)");
  strcat(b, nialversion);
  strcat(b, machine);
  strcat(b, opsys);
  strcat(b, debugstatus);
  strcat(b, " ");
  strcat(b, __DATE__);
}

/* routine to write the banner and copyright */

static void
signon()
{
  if (!quiet) {
    char        versionbuf[1000];

    banner(versionbuf);
    writechars(STDOUT, versionbuf, (nialint) strlen(versionbuf), true);
    writechars(STDOUT, qcr, (nialint) strlen(qcr), true);
  }
}

/* routine to implement primitive expression Version */

void
iversion(void)
{
  char versionbuf[120];
  banner(versionbuf);
  mkstring(versionbuf);
}

/* routine to implement primitive expression Copyright */

void
icopyright(void)
{
  mkstring(qcr);
}

/* routine to implement primitive expression Nialroot */

void
inialroot(void)
{
  mkstring(nialrootname);
}

/* routine to implement primitive expression System */


void
isystem(void)
{
  apush(makephrase(systemname));
}

/* routine to implement primitive expression Atversion */

void
iatversion(void)
{
#ifdef V4AT
  apush(makephrase("V4AT"));
#else
  apush(makephrase("V6AT"));
#endif
}

/* routines to create primitive routines that return faults for 
   undefined expressions, operations and trs */

void
ino_expr(void)
{
  buildfault("missing_expr");
}

void
ino_op(void)
{
  freeup(apop());
  buildfault("missing_op");
}

void
ino_tr(void)
{
  freeup(apop());
  apop();
  buildfault("missing_tr");
}

/* primitive constant expressions */

void
inull(void)
{
  apush(Null);
}

void
itrue(void)
{
  apush(True_val);
}

void
ifalse(void)
{
  apush(False_val);
}

/* routine sysinit initializes Nial variables for the initial workspace. They are 
   stored in the struct G and saved in a workspace dump.
*/

static void
sysinit()
{
  nialptr     n;
  int         i;
  nialint     zero = 0,
              one = 1;
  nialint     dummy;         /* used as a ptr to an empty list of ints */

  topstack = -1;

#ifdef DEBUG
  memchk();
#endif

  /* set up initial arrays */

/* put in initial ints. Used to avoid constructing frequent small values */
  for (i = 0; i < NOINTS; i++) {
    n = new_create_array(inttype, 0, 1, &dummy);
    store_int(n, 0, i);
    intvals[i] = n;
    incrrefcnt(n);
  }

  Zero = intvals[0];
  One = intvals[1];
  Two = intvals[2];

  Blank = createchar(BLANK);
  incrrefcnt(Blank);

/* Null made here, so it can be used in maketkn for initial properties
   of phrases and faults */

  Null = new_create_array(atype, 1, 0, &zero);
#ifdef V4AT
  store_array(Null,0,Zero);
#endif
  incrrefcnt(Null);

  grounded = createatom(faulttype, "?Grnd");
  incrrefcnt(grounded);

  True_val = new_create_array(booltype, 0, 1, &dummy);
  store_bool(True_val, 0, true);
  incrrefcnt(True_val);

  False_val = new_create_array(booltype, 0, 1, &dummy);
  store_bool(False_val, 0, false);
  incrrefcnt(False_val);

  Voidstr = new_create_array(chartype, 1, 0, &one);
  store_char(Voidstr, 0, '\0');
  incrrefcnt(Voidstr);

  Nullexpr = createatom(faulttype, "?noexpr");
  incrrefcnt(Nullexpr);

  r_EOL = createatom(phrasetype, "E O L");
  incrrefcnt(r_EOL);

  Nulltree = b_nulltree();
  incrrefcnt(Nulltree);

  { double      r = 0.0;
    Zeror = new_create_array(realtype, 0, 1, &dummy);
    store_real(Zeror, 0, r);
    incrrefcnt(Zeror);
  }

  Typicalphrase = makephrase("\"");
  incrrefcnt(Typicalphrase);

  Eoffault = createatom(faulttype, "?eof");
  incrrefcnt(Eoffault);

  Zenith = createatom(faulttype, "?I");
  incrrefcnt(Zenith);

  Nadir = createatom(faulttype, "?O");
  incrrefcnt(Nadir);

  no_value = createatom(faulttype, "?no_value");
  incrrefcnt(no_value);

  Logical = createatom(faulttype, "?L");
  incrrefcnt(Logical);


#ifdef DEBUG
  memchk();
#endif

  /* set up the global symbol table */

  global_symtab = addsymtab(stpglobal, "GLOBAL");
  incrrefcnt(global_symtab);
  current_env = Null;

  /* Debug lists initialized */
  breaklist = Null;
  incrrefcnt(breaklist);
  watchlist = Null;
  incrrefcnt(watchlist);

#ifdef CALLDLL
  dlllist = Null;
  incrrefcnt(dlllist);
#endif

  /* initialize the list of reserved words */
  init_res_words();

  nmcnt = 0;
  bincnt = 0;

/* call the routine generated in basics.c that initializes the
   primitives tables.
*/

  initprims();     /* defined in the generated file basics.c */

  init_fncodes();            /* initializes parse tree nodes
                                for internally referenced Nial objects */

#ifdef DEBUG
  memchk();
#endif

}

#if defined(UNIXSYS)
#define initialpath "initial/"

#elif defined(WINDOWS_SYS)
#define initialpath "initial\\"

#else
#error define initialpath for this system
#endif

#include "resnms.h"

/* routine to set up reserved words in initial ws.
   The info is in table reswords, which is generated from
   reserved.h. The latter gives a defined name
   to each reserved word as strings in upper case.
   The defined names are used in the parser so that changes
   to the reserved words can be made easily.
*/

static void
init_res_words()
{
  int         i;

  for (i = 0; i < NORESWORDS; i++)
    mkSymtabEntry(global_symtab, makephrase(reswords[i]), Rres, no_value, true);
}

/* routine to initialize a C variable to a parse tree node */

static void
do_init(char *name, nialptr * globevar)
{
  nialptr     tkn,
              sym,
              entr;

  tkn = makephrase(name);
  entr = lookup(tkn, &sym, passive);
  if (entr == notfound) {
    nprintf(OF_MESSAGE, "Error in init_fncodes\n");
    nprintf(OF_NORMAL_LOG, name);
  }
  else {
    *globevar = sym_valu(entr);
    incrrefcnt(*globevar);
  }
}

/* looks up function table pointers and stores them in global variables
for the evaluator. Done after initializing basics so that it can
link an internal code with a nial basic expression, operation or
transformer. The internal code is used to access the definition
using a parse tree rather than a C pointer, allowing them to be
fed to the nial evaluation mechanism.

In the SHELL versions of the interpreter the symbols like + and *
are replaced by names like SUM and PRODUCT.
*/

static void
init_fncodes()
{
#if defined(V4SHELL) || defined(V6SHELL)
  do_init("NO_EXPR", &no_excode);
  do_init("NO_OP", &no_opcode);
  do_init("NO_TR", &no_trcode);
  do_init("_LTE", &ltecode);
  do_init("_GTE", &gtecode);
  do_init("_LEAF", &leafcode);
  do_init("_TWIG", &twigcode);
  do_init("_SUM", &sumcode);
  do_init("_PRODUCT", &productcode);
  do_init("_MIN", &mincode);
  do_init("_MAX", &maxcode);
  do_init("_AND", &andcode);
  do_init("_OR", &orcode);
  do_init("_LINK", &linkcode);
  do_init("_PLUS", &pluscode);
  do_init("_TIMES", &timescode);
  do_init("_FIRST", &firstcode);
  do_init("_SINGLE", &singlecode);
  do_init("_CONVERSE", &conversecode);
  do_init("_UP", &upcode);
  do_init("_EQUAL", &equalcode);
  do_init("_EACHRIGHT", &eachrightcode);
#else
  do_init("NO_EXPR", &no_excode);
  do_init("NO_OP", &no_opcode);
  do_init("NO_TR", &no_trcode);
  do_init("<=", &ltecode);
  do_init(">=", &gtecode);
  do_init("LEAF", &leafcode);
  do_init("TWIG", &twigcode);
  do_init("+", &sumcode);
  do_init("*", &productcode);
  do_init("MIN", &mincode);
  do_init("MAX", &maxcode);
  do_init("AND", &andcode);
  do_init("OR", &orcode);
  do_init("LINK", &linkcode);
  do_init("PLUS", &pluscode);
  do_init("TIMES", &timescode);
  do_init("FIRST", &firstcode);
  do_init("SINGLE", &singlecode);
  do_init("CONVERSE", &conversecode);
  do_init("UP", &upcode);
  do_init("=", &equalcode);
  do_init("EACHRIGHT", &eachrightcode);
#endif
}

/* routine to install the Nial name corresponding to the internal C
routine for a primitive.  Called in init_prims() */

void
init_primname(char *opname, char prop)
{
  nialptr     id,
              tree,
              role = -1;

  switch (prop) {
    case 'B':                /* binary not pervasive */
    case 'C':                /* binary pervasive */
    case 'P':                /* unary pervasive */
    case 'R':                /* multi pervasive */
    case 'U':                /* unary not pervasive */
        role = Roptn;
        break;
    case 'E':
        role = Rexpr;
        break;
    case 'T':
        role = Rtrans;
        break;
    default:
        nprintf(OF_MESSAGE, "invalid entry in init_primname\n");
  }
  id = makephrase(opname);
  tree = b_basic(nmcnt, role, prop, bincnt);
  mkSymtabEntry(global_symtab, id, role, tree, true);
  bnames[nmcnt] = id;
  incrrefcnt(id);
  nmcnt++;                   /* counter for unary routines */
  if (prop == 'B' || prop == 'C' || prop == 'R')
    bincnt++;                /* these operation classes also have a binary
                              * routine */
}

/* routine to invert the collating sequence for rapid comparison
   of character ordering.
*/

static void
set_cseq(char *cseq)
{
  int         i;
  int         len = strlen(cseq);

  for (i = 0; i < HIGHCHAR - LOWCHAR + 1; i++)
    invseq[i] = i;
  for (i = 0; i < len; i++)
    invseq[cseq[i] - LOWCHAR] = i + 32;
}


/* routine to create the initial workspace:
  - call sysinit to set up abstract machine and initialize some constants
  - attempt to load "defs.ndf" from a local initial directory, otherwise
  - attempt to load "defs.ndf" from the initial directory at NIALROOT,
    otherwise
  - use built-in defs.ndf, which corresponds to the default file for defs.ndf.
   
MAJ: Consider removing the use of the "initial" subdirectory and always using the
    the internal defs file.
 */

static void
makeinitial()
{
  doingclearws = true;
  sysinit();
  doingclearws = false;

  /* look first for a local initial directory */
  strcpy(gcharbuf, initialpath);
  strcat(gcharbuf, sysdefsfnm);
  if (!loaddefs(true, gcharbuf, 0)) { /* silent loaddefs from local initial.
                                        If not found, look for one under nialroot */
    strcpy(gcharbuf, nialrootname);
    strcat(gcharbuf, initialpath);
    strcat(gcharbuf, sysdefsfnm);
    if (!loaddefs(true, gcharbuf, 0)) /* silent loaddefs for system initial */
      loaddefs(false, "", 0);  /* silent loaddefs of internal code */
  }

#ifdef DEBUG
  if (debug)
    nprintf(OF_DEBUG, " finished loaddefs \n");
  memchk();
#endif
  clearheap();               /* to remove anything left over */
}
