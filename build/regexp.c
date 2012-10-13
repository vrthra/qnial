/*==============================================================

MODULE   REGEXP.C

COPYRIGHT NIAL Systems Limited  1983-2005

This contains low level Nial operations used by regexp.ndf code
included in defs.ndf if REGEXP is defined.

================================================================*/


/* Q'Nial file that selects features and loads the xxxspecs.h file */

#include "switches.h"
#include "mainlp.h"

#if defined(WINDOWS_SYS)
#include "windows.h"
#endif


/* standard library header files */

#define IOLIB
#define STDLIB
#define LIMITLIB
#define SJLIB
#include "sysincl.h"

#include "messages.h"
#include "absmach.h"
#include "qniallim.h"
#include "lib_main.h"
#include "ops.h"
#include "trs.h"
#include "fileio.h"


#ifdef REGEXP /* ( */

#include "re_main.h"

#define FIRSTCHAR_OR_NULL(a,i) (tally(fetch_array(a,i))?pfirstchar(fetch_array(a,i)):"")

void
iregexp_m(void)
{
  nialptr     z = apop();
  int         i;
  int         res;

  /* validate the arguments */

  if (!((tally(z) == 3) || (kind(z) == atype))) {
    apush(makefault("Must supply 3 string/phrases as arguments to regexp_m"));
    freeup(z);
    return;
  }

  for (i = 0; i < 3; i++) {
    nialptr     t = fetch_array(z, i);

    if ((kind(t) != chartype) && (kind(t) != phrasetype) && (t != Null)) {
      apush(makefault("Each of the arguments must be strings/phrases to regexp_m"));
      freeup(z);
      return;
    }
  }

  /* call the C level implementation */

  res = regex_m(FIRSTCHAR_OR_NULL(z, 0),
                FIRSTCHAR_OR_NULL(z, 1),
                FIRSTCHAR_OR_NULL(z, 2));

  apush(createint(res));
  freeup(z);
  return;
}

void
iregexp_tr(void)
{
  nialptr     z = apop();
  int         i;
  char       *res;

/* validate the arguments */

  if (((tally(z) != 4) && (kind(z) != atype))) {
    apush(makefault("Must supply 4 string/phrases as arguments to regexp_tr"));
    freeup(z);
    return;
  }
  for (i = 0; i < 4; i++) {
    nialptr     t = fetch_array(z, i);

    if ((kind(t) != chartype) && (kind(t) != phrasetype) && (t != Null)) {
      apush(makefault("Each of the arguments must be strings/phrases to regexp_tr"));
      freeup(z);
      return;
    }
  }

  /* call the C level implementation */

  res = (char *) regex_tr(FIRSTCHAR_OR_NULL(z, 0),
                          FIRSTCHAR_OR_NULL(z, 1),
                          FIRSTCHAR_OR_NULL(z, 2),
                          FIRSTCHAR_OR_NULL(z, 3));

  mkstring(res);  /* mkstring pushes the result */
  freeup(z);
  return;
}

void
iregexp_s(void)
{
  nialptr     z = apop();
  int         i;
  char       *res;

  /* validate the arguments */

  if (((tally(z) != 4) && (kind(z) != atype))) {
    apush(makefault("Must supply 4 string/phrases as arguments to regexp_s"));
    freeup(z);
    return;
  }

  for (i = 0; i < 4; i++) {
    nialptr     t = fetch_array(z, i);

    if ((kind(t) != chartype) && (kind(t) != phrasetype) && (t != Null)) {
      apush(makefault("Each of the arguments must be strings/phrases to regexp_tr"));
      freeup(z);
      return;
    }
  }

  /* call the C level implementation */

  res = (char *) regex_s(FIRSTCHAR_OR_NULL(z, 0),
                         FIRSTCHAR_OR_NULL(z, 1),
                         FIRSTCHAR_OR_NULL(z, 2),
                         FIRSTCHAR_OR_NULL(z, 3));

  mkstring(res); /* mkstring pushes the result */
  freeup(z);
  return;
}

void
iregexp_setcachesize(void)
{
  nialptr     z = apop();
  int         res;

  /* validate the argument */

  if ((kind(z) != inttype) && (tally(z) != 1)) {
    apush(makefault("regexp_setcachesize takes a single integer argument"));
    freeup(z);
    return;
  }

  /* call the C level implementation */

  res = setcachesize(*pfirstint(z));
  apush(createint(res));
  freeup(z);
  return;
}


void
iregexp_getsub(void)
{
  nialptr     z = apop();
  char       *res;

  /* validate the argument */

  if ((kind(z) != inttype) && (tally(z) != 1)) {
    apush(makefault("regexp_getsub takes a single integer argument"));
    freeup(z);
    return;
  }

/* call the C level implementation */

  res = getsub(*pfirstint(z));
  mkstring(res);  /* mkstring pushes the result */

  /* A free of "res" is not needed because getsub reuses a statically malloced
     pointer, resulting in a memory leak only once, not accumulative. */
  freeup(z);
  return;
}



#endif /* ) REGEXP */

