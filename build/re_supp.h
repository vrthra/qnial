/* ==============================================================

     RE_SUPP.H

  COPYRIGHT NIAL Systems Limited  1983-2005

   This header file is included by re_class.h.

================================================================*/

/*
 * version 1.90
 * Regexp is a class that encapsulates the Regular expression
 * stuff. Hopefully this means I can plug in different regexp
 * libraries without the rest of my code needing to be changed.
 * Written by Jim Morris,  morris@netcom.com
 */

#ifndef	_REGEXP_H
#define _REGEXP_H

#define STREAMLIB
#define STDLIB
#define MALLOCLIB
#define STLIB
#define ASSERTLIB
#define CLIB
#define LIMITLIB
#include "sysincl.h"


#include    "re_core.h"

/*
 * Note this is an inclusive range where it goes
 * from start() to, and including, end()
 */

class Range {
 private:
  int         st,
              en,
              len;
 public:
              Range() {
    st = 0;
    en = -1;
    len = 0;
  }

              Range(int s, int e, int l = -1) {
    st = s;
    en = e;
    len = (en - st) + 1;
    if (l != -1)
      len = l;
    assert(st <= en && st >= 0);
  }

  // test validity of the range
  operator void *() {
    return (st <= en && st >= 0) ? this : 0;
  }
  int         start(void) const {
    return st;
  }
  int         end(void) const {
    return en;
  }
  int         length(void) const {
    return len;
  }

  void        set(int a, int b) {
    st = a;
    en = b;
    len = (en - st) + 1;
    assert(st <= en && st >= 0);
  }

  int         operator < (const Range & r) const  // for sorting
  {
    return ((st == r.st) ? (en < r.en) : (st < r.st));
  }

  // x++ operator extends end of range by one
  void        operator++ (int) {
    en++;
    len++;
  }

  // ++x operator extends start of range by one
  void        operator++ (void) {
    st++;
    len--;
  }

  friend      std::ostream & operator << (std::ostream & os, const Range & r) {
    os << r.st << " - " << r.en << " (" << ((r.en - r.st) + 1) << ")";
    return os;
  }
};

#ifdef RE_CACHE

typedef struct {
  char       *s;
  regexp     *r;
  int         uses;
}           cache_t;

#endif


class Regexp {
 public:
  enum options {
  def = 0, nocase = 1};

 private:
              regexp * repat;
  const char *target;        // only used as a base address to get an offset
  int         res;
  int         iflg;
#ifdef RE_CACHE
  regexp     *in_cache(const char *str, int ifl) {
    for (int i = 0; i < currentcachesize; i++) {
      cout << "Looking In position :" << i << endl;
      if ((cache[i].s != NULL) && (strcmp(str, cache[i].s) == 0)) {
        cout << "Found In position :" << i << "with s = " << cache[i].s << "and r = " << ((int) cache[i].r) << endl;
        cache[i].uses += (cache[i].uses == INT_MAX ? 0 : 1);
        return (cache[i].r);
      }
    }
                return (NULL);
  }
#endif

#ifdef RE_CACHE
  regexp     *cache_regcomp(const char *pat) {
    int         i;
    int         uses = 0;
    int         minuses = INT_MAX;
    if          (currentcachesize == 0)
                  return (regcomp(pat));
    for         (i = 0; i < currentcachesize; i++) {
      if (cache[i].uses == 0) {
        uses = i;
        break;
      }
      if          (cache[i].uses <= minuses) {
        uses = i;
        minuses = cache[i].uses;
      }
    }
    if (cache[uses].r)
      free(cache[uses].r);
    cache[uses].r = regcomp(pat);
    cout << "Placing in pos: " << uses << " with string: " << pat << "And r = " << ((int) cache[uses].r) << endl;

    if (!cache[uses].r) {
      cache[uses].uses = 0;
      if (cache[uses].s)
        free(cache[uses].s);
      cache[uses].s = NULL;
      cache[uses].r = NULL;
      return (NULL);
    }
    cache[uses].uses = 1;
    if (cache[uses].s)
      free(cache[uses].s);
    cache[uses].s = strdup(pat);
    return (cache[uses].r);
  }
#else
  regexp     *cache_regcomp(const char *pat) {
    return (regcomp(pat));
  }
#endif
#ifndef	__TURBOC__
  void        strlwr(char *s) {
    while (*s) {
      *s = tolower(*s);
      s++;
    }
  }
#endif
 public:
              Regexp(const char *rege, int ifl = 0) {
    repat = NULL;
#ifdef RE_CACHE
    if (!(repat = in_cache(rege, ifl))) {
#endif
      iflg = ifl;
      if (iflg == nocase) {  // lowercase fold
        char       *r = new char[strlen(rege) + 1];

        strcpy(r, rege);
        strlwr(r);
        if ((repat = cache_regcomp(r)) == NULL) {
//					 cerr << "regcomp() error" << endl;
          delete[] r;
//					 MessageBeep(MB_OK);
          throw 10;
        }
        delete[] r;
      }
      else {
        if ((repat = cache_regcomp(rege)) == NULL) {
//					cerr << "regcomp() error" << endl;
//					MessageBeep(MB_OK);
          throw 10;
        }
      }
#ifdef RE_CACHE
    }
#endif
  }

  ~Regexp() {
    //-- only free it up if there is no cache
#ifdef RE_CACHE
    if (currentcachesize == 0)
      free(repat);
#else
    free(repat);
#endif
  }

  int         match(const char *targ) {
    int         res;
    if          (iflg == nocase) {  // fold lowercase
      char       *r = new char[strlen(targ) + 1];
                  strcpy(r, targ);
                  strlwr(r);
                  res = regexec(repat, r);
                  target = r;// looks bad but is really ok, really
                  delete[] r;
    }
    else {
      res = regexec(repat, targ);
      target = targ;
    }

    return ((res == 0) ? 0 : 1);
  }

#ifdef __TURBOC__
  int         groups(void) const;

#else
  int         groups(void) const
  {
    int         res = 0;
    for         (int i = 0; i < NSUBEXP; i++) {
      if (repat->startp[i] == NULL)
        break;
      res++;
    }
                return res;
  }
#endif

  Range       getgroup(int n) const
  {
    assert(n < NSUBEXP);
    /* cout << "!(int) *repat->startp[n]:" <<  ((int) *(repat->startp[n])) <<
     * endl; cout << "!(int) *repat->endp[n]:" <<  ((int) *(repat->endp[n]))
     * << endl; */

    if (repat->endp[n] == repat->startp[n]) { // pointing to end of string
      int         length = 1;
      if          (!repat->len[n])
                    length = 0;
      if          (*(repat->startp[n]) == '\0')
                    length = 0;
                  return Range((int) (repat->startp[n] - (char *) target),
                  (int) (repat->endp[n] - (char *) target), length ? -1 : 0);
    }
    else
                  return Range((int) (repat->startp[n] - (char *) target),
                              (int) (repat->endp[n] - (char *) target) - 1);
  }
};


#endif
