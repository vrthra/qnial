/* ==============================================================

   MODULE     RE_CLASS.H

  COPYRIGHT NIAL Systems Limited  1983-2005


   The header file for re_class.cpp.
   Implemented by Jean Michel from open source code described below.


================================================================*/

/*
 * Version 1.91
 * Written by Jim Morris,  morris@netcom.com
 * Kudos to Larry Wall for inventing Perl
 * Copyrights only exist on the regex stuff, and all have been left intact.
 * The only thing I ask is that you let me know of any nifty fixes or
 * additions.
 *
 * Credits:
 * I'd like to thank Michael Golan <mg@Princeton.EDU> for his critiques
 * and clever suggestions. Some of which have actually been implemented
 */

#ifndef	_SPLASH_H
#define	_SPLASH_H


#include "re_supp.h"

#ifdef	RE_DEBUG
#include	<stdio.h>
#endif

#define	INLINE	inline





//****************************************************************
// just a mechanism for self deleteing strings which can be hacked
//****************************************************************

class TempString {
 private:
  char       *str;
 public:
              TempString(const char *s) {
    str = new char[strlen(s) + 1];
                strcpy(str, s);
  }

              TempString(const char *s, int len) {
    str = new char[len + 1];

    if (len)
      strncpy(str, s, len);
    str[len] = '\0';
  }

  ~TempString() {
    delete[] str;
  }

  operator char *() const {
    return str;
  }
};

//************************************************************
// This class takes care of the mechanism behind variable
// length strings
//************************************************************

class VarString {
 private:
  enum {
  ALLOCINC = 32};
  char       *a;
  int         len;
  int         allocated;
  int         allocinc;
  INLINE void grow(int n = 0);

 public:
#ifdef	USLCOMPILER
  // USL 3.0 bug with enums losing the value
              INLINE VarString(int n = 32);
#else
              INLINE VarString(int n = ALLOCINC);
#endif

  INLINE      VarString(const VarString & n);
  INLINE      VarString(const char *);
  INLINE      VarString(const char *s, int n);
  INLINE      VarString(char);

              ~VarString() {
#       ifdef	RE_DEBUG
    fprintf(stderr, "~VarString() a= %p, allocinc= %d\n", a, allocinc);
#       endif
    delete[] a;
  }

              VarString & operator = (const VarString & n);

  VarString & operator = (const char *);

  INLINE const char operator[] (const int i) const;
  INLINE char &operator[] (const int i);

  operator const char *() const {
    return a;
  }

  int         length(void) const {
    return len;
  }

  void        add(char);
  void        add(const char *);
  void        add(int, const char *);
  void        remove(int, int = 1);

  void        erase(void) {
    len = 0;
  }
};


//************************************************************
// Implements the perl specific string functionality
//************************************************************

class SPString {
 private:
  VarString pstr;            // variable length string mechanism
  class substring;
  friend class substring;

 public:

             SPString():
              pstr() {
  }
             SPString(const SPString & n):
              pstr(n.pstr) {
  }
 SPString(const char *s):
  pstr(s) {
  }
 SPString(const char c):
  pstr(c) {
  }
 SPString(const substring & sb):
  pstr(sb.pt, sb.len) {
  }

  SPString & operator = (const char *s) {
    pstr = s;
    return *this;
  }
  SPString & operator = (const SPString & n);
  SPString & operator = (const substring & sb);

  operator const char *() const {
    return pstr;
  }
  const char  operator[] (int n) const {
    return pstr[n];
  }

  int         length(void) const {
    return pstr.length();
  }

  char        chop(void);

  int         index(const SPString & s, int offset = 0);
  int         rindex(const SPString & s, int offset = -1);
  substring   substr(int offset, int len = -1);
  substring   substr(const Range & r) {
    return substr(r.start(), r.length());
  }

  int         m(const char *, const char *opts = ""); // the regexp match
  // m/.../ equiv
  int         m(Regexp &);

  int         tr(const char *, const char *, const char *opts = "");
  int         s(const char *, const char *, const char *opts = "");


  int         operator < (const SPString & s) const {
    return (strcmp(pstr, s) < 0);
  }
  int         operator > (const SPString & s) const {
    return (strcmp(pstr, s) > 0);
  }
  int         operator <= (const SPString & s) const {
    return (strcmp(pstr, s) <= 0);
  }
  int         operator >= (const SPString & s) const {
    return (strcmp(pstr, s) >= 0);
  }
  int         operator == (const SPString & s) const {return (strcmp(pstr, s) == 0);
  }
  int         operator != (const SPString & s) const {
    return (strcmp(pstr, s) != 0);
  }

  int         operator < (const char *s) const {
    return (strcmp(pstr, s) < 0);
  }
  int         operator > (const char *s) const {
    return (strcmp(pstr, s) > 0);
  }
  int         operator <= (const char *s) const {
    return (strcmp(pstr, s) <= 0);
  }
  int         operator >= (const char *s) const {
    return (strcmp(pstr, s) >= 0);
  }
  int         operator == (const char *s) const {return (strcmp(pstr, s) == 0);
  }
  int         operator != (const char *s) const {
    return (strcmp(pstr, s) != 0);
  }

