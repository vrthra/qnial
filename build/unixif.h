/*==============================================================

  UNIXIF.H:  header for UNIXIF.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  Contains macros and function prototypes visible to users of the
  UNIX interface module.


================================================================*/

/* return code indicating normal command completion */
#define NORMALRETURN 0

#ifdef FP_EXCEPTION_FLAG
extern int  fp_checksignal();

#endif

/* add some prototypes to avoid annoying warnings
   when compiling with gcc on SunOS4.x */

#ifndef SOLARIS2


extern int  printf(const char *,...);
extern int  fprintf(FILE *, const char *,...);
extern int  fclose(FILE *);
extern int  fflush(FILE *);
extern size_t fread(void *, size_t, size_t, FILE *);
extern size_t fwrite(const void *, size_t, size_t, FILE *);
extern int  fseek(FILE *, long, int);
extern int  fgetc(FILE *);
extern int  rename(const char *, const char *);

extern int  system(const char *);

extern time_t time(time_t *);

/*
extern int  tolower(int);
extern int  toupper(int);
*/

#endif



 /* failure indicator on return from openfile() */
#define OPENFAILED NULL

#define BADFILENAME (-2)
 /* indicates name is not an empty string       */
 /* or phrase- must differ from OPENFAILED.     */
#define IOERR   (-2)         /* EOF is -1 in stdio */
#define WINDOWERR (-3)



/* stream i/o functions */

extern FILE *openfile(char *flnm, char modechar, char type);
extern void closefile(FILE * file);
extern long fileptr(FILE * file);
extern int  readchars(FILE * file, char *buf, int n, int *nlflag);
extern int  writechars(FILE * file, char *buf, nialint n, int nlflag);
extern int  readblock(FILE * file, char *buf, long n, int seekflag, long pos, int dir);
extern int  writeblock(FILE * file, char *buf, long n, int seekflag, long pos, int dir);

#ifdef OMITTED
extern void rename(char *old, char *new);

#endif

/* command functions */

#ifdef COMMAND
extern int  command(char *string);
extern void calleditor(char *flnm);
#endif

/* timing functions */

#ifdef TIMEHOOK
extern void get_tstamp(char *timebuf);
extern double get_cputime(void);
extern void inittime(void);
#endif

/* environment support */

extern void setup_nialroot(char *nialroot);

/* exception handlers */

extern void initfpsignal(void);
extern void checksignal(int);


extern void initunixsignals();
extern void nial_suspend(int signo);

