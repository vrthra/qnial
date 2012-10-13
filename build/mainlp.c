/*==============================================================

  MODULE   MAINLP.C


  COPYRIGHT NIAL Systems Limited  1983-2005

  This module does the execution of one command. The main loop
  is now encoded in main_stu.c.
  It also contains the routine exit_cover which catches jumps from
  lower level routines.

================================================================*/


/* Q'Nial file that selects features and loads the xxxspecs.h file */

#include "switches.h"

#define STLIB
#define IOLIB
#define SJLIB
#ifdef WINDOWS_SYS
#define MALLOCLIB
#define WINDOWSLIB
   /* needed for dll.h */
#endif

#include "sysincl.h"

/* standard library header files */


/* Q'Nial header files */

#include "coreif_p.h"        /* for error_env and rc codes */
#include "dll.h"             /* for call to DLLcleanup */

#include "mainlp.h"
#include "qniallim.h"
#include "lib_main.h"
#include "absmach.h"
#include "basics.h"
#include "if.h"

#include "fileio.h"          /* for nprintf */
#include "lexical.h"         /* for BLANK and HASHSYMBOL */
#include "symtab.h"          /* symbol table macros */
#include "roles.h"           /* role codes */
#include "eval.h"            /* for varname */
#include "parse.h"           /* for parse */
#include "wsmanage.h"        /* for wsload */
#include "profile.h"         /* for clear_profiler */
#include "systemops.h"       /* for ihost */
#include "blders.h"
#include "token.h"


#ifdef HISTORY
static void hist(void);
#endif

static int  leaving_rc;      /* used to pass return code when leaving with a
                              * continue. */

static int  doloadsave(int loadsavesw);


int
process_loadsave_recovery(int loadsavesw)
{
  if (loadsavesw != NC_WS_LEAVING) {
    cleanup_ws();
    doloadsave(loadsavesw);
    return NC_NO_ERR_N;
  }
  else {                     /* come here to save the continue ws on way out */
    clearstack();
    clearheap();             /* to ensure no temporaries are stored */
    wsdump(wsfileport);
    
/* we assume NC_StopNial will be called so we do not call cleanup_session(); */
    longjmp(error_env, leaving_rc);
    return NC_NO_ERR_N;     /* not needed but it avoids a warning during compile */
  }
}



/* main interpreter function to execute a single action */

int
do1command(char *input)
{                            /* for some hosts no locals are allowed in this
                              * routine.  */
  int         loadsavesw;
#ifdef DEBUG
  nialint     total;
#endif


  /* isave and iload will do a longjmp to here to do the reading and writing
     with the workspace in a normal state */
  loadsavesw = setjmp(loadsave_env);
  if (loadsavesw)
    return process_loadsave_recovery(loadsavesw);


  /* normal command processing starts here */

  /* These variables are set here rather than in ieval() because of
  the possibility of ieval calling itself recursively using execute. */

  current_env = Null;        /* set to global environment */
  nialexitflag = false;
  continueflag = false;
  topstack = (-1);
  
#ifdef DEBUG
  total = checkavailspace();
  nprintf(OF_DEBUG, "available space %d\n", total);
  checkfortemps();
#endif

  mkstring(input);

#ifdef HISTORY
#ifndef RUNTIMEONLY
  if (usehistory)
    if (tally(top) != 0 && fetch_char(top, 0) == ']')
      hist();
#endif
#endif


  if (tally(top) == 0) 
  {
    freeup(apop());
  }
  /* to get rid of empty line */
  else {
    char        firstchar;
    int         i = 0;

    while (i < tally(top) && fetch_char(top, i) <= BLANK)
      i++;
    firstchar = fetch_char(top, i);
    if (firstchar == HASHSYMBOL) {
      /* the line is a remark, just remove it */
      freeup(apop());
    }                       
#ifdef COMMAND
    else 
    if (firstchar == '!') {  /* treat line as host command */
      nialptr     x = apop();

      mkstring(pfirstchar(x) + (i + 1));  /* creates a Nial string from the
                                             command with the exclamation
                                             symbol removed */
      ihost();
      freeup(apop());
      freeup(x);
    }
#endif
    else {                   /* execute the line */
      iscan();
      parse(true);
#ifdef DEBUG
      if (debug) {
        nprintf(OF_DEBUG, "memcheck in mainloop before ieval\n");
        memchk();
        nprintf(OF_DEBUG, "done\n");
      }
#endif
      /* remember parse tree so if doing a save it can be freed */
      ieval();

      init_debug_flags();

#ifdef HISTORY
      /* remember last value for history */
      /* put here so it never is done recursively */

      if (usehistory) {
        if (lasteval != -1) {
          decrrefcnt(lasteval);
          freeup(lasteval);
        }
        lasteval = top;
        incrrefcnt(lasteval); /* protect lasteval */
      }
#endif

#ifdef DEBUG
      if (debug) {
        nprintf(OF_DEBUG, "memcheck in mainloop after ieval\n");
        memchk();
        nprintf(OF_DEBUG, "done\n");
      }
#endif
      if (top != Nullexpr) {
        ipicture();  /* prepare the out put picture */

#ifdef DEBUG
        if (debug) {
          nprintf(OF_DEBUG, "memcheck in mainloop after ipicture\n");
          memchk();
          nprintf(OF_DEBUG, "done\n");
        }
#endif
        show(apop());  /* call routine that displays the picture,
                          wrapping it if necessary */

#ifdef DEBUG
        if (debug) {
          nprintf(OF_DEBUG, "memcheck in mainloop after display\n");
          memchk();
          nprintf(OF_DEBUG, "done\n");
        }
#endif
      }
      else
        apop(); /* remove the Nullexpr. No need to test it as free. */
      if (topstack != -1) {
        /* the stack not empty here indicates an internal error in the interpreter */
        while (topstack >= 0)
          freeup(apop());
        exit_cover(NC_STACK_NOT_EMPTY_I);
      }
#ifdef DEBUG
#ifdef __BORLANDC__
/*  This helps find fatal errors during debugging. If this aborts
   then activate similar checks in reserve and release and use the
   debugger to isolate where the messup is occurring.
*/
      if (heapcheck() < 0) {
        nprintf(OF_DEBUG, "C heap has been corrupted at end of mainloop\n");
        nabort(NC_ABORT_F);
      }
#endif
#endif
    }
  }
  return (NC_NO_ERR_N);      /* Normal result (possibly a fault if triggered
                              * is false). */
}

