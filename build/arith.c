/*==============================================================

  MODULE   ARITH.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains the arithmetic operation primitives.

================================================================*/


/* Q'Nial file that selects features and loads the xxxspecs.h file */

#include "switches.h"

/* standard library header files are loaded by sysincl.h */

#define MATHLIB
#define STDLIB
#define IOLIB
#define LIMITLIB
#define SJLIB
#include "sysincl.h"


/* Q'Nial header files */

#include "arith.h"
#include "qniallim.h"
#include "lib_main.h"
#include "absmach.h"
#include "basics.h"
#include "trs.h"             /* needed for int_each etc. */
#include "utils.h"           /* conversion utilities */
#include "ops.h"             /* needed for simple, pair etc. */
#include "faults.h"          /* definition of Faults used here */


/* declaration of internal static routines */


static void nial_plus(nialptr x, nialptr y);
static void nial_minus(nialptr x, nialptr y);
static void nial_times(nialptr x, nialptr y);
static void nial_divide(nialptr x, nialptr y);
static int  sumbools(nialptr x, nialint n);
static double sumreals(double *ptrx, nialint n);
static int  addintvectors(nialint * x, nialint * y, nialint * z, nialint n);
static int  addintscalarvector(nialint x, nialint * y, nialint * z, nialint n);
static void addrealvectors(double *x, double *y, double *z, nialint n);
static void addrealscalarvector(double x, double *y, double *z, nialint n);
static int  prodbools(nialptr x, nialint n);
static double prodreals(double *ptrx, nialint n);
static int  multintvectors(nialint * x, nialint * y, nialint * z, nialint n);
static int  multintscalarvector(nialint x, nialint * y, nialint * z, nialint n);
static void multrealvectors(double *x, double *y, double *z, nialint n);
static void multrealscalarvector(double x, double *y, double *z, nialint n);
static int  floorreals(double *ptrx, nialint * ptrz, nialint n);
static int  subintvectors(nialint * x, nialint * y, nialint * z, nialint n);
static void subrealvectors(double *x, double *y, double *z, nialint n);
static int  subintscalarvector(nialint x, nialint * y, nialint * z, nialint n, int negate);
static void subrealscalarvector(double x, double *y, double *z, nialint n, int negate);
static void divrealvectors(double *x, double *y, double *z, nialint n);
static void divrealscalarvector(double x, double *y, double *z, nialint n, int negate);
static void quotientintvectors(nialint * x, nialint * y, nialint * z, nialint n);
static void quotientintscalarvector(nialint x, nialint * y, nialint * z, nialint n, int negate);
static void modintvectors(nialint * x, nialint * y, nialint * z, nialint n);
static void modintscalarvector(nialint x, nialint * y, nialint * z, nialint n, int negate);
static nialint entier(double r);
static void absints(nialint * ptrx, nialint * ptrz, nialint n);
static void absreals(double *ptrx, double *ptrz, nialint n);
static void randomreals(double *ptrz, nialint n);
static double frand(void);


/*  routines to implement binary addition and the sum operation >
    The equations for sum are:
    sum N = N , for a scalar number N
    sum S = the sum of the numbers in a simple array S
    sum A = EACH sum pack A, where pack trims the items of
    A to the same size and then flips them. i.e.
    sum (2 3 4) (5 6 7) =
    EACH sum (2 5) (3 6) (4 7)

    The routine b_plus does the binary addition if the arguments are presented
    separately, or if the argument to sum is not homogeneous and has tally 2.
    The routine isum sums homogenous arrays and uses the 3rd identity
    otherwise.


   This is called by isum when adding pairs of arrays.
   It uses supplementary routines to isolate vectro arithmetic
   so that specialized routines for vector processing can be used
   if such support routines are available.
   */

void
b_plus()
{
  nialptr     y = apop(),
              x = apop();

  nial_plus(x, y);
}

/* routine used by b_plus, iplus and isum on a pair */

static void
nial_plus(nialptr x, nialptr y)
{
  nialptr     z;
  int         kx = kind(x),
              ky = kind(y);
  int         res = false;

  if (kx == ky && valence(x) == 0 && valence(y) == 0) {
    if (kx == inttype) {     /* special case for integer addition */
      nialint     s,
                  xv = intval(x),
                  yv = intval(y);

      s = xv + yv;
      /* test if s has wrapped */
      if ((xv >= 0 && yv > 0 && s < xv) || (xv <= 0 && yv < 0 && s > xv))
        z = makefault("?Integer overflow");
      else
        z = createint(s);
      goto finish;
    }
    else if (kx == realtype) {  /* special case for real addition */
      z = createreal(realval(x) + realval(y));
      goto finish;
    }
  }
  if (numeric(kx) && numeric(ky)) {
    if (kx == ky)
      res = true;
    else if (kx < ky)
      res = convert(&x, &kx, ky); /* convert x to y's type */
    else                     /* (ky < kx) */
      res = convert(&y, &ky, kx); /* convert y to x's type */
  }
  if (res                    /* the arguments are of the same numeric type */
      && (atomic(x) || atomic(y) || equalshape(x, y))) {
    switch (kx) {
      case booltype:
          convert(&x, &kx, inttype);
          convert(&y, &ky, inttype);
          /* fall through to inttype case */
      case inttype:
          {
            if (atomic(x)) {
              /* no longer needed if (atomic(y)) { double s = (double)
               * intval(x) + (double) intval(y); if (s<SMALLINT ||
               * s>LARGEINT) z = makefault("?Integer overflow"); else z =
               * createint((nialint) floor(s)); } else */
              {
                int         v = valence(y);
                nialint     ty = tally(y);

                z = new_create_array(inttype, v, 0, shpptr(y, v));
                if (!addintscalarvector(intval(x), pfirstint(y),
                                        pfirstint(z), ty)) {
                  freeup(z);
                  goto use_eachboth;
                }
              }
            }
            else {
              int         v = valence(x);
              nialint     tx = tally(x);

              z = new_create_array(inttype, v, 0, shpptr(x, v));
              if (atomic(y)) {
                if (!addintscalarvector(intval(y), pfirstint(x),
                                        pfirstint(z), tx)) {
                  freeup(z);
                  goto use_eachboth;
                }
              }
              else if (!addintvectors(pfirstint(x), pfirstint(y),
                                      pfirstint(z), tx)) {
                freeup(z);
                goto use_eachboth;
              }
            }
          }
          break;
      case realtype:
          {
            if (atomic(x)) {
              /* no longer needed if (atomic(y)) z =
               * createreal(realval(x)+realval(y)); else */
              {
                int         v = valence(y);
                nialint     ty = tally(y);

                z = new_create_array(realtype, v, 0, shpptr(y, v));
                addrealscalarvector(realval(x), pfirstreal(y), pfirstreal(z), ty);
              }
            }
            else {
              int         v = valence(x);
              nialint     tx = tally(x);

              z = new_create_array(realtype, v, 0, shpptr(x, v));
              if (atomic(y))
                addrealscalarvector(realval(y), pfirstreal(x), pfirstreal(z), tx);
              else
                addrealvectors(pfirstreal(x), pfirstreal(y), pfirstreal(z), tx);
            }
          }
          break;
      default:
          goto use_eachboth;
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
          z = Arith;
      }
      else
#ifdef V4AT
      if (!numeric(ky))
        z = Arith;
      else
        z = x;
#else
          z = x; 
#endif
    }
    else 
    if (ky == faulttype)
#ifdef V4AT
    { if (!numeric(kx))
        z = Arith;
      else
        z = y;
    }
#else
      z = y;
#endif
    else                     /* chartype or phrasetype cause a fault */
      z = Arith;
  }
  else
  {
use_eachboth:
    int_eachboth(b_plus, x, y);
    return;
  }
