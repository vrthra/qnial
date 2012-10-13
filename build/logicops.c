/*==============================================================

  MODULE   LOGICOPS.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains logic operation primitives.

================================================================*/



/* Q'Nial file that selects features and loads the xxxspecs.h file */

#include "switches.h"

/* standard library header files are loaded by sysincl.h */

#define CLIB
#define IOLIB
#define STDLIB
#define STLIB
#define SJLIB
#include "sysincl.h"


/* Q'Nial header files */

#include "logicops.h"
#include "qniallim.h"
#include "lib_main.h"
#include "absmach.h"
#include "basics.h"

#include "utils.h"           /* for converters */
#include "trs.h"             /* for int_each etc */
#include "ops.h"             /* for splitfb and simple */
#include "faults.h"          /* for Logical fault */


/* declaration of internal static routines */

static void orboolvectors(nialptr x, nialptr y, nialptr z, nialint n);
static void andboolvectors(nialptr x, nialptr y, nialptr z, nialint n);
static void notbools(nialptr x, nialptr z, nialint n);
static void initbool(nialptr x, int v);

/* routine to implement the binary pervading operation or.
   This is called by ior when adding pairs of arrays.
   It uses supplementary routines to permit vector processing
   if such support routines are available.
*/

void
b_or()
{
  nialptr     y = apop(),
              x = apop(),
              z;
  int         kx = kind(x),
              ky = kind(y);

  if (kx == booltype && ky == booltype
      && (atomic(x) || atomic(y) || equalshape(x, y))) {
    if (atomic(x)) {
      if (atomic(y))
        z = createbool(boolval(x) || boolval(y));
      else if (boolval(x)) {
        int         v = valence(y);

        z = new_create_array(booltype, v, 0, shpptr(y, v));
        initbool(z, 1);
      }
      else
        z = y;
    }
    else if (atomic(y)) {
      if (boolval(y)) {
        int         v = valence(x);

        z = new_create_array(booltype, v, 0, shpptr(x, v));
        initbool(z, 1);
      }
      else
        z = x;
    }
    else {
      int         v = valence(x);
      nialint     tx = tally(x);

      z = new_create_array(booltype, v, 0, shpptr(x, v));
      orboolvectors(x, y, z, tx);
    }
  }
  else
   /* handle remaining cases */ 
  if (atomic(x) && atomic(y)) 
  { if (kx == faulttype)
    { if (ky == faulttype)
      { if (x == y)
          z = x;
        else
          z = Logical;
      }
      else
#ifdef V4AT
        z = Logical;
#else
        z = x;
#endif
    }
    else 
    if (ky == faulttype)
#ifdef V4AT
      z = Logical;
#else
      z = y;
#endif
    else                     /* other types cause a fault */
      z = Logical;
  }
  else {
    int_eachboth(b_or, x, y);
    return;
  }
  apush(z);
  freeup(x);
  freeup(y);
}

/* unary version of or */

void
ior()
{
  nialptr     x;
  nialint     tx;
  int         kx;

  x = apop();
  kx = kind(x);
  tx = tally(x);
  switch (kx) {
    case booltype:
        apush(createbool(orbools(x, tx)));
        break;
    case inttype:
    case chartype:
    case realtype:
    case phrasetype:
        apush(Logical);
        break;
    case faulttype:
        apush(x);
        break;
    case atype:
        if (tx == 0)
#ifdef V4AT
        { nialptr archetype = fetch_array(x,0);
          if (atomic(archetype))
          { if (kind(archetype)==booltype)
              { apush(False_val); }
            else
              apush(Logical);
          }
          else
          { apush(x);
            ipack();
            int_each(ior,apop());
            return;
          }
        }
#else
        {
          apush(False_val);
        }
#endif
        else if (simple(x)) {/* has non-numeric items */
          apush(testfaults(x, Logical));
        }
        else if (tx == 2) {
          apush(fetch_array(x, 0));
          apush(fetch_array(x, 1));
          b_or();
        }
        else {
          apush(x);
          ipack();
          int_each(ior, apop());
          return;
        }
        break;
  }
  freeup(x);
}

/* support routines for or 
  Does it word by word except for partial word at the end 
*/

int
orbools(nialptr x, nialint n)
{
  nialint     i = 0,
             *ptrx = pfirstint(x),  /* safe: no allocations */
              s = false,
              wds = n / boolsPW;

  while (!s && i++ < wds)
    s = *ptrx++ != 0;        /* at least one bit is on */
  i = wds * boolsPW;
  while (!s && i < n) {
    s = fetch_bool(x, i);
    i++;                     /* don't combine with above. macro uses i twice*/
  }
  return (s);
}

static void
orboolvectors(nialptr x, nialptr y, nialptr z, nialint n)
{
  nialint     i,
             *ptrx = pfirstint(x),  /* safe: no allocations */
             *ptry = pfirstint(y),  /* safe: no allocations */
             *ptrz = pfirstint(z),  /* safe: no allocations */
              wds = n / boolsPW;

  for (i = 0; i < wds; i++)
    *ptrz++ = *ptrx++ | *ptry++;
  for (i = wds * boolsPW; i < n; i++) {
    int         it = fetch_bool(x, i);  /* spread out to avoid Sun C compiler
                                         * problem */

    it = it || fetch_bool(y, i);
    store_bool(z, i, it);
  }
}

static void
initbool(nialptr x, int v)
{
  nialint     i,
             *ptrx = pfirstint(x),  /* safe: no allocations */
              wv = (v ? ALLBITSON : 0),
              n = tally(x),
              wds = n / boolsPW;

  for (i = 0; i < wds; i++)
    *ptrx++ = wv;
  for (i = wds * boolsPW; i < n; i++)
    store_bool(x, i, v);
}