/* exit_cover is the routine that handles all jumps out of the normal
   flow of the evaluator. There are several circumstances where this
   can happen, such as:
     fatal errors
     termination request by program control
     user interrupt (Ctrl C or equivalent)
     program interrupt (toplevel call)
     internal warnings (to be recovered from)
  Each return code is assigned to a class.
*/

void
exit_cover(int flag)
{
  static int  recursive = 0;

#ifdef DEBUG
printf("exit_cover called with flag %d recursive %d\n",flag,recursive);
#endif

  /* prevent exit_cover from getting in a recursion due to the routines it is
   * calling. It would be a design error if it happens, but we leave this
   * check in for safety. */
  if (!recursive)
    recursive = 1;
  else {
    /* This exit prevents an infinite error recovery loop */
    nprintf(OF_MESSAGE_LOG, "a recursive use of exit_cover was encountered. %d\n", flag);
    longjmp(error_env, NC_ERROR_LOOP_F);
  }

  /* do not place a CSTACKFULL test here because it prevents a cleanup. */

  switch (NC_ErrorType(flag)) {
    case NC_NORMAL:          /* there should be no "NORMAL" return codes used
                                in exit_cover calls */
#ifdef DEBUG
        nprintf(OF_DEBUG, "exit_cover call with NC_NO_ERROR_N\n");
#endif
        break;

    case NC_BREAK:           /* treat a break the same as a warning */

#ifdef DEBUG
printf("in NC_BREAK case of exit_cover\n");
#endif

    case NC_WARNING:
        {
          nialptr     sym,
                      entr,
                      x;

          /* show the warning */
          nprintf(OF_MESSAGE_LOG, "Warning: %s\n", NC_ErrorMessage(flag));

          /* store the message and clean up */
          strcpy(toplevelmsg, NC_ErrorMessage(flag));
          cleanup_ws();

          /* see if there is a RECOVER expression */
          x = makephrase("RECOVER");
          entr = lookup(x, &sym, passive);
          freeup(x);
          if (entr != notfound && sym == global_symtab && sym_role(entr) == Roptn) {  
           /* RECOVER exists. jump back with return code to trigger it. */
            recursive = 0;   /* turn this off since we hope to recover */
            longjmp(error_env, NC_RECOVER_N);
          }
          break;
        }

    case NC_INTERNAL_ERROR:
        cleanup_ws();
        break;

    case NC_TERMINATE:
        cleanup_ws();
        /* fall through to FATAL */
    case NC_FATAL:
        recursive = 0;       /* reset since leavenial does a longjmp to error_env */
        leavenial(flag);
        break;

    case NC_STARTUP_ERROR:
        break;

    default:
        break;
  }
  recursive = 0;             /* reset since we are leaving exit_recover */

#ifdef DEBUG
  if (!(NC_IsWarning(flag) || NC_IsTerminate(flag) || NC_IsBreak(flag)))
    nabort(flag);
  else
    longjmp(error_env, flag);
#else
  longjmp(error_env, flag);
#endif
}



#ifdef HISTORY               /* ( */

/* this routine is called whenever a ']' has been noticed at the beginning
   of the line. It is used to capture last computed value in a Nial variable.
*/