finish:
  apush(z);
  freeup(x);
  freeup(y);
}

/* the unary version of the primitive */

void
isum()
{
  nialptr     x;
  nialint     tx;
  int         kx;

  x = apop();
retry:
  kx = kind(x);
  tx = tally(x);
  switch (kx) {
    case booltype:
        apush(createint(sumbools(x, tx)));
        break;
    case inttype:
        {
          nialint     res;

          if (sumints(pfirstint(x), tx, &res)) {
            apush(createint(res));
          }
          else {
            apush(makefault("?Integer overflow"));
          }
          break;
        }
    case realtype:
        apush(createreal(sumreals(pfirstreal(x), tx)));
        break;
    case chartype:
    case phrasetype:
        apush(Arith);
        break;
    case faulttype:
        apush(x);
        break;
    case atype:
        if (tx == 0) 
#ifdef V4AT
        { nialptr archetype;
          archetype = fetch_array(x,0);
          apush(archetype);
          apush(Zero);
          b_times();
        }
#else
        {
          apush(Zero);
        }
#endif
        else if (simple(x)) {
          int         newk;

          x = arithconvert(x, &newk); /* returns the highest type value in
                                       * newk and converts x to this type if
                                       * numeric */
          if (numeric(newk))
            goto retry;      /* conversion has created a homogeneous array */
          if (newk == faulttype) {
            apush(testfaults(x, Arith));
          }
          else
            apush(Arith);
        }
        else if (tx == 2)
          nial_plus(fetch_array(x, 0), fetch_array(x, 1));
        else {
          apush(x);
          ipack();
          int_each(isum, apop());
          return;
        }
        break;
  }
  freeup(x);
}

/* routines to actually do summations and vector additions. These
   are separated out so that they can be replaced by calls on
   library routines for vector hardware, or parallel machines.
   */

static int
sumbools(nialptr x, nialint n)
{
  nialint     i,
              s = 0;

  for (i = 0; i < n; i++)
    s += fetch_bool(x, i);
  return (s);
}

int
sumints(nialint * ptrx, nialint n, nialint * res)
{
  nialint     i;
  nialint     s = 0;

  for (i = 0; i < n; i++) {  /* special case for integer addition */
    nialint     xv = s,
                yv = *ptrx++;

    s = xv + yv;
    /* test if s has wrapped */
    if ((xv >= 0 && yv > 0 && s < xv) || (xv <= 0 && yv < 0 && s > xv))
      return false;
  }
  *res = s;

  return true;
}

static double
sumreals(double *ptrx, nialint n)
{
  nialint     i;
  double      s = 0.0;

  for (i = 0; i < n; i++)
    s += *ptrx++;
  return (s);
}

static int
addintvectors(nialint * x, nialint * y, nialint * z, nialint n)
{
  nialint     i;

  for (i = 0; i < n; i++) {  /* special case for integer addition */
    nialint     s,
                xv = *x++,
                yv = *y++;

    s = xv + yv;
    /* test if s has wrapped */
    if ((xv >= 0 && yv > 0 && s < xv) || (xv <= 0 && yv < 0 && s > xv))
      return false;
    *z++ = s;
  }
  return (true);
}

static int
addintscalarvector(nialint x, nialint * y, nialint * z, nialint n)
{
  nialint     i;

  for (i = 0; i < n; i++) {  /* special case for integer addition */
    nialint     s,
                xv = x,
                yv = *y++;

    s = xv + yv;
    /* test if s has wrapped */
    if ((xv >= 0 && yv > 0 && s < xv) || (xv <= 0 && yv < 0 && s > xv))
      return false;
    *z++ = s;
  }
  return (true);
}

static void
addrealvectors(double *x, double *y, double *z, nialint n)
{
  nialint     i;

  for (i = 0; i < n; i++)
    *z++ = *x++ + *y++;
}

static void
addrealscalarvector(double x, double *y, double *z, nialint n)
{
  nialint     i;

  for (i = 0; i < n; i++)
    *z++ = x + *y++;
}

/* routine to implement the binary pervading operation times.
   This is called by iproduct when multiplying pairs of arrays.
   It uses supplementary routines to permit vector processing
   if such support routines are available.
   The structure is symmetric with that of b_plus */

void
b_times()
{
  nialptr     y = apop(),
              x = apop();

  nial_times(x, y);
}

/* routine used by b_times, itimes, iproduct on a pair */

