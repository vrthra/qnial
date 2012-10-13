/* ==============================================================

    RE_MAIN.H

  COPYRIGHT NIAL Systems Limited  1983-2005


   The header file for re_main.cpp. Provides the prototypes for exported
   routines.

================================================================*/
extern char *regex_s(char *pattern, char *repl, char *string, char *opts);
extern char *regex_tr(char *pattern, char *repl, char *string, char *opts);
extern int  regex_m(char *pattern, char *string, char *opts);
extern char *getsub(int i);
extern int  setcachesize(int newsize);
