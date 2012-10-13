/*==============================================================

MODULE   USEROPS.C

COPYRIGHT NIAL Systems Limited  1983-2005

This contains some additional operations originally added for users, 
but now part of the Windows versions.


This file can be used to add new operations for a specific need.

================================================================*/


/* Q'Nial file that selects features and loads the xxxspecs.h file */

#include "switches.h"

/* standard library header files */

#define IOLIB
#define STDLIB
#define LIMITLIB
#define SJLIB
#define STLIB
#if defined(ODBC) || defined(WINDOWS_SYS)
#define WINDOWSLIB
#endif

#include "sysincl.h"

#include "mainlp.h"

#include "if.h"
#include "messages.h"
#include "absmach.h"
#include "basics.h"
#include "qniallim.h"
#include "lib_main.h"
#include "ops.h"
#include "trs.h"
#include "fileio.h"


/* See the section in the Q'Nial Design document on User Defined Primitives
   for instructions  on how to  develop new primitives. */


/* place new primitive routines here */


/*
The next section of code implements an Internet access operation that
given a URL as text, fetches the corresponding page as a list of strings.
It is useful for prototyping applications that scan the web for data.
*/



#ifdef URLGETFILE /* ( */

#include "wininet.h"

#define URLBUFSIZE 100001
#define URLLOOPLIMIT 500

void iurlgetfile()
{
   nialptr z;
   HINTERNET m_hSession;
   HINTERNET hHttpFile;
   char * url;
   char m_buffer[URLBUFSIZE];
   DWORD dwBytesRead ;
   BOOL bRead = 0;

   z = apop(); /* get argument */

   if ((kind(z) != chartype) && (kind(z) != phrasetype)) 
   { apush(makefault("?must supply a string or phrase to URLGetFile"));
     goto cleanup;
   }

   url = pfirstchar(z);

   /* Initialize the Internet Functions. */
   m_hSession = InternetOpen("Nial URL Getter",
      PRE_CONFIG_INTERNET_ACCESS,
      NULL,
      INTERNET_INVALID_PORT_NUMBER,
      0 ) ;

   if (m_hSession)
   { /* internet open successful, access the page */
     hHttpFile = InternetOpenUrl(m_hSession,
       url,
       NULL,
       0,
       INTERNET_FLAG_RELOAD,
       0);

     if (hHttpFile)   /* access successful */
     { nialptr Res = Null;
       BOOL Done = 0;
       int count = 0;

       /* loop attempting to read */
       while (!Done && count < URLLOOPLIMIT) 
       { bRead = InternetReadFile(hHttpFile,
           m_buffer,
           URLBUFSIZE - 1,
           &dwBytesRead);

         /* make a string from the text read and join it to Res */
         mknstring(m_buffer,dwBytesRead);
         pair(Res,apop());
         ilink();
         Res = apop();

         /* test for end of loop condition */
         Done = !bRead || dwBytesRead == 0;
         count++;
         checksignal(NC_CS_NORMAL);
       }

       /* check end of loop condition, display warnings */
       if (!bRead)
       { /* put in last error test */
         nprintf(OF_MESSAGE,"InternetReadFile failed\n");
       }
       if (count == URLLOOPLIMIT)
       { 
         nprintf(OF_MESSAGE,"InternetReadFile stalled\n");
       }
       apush(Res); /* push result even if error occurred */
       InternetCloseHandle(hHttpFile);
     } 
     else 
     { /* put in call to GetLastError */
       apush(makefault("?URLGetFile: failed to open url"));
       goto cleanup;
     }
     InternetCloseHandle(m_hSession) ;
   }
   else
   { /* put in call to GetLastError */
     apush(makefault("?URLGetFile: failed to establish internet connection"));
   }
cleanup:
   freeup(z);
}

#endif /* URLGETFILE ) */

/* We could move the routines for CMDLINE and GETENV into 
   the interface routine for each operating system. */

#ifdef CMDLINE /* ( */

extern int  global_argc;
extern char **global_argv;

void
iGetCommandLine(void)
{
#if defined(WINDOWS_SYS)
  mkstring(GetCommandLine());
#elif defined(UNIXSYS) 
  int         sz = 0;
  int         i;
  char       *tmp;

  for (i = 0; i < global_argc; i++)
    sz += strlen(global_argv[i]) + 1;

  tmp = (char *) malloc(sz + 1);
  if (!tmp)
    exit_cover(NC_MEM_ERR_F);

  strcpy(tmp, "");

  for (i = 0; i < global_argc; i++) {
    char       *flg = NULL;

    if ((flg = strchr(global_argv[i], ' ')))
      strcat(tmp, "\"");
    strcat(tmp, global_argv[i]);
    if (flg)
      strcat(tmp, "\"");
    strcat(tmp, " ");
  }


  mkstring(tmp);
  free(tmp);
#else
#error You must Define a GetCommandLine for this system
#endif
}