static void
nial_times(nialptr x, nialptr y)
{
  nialptr     z;
  int         kx = kind(x),
              ky = kind(y);
  int         res = false;

  if (kx == ky && valence(x) == 0 && valence(y) == 0) {
    if (kx == inttype) {
      double      s = (double) intval(x) * (double) intval(y);

      if (s < SMALLINT || s > LARGEINT)
        z = makefault("?Integer overflow");
      else
        z = createint((nialint) floor(s));
      goto finish;
    }
    else if (kx == realtype) {  /* special case for real multiplication */
      z = createreal(realval(x) * realval(y));
      goto finish;
    }
  }
  if (numeric(kx) && numeric(ky)) {
    if (kx < ky)
      res = convert(&x, &kx, ky); /* convert x to y's type */
    else if (ky < kx)
      res = convert(&y, &ky, kx); /* convert y to x's type */
    else
      res = true;
  }
  if (res                    /* the arguments are of the same numeric type */
      && (atomic(x) || atomic(y) || equalshape(x, y))) {
    switch (kx) {
      case booltype:
          convert(&x, &kx, inttype);
          convert(&y, &ky, inttype);
          /* fall through to inttype case */
      case inttype:
          {
            if (atomic(x)) {
              /* not needed if (atomic(y)) { double s = (double) intval(x) *
               * (double) intval(y); if (s<SMALLINT || s>LARGEINT) z =
               * makefault("?Integer overflow"); else z = createint((nialint)
               * floor(s)); } else */
              {
                int         v = valence(y);
                nialint     ty = tally(y);

                z = new_create_array(inttype, v, 0, shpptr(y, v));
                if (!multintscalarvector(intval(x), pfirstint(y),
                                         pfirstint(z), ty)) {
                  freeup(z);
                  goto use_eachboth;
                }
              }
            }
            else {
              int         v = valence(x);
              nialint     tx = tally(x);

              z = new_create_array(inttype, v, 0, shpptr(x, v));
              if (atomic(y)) {
                if (!multintscalarvector(intval(y), pfirstint(x),
                                         pfirstint(z), tx)) {
                  freeup(z);
                  goto use_eachboth;
                }
              }
              else if (!multintvectors(pfirstint(x), pfirstint(y),
                                       pfirstint(z), tx)) {
                freeup(z);
                goto use_eachboth;
              }
            }
          }
          break;
      case realtype:
          {
            if (atomic(x)) {
              if (atomic(y))
                z = createreal(realval(x) * realval(y));
              else {
                int         v = valence(y);
                nialint     ty = tally(y);

                z = new_create_array(realtype, v, 0, shpptr(y, v));
                multrealscalarvector(realval(x), pfirstreal(y), pfirstreal(z), ty);
              }
            }
            else {
              int         v = valence(x);
              nialint     tx = tally(x);

              z = new_create_array(realtype, v, 0, shpptr(x, v));
              if (atomic(y))
                multrealscalarvector(realval(y), pfirstreal(x), pfirstreal(z), tx);
              else
                multrealvectors(pfirstreal(x), pfirstreal(y), pfirstreal(z), tx);
            }
          }
          break;
      default:
          goto use_eachboth;
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
          z = Arith; 
      }
      else
#ifdef V4AT
      if (!numeric(ky))
        z = Arith;
      else
        z = x;
#else
        z = x; 
#endif
    }
    else 
    if (ky == faulttype)
#ifdef V4AT
    { if (!numeric(kx))
        z = Arith;
      else
        z = y;
    }
#else
      z = y;
#endif
    else                     /* chartype or phrasetype cause a fault */
      z = Arith;
  }
  else {
use_eachboth:
    int_eachboth(b_times, x, y);
    return;
  }
finish:
  apush(z);
  freeup(x);
  freeup(y);
}


/* product implements the operation product.  The equations for prod
   are analogous to those for sum and the structure of the
   algorithm is identical with appropriate substitutions.  */


void
iproduct()
{
  nialptr     x;
  nialint     tx;
  int         kx;

  x = apop();
retry:
  kx = kind(x);
  tx = tally(x);
  switch (kx) {
    case booltype:
        apush(createint(prodbools(x, tx)));
        break;
    case inttype:
        {
          nialint     res;

          if (prodints(pfirstint(x), tx, &res)) {
            apush(createint(res));
          }
          else {
            apush(makefault("?Integer overflow"));
          }
          break;
        }
    case realtype:
        apush(createreal(prodreals(pfirstreal(x), tx)));

        break;
    case chartype:
    case phrasetype:
        apush(Arith);
        break;
    case faulttype:
        apush(x);
        break;
    case atype:
        if (tx == 0) 
#ifdef V4AT
        { apush(fetch_array(x,0));
          apush(Zero);
          b_times();
          apush(One);
          b_plus();
        }
#else
        {
          apush(One);
        }
#endif
        else if (simple(x)) {
          int         newk;

          x = arithconvert(x, &newk); /* returns the highest type value in
                                       * newk and converts x to this type if
                                       * numeric */
          if (numeric(newk))
            goto retry;      /* conversion has created a homogeneous array */
          if (newk == faulttype) {
            apush(testfaults(x, Arith));
          }
          else
            apush(Arith);
        }
        else if (tx == 2)
          nial_times(fetch_array(x, 0), fetch_array(x, 1));
        else {
          apush(x);
          ipack();
          int_each(iproduct, apop());
          return;
        }
        break;
  }
  freeup(x);
}

/* unary version of plus restricted to a pair. */

void
iplus()
{
  if (kind(top) == faulttype)
    return;
  if (tally(top) != 2) {
    freeup(apop());
    apush(makefault("?plus expects a pair"));
  }
  else {
    nialptr     x,
                y,
                z;

    z = apop();
    splitfb(z, &x, &y);
    nial_plus(x, y);
    freeup(z);
  }
}

/* unary version of times restricted to a pair. */

void
itimes()
{
  if (kind(top) == faulttype)
    return;
  if (tally(top) != 2) {
    freeup(apop());
    apush(makefault("?times expects a pair"));
  }
  else {
    nialptr     x,
                y,
                z;

    z = apop();
    splitfb(z, &x, &y);
    nial_times(x, y);
    freeup(z);
  }
}


/* routines for products and vector multiplications. Separated
   out for the same reason as for the summation routines. */

static int
prodbools(nialptr x, nialint n)
{
  nialint     i,
              s = 1;

  for (i = 0; i < n; i++)
    s *= fetch_bool(x, i);
  return (s);
}


int
prodints(nialint * ptrx, nialint n, nialint * res)
{
  nialint     i;
  double      s = 1.0;

  for (i = 0; i < n; i++) {
    s *= (double) *ptrx++;
    if (s < SMALLINT || s > LARGEINT)
      return false;
  }
  *res = (nialint) floor(s);

  return true;
}

static double
prodreals(double *ptrx, nialint n)
{
  nialint     i;
  double      s = 1.0;

  for (i = 0; i < n; i++)
    s *= *ptrx++;
  return (s);
}


