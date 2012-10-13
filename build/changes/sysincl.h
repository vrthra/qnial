/*==============================================================

  HEADER SYSINCL.H

  COPYRIGHT NIAL Systems Limited  1983-2005

This header file is included in routines that need to import C libraries
for various systems and compilers. The defined symbols control which subset
of C libraries are brought in.

================================================================*/

/* MAJ this code should be checked for currency of C library usage */

/*
The following Define switches affect the code

SIGLIB -- gets the signal library <signal.h>
MATHLIB - gets the math library <math.h>
STLIB   - gets the string library <string.h>
CLIB    - gets <ctype.h>
SJLIB   - gets <setjmp.h>
STDLIB  - gets <stdlib.h>
STREAMLIB - gets <iostream.h>

IOLIB   - gets <stdio.h> and <sys/stat.h>
           plus
               <unistd.h> on UNIX systems
               and <dos.h> and <io.h> on Windows systems

TIMELIB - gets the time library <time.h> on Windows systems
          or <sys/types.h>, <sys/times.h>, <sys/param.h> on UNIX systems


LIMITLIB - gets <limits.h> 
            plus <float.h> on UNIX systems

MALLOCLIB - gets <malloc.h> 
              plus <alloc.h> on Windows systems under Borland C.

VARARGSLIB - gets <stdarg.h> and <stddef.h>

WINDOWSLIB - gets <windows.h> under Windows systems

FLOATLIB - gets <float.h> used in handling floating point exceptions on Windows

PROCESSLIB - gets additional system libraries needed by process.c

SOCKETLIB - gets additional system libraries needed by sockets.c


*/

#ifdef SIGLIB
#include <signal.h>
#endif

#ifdef MATHLIB
#include <math.h>
#endif

#ifdef STLIB
#include <string.h>
#endif

#ifdef CLIB
#include <ctype.h>
#endif

#ifdef SJLIB
#include <setjmp.h>
#endif

#ifdef STDLIB
#include <stdlib.h>
#endif

#ifdef STREAMLIB
#include <iostream.h>
#endif


#ifdef IOLIB                 

#include <stdio.h>
#include <sys/stat.h>

#if defined(WINDOWS_SYS)
#include <errno.h>
#include <dos.h>
#include <io.h>
#ifdef __MSC__
#include <graph.h>
#endif
#endif

#ifdef UNIXSYS               
#include <unistd.h>
#endif             

#endif             

#ifdef TIMELIB               /* ( */

#ifdef TIMEHOOK
#ifdef UNIXSYS

#include <sys/types.h>
#include <sys/times.h>
#include <sys/param.h>

#ifdef SOLARIS2
#include <sys/lwp.h>
#include <sys/time.h>
#endif

#else

#include <time.h>

#endif
#endif

#endif             /* TIMELIB  ) */


#ifdef LIMITLIB

#include <limits.h>

#ifndef UNIXSYS
#include <float.h>
#endif

#endif


#ifdef MALLOCLIB
#ifdef __BORLANDC__
#include <alloc.h>
#include <malloc.h>
#else
#include <malloc.h>
#endif
#endif


#ifdef VARARGSLIB
#include <stdarg.h>
#include <stddef.h>
#endif

#ifdef WINDOWSLIB
#include <windows.h>
#endif

#ifdef FLOATLIB
#include <float.h>
#endif

#ifdef PROCESSLIB
#include <sys/types.h>
#include <pwd.h>
#include <fcntl.h>
#ifdef SOLARIS2
#include <shadow.h>
#include <crypt.h>
#endif
#include <netdb.h>
#include <errno.h>
#endif

#ifdef SOCKETLIB

#ifdef WINDOWS_SYS

#  include <winsock.h>

#elif defined(UNIXSYS)

#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netdb.h>
#  include <sys/types.h>
#  include <fcntl.h>
#  include <errno.h>

#else
#error include socket.h and related files
#endif

#endif