/*==============================================================

  WIN32IF.H:  header for WIN32IF.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  Contains macros and function prototypes visible to users of the
  Windows 32-bit interface module.


================================================================*/

/* return code indicating normal command completion */
#define NORMALRETURN 0


 /* failure indicator on return from openfile() */
#define OPENFAILED NULL

#define BADFILENAME (-2)
 /* indicates name is not an empty string       */
 /* or phrase- must differ from OPENFAILED.     */
#define IOERR   (-2)         /* EOF is -1 in stdio */
#define WINDOWERR (-3)

/* win32if.c  prototypes */

#ifdef FP_EXCEPTION_FLAG
extern int  fp_checksignal(void);
#endif

/* stream i/o functions */

extern FILE *openfile(char *flnm, char modechar, char type);
extern void closefile(FILE * file);
extern long fileptr(FILE * file);
extern int  readchars(FILE * file, char *buf, int n, int *nlflag);
extern int  writechars(FILE * file, char *buf, nialint n, int nlflag);
extern int  readblock(FILE * file, char *buf, long n, char seekflag, long pos, char dir);
extern int  writeblock(FILE * file, char *buf, long n, char seekflag, long pos, char dir);

/* command functions */

extern int  command(char *string);
extern void calleditor(char *flnm);

/* timing functions */

extern void get_tstamp(char *timebuf);
extern double get_cputime(void);
extern void inittime(void);

/* environment support */

extern void setup_nialroot(char *nialroot);

/* exception handlers */

extern void initfpsignal(void);
extern void checksignal(int);