static int
multintvectors(nialint * x, nialint * y, nialint * z, nialint n)
{
  nialint     i;

  for (i = 0; i < n; i++) {
    double      s = (double) *x++;

    s *= (double) *y++;
    if (s < SMALLINT || s > LARGEINT) /* blowup required */
      return (false);
    *z++ = (nialint) floor(s);
  }
  return (true);
}

static int
multintscalarvector(nialint x, nialint * y, nialint * z, nialint n)
{
  nialint     i;

  for (i = 0; i < n; i++) {
    double      s = (double) x;

    s *= (double) *y++;
    if (s < SMALLINT || s > LARGEINT) /* blowup required */
      return (false);
    *z++ = (nialint) floor(s);
  }
  return (true);
}


static void
multrealvectors(double *x, double *y, double *z, nialint n)
{
  nialint     i;

  for (i = 0; i < n; i++)
    *z++ = *x++ * *y++;
}

static void
multrealscalarvector(double x, double *y, double *z, nialint n)
{
  nialint     i;

  for (i = 0; i < n; i++)
    *z++ = x * *y++;
}

/* routine to implement the binary pervading operation minus.
   It uses supplementary routines to permit vector processing
   if such support routines are available.
   */

void
b_minus()
{
  nialptr     y = apop(),
              x = apop();

  nial_minus(x, y);
}

static void
nial_minus(nialptr x, nialptr y)
{
  nialptr     z;
  int         kx = kind(x),
              ky = kind(y);
  int         res = false;

  if (kx == ky && valence(x) == 0 && valence(y) == 0) {
    if (kx == inttype) {     /* special case for integer subtraction */
      nialint     s,
                  xv = intval(x),
                  yv = intval(y);

      s = xv - yv;
      /* test if s has wrapped */
      if ((xv >= 0 && yv < 0 && s < xv) || (xv <= 0 && yv > 0 && s > xv))
        z = makefault("?Integer overflow");
      else
        z = createint(s);
      goto finish;
    }
    else if (kx == realtype) {  /* special case for real subtraction */
      z = createreal(realval(x) - realval(y));
      goto finish;
    }
  }
  if (numeric(kx) && numeric(ky)) {
    if (kx < ky)
      res = convert(&x, &kx, ky); /* convert x to y's type */
    else if (ky < kx)
      res = convert(&y, &ky, kx); /* convert y to x's type */
    else
      res = true;
  }
  if (res                    /* the arguments are of the same numeric type */
      && (atomic(x) || atomic(y) || equalshape(x, y))) {
    switch (kx) {
      case booltype:
          convert(&x, &kx, inttype);
          convert(&y, &ky, inttype);
          /* fall through to inttype case */
      case inttype:
          {
            if (atomic(x)) {
              /* no longer needed if (atomic(y)) { double s = (double)
               * intval(x) - (double) intval(y); if (s<SMALLINT ||
               * s>LARGEINT) z = makefault("?Integer overflow"); else z =
               * createint((nialint) floor(s)); } else */
              {
                int         v = valence(y);
                nialint     ty = tally(y);

                z = new_create_array(inttype, v, 0, shpptr(y, v));
                if (!subintscalarvector(intval(x), pfirstint(y),
                                        pfirstint(z), ty, false)) {
                  freeup(z);
                  goto use_eachboth;
                }
              }
            }
            else {
              int         v = valence(x);
              nialint     tx = tally(x);

              z = new_create_array(inttype, v, 0, shpptr(x, v));
              if (atomic(y)) {
                if (!subintscalarvector(intval(y), pfirstint(x),
                                        pfirstint(z), tx, true)) {
                  freeup(z);
                  goto use_eachboth;
                }
              }
              else if (!subintvectors(pfirstint(x), pfirstint(y),
                                      pfirstint(z), tx)) {
                freeup(z);
                goto use_eachboth;
              }
            }
          }
          break;
      case realtype:
          {
            if (atomic(x)) {
              /* no longer needed if (atomic(y)) z =
               * createreal(realval(x)-realval(y)); else */
              {
                int         v = valence(y);
                nialint     ty = tally(y);

                z = new_create_array(realtype, v, 0, shpptr(y, v));
                subrealscalarvector(realval(x), pfirstreal(y), pfirstreal(z), ty, false);
              }
            }
            else {
              int         v = valence(x);
              nialint     tx = tally(x);

              z = new_create_array(realtype, v, 0, shpptr(x, v));
              if (atomic(y))
                subrealscalarvector(realval(y), pfirstreal(x), pfirstreal(z), tx, true);
              else
                subrealvectors(pfirstreal(x), pfirstreal(y), pfirstreal(z), tx);
            }
          }
          break;
      default:
          goto use_eachboth;
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
          z = Arith; 
      }
      else
#ifdef V4AT
      if (!numeric(ky))
        z = Arith;
      else
        z = x;
#else
        z = x; 
#endif
    }
    else 
    if (ky == faulttype)
#ifdef V4AT
    { if (!numeric(kx))
        z = Arith;
      else
        z = y;
    }
#else
      z = y;
#endif
    else                     /* chartype or phrasetype cause a fault */
      z = Arith;
  }
  else {
use_eachboth:
    int_eachboth(b_minus, x, y);
    return;
  }
finish:
  apush(z);
  freeup(x);
  freeup(y);
}




/* routine to implement the prefix calls to the
   binary pervading operation minus.
   */

void
iminus()
{
  nialptr     z;

  if (kind(top) == faulttype)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?minus expects a pair"));
  }
  else {
    nialptr     x,
                y;

    splitfb(z, &x, &y);
    nial_minus(x, y);
  }
  freeup(z);
}


static int
subintvectors(nialint * x, nialint * y, nialint * z, nialint n)
{
  nialint     i;

  for (i = 0; i < n; i++) {  /* special case for integer subtraction */
    nialint     s,
                xv = *x++,
                yv = *y++;

    s = xv - yv;
    /* test if s has wrapped */
    if ((xv >= 0 && yv < 0 && s < xv) || (xv <= 0 && yv > 0 && s > xv))
      return false;
    *z++ = s;
  }
  return (true);
}