/* routines to implement and. Same algorithm as ior */

void
b_and()
{
  nialptr     y = apop(),
              x = apop(),
              z;
  int         kx = kind(x),
              ky = kind(y);

  if (kx == booltype && ky == booltype
      && (atomic(x) || atomic(y) || equalshape(x, y))) {
    if (atomic(x)) {
      if (atomic(y))
        z = createbool(boolval(x) && boolval(y));
      else if (boolval(x))
        z = y;
      else {
        int         v = valence(y);

        z = new_create_array(booltype, v, 0, shpptr(y, v));
        initbool(z, 0);
      }
    }
    else if (atomic(y)) {
      if (boolval(y))
        z = x;
      else {
        int         v = valence(x);

        z = new_create_array(booltype, v, 0, shpptr(x, v));
        initbool(z, 0);
      }
    }
    else {
      int         v = valence(x);
      nialint     tx = tally(x);

      z = new_create_array(booltype, v, 0, shpptr(x, v));
      andboolvectors(x, y, z, tx);
    }
  }
  else
   /* handle remaining cases */ 
  if (atomic(x) && atomic(y)) 
  { if (kx == faulttype)
    { if (ky == faulttype)
      { if (x == y)
           z = x;
         else
           z = Logical;
      }
      else
#ifdef V4AT
        z = Logical;
#else
        z = x;
#endif
    }
    else 
    if (ky == faulttype)
#ifdef V4AT
      z = Logical;
#else
      z = y;
#endif
    else                     /* other types cause a fault */
      z = Logical;
  }
  else {
    int_eachboth(b_and, x, y);
    return;
  }
  apush(z);
  freeup(x);
  freeup(y);
}

void
iand()
{
  nialptr     x;
  nialint     tx;
  int         kx;

  x = apop();
  kx = kind(x);
  tx = tally(x);
  switch (kx) {
    case booltype:
        apush(createbool(andbools(x, tx)));
        break;
    case inttype:
    case chartype:
    case realtype:
    case phrasetype:
        apush(Logical);
        break;
    case faulttype:
        apush(x);
        break;
    case atype:
        if (tx == 0) 
#ifdef V4AT
        { nialptr archetype = fetch_array(x,0);
          if (atomic(archetype))
          { if (kind(archetype)==booltype)
              { apush(True_val); }
            else
              apush(Logical);
          }
          else
          { apush(x);
            ipack();
            int_each(iand,apop());
            return;
          }
        }
#else
        {
          apush(True_val);
        }
#endif
        else if (simple(x)) {/* has non-numeric items */
          apush(testfaults(x, Logical));
        }
        else if (tx == 2) {
          apush(fetch_array(x, 0));
          apush(fetch_array(x, 1));
          b_and();
        }
        else {
          apush(x);
          ipack();
          int_each(iand, apop());
          return;
        }
        break;
  }
  freeup(x);
}

/* support routines for and. 
   Does it word by word except for partial word at the end 
*/

int
andbools(nialptr x, nialint n)
{
  nialint     i = 0,
             *ptrx = pfirstint(x),  /* safe: no allocations */
              s = true,
              wds = n / boolsPW;

  while (s && i++ < wds)
    s = *ptrx++ == ALLBITSON;/* all bits are on */
  i = wds * boolsPW;
  while (s && i < n) {
    s = fetch_bool(x, i);
    i++;                     /* don't combine with above. macro uses i twice */
  }
  return (s);
}

static void
andboolvectors(nialptr x, nialptr y, nialptr z, nialint n)
{
  nialint     i,
             *ptrx = pfirstint(x),  /* safe: no allocations */
             *ptry = pfirstint(y),  /* safe: no allocations */
             *ptrz = pfirstint(z),  /* safe: no allocations */
              wds = n / boolsPW;

  for (i = 0; i < wds; i++)
    *ptrz++ = *ptrx++ & *ptry++;
  for (i = wds * boolsPW; i < n; i++) {
    int         it = fetch_bool(x, i);

    it = it && fetch_bool(y, i);
    store_bool(z, i, it);
  }
}

/* routine to implement not. Same algorithm as abs in arith.c */

void
inot()
{
  nialptr     z,
              x = apop();
  int         k = kind(x),
              v = valence(x);
  nialint     t = tally(x);

  switch (k) {
    case booltype:
        z = new_create_array(booltype, v, 0, shpptr(x, v));
        notbools(x, z, t);
        apush(z);
        freeup(x);
        break;
    case inttype:
    case realtype:
    case chartype:
        if (atomic(x)) {
          apush(Logical);
          freeup(x);
        }
        else
          int_each(inot, x);
        break;
    case phrasetype:
        apush(Logical);
        break;
    case faulttype:
        apush(x);
        break;
    case atype:
        int_each(inot, x);
        break;
  }
}

/* support routines for not 
   Does it word by word except for partial word at end.
*/

static void
notbools(nialptr x, nialptr z, nialint n)
{
  nialint     i,
              wds = n / boolsPW,
             *ptrx = pfirstint(x),  /* safe: no allocations */
             *ptrz = pfirstint(z);  /* safe: no allocations */

  for (i = 0; i < wds; i++)
    *ptrz++ = ~(*ptrx++);
  for (i = wds * boolsPW; i < n; i++) {
    int         it = !fetch_bool(x, i);

    store_bool(z, i, it);
  }
}
