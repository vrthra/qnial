
# A generic makefile for building Q'Nial versions
# This file contains examples of switch settings and 
# library information for a number of configurations.

.SUFFIXES: .cpp $(SUFFIXES)


COMPILE.cpp=$(CCC) $(CFLAGS) $(CCCFLAGS) -c

.cpp.o:
	$(COMPILE.cpp) $(OUTPUT_OPTION) $<


# flags for specialized versions. These pick up switches set in switches.h
#  and control what definitions and libraries are included. Choose one only.

VERSION= 

#VERSION= -DRUNTIMEONLY
#   remake mainlp.o  main_stu.o

#VERSION= -DCGINIAL
#   remake mainlp.o main_stu.o coreif.o


#this rest of the file has to be edited to select options:
#	- compiler CC
#	- computer system  SYSFLAG
#	- warning level WFLAG
#	- special compiler options CSFLAG
#	- C level debugging DEBUGFLAG
#	- DEBUG version of Q'Nial N_DEBUG
#	- optimization level  OPTFLAGS


# Place a comment here for the particular configuration a
# version of this is made for, e.g.

# target is SUN running SOLARIS and compiled using gcc

#if SOLARIS & gcc turn these on

#CC= gcc
#CCC= g++
#WFLAG= -Wall
#CSFLAG= -pipe
#SYSFLAG= -DSUN4 -DSOLARIS2 -DUNIXSYS  $(VERSION)


# Choose an alternate target by uncommenting a group below or creating a new group



#if LINUX & gcc turn these on

CCC= g++
CC= gcc
WFLAG= -Wall
CSFLAG= -pipe
SYSFLAG= -DLINUX -DUNIXSYS 


#if SUNOS & cc on a SUN turns these on

#CC= cc
#WFLAG=
#CSFLAG=
#SYSFLAG= -DSUN4  -DUNIXSYS $(VERSION)

#if SOLARIS & cc on a SUN turn these on

#CC= cc
#WFLAG=
#CSFLAG=
#SYSFLAG= -DSUN4 -DSOLARIS2 -DUNIXSYS $(VERSION)

#if SUNOS & gcc on a SUN turn these on

#CC= gcc
#CCC= g++
#WFLAG= -Wall
#CSFLAG= -pipe
#SYSFLAG= -DSUN4  -DUNIXSYS $(VERSION)

#if SGI & MipsPro

#CCC= CC
#CC= cc -mips4
#WFLAG= -Wall
#CSFLAG=
#SYSFLAG= -DSGI -DUNIXSYS $(VERSION)


#if OSX  on Intel & gcc turn these on

#CCC=g++
#CC=gcc
#WFLAG=-Wall
#CSFLAG=-pipe
#SYSFLAG=-DOSX -DOSX-INTEL -DUNIXSYS


#if OSX  on POWERPC & gcc turn these on

#CCC=g++
#CC=gcc
#WFLAG=-Wall
#CSFLAG=-pipe
#SYSFLAG=-DOSX -DOSX-POWERPC -DUNIXSYS



#if IBMRISC & cc

#WFLAG=
#CSFLAG=
#SYSFLAG= -DIBMRISC $(VERSION)

########## optimization or debugging section ##############

# the flag to indicate optimization. Set optimization off if C debugging
# is on or during the initial port. Use O2 or O3 for production.

#OPTFLAGS=
#OPTFLAGS= -O
OPTFLAGS= -O2
#OPTFLAGS= -O3

# a flag added to CFLAGS if NIAL debugging is on

#N_DEBUG= -DDEBUG
N_DEBUG= 


# the C debugging switch

#DEBUGFLAG= -ggdb
DEBUGFLAG=

# the debug library which is needed with gdb

#DEBUG_LIB= -lg
DEBUG_LIB= 

# the socket lib used with Windows and gcc on Solaris
#SOCKLIB= -lsocket -lnsl
#SOCKLIB=

LIBS= -lm -ltermcap $(DEBUG_LIB) $(SOCKLIB) 


# The Actual CFLAGS line

# FULL 
CFLAGS= $(SYSFLAG) $(DEBUGFLAG) $(N_DEBUG)  $(OPTFLAGS)  $(WFLAG) $(XCFLAGS) 

CCCFLAGS=

COREOBJS=	absmach.o arith.o atops.o \
	basics.o blders.o \
	compare.o coreif.o \
	diag.o \
	eval.o\
	insel.o \
	lib_main.o linalg.o logicops.o \
	main_stu.o mainlp.o messages.o \
	parse.o picture.o profile.o\
	scan.o symtab.o systemops.o \
	trig.o trs.o \
	userops.o unixif.o utils.o \
	wsmanage.o \
	fileio.o sockets.o process.o 
	

REGEXPOBJS= re_core.o re_main.o re_class.o regexp.o

ALLOBJS= $(COREOBJS) $(REGEXPOBJS)

all: 	nial 

lib_main.o:	version.h
wsmanage.o:	version.h

nial: $(ALLOBJS) 
	$(CC) $(LINKFLAG) $(ALLOBJS) $(LIBS) -o nial


# This needs to be run a root
makeserver:
	chown root nial
	chmod u+s nial