static int
subintscalarvector(nialint x, nialint * y, nialint * z, nialint n, int negate)
{
  nialint     i;

  if (negate) {
    for (i = 0; i < n; i++) {/* special case for integer subtraction */
      nialint     s,
                  xv = *y++,
                  yv = x;

      s = xv - yv;
      /* test if s has wrapped */
      if ((xv >= 0 && yv < 0 && s < xv) || (xv <= 0 && yv > 0 && s > xv))
        return false;
      *z++ = s;
    }
  }
  else {
    for (i = 0; i < n; i++) {/* special case for integer subtraction */
      nialint     s,
                  xv = x,
                  yv = *y++;

      s = xv - yv;
      /* test if s has wrapped */
      if ((xv >= 0 && yv < 0 && s < xv) || (xv <= 0 && yv > 0 && s > xv))
        return false;
      *z++ = s;
    }
  }
  return (true);
}


static void
subrealvectors(double *x, double *y, double *z, nialint n)
{
  nialint     i;

  for (i = 0; i < n; i++)
    *z++ = *x++ - *y++;
}


static void
subrealscalarvector(double x, double *y, double *z, nialint n, int negate)
{
  nialint     i;

  if (negate) {
    for (i = 0; i < n; i++)
      *z++ = *y++ - x;
  }
  else {
    for (i = 0; i < n; i++)
      *z++ = x - *y++;
  }
}

/* routines to support division. They structurally symmetric with those
   for subtraction. */

void
b_divide()
{
  nialptr     y = apop(),
              x = apop();

  nial_divide(x, y);
}

static void
nial_divide(nialptr x, nialptr y)
{
  nialptr     z;
  int         kx = kind(x),
              ky = kind(y);
  int         res = false;

  if (numeric(kx) && numeric(ky)) {
    if (kx < ky)
      res = convert(&x, &kx, ky); /* convert x to y's type */
    else if (ky < kx)
      res = convert(&y, &ky, kx); /* convert y to x's type */
    else
      res = true;
  }
  if (res                    /* the arguments are of the same numeric type */
      && (atomic(x) || atomic(y) || equalshape(x, y))) {
    switch (kx) {
      case booltype:
          convert(&x, &kx, realtype);
          convert(&y, &ky, realtype);
          goto joinreal;
      case inttype:
          convert(&x, &kx, realtype);
          convert(&y, &ky, realtype);
          /* fall through to realtype case */
      case realtype:
      joinreal:
          {
            nialint     i,
                        t1 = tally(y);
            double     *yptr = pfirstreal(y); /* ptr is safe. Only used
                                               * locally */

            for (i = 0; i < t1; i++) {
              double      r = *yptr++;

              if (r == 0. && !(atomic(x) && atomic(y)))
                goto use_eachboth;
            }
            if (atomic(x)) {
              if (atomic(y)) {
                if (realval(y) == 0.0)
                  z = Divzero;
                else
                  z = createreal(realval(x) / realval(y));
              }
              else {
                int         v = valence(y);
                nialint     ty = tally(y);

                z = new_create_array(realtype, v, 0, shpptr(y, v));
                divrealscalarvector(realval(x), pfirstreal(y), pfirstreal(z), ty, false);
              }
            }
            else {
              int         v = valence(x);
              nialint     tx = tally(x);

              z = new_create_array(realtype, v, 0, shpptr(x, v));
              if (atomic(y))
                divrealscalarvector(realval(y), pfirstreal(x), pfirstreal(z), tx, true);
              else
                divrealvectors(pfirstreal(x), pfirstreal(y), pfirstreal(z), tx);
            }
          }
          break;
      default:
          goto use_eachboth;
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
          z = Arith; 
      }
      else
#ifdef V4AT
      if (!numeric(ky)|| y==Zero || y==Zeror || y==False_val)
        z = Arith;
      else
        z = x;
#else
        z = x; 
#endif
    }
    else 
    if (ky == faulttype)
#ifdef V4AT
    { if (!numeric(kx))
        z = Arith;
      else
        z = y;
    }
#else
      z = y;
#endif
    else                     /* chartype or phrasetype cause a fault */
/*
#ifdef V4AT
    if (y==Zero || y ==Zeror || y == False_val)
      z = Divzero;
    else
#endif
*/
      z = Arith;
  }
  else {
use_eachboth:
    int_eachboth(b_divide, x, y);
    return;
  }
  apush(z);
  freeup(x);
  freeup(y);
}

/* routine to implement the prefix calls to the
   binary pervading operation divide.
   */

void
idivide()
{
  nialptr     z;

  if (kind(top) == faulttype)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?divide expects a pair"));
  }
  else {
    nialptr     x,
                y;

    splitfb(z, &x, &y);
    nial_divide(x, y);
  }
  freeup(z);
}

static void
divrealvectors(double *x, double *y, double *z, nialint n)
{
  nialint     i;

  for (i = 0; i < n; i++)
    *z++ = *x++ / *y++;
}

static void
divrealscalarvector(double x, double *y, double *z, nialint n, int reciprocate)
{
  nialint     i;

  if (reciprocate) {
    for (i = 0; i < n; i++)
      *z++ = *y++ / x;
  }
  else {
    for (i = 0; i < n; i++)
      *z++ = x / *y++;
  }
}


/* routine to implement the monadic pervasive operations abs
   Satisfies:    abs A = EACH abs A
   Algorithm:
   switch on kind of argument:
   if numeric apply abs to the items in a routine
   if literal return appropriate fault
   if nested use the equation.
   */

void
iabs()
{
  nialptr     z,
              x = apop();
  int         k = kind(x),
              v = valence(x);
  nialint     t = tally(x);

  switch (k) {
    case booltype:
        apush(boolstoints(x));
        freeup(x);
        break;
    case inttype:
        z = new_create_array(inttype, v, 0, shpptr(x, v));
        absints(pfirstint(x), pfirstint(z), t);
        apush(z);
        freeup(x);
        break;
    case realtype:
        z = new_create_array(realtype, v, 0, shpptr(x, v));
        absreals(pfirstreal(x), pfirstreal(z), t);
        apush(z);
        freeup(x);
        break;
    case chartype:
        if (atomic(x)) {
          apush(Arith);
          freeup(x);
        }
        else
          int_each(iabs, x);

        break;
    case phrasetype:
        apush(Arith);
        break;
    case faulttype:
        apush(x);
        break;
    case atype:
        int_each(iabs, x);
        break;
  }
}


static void
absints(nialint * ptrx, nialint * ptrz, nialint n)
{
  nialint     i,
              it;

  for (i = 0; i < n; i++) {
    it = *ptrx++;
    *ptrz++ = (it < 0 ? -it : it);
  }
}