  friend int  operator < (const char *s, const SPString & sp) {
    return (strcmp(s, sp.pstr) < 0);
  }
  friend int  operator > (const char *s, const SPString & sp) {
    return (strcmp(s, sp.pstr) > 0);
  }
  friend int  operator <= (const char *s, const SPString & sp) {
    return (strcmp(s, sp.pstr) <= 0);
  }
  friend int  operator >= (const char *s, const SPString & sp) {
    return (strcmp(s, sp.pstr) >= 0);
  }
  friend int  operator == (const char *s, const SPString & sp) {return (strcmp(s, sp.pstr) == 0);
  }
  friend int  operator != (const char *s, const SPString & sp) {
    return (strcmp(s, sp.pstr) != 0);
  }

  SPString    operator + (const SPString & s) const;
  SPString    operator + (const char *s) const;
  SPString    operator + (char c) const;
  friend SPString operator + (const char *s1, const SPString & s2);

  SPString & operator += (const SPString & s) {
    pstr.add(s);
    return *this;
  }
  SPString & operator += (const char *s) {
    pstr.add(s);
    return *this;
  }
  SPString & operator += (char c) {
    pstr.add(c);
    return *this;
  }

private:
  void        insert(int pos, int len, const char *pt, int nlen);

  // This idea lifted from NIH class library -
  // to handle substring LHS assignment
  // Note if subclasses can't be used then take external and make
  // the constructors private, and specify friend SPString
  class substring {
 public:
    int         pos,
                len;
                SPString & str;
    char       *pt;
 public:
             substring(SPString & os, int p, int l):
                str(os) {
      if (p > os.length())
        p = os.length();
      if ((p + l) > os.length())
        l = os.length() - p;
      pos = p;
      len = l;
      if (p == os.length())
        pt = 0;              // append to end of string
      else
        pt = &os.pstr[p];    // +++ WARNING this may be illegal as nested
      // classes
      // can't access its enclosing classes privates!
    }

    void        operator = (const SPString & s) {
      if (&str == &s) {      // potentially overlapping
        VarString tmp(s);
        str.insert(pos, len, tmp, strlen(tmp));
      }
      else
        str.insert(pos, len, s, s.length());
    }

    void        operator = (const substring & s) {
      if (&str == &s.str) {  // potentially overlapping
        VarString tmp(s.pt, s.len);
        str.insert(pos, len, tmp, strlen(tmp));
      }
      else
        str.insert(pos, len, s.pt, s.len);
    }

    void        operator = (const char *s) {
      str.insert(pos, len, s, strlen(s));
    }
  };
};


//************************************************************
// Streams operators
//************************************************************



//************************************************************
// Implementation of template functions for splistbase
//************************************************************

//************************************************************
// VarString Implementation
//************************************************************

INLINE
VarString::VarString(int n)
{
  a = new char[n];

  *a = '\0';
  len = 0;
  allocated = n;
  allocinc = n;
#   ifdef	RE_DEBUG
  fprintf(stderr, "VarString(int %d) a= %p\n", allocinc, a);
#   endif
}

INLINE
VarString::VarString(const char *s)
{
  int         n = strlen(s) + 1;
  a = new char[n];

  strcpy(a, s);
  len = n - 1;
  allocated = n;
  allocinc = ALLOCINC;
#   ifdef	RE_DEBUG
  fprintf(stderr, "VarString(const char *(%d)) a= %p\n", allocinc, a);
#   endif
}

INLINE
VarString::VarString(const char *s, int n)
{
  a = new char[n + 1];

  if (n)
    strncpy(a, s, n);
  a[n] = '\0';
  len = n;
  allocated = n + 1;
  allocinc = ALLOCINC;
#   ifdef	RE_DEBUG
  fprintf(stderr, "VarString(const char *, int(%d)) a= %p\n", allocinc, a);
#   endif
}

INLINE
VarString::VarString(char c)
{
  int         n = 2;
  a = new char[n];

  a[0] = c;
  a[1] = '\0';
  len = 1;
  allocated = n;
  allocinc = ALLOCINC;
#   ifdef	RE_DEBUG
  fprintf(stderr, "VarString(char (%d)) a= %p\n", allocinc, a);
#   endif
}


INLINE      ostream & operator << (ostream & os, const VarString & arr) {
#ifdef TEST
  os << "(" << arr.length() << ")" << (const char *) arr;
#else
  os << (const char *) arr;
#endif

  return os;
}

INLINE const char VarString::operator[] (const int i) const
{
  assert((i >= 0) && (i < len) && (a[len] == '\0'));
  return a[i];
}

INLINE char &VarString::operator[] (const int i) {
  assert((i >= 0) && (i < len) && (a[len] == '\0'));
  return a[i];
}

INLINE
VarString::VarString(const VarString & n)
{
  allocated = n.allocated;
  allocinc = n.allocinc;
  len = n.len;
  a = new char[allocated];

  strcpy(a, n.a);
#ifdef	RE_DEBUG
  fprintf(stderr, "VarString(VarString&) a= %p, source= %p\n", a, n.a);
#endif

}

//************************************************************
// Sublist and Slice stuff
//************************************************************



#endif
