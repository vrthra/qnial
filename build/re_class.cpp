/* ==============================================================

   MODULE     RE_CLASS.CPP

  COPYRIGHT NIAL Systems Limited  1983-2005


   This module provides C++ Class support for the regular expression code.
   Implemented by Jean Michel from open source code described below.
   The class is defined in re_class.h.


================================================================*/

/* Q'Nial file that selects features and loads the xxxspecs.h file */

#include "switches.h"

#ifdef REGEXP
/*
 * Version 1.91
 * Written by Jim Morris,  morris@netcom.com
 * Kudos to Larry Wall for inventing Perl
 * Copyrights only exist on the regex stuff,  and all
 * have been left intact.
 * The only thing I ask is that you let me know of any nifty fixes or
 * additions.
 * Credits:
 * I'd like to thank Michael Golan <mg@Princeton.EDU> for his critiques
 * and clever suggestions. Some of which have actually been implemented
 */

/* get system files directly rather than using sysincl.h */



#define STLIB
#define MALLOCLIB
#define IOLIB
#define STREAMLIB
#include "sysincl.h"

#ifdef	__TURBOC__
#pragma hdrstop
#endif

#include "re_class.h"

//************************************************************
// VarString Implementation
//************************************************************

VarString & VarString::operator = (const char *s) {
  int         nl = strlen(s);

  if (nl + 1 >= allocated)
    grow((nl - allocated) + allocinc);
  assert(allocated > nl + 1);
  strcpy(a, s);
  len = nl;
  return *this;
}

VarString & VarString::operator = (const VarString & n) {
  if (this != &n) {
    if (n.len + 1 >= allocated) { // if it is not big enough
#	    ifdef	RE_DEBUG
      fprintf(stderr, "~operator=(VarString&) a= %p\n", a);
#	    endif
      delete[] a;            // get rid of old one
      allocated = n.allocated;
      allocinc = n.allocinc;
      a = new char[allocated];

#	    ifdef	RE_DEBUG
      fprintf(stderr, "operator=(VarString&) a= %p, source= %p\n", a, n.a);
#	    endif
    }
    len = n.len;
    strcpy(a, n.a);
  }
  return *this;
}

void
VarString::grow(int n)
{
  if (n == 0)
    n = allocinc;
  allocated += n;
  char       *tmp = new char[allocated];

  strcpy(tmp, a);
#ifdef	RE_DEBUG
  fprintf(stderr, "VarString::grow() a= %p, old= %p, allocinc= %d\n", tmp, a, allocinc);
  fprintf(stderr, "~VarString::grow() a= %p\n", a);
#endif
  delete[] a;
  a = tmp;
}

void
VarString::add(char c)
{
  if (len + 1 >= allocated)
    grow();
  assert(allocated > len + 1);
  a[len++] = c;
  a[len] = '\0';
}

void
VarString::add(const char *s)
{
  int         nl = strlen(s);

  if (len + nl >= allocated)
    grow(((len + nl) - allocated) + allocinc);
  assert(allocated > len + nl);
  strcat(a, s);
  len += nl;
}

void
VarString::add(int ip, const char *s)
{
  int         nl = strlen(s);

  if (len + nl >= allocated)
    grow(((len + nl) - allocated) + allocinc);
  assert(allocated > len + nl);
  memmove(&a[ip + nl], &a[ip], (len - ip) + 1); // shuffle up
  memcpy(&a[ip], s, nl);
  len += nl;
  assert(a[len] == '\0');
}

void
VarString::remove(int ip, int n)
{
  assert(ip + n <= len);
  memmove(&a[ip], &a[ip + n], (len - ip) - n + 1);  // shuffle down
  len -= n;
  assert(a[len] == '\0');
}

//************************************************************
// SPString stuff
//************************************************************

// assignments
SPString & SPString::operator = (const SPString & n) {
  if (this == &n)
    return *this;
  pstr = n.pstr;
  return *this;
}

SPString & SPString::operator = (const substring & sb) {
  VarString   tmp(sb.pt, sb.len);

  pstr = tmp;
  return *this;
}

