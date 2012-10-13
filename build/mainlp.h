/*==============================================================

  MAINLP.H: header for MAINLP.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  Prototypes of functions that control the outer loop of the interpreter

================================================================*/

extern int  process_loadsave_recovery(int); /* used in coreif.c and mainlp.c */
extern int  do1command(char *input); /* used in coreif.c */
extern void exit_cover(int);  /* used in many modules for error recover */
extern int  latenttest(void); /* used in coreif.c and mainlp.c */
extern void cleanup_ws(void);  /* used in coreif.c main_Stu.c and mainlp.c */
extern void leavenial(int);   /* used in absmach.c and mainlp.c */
