/*==============================================================

  HEADER SWITCHES.H

  COPYRIGHT NIAL Systems Limited  1983-2005

  This switch settings for various versions

================================================================*/


#include "pkgswchs.h"
#ifndef _SWITCHES_H_
#define _SWITCHES_H_

/*
This file controls what code gets included or excluded when
the code of Q'Nial is processed.

A section at the end must be set up for each port of Q'Nial
for a particular system. The section of switches to be included
is surrounded by

#ifdef SYSNAME
  ...
#endif


where SYSNAME names the system. The setting of SYSNAME must be
made in the compiler command line or included in the pkgswchs.h file
as it is generated. Many of the switches described below are included 
when pkgswchs.h is generated. They are descibed here in either case.

Each such SYSNAME block must also include a *specs.h file for the
system. The specs files give system specific information. 
The following ones are currently available:
	sunspecs.h
	win32sp.h
	linuxspecs.h

Sections are present for:
    SUN
    WINDOWS_SYS
    LINUX
    SGI

If a new port is being attempted then the system specific
*specs.h file must be prepared. In addition the file *if.c
must be constructed to do the host system interface (unless it
is a Unix system).

The *specs.h file must be included as the last step in the switches block.


Description of the switches in alphabetical order:

ABORTSIGNAL defined if there is a keystroke that generates an abort
            signal. Only used on UNIX versions to ignore the effect 
            of the signal.

BACKSLASHSEPARATOR   defined in PC versions where directory structure
                     separator is different than that in UNIX.

COMMAND       defined if there is a facility to call a host
              operating system command. Switches in the host
              operation.

DEBUG         generates a debug version which includes many checks.
              Include at the top of the file or on a command line.

HISTORY      switches in the code that remembers the last value. It
             is removed in a DEBUG version to assist in finding
             memory leaks.

JOBCONTROL   defined if a UNIX system has Berkeley style job control.

*LIB            all C includes to standard libraries are handled by
                sysincl.h. Which sections of it to use are controlled in
                each source file by naming the corresponding LIB. This
                is a portability device to assist different operating
                system and compiler conventions. The need for this
                trick has lessened with adoption of ANSI Standard C.

*MACRO*           there are 4 switches that control whether certain
                  small internal routines are treated as a C routine
                  or as a macro. These are used for fine tuning the
                  performance of Q'Nial.

SCIENTIFIC        defined to include the scientific functions.

SYNTACTIC_DOT   Includes codes to support using '.' in the syntax to
                avoid parentheses. Omitted in Version 6 array theory 
                but included in Version 4.


TIMEHOOK   defined if there is a way to get at a run-time clock.
           It switches in the expressions time and timestamp.
           Available in all current versions.

UNIXSYS    defined for a UNIX based system. This is used to switch
           in unix specific code in a few cases where resorting
           to a special interface routine is too messy.

There are a number of switches that omit old code or switch out a
variant that has been tried on a particular version. These are
retained in the code as a reminder of past decisions.


*/

/* Q'Nial features that are normally included */

#define HISTORY
#define SCIENTIFIC
#define COMMAND


/* compiler command lines can set the following switches to remove
   some capabilities and set up variant versions.
      DEBUG  - removes features that interfere with debugging.
               Also includes substantial debugging checks.
*/

#if (defined(DEBUG) || defined(RUNTIMEONLY) || defined(CGINIAL))
#undef HISTORY
#endif



/* system dependent switches */

/*------------------LINUX-----------------------*/



/* the switches for the LINUX version of Unix */

#ifdef LINUX

/* operating system capabilities */
#define ABORTSIGNAL
#define JOBCONTROL
#define FP_EXCEPTION_FLAG
#define USER_BREAK_FLAG


/* define these four switches below to trade speed for space */
#define FETCHARRAYMACRO
#define STOREARRAYMACRO
#define FREEUPMACRO
#define STACKMACROS

/* they should always be undefined if DEBUG is on */
#ifdef DEBUG
#undef FETCHARRAYMACRO
#undef STOREARRAYMACRO
#undef FREEUPMACRO
#undef STACKMACROS
#endif

#include "linuxspecs.h"

#endif             /* LINUX */

/*------------------SUN-----------------------*/

#if defined(SUN4) || defined(SUN3) || defined (SUN386i)

/* the switches for the SUN versions of Unix */

/* operating system capabilities */
#define ABORTSIGNAL
#define JOBCONTROL
#define FP_EXCEPTION_FLAG
#define USER_BREAK_FLAG


/* define these four switches below to trade speed for space */
#define FETCHARRAYMACRO
#define STOREARRAYMACRO
#define FREEUPMACRO
#define STACKMACROS

/* they should always be undefined if DEBUG is on */
#ifdef DEBUG
#undef FETCHARRAYMACRO
#undef STOREARRAYMACRO
#undef FREEUPMACRO
#undef STACKMACROS
#endif

#include "sunspecs.h"

#endif             /* SUN */

/*------------------SGI-----------------------*/

#if defined(SGI)

/* the switches for the SGI versions of Unix */

/* operating system capabilities */
#define ABORTSIGNAL
#define JOBCONTROL
#define FP_EXCEPTION_FLAG
#define USER_BREAK_FLAG


/* define these four switches below to trade speed for space */
#define FETCHARRAYMACRO
#define STOREARRAYMACRO
#define FREEUPMACRO
#define STACKMACROS

/* they should always be undefined if DEBUG is on */
#ifdef DEBUG
#undef FETCHARRAYMACRO
#undef STOREARRAYMACRO
#undef FREEUPMACRO
#undef STACKMACROS
#endif

#include "sgispecs.h"

#endif             /* SGI */

/*------------------OSX-----------------------*/

#ifdef OSX

/* operating system capabilities */
#define ABORTSIGNAL
#define JOBCONTROL
#define FP_EXCEPTION_FLAG
#define USER_BREAK_FLAG


/* define these four switches below to trade speed for space */
#define FETCHARRAYMACRO
#define STOREARRAYMACRO
#define FREEUPMACRO
#define STACKMACROS

/* they should always be undefined if DEBUG is on */
#ifdef DEBUG
#undef FETCHARRAYMACRO
#undef STOREARRAYMACRO
#undef FREEUPMACRO
#undef STACKMACROS
#endif

#include "osxspecs.h"

#endif             /* OSX */

/*----------------WINDOWS -----------------------*/

/* the set of WINDOWS_SYS switches */

#ifdef WINDOWS_SYS


#define BACKSLASHSEPARATOR

/* operating system capabilities */
#define FP_EXCEPTION_FLAG
#define USER_BREAK_FLAG


/* define these four switches below to trade speed for space */
#define FETCHARRAYMACRO
#define STOREARRAYMACRO
#define FREEUPMACRO
#define STACKMACROS

/* they should always be undefined if DEBUG is on */
#ifdef DEBUG
#undef FETCHARRAYMACRO
#undef STOREARRAYMACRO
#undef FREEUPMACRO
#undef STACKMACROS
#endif

#include "win32sp.h"

#endif             /* WINDOWS_SYS */


#endif             /* _SWITCHES_H_ */