// concatenations
SPString    SPString::operator + (const SPString & s) const
{
  SPString    ts(*this);

  ts.pstr.add(s);
  return ts;
}

SPString    SPString::operator + (const char *s) const
{
  SPString    ts(*this);

  ts.pstr.add(s);
  return ts;
}

SPString    SPString::operator + (char c) const
{
  SPString    ts(*this);

  ts.pstr.add(c);
  return ts;
}

SPString    operator + (const char *s1, const SPString & s2) {
  SPString    ts(s1);

  ts = ts + s2;
//    cout << "s2[0] = " << s2[0] << endl; // gives incorrect error
  return ts;
}

//************************************************************
// other stuff
//************************************************************

char
SPString::chop(void)
{
  int         n = length();

  if (n <= 0)
    return '\0';             // empty
  char        tmp = pstr[n - 1];

  pstr.remove(n - 1);
  return tmp;
}

int
SPString::index(const SPString & s, int offset)
{
  if (offset < 0)
    offset = 0;
  for (int i = offset; i < length(); i++) {
    if (strncmp(&pstr[i], s, s.length()) == 0)
      return i;
  }

  return -1;
}

int
SPString::rindex(const SPString & s, int offset)
{
  if (offset == -1)
    offset = length() - s.length();
  else
    offset = offset - s.length() + 1;
  if (offset > length() - s.length())
    offset = length() - s.length();

  for (int i = offset; i >= 0; i--) {
    if (strncmp(&pstr[i], s, s.length()) == 0)
      return i;
  }
  return -1;
}

SPString::substring
SPString::substr(int offset, int len)
{
  if (len == -1)
    len = length() - offset; // default use rest of string
  if (offset < 0) {
    offset = length() + offset; // count from end of string
    if (offset < 0)
      offset = 0;            // went too far, adjust to
    // start
  }
  return substring(*this, offset, len);
}

// this is private
// it shrinks or expands string as required
void
SPString::insert(int pos, int len, const char *s, int nlen)
{
  if (pos < length()) {      // nothing to delete if not
    // true
    if ((len + pos) > length())
      len = length() - pos;
    pstr.remove(pos, len);   // first remove subrange
  }
  else
    pos = length();

  VarString   tmp(s, nlen);

  pstr.add(pos, tmp);        // then insert new substring
}

int
SPString::m(Regexp & r)
{
  return r.match(*this);
}

int
SPString::m(const char *pat, const char *opts)
{
  int         iflg = strchr(opts, 'i') != NULL;
  Regexp      r(pat, iflg ? Regexp::nocase : 0);

  return m(r);
}