#endif /* CMDLINE ) */


#ifdef GETENV /* ( */

#ifdef UNIXSYS
extern char **environ;
#endif

void
iGetEnv(void)
{
  nialptr     z = apop();
  char       *arg;

  if ((kind(z) != chartype) && (kind(z) != phrasetype) && (tally(z) != 0)) {
    apush(makefault("?Must supply a string or phrase to GetEnv"));
    freeup(z);
    return;
  }


  if (tally(z) == 0)
    arg = NULL;
  else
    arg = pfirstchar(z);     /* safe */

  {
    if (arg) {

#  if defined(WINDOWS_SYS)
      char       *buf = malloc(1024);
      DWORD       rc;

      if (!buf)
        exit_cover(NC_MEM_ERR_F);

      buf[0] = '\0';
      rc = GetEnvironmentVariable(arg, buf, 1024);
      if (rc > 1024) {
        buf = (char *) realloc(buf, rc);
        buf[0] = '\0';
        GetEnvironmentVariable(arg, buf, rc);
      }
      mkstring((buf ? buf : ""));
      freeup(z);
      return;

#  elif defined(UNIXSYS)
      char       *buf;
      buf = getenv(arg);
      mkstring((buf ? buf : ""));
      return;

#  else
#  error Must define a GetEnv routines for this system
#  endif

    }

    else {

#  if defined(WINDOWS_SYS)
      nialptr     res;
      int         pos = 0;
      int         numnames = 0;
      char       *environ;

      environ = GetEnvironmentStrings();
      while (environ[pos]) {
        char       *str = strdup(&(environ[pos]));
        char       *name = strtok(str, "=");
        char       *value = strtok(NULL, "");

        numnames++;
        mkstring(name);
        if (value)
          mkstring(value);
        else
          mkstring("");
        pos += (strlen(&environ[pos]) + 1);
        free(str);
      }
      FreeEnvironmentStrings(environ);

#  elif defined(UNIXSYS)

      nialptr     res;
      int         pos = 0;
      int         numnames = 0;

      while (environ[pos]) {
        char       *str = strdup((environ[pos]));
        char       *name = strtok(str, "=");
        char       *value = strtok(NULL, "");

        numnames++;
        mkstring(name);
        if (value)
          mkstring(value);
        else
          mkstring("");
        free(str);
        pos++;
      }

#  else
#  error Must define a GetEnv routines for this system
#  endif

      if (numnames) {
        int         i;

        res = new_create_array(atype, 1, 0, &numnames);
        for (i = (numnames - 1); i >= 0; i--) {
          int         two = 2;
          nialptr     pr = new_create_array(atype, 1, 0, &two);

          store_array(pr, 1, apop());
          store_array(pr, 0, apop());
          store_array(res, i, pr);
        }
        apush(res);
        freeup(z);
        return;
      }
      else {
        apush(Null);
        freeup(z);
        return;
      }
    }
  }
}

#endif             /* ) GETENV */

#ifdef ODBC /* ( */

/* routine provided to extend support of the ODBC interface under Windows.
   The main ODBC work is supported through dll calls using the facility
   implemented by dll.c . However, for efficent fetching of large tables
   we have implemented the primitive sqlfetchnext Sqlhstmt Norows. It is used
   in the ODBC package provided in the Nial library file winodbc.ndf. */



#include "sql.h"

/* 
   When the number of records requested is <= 0, then SQLFetchNext
   will retrieve all of the records, incrementing the container
   by BLOCK_SIZE as necessary.
*/


#define BLOCK_SIZE 1000
#define MAX_DATA_SIZE 1000000
#define MIN_DATA_SIZE 128
#define TRUNCATED_STR "[This String has been Truncated]"