static void
hist(void)
{
  nialptr     z,
              name,
              sym,
              entr,
              idlist;

  /* scan the line beginning with ']' into z */

  iscan();
  z = apop();

  if (lasteval == -1) {      /* no last eval to assign */
    freeup(z);
    apush(makefault("?no previous value"));
    return;
  }

  if (tally(z) >= 5) {       /* there is an argument to hist */
    if (intval(fetch_array(z, 3)) == identprop)
      /* do assignment directly using assign */
    { /* get the name and validate it */
      name = fetch_array(z, 4);
      if (varname(name, &sym, &entr, false, true)) {
        if (sym_role(entr) == Rvar || sym_role(entr) == Rident) {
          if (sym_role(entr) == Rident)
            st_s_role(entr, Rvar);
          /* build an idlist parse tree node and do the assignment */
          idlist = nial_extend(st_idlist(), b_variable((nialint) sym, (nialint) entr));
          assign(idlist, lasteval, false, false);
          freeup(idlist);
          apush(Nullexpr);
        }
        else
          buildfault("not a variable");
      }
      else
        buildfault("invalid name *** this shouldn't happen");
      freeup(z);
      return;
    }
    else {
      buildfault("bad argument to history notation ]");
      freeup(z);
      return;
    }
  }
  else {
    buildfault("bad argument to history notation ]");
    freeup(z);
    return;
  }
}

#endif             /* ) HISTORY */

/* routine to test if a Latent expression exists in the workspace */

int
latenttest(void)
{
  nialptr     entr,
              sym,
              x;

  /* looks up the term Latent in the global environment */
  current_env = Null;
  x = makephrase("LATENT");
  entr = lookup(x, &sym, passive);
  freeup(x);
  return (entr != notfound) && sym == global_symtab && (sym_role(entr) == Rexpr);
}

/* routine to clean up resources used during evaluation on a longjmp to
   top level */

void
cleanup_ws()
{
  /* cleanup common to all other longjmp calls */
  cleardeffiles();           /* clean up open ndf files */
  clearstack();              /* clear the stack and list of temp arrays */
  clearheap();               /* remove all arrays with refcnt 0 */
  closeuserfiles();          /* close files to avoid interference */
  clear_call_stack();        /* clears names of called routines */
#ifdef PROFILE
  clear_profiler();          /* remove profiling data structures if in use */
#endif

  /* reset all debugging flags */
  init_debug_flags();
  trace = false;             /* to avoid it being left on accidentally */
  startfilesystem();
#ifdef HISTORY
  lasteval = -1;
#endif
  fflush(stdin);
}

static int
doloadsave(int loadsavesw)
{                            /* handle workspace save and load */
  int         res = NC_NO_ERR_N;

  if (loadsavesw == NC_WS_LOAD) { /* a load to do */
    wsload(wsfileport, true);
    current_env = Null;      /* set to global environment */
#ifdef HISTORY
    lasteval = -1;
#endif
    /* clear out old file system when the workspace was saved */
    decrrefcnt(filenames);
    freeup(filenames);
    /* restart file system of loaded workspace */
    startfilesystem();
    /* set initial prompt */
    strcpy(prompt, "     ");
    init_debug_flags();
    if (latenttest()) {      /* Latent exists. jump back with return code to
                              * trigger it. */
      longjmp(error_env, NC_LATENT_N);
    }
  }
  else {    /* (loadsavesw == NC_WS_SAVE) by design so a save to do */
    wsdump(wsfileport);
    {
      nialptr     entr,
                  sym,
                  checkname;

      /* CHECKPOINT expression handling */
      current_env = Null;
      checkname = makephrase("CHECKPOINT");
      entr = lookup(checkname, &sym, passive);
      freeup(checkname);
      if ((entr != notfound) && sym == global_symtab && (sym_role(entr) == Rexpr)) { 
         /* Checkpoint exists. jump back with return code to trigger it. */
        longjmp(error_env, NC_CHECKPOINT_N);
      }
    }
  }
  return res;
}

void
leavenial(int rc)
{
  leaving_rc = rc;
  if (continueflag) {        /* use global wsfileport to hold file ptr so
                                code at longjmp target can find it. */
    wsfileport = openfile("continue.nws", 'w', 'b');
    if (wsfileport != OPENFAILED)
      longjmp(loadsave_env, NC_WS_LEAVING);
    /* this ensures the wssave for continue.nws is done at top level */
    else {
      nprintf(OF_NORMAL_LOG, "unable to open continue.nws\n");
      longjmp(error_env, rc);
    }
  }

  else 
  if (continuefound) {  /* new case added to remove a continue ws after a bye */
    unlink("continue.nws");
    longjmp(error_env, rc);
  }

  else {
    longjmp(error_env, rc);
  }
}