//
// I know! This is not fast, but it works!!
//
int
SPString::tr(const char *sl, const char *rl, const char *opts)
{
  if (length() == 0 || strlen(sl) == 0)
    return 0;

  int         cflg = strchr(opts, 'c') != NULL; // thanks Michael
  int         dflg = strchr(opts, 'd') != NULL;
  int         sflg = strchr(opts, 's') != NULL;

  int         cnt = 0,
              flen = 0;
  unsigned int i;
  SPString    t;
  unsigned char lstc = '\0',
              fr[256];

  // build search array, which is a 256 byte array that stores the index+1
  // in the search string for each character found, == 0 if not in search
  memset(fr, 0, 256);
  for (i = 0; i < strlen(sl); i++) {
    if (i && sl[i] == '-') { // got a range
      assert(i + 1 < strlen(sl) && lstc <= sl[i + 1]);  // sanity check
      for (unsigned char c = lstc + 1; c <= sl[i + 1]; c++) {
        fr[c] = (unsigned char) ++flen;
      }
      i++;
      lstc = '\0';
    }
    else {
      lstc = sl[i];
      fr[sl[i]] = (unsigned char) ++flen;
    }
  }

  unsigned int rlen;

  // build replacement list
  if ((rlen = strlen(rl)) != 0) {
    for (i = 0; i < rlen; i++) {
      if (i && rl[i] == '-') {  // got a range
        assert(i + 1 < rlen && t[t.length() - 1] <= rl[i + 1]); // sanity check
        for (char c = t[i - 1] + 1; c <= rl[i + 1]; c++)
          t += c;
        i++;
      }
      else
        t += rl[i];
    }
  }

  // replacement string that is shorter uses last character for rest of
  // string
  // unless the delete option is in effect or it is empty
  while (!dflg && rlen && flen > t.length()) {
    t += t[t.length() - 1];  // duplicate last character
  }

  rlen = t.length();         // length of translation
  // string

  // do translation, and deletion if dflg (actually falls out of length of t)
  // also squeeze translated characters if sflg

  SPString    tmp;           // need this in case dflg, and string changes
  // size
  for (i = 0; i < (unsigned int) length(); i++) {
    unsigned int off;

    if (cflg) {              // complement, ie if NOT in
      // f
      char        rc = (!dflg && t.length() ? t[t.length() - 1] : '\0');  // always use last
      // character for
      // replacement
      if ((off = fr[(*this)[i]]) == 0) {  // not in map
        cnt++;
        if (!dflg && (!sflg || tmp.length() == 0 || tmp[tmp.length() - 1] != rc))
          tmp += rc;
      }
      else
        tmp += (*this)[i];   // just stays the same
    }
    else {                   // in fr so substitute with
      // t, if no equiv in t then
      // delete
      if ((off = fr[(*this)[i]]) > 0) {
        off--;
        cnt++;
        if (rlen == 0 && !dflg && (!sflg || tmp.length() == 0 || tmp[tmp.length() - 1] != (*this)[i]))
          tmp += (*this)[i]; // stays the same
        else if (off < rlen && (!sflg || tmp.length() == 0 || tmp[tmp.length() - 1] != t[off]))
          tmp += t[off];     // substitute
      }
      else
        tmp += (*this)[i];   // just stays the same
    }
  }

  *this = tmp;
  return cnt;
}

int
SPString::s(const char *exp, const char *repl, const char *opts)
{
  int         gflg = strchr(opts, 'g') != NULL;
  int         iflg = strchr(opts, 'i') != NULL;
  int         cnt = 0;
  Regexp      re(exp, iflg ? Regexp::nocase : 0);
  Range       rg;

  if (re.match(*this)) {
    // OK I know, this is a horrible hack, but it seems to work
    if (gflg) {              // recursively call s()
      // until applied to whole
      // string
      rg = re.getgroup(0);
      if (rg.end() + 1 < length()) {
        SPString    st(substr(rg.end() + 1));

//		cout << "Substring: " << st << endl;
        cnt += st.s(exp, repl, opts);
        substr(rg.end() + 1) = st;
//		cout << "NewString: " << *this << endl;
      }
    }

    if (!strchr(repl, '$')) {// straight, simple
      // substitution
      rg = re.getgroup(0);
      substr(rg.start(), rg.length()) = repl;
      cnt++;
    }
    else {                   // need to do subexpression
      // substitution
      char        c;
      const char *src = repl;
      SPString    dst;
      int         no;

      while ((c = *src++) != '\0') {
        if (c == '$' && *src == '&') {
          no = 0;
          src++;
        }
        else if (c == '$' && '0' <= *src && *src <= '9')
          no = *src++ - '0';
        else
          no = -1;

        if (no < 0) {        /* Ordinary character. */
          if (c == '\\' && (*src == '\\' || *src == '$'))
            c = *src++;
          dst += c;
        }
        else {
          rg = re.getgroup(no);
          dst += substr(rg.start(), rg.length());
        }
      }
      rg = re.getgroup(0);
      substr(rg.start(), rg.length()) = dst;
      cnt++;
    }

    return cnt;
  }
  return cnt;
}


#ifdef __TURBOC__
int
Regexp::groups(void) const
{
  int         res = 0;

  for (int i = 0; i < NSUBEXP; i++) {
    if (repat->startp[i] == NULL)
      break;
    res++;
  }
  return res;
}

#endif
#endif             /* REGEXP */
