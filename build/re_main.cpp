/* ==============================================================

   MODULE     RE_MAIN.CPP

  COPYRIGHT NIAL Systems Limited  1983-2005


   This module provides the C++ implementation of generic regular 
   expression code.
   Implemented by Jean Michel from open source code described 
   in RE_CORE.C.

================================================================*/

/* Q'Nial file that selects features and loads the xxxspecs.h file */

#include "switches.h"

#ifdef REGEXP

#define STREAMLIB
#include "sysincl.h"

#include "re_class.h"


typedef const char *ccs;

Regexp     *r = NULL;
char       *target = NULL;
SPString   *s = NULL;

extern      "C" char *
regex_s(char *pattern, char *repl, char *string, char *opts)
{
  try {
    SPString    s(string);

    if (target)
      free(target);
    target = NULL;
    s.s(pattern, repl, opts);
    return (target = strdup((const char *) s));
  }
  catch(...) {
    return (strdup(""));
  }
}

extern      "C" char *
regex_tr(char *pattern, char *repl, char *string, char *opts)
{
  try {
    SPString    s(string);

    if (target)
      free(target);
    target = NULL;
    s.tr(pattern, repl, opts);
    return (target = strdup((const char *) s));
  }
  catch(...) {
    return (strdup(""));
  }
}

extern      "C" int
regex_m(char *pattern, char *string, char *opts)
{
  int         rc;

  try {
    if (!string)
      return (0);
    if (s)
      delete      s;

    s = NULL;
    s = new SPString(string);
    if (target)
      free(target);
    target = strdup(string);
    if (r)
      delete      r;

    r = NULL;                // must set to NULL in case
    // we throw inside Regexp
    int         iflg = strchr(opts, 'i') != NULL;

    r = new Regexp(pattern, iflg ? Regexp::nocase : 0);
    rc = (s->m(*r) ? r->groups() : -1);
    return (rc);
  }
  catch(...) {
    if (r)
      delete      r;

    r = NULL;
    return (-1);
  }
}
extern      "C" char *
getsub(int i)
{
  static char *res = NULL;

  if (r) {
    if (i < r->groups() && i >= 0) {
      if (!r)
        return ("CALLING GETSUB WITH NO REGULAR EXPRESSION");
      Range       range = r->getgroup(i);

      if (res)
        free(res);
      int         ll = range.length();

      res = (char *) malloc(ll + 1);
      if ((range.start() == range.end()) && (target[range.end()] == '\0'))
        ll = 0;
      else
        strncpy(res, target + range.start(), range.end() - range.start() + 1);
      res[ll] = '\0';
      /* cout << "group #" << i <<" length = "<< range.length() << endl; cout
       * << "spos = " <<range.start()<<", endp = "<< range.end() << endl;
       * cout << "(int) res[range.start()]:" <<  ((int) res[range.start()])
       * << endl; cout << "(int) res[range.end()]:" <<  ((int)
       * res[range.end()]) << endl; cout << "The group is = |" << res <<"|"
       * <<endl; */
      return (res);
    }
    else
      return ("INDEX OUT OF RANGE");
  }
  else
    return ("NO RESULTS");
}

#pragma argused
extern      "C" int
setcachesize(int newsize)
{
#ifdef RE_CACHE
  int         prevsize = currentcachesize;

  if (newsize == currentcachesize)
    return (newsize);
  if (newsize < 0)
    newsize = 0;
  if (newsize > currentcachesize) {
    if (cache)
      cache = (cache_t *) realloc(cache, newsize * sizeof(cache_t));
    else
      cache = (cache_t *) malloc(newsize * sizeof(cache_t));

    for (int i = currentcachesize; i < newsize; i++) {
      cache[i].s = NULL;
      cache[i].r = NULL;
      cache[i].uses = 0;
    }
    currentcachesize = newsize;
    return (prevsize);
  }
  else {                     //-- must be less
    //-- First free up excess members
    for (int i = newsize; i < currentcachesize; i++) {
      if (cache[i].r)
        free(cache[i].r);
      if (cache[i].s)
        free(cache[i].s);
      cache[i].uses = 0;     //-- not needed, but just in case
    }
    //-- realloc or throw away if size = 0
    if (newsize != 0)
      cache = (cache_t *) realloc(cache, newsize * sizeof(cache_t));
    else {
      free(cache);
      cache = NULL;
    }
    currentcachesize = newsize;
    return (prevsize);
  }
#else
  return (newsize);
#endif
}

#endif             /* REGEXP */