static void
absreals(double *ptrx, double *ptrz, nialint n)
{
  nialint     i;
  double      it;

  for (i = 0; i < n; i++) {
    it = *ptrx++;
    *ptrz++ = (it < 0 ? -it : it);
  }
}

/* routine to implement floor. It has the same
   structure and algorithm as abs */

void
ifloor()
{
  nialptr     z,
              x = apop();
  int         k = kind(x),
              v = valence(x);
  nialint     t = tally(x);

  switch (k) {
    case booltype:
        apush(boolstoints(x));
        freeup(x);
        break;
    case inttype:
        apush(x);
        break;
    case realtype:
        z = new_create_array(inttype, v, 0, shpptr(x, v));
        if (floorreals(pfirstreal(x), pfirstint(z), t)) {
          apush(z);
          freeup(x);
          break;
        }
        else {
          freeup(z);
          apush(makefault("?Integer overflow"));
          freeup(x);
          break;
        }
    case chartype:
        if (atomic(x)) {
          apush(Arith);
          freeup(x);
        }
        else
          int_each(ifloor, x);
        break;
    case phrasetype:
        apush(Arith);
        break;
    case faulttype:
        apush(x);
        break;
    case atype:
        int_each(ifloor, x);
        break;
  }
}

static int
floorreals(double *ptrx, nialint * ptrz, nialint n)
{
  nialint     i;
  double      it;

  for (i = 0; i < n; i++) {
    it = *ptrx++;
    if (it >= (LARGEINT + 1) || it < SMALLINT)
      return (false);
    /* do the truncation */
    *ptrz++ = floor(it);
  }
  return (true);
}

/* routine to implement the pervasive operation type, which returns
   a characteristic atom for each atom class */

void
itype()
{
  nialptr     x = apop();

  if (atomic(x)) {
    switch (kind(x)) {
      case booltype:
          apush(False_val);
          break;
      case inttype:
          apush(Zero);
          break;
      case realtype:
          apush(Zeror);
          break;
      case chartype:
          apush(Blank);
          break;
      case phrasetype:
          apush(Typicalphrase);
          break;
      case faulttype:
          apush(Cipher);
          break;
    }
    freeup(x);
  }
  else
    int_each(itype, x);
}


/* routine to implement the pervasive operation char which maps
   an integer onto the character that the integer represents in the
   coding scheme for the system.
   */

void
ichar()
{
  nialptr     x = apop();

  if (atomic(x)) {
    switch (kind(x)) {
      case inttype:
          apush(createchar((char) intval(x)));
          break;
      case booltype:
      case realtype:
      case chartype:
      case phrasetype:
          apush(makefault("?not an integer"));
          break;
      case faulttype:
          apush(x);
          break;
    }
    freeup(x);
  }
  else if (kind(x) == inttype) {
    int         v = valence(x);
    nialint     i,
                tx = tally(x);
    nialptr     z = new_create_array(chartype, v, 0, shpptr(x, v));

    for (i = 0; i < tx; i++)
      store_char(z, i, (char) fetch_int(x, i));
    apush(z);
    freeup(x);
  }
  else
    int_each(ichar, x);
}

/* routine to implement charrep which is the pervasive inverse of char */

void
icharrep()
{
  nialptr     x = apop();

  if (atomic(x)) {
    switch (kind(x)) {
      case chartype:
          {
            int         c = charval(x);

/* MAJ removed. Caused char charep S = S identity to fail
            if (c < 0)
              c = HIGHCHAR + 1 + c;
*/
            apush(createint((nialint) c));
            break;
          }
      case booltype:
      case inttype:
      case realtype:
      case phrasetype:
          apush(makefault("?not a character"));

          break;
      case faulttype:
          apush(x);
          break;
    }
    freeup(x);
  }
  else if (kind(x) == chartype) {
    int         v = valence(x);
    nialint     i,
                tx = tally(x);
    nialptr     z = new_create_array(inttype, v, 0, shpptr(x, v));

    for (i = 0; i < tx; i++) {
      int         c = fetch_char(x, i);

/* MAJ removed. Caused char charep S = S identity to fail
      if (c < 0)
        c = HIGHCHAR + 1 + c;
*/
      store_int(z, i, (nialint) c);
    }
    apush(z);
    freeup(x);
  }
  else
    int_each(icharrep, x);
}

/* utility routine used in conjunction with the library routine floor */


static      nialint
entier(double r)
{                            /* finds largest integer contained in r */
  nialint     x;
  double      f;

  if (r > LARGEINT)
    r = LARGEINT;
  if (r < SMALLINT)
    r = SMALLINT;
  f = floor(r);
  x = f;
  return (x);
}


/* other arithmetic routines constructed from implemented ones */

void
iopposite()
{
#ifdef V4AT 
  if (kind(top)==faulttype)
    return;
#endif
  if (!atomic(top))
    int_each(iopposite,apop());
  else
  { pair(Zero, apop());
    iminus();
  }
}


void
ireciprocal()
{
#ifdef V4AT 
  if (kind(top)==faulttype)
    return;
#endif
  if (!atomic(top))
    int_each(ireciprocal,apop());
  else
  { pair(One, apop());
    idivide();
  }
}

void
iceiling()
{
  iopposite();
  ifloor();
  iopposite();
}





/* routine to implement the binary pervading operation power
   This routine is complicated by the fact that the algorithm is
   different if the second item is an integer and in that case,
   if the first item is integer then the result may be either integer
   or real depending on the sign of the second item.
   There is no major speed advantage in having a special binary routine.
   */

void
b_power()
{
  nialptr     y = apop(),
              x = apop();

  pair(x, y);
  ipower();
}