void
iSQLFetchNext(void)
{
  int         getallflag,
              numrows,
              doneflag = 0;
  SQLHSTMT    hstmt;
  SWORD       nresultcols = 0;
  int         i,
              cntr;
  SHORT       rc;
  UCHAR     **data;
  UDWORD     *collen;
  SDWORD     *outlen;
  nialptr     z,
              result;

  z = apop();

  /* validate the argument */
  if (kind(z) != inttype || tally(z) !=2)
  { buildfault("invalid argument to SQLFetchNext");
    freeup(z);
    return;
  }

  hstmt = (SQLHSTMT) * pfirstint(fetch_array(z, 0));
  numrows = *pfirstint(fetch_array(z, 1));

  /* Are we requesting all of the rows */
  getallflag = (numrows <= 0);
  numrows = (getallflag ? BLOCK_SIZE : numrows);

  /* pick up number of columns */
  SQLNumResultCols(hstmt, &nresultcols);

  /* allocated data areas */
  data = (UCHAR **) malloc(nresultcols * sizeof(UCHAR *));
  collen = (UDWORD *) malloc(nresultcols * sizeof(UDWORD));
  outlen = (SDWORD *) malloc(nresultcols * sizeof(SDWORD));
  if (!data || !collen || !outlen)
    exit_cover(NC_MEM_ERR_F);

  /* loop over the columns */
  for (i = 0; i < nresultcols; i++) {
    char        colname[256];
    SWORD       colnamelen,
                coltype,
                scale,
                nullable;

    /* get the column information */
    SQLDescribeCol(hstmt, i + 1, colname, (SWORD) sizeof(colname),
                      &colnamelen, &coltype, &collen[i], &scale, &nullable);

    /* Use an upper limit on the size of the string data */
    collen[i] = ((collen[i] > MAX_DATA_SIZE) ? MAX_DATA_SIZE : collen[i]);

    /* Lower limit the data size to assure that we can store the NULL and
       TRUNCATED strings */
    collen[i] = ((collen[i] < MIN_DATA_SIZE) ? MIN_DATA_SIZE : collen[i]);

    /* allocate space for the data */
    data[i] = (UCHAR *) malloc((collen[i] + 1) * sizeof(UCHAR));
    if (!data[i])
      exit_cover(NC_MEM_ERR_F);


    data[i][0] = '\0';
    outlen[i] = 0;

    /* get the info for column i */
    SQLBindCol(hstmt, (UWORD) (i + 1), SQL_C_CHAR, data[i], collen[i], &outlen[i]);
  }

  cntr = 0;

  /* create the result container */
  result = new_create_array(atype, 1, 0, &numrows);

getmore:                     /* label for infinite get */

  /* loop to fetch the data in chunks */
  while (cntr < numrows) {
    /* do the fetch */
    rc = SQLFetch(hstmt);
    if (rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO) {
      int         nrc = (int) nresultcols;
      nialptr     nrow = new_create_array(atype, 1, 0, &nrc);

      for (i = 0; i < nresultcols; i++) {
        if (outlen[i] == SQL_NULL_DATA) {
          store_array(nrow, i, Null);
        }
        else 
        { if (outlen[i] > collen[i]) /* indicate a truncated string */
            strcpy(data[i] + collen[i] - (strlen(TRUNCATED_STR) + 1), TRUNCATED_STR);
          /* make the item for column i and store it in nrow */
          mkstring(data[i]);
          store_array(nrow, i, apop());
        } /* end of loop over columns */
      }
      /* store the row in the result */
      store_array(result, cntr, nrow);
      cntr++;
    }  /* end of loop for numrows */

    else 
    { /* error occurred in SQLFetch, report it */
      if (rc == -1) {
        char        x[256],
                    y[256];
        SQLINTEGER  i;
        SQLSMALLINT j;

        SQLError(0, 0, hstmt, x, &i, y, 255, &j);
        nprintf(OF_DEBUG_LOG, "Error:%s,%s\n", x, y);
      }

      nprintf(OF_DEBUG_LOG, "SQLFetch=%d\n", rc);
      doneflag = 1;
      break;
    }
  }


  /* if we want all data and we have not hit the end of the data, then
     increase our container and go back and get more */
  if (getallflag && !doneflag) {
    nialptr     tresult;

    numrows += BLOCK_SIZE;
    tresult = new_create_array(atype, 1, 0, &numrows);
    copy(tresult, 0, result, 0, cntr);
    freeup(result);
    result = tresult;
    goto getmore;
  }

  /* contract the container down to the actual size of the data */
  if (cntr < numrows) {
    nialptr     tresult = new_create_array(atype, 1, 0, &cntr);

    copy(tresult, 0, result, 0, cntr);
    freeup(result);
    result = tresult;
  }

  freeup(z);
  /* clean up malloced space */
  for (i = 0; i < nresultcols; i++)
    free(data[i]);
  free(data);
  free(collen);
  free(outlen);
  apush(result);
}

#endif  /* ) ODBC */