void
ipower()
{
  nialptr     x = apop();
  nialint     tx = tally(x);
  int         newk,
              i,
              kx = kind(x);

  if (kind(x) == faulttype) {
    apush(x);
    return;
  }
  if (tx != 2) {
    apush(makefault("?power expects a pair"));
    freeup(x);
    return;
  }
  if (kx == atype && simple(x)) {
    nialptr     x0,
                x1;

    x0 = fetch_array(x, 0);
    x1 = fetch_array(x, 1);
    if (kind(x1) == inttype) {
      switch (kind(x0)) {
        case booltype:
            if (intval(x1) == 0) {
              apush(One);
            }
            else
              apush(createint(boolval(x0)));
            break;
        case realtype:
            {
              double      y = 1.0,
                          ix0 = realval(x0);
              int         ix1 = intval(x1),
                          negpower = ix1 < 0;

              if (negpower)
                ix1 = (-ix1);
              for (i = 0; i < ix1; i++) 
              {
                y *= ix0;
              }
              if (negpower)
              { if (y == 0.) 
                  { apush(Divzero); }
                else 
                  { apush(createreal(1.0 / y)); }
              }
              else
                { apush(createreal(y)); }
              break;
            }
        case chartype:
        case phrasetype:
            apush(Arith);
            break;
        case faulttype:
            apush(x0);
            break;           /* keep same fault */
      }
      freeup(x);
      return;
    }
    x = arithconvert(x, &newk); /* returns the highest type value in newk and
                                 * converts x to this type if numeric */
  }
  switch (kind(x)) {         /* switch on converted kind */
    case booltype:
        if (fetch_bool(x, 1) == 0) {
          apush(One);
        }
        else
          apush(createint(fetch_bool(x, 0)));
        break;
    case inttype:
        {
          double      n = 1.0;
          nialint     ix0 = fetch_int(x, 0),
                      ix1 = fetch_int(x, 1);
          int         negpower = ix1 < 0;

          if (negpower)
            ix1 = (-ix1);
          for (i = 0; i < ix1; i++)
            n *= (double) ix0;
          if (n < SMALLINT || n > LARGEINT) 
            { apush(makefault("?Integer overflow")); }
          else 
          if (negpower)
          { if (n == 0.0) 
              { apush(Divzero); }
            else 
              { apush(createreal(1.0 / n)); }
          }
          else
            { apush(createint((nialint) floor(n))); }
          break;
        }
    case realtype:
        {
          double      r,
                      r0 = fetch_real(x, 0),
                      r1 = fetch_real(x, 1);

          if (r0 == 0.) {    /* to avoid calling ln with 0. */
            if (r1 != 0.) {
              apush(Zeror);
            }
            else
              apush(createreal(1.0)); /* for consistency */
            freeup(x);
            return;
          }
          if (r0 > 0) {
            r = exp((r1) * log(r0));
            apush(createreal(r));
          }
          else if (r0 <= 0) {
            if (r1 == floor(r1)) {
              apush(createreal(r0));
              apush(createint(entier(r1)));
              b_power();
            }
            else
              apush(makefault("?power"));
          }
          else
            apush(makefault("?power"));
          break;
        }
    case chartype:
        apush(Arith);
        break;
    case atype:
        if (simple(x))       /* has non-numeric items */
        { if (newk == faulttype)
            { apush(testbinfaults(x, Arith, false)); }
          else 
            { apush(Arith); }
        }
        else
        { nialptr     x0,
                      x1;

          x0 = fetch_array(x, 0);
          x1 = fetch_array(x, 1);
          int_eachboth(b_power, x0, x1);
        }
        break;
  }
  freeup(x);
}

/* implements the binary pervading quotient operation */

void
b_quotient()
{
  nialptr     y = apop(),
              x = apop(),
              z = Null;
  int         kx = kind(x),
              ky = kind(y);
  int         res = false;

  if (numeric(kx) && numeric(ky)) {
    if (kx < ky)
      res = convert(&x, &kx, ky); /* convert x to y's type */
    else if (ky < kx)
      res = convert(&y, &ky, kx); /* convert y to x's type */
    else
      res = true;
  }
  if (res                    /* the arguments are of the same numeric type */
      && (atomic(x) || atomic(y) || equalshape(x, y))) {
    switch (kx) {
      case booltype:
          convert(&x, &kx, inttype);
          convert(&y, &ky, inttype);
          /* fall through to inttype case */
      case inttype:
          {
            nialint     i,
                        ty = tally(y);
            nialint    *yptr = pfirstint(y);  /* safe: only used locally */

            for (i = 0; i < ty; i++) {
              nialint     yi = *yptr++;

              if (yi < 0 && !(atomic(x) && atomic(y)))
                goto use_eachboth;
            }
            if (atomic(x)) {
              if (atomic(y)) {
                if (intval(y) == 0)
                  z = Zero;
                else if (intval(y) < 0)
                  z = makefault("?negative divisor");
                else
                  z = createint(entier((intval(x) + .5) / intval(y)));
              }
              else {
                int         v = valence(y);
                nialint     ty = tally(y);

                z = new_create_array(inttype, v, 0, shpptr(y, v));
                quotientintscalarvector(intval(x), pfirstint(y),
                                        pfirstint(z), ty, false);
              }
            }
            else {
              int         v = valence(x);
              nialint     tx = tally(x);

              z = new_create_array(inttype, v, 0, shpptr(x, v));
              if (atomic(y)) {
                quotientintscalarvector(intval(y), pfirstint(x),
                                        pfirstint(z), tx, true);
              }
              else
                quotientintvectors(pfirstint(x), pfirstint(y),
                                   pfirstint(z), tx);
            }
          }
          break;
      case realtype:
      case chartype:
      case phrasetype:
          if (atomic(x) && atomic(y))
            z = Arith;
          else
            goto use_eachboth;
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
          z = Arith; 
      }
      else
#ifdef V4AT
      if (!numeric(ky))
        z = Arith;
      else
        z = x;
#else
        z = x; 
#endif
    }
    else 
    if (ky == faulttype)
#ifdef V4AT
    { if (!numeric(kx))
        z = Arith;
      else
        z = y;
    }
#else
      z = y;
#endif
    else                     /* chartype or phrasetype cause a fault */
      z = Arith;
  }
  else {
use_eachboth:
    int_eachboth(b_quotient, x, y);
    return;
  }
  apush(z);
  freeup(x);
  freeup(y);
}

/* routine to implement the prefix calls to the
   binary pervading operation quotient.
   */

void
iquotient()
{
  nialptr     z;

  if (kind(top) == faulttype)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?quotient expects a pair"));
  }
  else {
    nialptr     x,
                y;

    splitfb(z, &x, &y);
    apush(x);
    apush(y);
    b_quotient();
  }
  freeup(z);
}


static void
quotientintvectors(nialint * x, nialint * y, nialint * z, nialint n)
{
  nialint     i;

  for (i = 0; i < n; i++) {
    nialint     yv = *y++;

    if (yv == 0) {
      *z++ = 0;
      x++;
    }
    else
      *z++ = entier((*x++ + .5) / yv);
  }
}


static void
quotientintscalarvector(nialint x, nialint * y, nialint * z, nialint n, int reciprocate)
{
  nialint     i;

  if (reciprocate) {
    for (i = 0; i < n; i++) {
      if (x == 0) {
        *z++ = 0;
        y++;
      }
      else
        *z++ = entier((*y++ + .5) / x);
    }
  }
  else {
    for (i = 0; i < n; i++) {
      nialint     yv = *y++;

      if (yv == 0)
        *z++ = 0;
      else
        *z++ = entier((x + .5) / yv);
    }
  }
}


/* implements the binary pervading mod operation */

void
b_mod()
{
  nialptr     y = apop(),
              x = apop(),
              z = Null;
  int         kx = kind(x),
              ky = kind(y);
  int         res = false;

  if (numeric(kx) && numeric(ky)) {
    if (kx < ky)
      res = convert(&x, &kx, ky); /* convert x to y's type */
    else if (ky < kx)
      res = convert(&y, &ky, kx); /* convert y to x's type */
    else
      res = true;
  }
  if (res                    /* the arguments are of the same numeric type */
      && (atomic(x) || atomic(y) || equalshape(x, y))) {
    switch (kx) {
      case booltype:
          convert(&x, &kx, inttype);
          convert(&y, &ky, inttype);
          /* fall through to inttype case */
      case inttype:
          {
            nialint     i,
                        ty = tally(y);
            nialint    *yptr = pfirstint(y);  /* ptr is safe. Only used
                                               * locally */

            for (i = 0; i < ty; i++) {
              nialint     yi = *yptr++;

              if (yi < 0 && !(atomic(x) && atomic(y)))
                goto use_eachboth;
            }
            if (atomic(x)) {
              if (atomic(y)) {
                nialint     xv = intval(x),
                            yv = intval(y);

                if (yv == 0)
                  z = x;
                else if (yv < 0)
                  z = makefault("?negative divisor");
                else if (xv >= 0)
                  z = createint(xv % yv);
                else
                  z = createint(xv - yv * entier((xv + .5) / yv));
              }
              else {
                int         v = valence(y);
                nialint     ty = tally(y);

                z = new_create_array(inttype, v, 0, shpptr(y, v));
                modintscalarvector(intval(x), pfirstint(y),
                                   pfirstint(z), ty, false);
              }
            }
            else {
              int         v = valence(x);
              nialint     tx = tally(x);

              z = new_create_array(inttype, v, 0, shpptr(x, v));
              if (atomic(y)) {
                modintscalarvector(intval(y), pfirstint(x),
                                   pfirstint(z), tx, true);
              }
              else
                modintvectors(pfirstint(x), pfirstint(y),
                              pfirstint(z), tx);
            }
          }
          break;
      case realtype:
      case chartype:
      case phrasetype:
          if (atomic(x) && atomic(y))
            z = Arith;
          else
            goto use_eachboth;
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
          z = Arith; 
      }
      else
#ifdef V4AT
      if (!numeric(ky))
        z = Arith;
      else
        z = x;
#else
        z = x; 
#endif
    }
    else 
    if (ky == faulttype)
#ifdef V4AT
    { if (!numeric(kx))
        z = Arith;
      else
        z = y;
    }
#else
      z = y;
#endif
    else                     /* chartype or phrasetype cause a fault */
      z = Arith;
  }
  else {
use_eachboth:
    int_eachboth(b_mod, x, y);
    return;
  }
  apush(z);
  freeup(x);
  freeup(y);
}

/* routine to implement the prefix calls to the
   binary pervading operation mod.
   */

void
imod()
{
  nialptr     z;

  if (kind(top) == faulttype)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?mod expects a pair"));
  }
  else {
    nialptr     x,
                y;

    splitfb(z, &x, &y);
    apush(x);
    apush(y);
    b_mod();
  }
  freeup(z);
}

static void
modintvectors(nialint * x, nialint * y, nialint * z, nialint n)
{
  nialint     i;

  for (i = 0; i < n; i++) {
    nialint     yv = *y++;

    if (yv == 0)
      *z++ = *x++;
    else {
      nialint     xv = *x++;

      if (xv >= 0)
        *z++ = xv % yv;
      else
        *z++ = xv - yv * entier((xv + .5) / yv);
    }
  }
}


static void
modintscalarvector(nialint x, nialint * y, nialint * z, nialint n, int reciprocate)
{
  nialint     i;

  if (reciprocate) {
    for (i = 0; i < n; i++) {
      if (x == 0)
        *z++ = *y++;
      else {
        nialint     yv = *y++;

        if (yv >= 0)
          *z++ = yv % x;
        else
          *z++ = yv - x * entier((yv + .5) / x);
      }
    }
  }
  else {
    for (i = 0; i < n; i++) {
      nialint     yv = *y++;

      if (yv == 0)
        *z++ = x;
      else if (x >= 0)
        *z++ = x % yv;
      else
        *z++ = x - yv * entier((x + .5) / yv);
    }
  }
}



static double frandx = 314159262.0;

static double M = 2147483647.0;
static double D = 16807.0;

static double
frand()
{
  if (frandx == 0.)
    frandx = 314159262.0;
  frandx = fmod(D * frandx, M);
  return frandx / M;
}

void
iseed()
{
  nialptr     x = apop();

  if (kind(x) == realtype) {
    apush(createreal(frandx / M));
    frandx = floor(realval(x) * M + .49999);
  }
  else
    apush(makefault("?fseed requires a real argument"));

  freeup(x);
}


/* routine to implement operation random, which generates an array
   of random numbers of shape given by its argument.
   */

void
irandom()
{
  nialptr     z,
              x = apop();

  if (kind(x) == inttype || x == Null) {  /* arg is a potential shape */
    nialint     i,
                n = 1;

    for (i = 0; i < tally(x); i++)
      n = n * fetch_int(x, i);
    if (n == 0) {            /* x contains a zero, build empty of that shape */
      apush(x);
      apush(Null);
      b_reshape();
      return;
    }
    else if (n > 0) {
      z = new_create_array(realtype, tally(x), 0, pfirstint(x));
      randomreals(pfirstreal(z), n);
      apush(z);
    }
    else                     /* n < 0 , therefore x is not a shape */
      apush(makefault("?invalid arg in random"));
  }
  else
    apush(makefault("?invalid arg in random"));
  freeup(x);
}

static void
randomreals(double *ptrz, nialint n)
{
  nialint     i;

  for (i = 0; i < n; i++)
    *ptrz++ = frand();
}
