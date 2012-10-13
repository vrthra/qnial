/*==============================================================


  MODULE   DLL.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains routines to allow Nial level access to some Windows
  DLLs
  --------:
  IMPLEMENTED:

  registerdllfun <nialname> <realdllname> <path/filenameOfDll>
                <resultType> <Array of arg types>

  calldllfun <nialname> [<argument> ...]

  _dlllist <integer>              ;0  returns the integer to type name list
                                  ;1  returns the internal dll call register list

  DLLLIST       ; Nial level implementation uses _dlllist to generate a
                ; a table that look like the original arguments to the
                ; registerdllfun calls

  POSSIBLE:

  unregisterdllfun <nialname>                     ;obvious

  registerstruct <structname> <structdescriptor>  ;see discussion below



  June 13, 1996 / Chris Walmsley
   First pass complete, also passing simple parameters to DLL routines.
  There is no argument checking (better be right).

  Oct 29, 1996 / Chris Walmsley
    Move the storeage of the register table to the workspace to allow
    preservation of the table across loads and saves.  This requires
    that all DLL be FreeLibrary upon a save, and that the existing table
    be deleted when loading a new workspace.

  Oct 30, 1996 / Chris Walmsley
    I was calling DLLcleanup() from wsload(), but it is possible that
    there is no existing workspace at this point.  So...I moved it to
    clear_abstract_machine.  This is called before loading a second
    or subsequent workspace into Nial.  My problem was that wsload()
    was being called when a workspace was specified on the commandline
    and the workspace did not have the dlllist value (the workspace did
    not exist at all) and thus the call to DLLcleanup() in wsload(), was
    dying.

    So:

      DLLcleanup() is called from:
       	 - clear_abstract_machine()
         - NC_StopNial()

         o This routine unloaded the DLLs and eliminates the actual table.

      and DLLclearflags() is called from:
         - wsdump()
         - DLLcleanup()

         o This routine simply unloades the DLLs and sets the flags to
           indicate that.

  Issues:
   - Variable parameters
    This is not hard, but requires designing a way to indicate
    that a parameter is variable, and designing a way to get it
    back to the user.  It may be that the results of variable
    parameters are added to the expected result. (IMPLEMENTED)

    ie.  If a DLL call returned one Integer, but had 2 variable
    parameters, then the result would be a triple, the first of
    which would be the Integer, and the second two would be the
    variable parameters.

   - Structures (used as variable params too)
    This is more tricky, but could be solved by allowing the user
    to register structure mappings.  These would be nested arrays
    that define the number of fields and their sizes.  The user
    could then use the newly defined name in a registerdllfun
    call.

    Additionally, nested structured should be available, and this would
    require being able to reference registered structure mappings.

    for example:

      registerstrutcure "POINT ["DWORD,"DWORD];
      registerstructure "RECT ["POINT,"POINT];

    where "DWORD is a base type.  And now "POINT and "RECT could be
    used in the registerdllfun calls.

    NOTE: we would also need something like "*POINT to indicate that
    NIAL should allocate the memory for a "POINT, but pass the pointer
    to the memory.

    for example:

      registerstructure "RECT_PTR ["*POINT,"*POINT];

      registerdllfun "getRect "getRect "some.dll "INT ["RECT_PTR,"*INT];

      res := calldllfun "getRect [[9,5],[3,5]] 5

    NIAL would have to check that the arguments matched the definition,
    and then malloc some space for two pointers to "POINT structures,
    then malloc space for two "POINT structures, and then fill in their
    values, and then place the pointer values in the first structure
    "RECT_PTR, and pass that as the first argument.  NIAL would also
    have to malloc a space for an integer and fill it in with (5), and
    then pass that pointer as the second argument.

    Variable parameters might be denoted with a '&' as in
      "&POINT      --variable parameter POINT
       or
      "&*POINT     --variable parameter to ?

The Nial DLL facility has NOT been extended to include the ability to
register structures. Most DLL calls can be handled with the supported
capability.


================================================================*/


/* Q'Nial file that selects features and loads the xxxspecs.h file */

#include "switches.h"

#ifdef CALLDLL               /* So we can switch out the entire package */

/* standard library header files */

#define STDLIB
#define CLIB
#define SJLIB
#define IOLIB
#define STLIB
#define VARARGSLIB
#define WINDOWSLIB   
        /* need <windows.h> for wvsprintf */

#include "sysincl.h"


/* Q'Nial header files */

#include "messages.h"
#include "qniallim.h"
#include "lib_main.h"
#include "absmach.h"
#include "if.h"
#include "dll.h"
#include "fileio.h"
#include "ops.h"     /* for append */
#include "basics.h"


int _dllentrysize_ = 15;

/* BASE TYPES */
/* This table maps NIAL level string names to
 * type constants
 *
 * The "ispointer" field indicates that the space for the variable
 * will always be malloced and free'd regardless of what the user
 * specifies as modifiers.  The IsPointer routine and the
 * StringToIsNativePointer are used to set this flag in the call table.
 */

/* INDENT OFF */
struct {
  ResType     type;          /* enum value */
  char       *name;          /* Nial name to be used */
  bool        ispointer;     /* indicates that the type a native pointer type, 
                                which allows for automatic clean up */
}           typetable[] = {

  { _tCHAR, "CHAR", false },
  { _tSHORT, "SHORT", false },
  { _tLONG, "LONG", false },
  { _tWCHAR, "WCHAR", false },
  { _tLPSTR, "LPSTR", true },
  { _tLPTSTR, "LPTSTR", true },
  { _tHANDLE, "HANDLE", false },
  { _tDWORD, "DWORD", false },
  { _tLPDWORD, "LPDWORD", true },
  { _tWORD, "WORD", false },
  { _tBOOL, "BOOL", false },
  { _tHWND, "HWND", false },
  { _tUINT, "UINT", false },
  { _tWPARAM, "LRESULT", false },
  { _tLPARAM, "LPARAM", false },
  { _tLRESULT, "LRESULT", false },
  { _tHMENU, "HMENU", false },
  { _tLPCTSTR, "LPCTSTR", true },
  { _tINT, "INT", false },
  { _tUCHAR, "UCHAR", false },
  { _tSCHAR, "SCHAR", false },
  { _tSDWORD, "SDWORD", false },
  { _tSWORD, "SWORD", false },
  { _tUDWORD, "UDWORD", false },
  { _tUWORD, "UWORD", false },
  { _tSLONG, "SLONG", false },
  { _tSSHORT, "SSHORT", false },
  { _tULONG, "ULONG", false },
  { _tUSHORT, "USHORT", false },
  { _tDOUBLE, "DOUBLE", false },
  { _tFLOAT, "FLOAT", false },
  { 0, NULL, false }
};

/* INDENT ON */

/* Some debuggin flags: Not yet used */
static      calldll_debug = 0;  /* 0 = off, 1 = count, 2 = count & log */

static bool IsVarArg(char *str);
static bool IsPointer(char *str);

/* the following routine saves a mapping to a dll call in the table.
   The arguments are:
  <nialname> <realdllname> <path/filenameOfDll> <resultType> <Array of arg types>
*/

void
iregisterdllfun(void)
{
  int         i,
              index;
  ResType     argtypes[MAX_ARGS];  /* list of types of arguments */
  bool        vararg[MAX_ARGS];    /* is the argument a variable arg */
  bool        ispointer[MAX_ARGS]; /* is the argument a pointer */
  int         varargcount = 0;     /* number of variable args */
  int         ispointercount = 0;  /* number of pointer args */
  nialptr     z = apop();
  char       *nialname;            /* names used by calldllfun */
  char       *dllname;             /* the real name of the function */
                                   /* in the DLL file */
  char       *library;             /* name of the DLL file */
  ResType     resulttype;          /* the type of the result */
  nialptr     nargtypes;           /* the arg type array */
  int         argcount;
  int         j;
  int         sz;
  nialptr     current;       /* usually hold the current entry in the dlllist */
  int         is_register;

  /* if we have 5 args we are registering a function */

  if ((tally(z) == 5) && (kind(z) == atype))
    is_register = 1;         /* register mode */

  else 
  /* only one arg and it is a char or a phrase, we are deleting the fun */
  if ((kind(z) == chartype) || (kind(z) == phrasetype))
    is_register = 0;         /* delete mode */

  else {                     /* error mode */
    apush(makefault("?Incorrect number of arguments to registerdllfun"));
    freeup(z);
    return;
  }

  if (is_register) {
    /* The Nial level name for the DLL function */
    STRING_CHECK(z, 0)
      nialname = STRING_GET(z, 0);


    /* The internal DLL name for the function */
    STRING_CHECK(z, 1)
      dllname = STRING_GET(z, 1);

    /* The name of the library file */
    STRING_CHECK(z, 2)
      library = STRING_GET(z, 2);

    /* The name of the result type */
    STRING_CHECK(z, 3)
      resulttype = StringToTypeID(STRING_GET(z, 3));

    /* did we find an unrecognized result type? */
    if (resulttype < 0) {
      apush(makefault("?Return type not recognized"));
      freeup(z);
      return;
    }

    if (kind(fetch_array(z, 4)) != atype) {
      apush(makefault("?Argument must be a list of strings or phrases"));
      freeup(z);
      return;
    }

    nargtypes = fetch_array(z, 4);
    argcount = tally(nargtypes);

    /* Check each of the argument type */
    for (j = 0; j < argcount; j++)
      STRING_CHECK_FREEUP(nargtypes, j, z)
      /* create an integer list of argument types from the phrase/string list */
        for (i = 0; i < argcount; i++) {
        char       *tmp;

        tmp = pfirstchar(fetch_array(nargtypes, i));  /* safe: no allocation */
        argtypes[i] = StringToTypeID(tmp);

        /* the ith argument name was not recognized */
        if (argtypes[i] < 0) {
          char        stmp[256];

          wsprintf(stmp, "?Type \"%s\" for argument %d not recognized", tmp, i + 1);
          apush(makefault(stmp));
          freeup(z);
          return;
        }
        /* set the vararg and ispointer flags for this arg */
        vararg[i] = IsVarArg(tmp);
        ispointer[i] = IsPointer(tmp);
        /* keep count of these special args */
        if (vararg[i])
          varargcount++;
        if (ispointer[i])
          ispointercount++;
      }

    /* NEW workspace Version */

    /* If the list does not yet exist, then create a one element list here */
    if (tally(dlllist) == 0) {
      nialptr     tmp = create_new_dll_entry; /* build a empty entry */

      setup_dll_entry(tmp)   /* fill it with empty data */
        apush(tmp);
      isolitary();           /* make it a list */
      decrrefcnt(dlllist);
      freeup(dlllist);
      dlllist = apop();
      incrrefcnt(dlllist);
      index = 0;
    }
    else {
      int         pos;

      /* does the requested name already exist in out list? */
      if ((pos = inlist(nialname)) >= 0) {
        /* yes it's here already, so note its position, and free the old
         * entry */
        index = pos;
        freeEntry(index);
      }
      else {
        /* if we got here, then we need to create a new entry and add it to
         * and existing dlllist */
        nialptr     tmp = create_new_dll_entry;

        setup_dll_entry(tmp)
          decrrefcnt(dlllist);
        append(dlllist, tmp);
        dlllist = apop();
        incrrefcnt(dlllist);
        index = tally(dlllist) - 1; /* this is the location of the new entry */
      }
    }



    /* grab the entry to work on */
    current = fetch_array(dlllist, index);

    /* fill in data */
    set_handle(current, NULL);
    set_nialname(current, nialname);
    set_dllname(current, dllname);
    set_callingconvention(current, (dllname[0] == '_' ? C_CALL : PASCAL_CALL));
    set_library(current, library);
    set_isloaded(current, false);
    set_resulttype(current, resulttype);
    set_argcount(current, argcount);
    set_varargcount(current, varargcount);
    set_ispointercount(current, ispointercount);

    sz = argcount;
    replace_array(current, 10, (sz == 0 ? Null : new_create_array(inttype, 1, 0, &sz)));
    for (j = 0; j < sz; j++)
      set_argtypes(current, j, argtypes[j]);

    replace_array(current, 11, (sz == 0 ? Null : new_create_array(booltype, 1, 0, &sz)));
    for (j = 0; j < sz; j++)
      set_ispointer(current, j, ispointer[j]);

    replace_array(current, 14, (sz == 0 ? Null : new_create_array(booltype, 1, 0, &sz)));
    for (j = 0; j < sz; j++)
      set_vararg(current, j, vararg[j]);
  }
  else { /* delete entry code */
    int         pos;

    if (tally(dlllist) == 0) {
      apush(makefault("?no DLLs registered"));
        freeup(z);
      return;
    }
    /* must be in the list */
    if ((pos = inlist(pfirstchar(z))) >= 0) {
      /* yes it's here already, so note its position, and free the old entry */
      freeEntry(pos);
      decrrefcnt(dlllist);
      apush(createint(tally(dlllist)));
      itell();
      pair(apop(), createint(pos));
      iexcept();
      pair(apop(), dlllist);
      ichoose();
      dlllist = apop();
      incrrefcnt(dlllist);
    }
  }

  apush(Nullexpr);
  freeup(z);
  return;
}



void
icalldllfun(void)
{
  nialptr     current;
  int         i;
  double      dresult;
  int         result;
  int         args[2 * MAX_ARGS]; /* need space for doubles */
  int         pos;
  nialptr     z = apop();
  char       *nialname;
  int         intsUsed;
  int         no_args = 0;

  /* reset all arguments to so we can see what's going on */
  for (i = 0; i < MAX_ARGS; args[i++] = -1);

  /* The following test allows for a call with no arguments */
  /* which will only have the NIAL name of the DLL function */
  if ((kind(z) == chartype) || (kind(z) == phrasetype)) {
    no_args = 1;
    nialname = pfirstchar(z);/* safe: no allocation */
    }
  else if (kind(z) == atype)
    nialname = pfirstchar(fetch_array(z, 0)); /* safe: no allocation */
  else {
    apush(makefault("?DLL Call Argument Error"));
    freeup(z);
    return;
    }

  /* look for name in table */
  pos = inlist(nialname);

  /* If not in table then exit */
  if (pos == -1) {
    char        tmp[256];

    wsprintf(tmp, "?DLL Function \"%s\" not registered", nialname);
    apush(makefault(tmp));
    freeup(z);
    return;
  }

  /* grab the current entry for convenient use */
  current = fetch_array(dlllist, pos);

  /* If DLL not loaded, then load it */
  if (!get_isloaded(current)) {
    /* Store the handle */
    set_handle(current, LoadLibrary(get_library(current)));
    /* if we failed */
    if (!get_handle(current)) {
      char        error[256];

      set_isloaded(current, false);
      wsprintf(error, "?DLL load error num: %d", GetLastError());
      apush(makefault(error));
      freeup(z);
      return;
    }
    else
      set_isloaded(current, true);

    /* we must now map the function */
    set_function_c(current, GetProcAddress(get_handle(current),
                                           get_dllname(current)));
    set_function_p(current, get_function_c32(current));

    /* If we failed to map the function */
    if (!(*(get_function_c32(current)))) {
      char        tmp_err[256];
      int         lasterror = GetLastError();

      set_isloaded(current, false);
      freeEntry(pos);        /* also unload the Library here */
      wsprintf(tmp_err, "?DLL function \"%s\" load error num: %d\n",
               get_dllname(current), lasterror);
      apush(makefault(tmp_err));
      freeup(z);
      return;
    }
  }



  /* Now the DLL should be loaded and the function mapped */

  /* Check the argument count */
  if (get_argcount(current) != (no_args?0:(tally(z) - 1))) {
    apush(makefault("?Incorrect number of arguments to call"));
    freeup(z);
    return;
  }

  /* map the arguments */
  intsUsed = 0;
  for (i = 1; i < (no_args?0:tally(z)); i++) {
    /* copy arguments from NIAL array (argument to this fun), into the C
     * level array for passing to the DLL functions */
    int         rc = MapNialArg(z,
                                i,
                                get_argtypes(current, i - 1),
                                &args[intsUsed],
                                get_vararg(current, i - 1),
                                get_ispointer(current, i - 1),
                                &intsUsed);

    if (rc < 0) {
      char        tmp[256];

      wsprintf(tmp, "?Error mapping argument number %d, check type!", -rc);
      apush(makefault(tmp));
      freeup(z);
      return;
    }
  }

  /* Only push on the correct number of arguments ? */
  /* Is this necessary, the answer is yes under Pascal calling conventions */
  if (get_callingconvention(current) == PASCAL_CALL) {
    switch (get_argcount(current)) {
      case 0:
          result = (*get_function_p(current)) ();
          break;
      case 1:
          result = (*get_function_p(current)) (args[0]);
          break;
      case 2:
          result = (*get_function_p(current)) (args[0], args[1]);
          break;
      case 3:
          result = (*get_function_p(current)) (args[0], args[1], args[2]);
          break;
      case 4:
          result = (*get_function_p(current)) (args[0], args[1], args[2], args[3]);
          break;
      case 5:
          result = (*get_function_p(current)) (args[0], args[1], args[2], args[3],
                                               args[4]);
          break;
      case 6:
          result = (*get_function_p(current)) (args[0], args[1], args[2], args[3],
                                               args[4], args[5]);
          break;
      case 7:
          result = (*get_function_p(current)) (args[0], args[1], args[2], args[3],
                                               args[4], args[5], args[6]);
          break;
      case 8:
          result = (*get_function_p(current)) (args[0], args[1], args[2], args[3],
                                        args[4], args[5], args[6], args[7]);
          break;
      case 9:
          result = (*get_function_p(current)) (args[0], args[1], args[2], args[3],
                                         args[4], args[5], args[6], args[7],
                                               args[8]);
          break;
      case 10:
          result = (*get_function_p(current)) (args[0], args[1], args[2], args[3],
                                         args[4], args[5], args[6], args[7],
                                               args[8], args[9]);
          break;
      default:
          result = 666;      /* ??? */
    }
  }
  else if (get_callingconvention(current) == C_CALL) {
    if ((get_resulttype(current) == _tFLOAT) ||
        (get_resulttype(current) == _tDOUBLE))
      dresult = (*get_function_c64(current)) (args[0], args[1], args[2], args[3],
                                         args[4], args[5], args[6], args[7],
                                              args[8], args[9]);
    else
      result = (*get_function_c32(current)) (args[0], args[1], args[2], args[3],
                                         args[4], args[5], args[6], args[7],
                                             args[8], args[9]);
  }
  else {
    result = 6666;
    dresult = 6666.6;
  }

  /* map the result type */
  switch (get_resulttype(current)) {
    case _tINT:
        apush(createint((int) result));
        break;
    case _tDWORD:
        apush(createint((DWORD) result));
        break;
    case _tLONG:
        apush(createint((long) result));
        break;
    case _tSHORT:
        apush(createint((short) result));
        break;
    case _tHANDLE:
        apush(createint((int) result));
        break;
    case _tHMENU:
        apush(createint((int) result));
        break;
    case _tHWND:
        apush(createint((int) result));
        break;
    case _tLRESULT:
        apush(createint((int) result));
        break;
    case _tUCHAR:
        apush(createint((unsigned char) result));
        break;
    case _tSCHAR:
        apush(createint((signed char) result));
        break;
    case _tSDWORD:
        apush(createint((long int) result));
        break;
    case _tSWORD:
        apush(createint((short int) result));
        break;
    case _tUDWORD:
        apush(createint((unsigned long int) result));
        break;
    case _tUWORD:
        apush(createint((unsigned short int) result));
        break;
    case _tSLONG:
        apush(createint((signed long) result));
        break;
    case _tSSHORT:
        apush(createint((signed short) result));
        break;
    case _tULONG:
        apush(createint((unsigned long) result));
        break;
    case _tUSHORT:
        apush(createint((unsigned short) result));
        break;
    case _tLPDWORD:
        apush(createint(*((int *) (int) result)));
        break;
    case _tLPSTR:
    case _tLPTSTR:
    case _tLPCTSTR:
        {
          /* deal will a possibel NULL result */
          if (!result || (strlen((char *) (int) result) == 0)) {
            apush(Null);
          }
          else
            mkstring((char *) (int) result);
          break;
        }
    case _tCHAR:
    case _tWCHAR:            /* Bogus...needs to be fixed */
        apush(createchar((char) result));
        break;
    case _tFLOAT:
    case _tDOUBLE:
        apush(createreal(dresult));
        break;
    default:
        apush(createint(result));
  }

  /* now build the list of varargs (if we have any) */
  if (get_varargcount(current) || get_ispointercount(current)) {
    int         i;

    intsUsed = 0;
    for (i = 0; i < get_argcount(current); i++) {
      /* If this arg is a var arg, then deal with it */
      int         intsUsed_hold = intsUsed;

      if (get_vararg(current, i))
        MapAndPush(get_argtypes(current, i), args[intsUsed],
                   get_ispointer(current, i), &intsUsed);
      else
        /* This will move along the list even if it didn't need to be
         * processed */
        intsUsed += (((get_argtypes(current, i) == _tDOUBLE) ||
                      (get_argtypes(current, i) == _tFLOAT)) ? 2 : 1);

      if (get_vararg(current, i) || get_ispointer(current, i))
        free((void *) args[intsUsed_hold]);
    }
    if (get_varargcount(current))
      LinkStack(get_varargcount(current) + 1);
  }
  freeup(z);
  return;
}



void
MapAndPush(ResType type, int arg, bool ispointer, int *intsUsed)
{
  (*intsUsed)++;
  switch (type) {
    case _tUCHAR:{
          unsigned char x = (unsigned char) arg;

          apush(createint((int) x));
          break;
        }
    case _tSCHAR:{
          signed char x = (signed char) arg;

          apush(createint((int) x));
          break;
        }
    case _tSDWORD:{
          long int    x;

          x = (long int) *(int *) arg;
          apush(createint((int) x));
          break;
        }
    case _tSWORD:{
          short int   x;

          x = (short int) *(int *) arg;
          apush(createint((int) x));
          break;
        }
    case _tUDWORD:{
          unsigned long int x = (unsigned long int) *(int *) arg;

          apush(createint((int) x));
          break;
        }
    case _tUWORD:{
          unsigned short int x = (unsigned short int) *(int *) arg;

          apush(createint((int) x));
          break;
        }
    case _tSLONG:{
          signed long x = (signed long) arg;

          apush(createint((int) x));
          break;
        }
    case _tSSHORT:{
          signed short x = (signed short) arg;

          apush(createint((int) x));
          break;
        }
    case _tULONG:{
          unsigned long x = (unsigned long) arg;

          apush(createint((int) x));
          break;
        }
    case _tUSHORT:{
          unsigned short x = (unsigned short) arg;

          apush(createint((int) x));
          break;
        }

    case _tINT:
        apush(createint((int) arg));
        break;
    case _tDWORD:
        apush(createint((DWORD) arg));
        break;
    case _tLONG:
        apush(createint((long) arg));
        break;
    case _tSHORT:
        apush(createint((short) arg));
        break;
    case _tHANDLE:
        apush(createint((int) arg));
        break;
    case _tHMENU:
        apush(createint((int) arg));
        break;
    case _tHWND:
        apush(createint((int) arg));
        break;
    case _tLRESULT:
        apush(createint((int) arg));
        break;
    case _tLPDWORD:
        apush(createint(*((int *) arg)));
        break;
    case _tLPSTR:
    case _tLPTSTR:
    case _tLPCTSTR:
        /* deal will a possible NULL result */
        if (!arg) {
          apush(Null);
        }
        else
          mkstring((char *) arg);
        break;
    case _tCHAR:
    case _tWCHAR:
        apush(createchar((char) arg));
        break;
    case _tDOUBLE:
    case _tFLOAT:
        apush(createreal((double) arg));
        /* be sure to indicate the a double read of the arg will use two int
         * args */
        (*intsUsed)++;
        break;
    default:
        apush(createint(arg));
  }

}

void
LinkStack(int num)
{
  int         i;
  nialptr     x;

  x = new_create_array(atype, 1, 0, &num);
  for (i = num - 1; i >= 0; i--) {
    nialptr     q;

    store_array(x, i, q = apop());
    freeup(q);
  }

  apush(x);
}

int
MapNialArg(nialptr arr,
           int index,
           ResType type,
           void *val,
           bool isvararg, bool ispointer, int *intsUsed)
{
  nialptr     element = fetch_array(arr, index);

  /* By default all argument use 1 word (int) */
  /* Special cases (DOUBLE) will increment intsUsed as necessary */
  (*intsUsed)++;

  switch (type) {
    case _tUCHAR:
        if ((kind(element) != inttype) || kind(element) != chartype)
          return (-index);
        if (kind(element) == chartype)
          *((int *) val) = (int) (unsigned char) *pfirstchar(element);
        else
          *((int *) val) = (int) (unsigned char) *pfirstint(element);
        break;
    case _tSCHAR:
        if ((kind(element) != inttype) || kind(element) != chartype)
          return (-index);
        if (kind(element) == chartype)
          *((int *) val) = (int) (signed char) *pfirstchar(element);
        else
          *((int *) val) = (int) (signed char) *pfirstint(element);
        break;
    case _tSDWORD:
        {
          if (kind(element) != inttype)
            return (-index);
          if (isvararg || ispointer) {
            *((int **) val) = (int *) malloc(8);
            if (!(*(int **) val))
              exit_cover(NC_MEM_ERR_F);

            **((long int **) val) = (long int) *pfirstint(element);
          }
          else
            *((long int *) val) = (long int) *pfirstint(element);
          break;
        }
    case _tSWORD:
        if (kind(element) != inttype)
          return (-index);
        if (isvararg || ispointer) {
          *((int **) val) = (int *) malloc(8);
          if (!*((int **) val))
            exit_cover(NC_MEM_ERR_F);

          **((int **) (val)) = (int) (short int) *pfirstint(element);
        }
        else
          *((int *) (val)) = (int) (short int) *pfirstint(element);
        break;
    case _tUDWORD:
        if (kind(element) != inttype)
          return (-index);
        if (isvararg || ispointer) {
          *((int **) val) = (int *) malloc(8);
          if (!*((int **) val))
            exit_cover(NC_MEM_ERR_F);

          **((unsigned long int **) (val)) = (unsigned long int) *(pfirstint(element));
        }
        else
          *((unsigned long int *) (val)) = (unsigned long int) *pfirstint(element);
        break;
    case _tUWORD:
        if (kind(element) != inttype)
          return (-index);
        *((unsigned int *) val) = (int) (unsigned short int) *pfirstint(element);
        break;
    case _tSLONG:
        if (kind(element) != inttype)
          return (-index);
        *((long int *) val) = (long int) *pfirstint(element);
        break;
    case _tSSHORT:
        if (kind(element) != inttype)
          return (-index);
        *((int *) val) = (int) (short int) *pfirstint(element);
        break;
    case _tULONG:
        if (kind(element) != inttype)
          return (-index);
        *((unsigned long *) val) = (unsigned long int) *pfirstint(element);
        break;
    case _tUSHORT:
        if (kind(element) != inttype)
          return (-index);
        *((unsigned int *) val) = (unsigned int) (unsigned short) *pfirstint(element);
        break;

    case _tINT:
    case _tDWORD:
    case _tLONG:
    case _tHANDLE:
    case _tHMENU:
    case _tHWND:
    case _tLRESULT:
        {
          if (kind(element) != inttype)
            return (-index);
          *((int *) val) = (int) *pfirstint(element);
          break;
        }
    case _tLPDWORD:
        {
          if (kind(element) != inttype)
            return (-index);

          *((int **) val) = (int *) malloc(sizeof(int));
          if (!*(int **) (val))
            exit_cover(NC_MEM_ERR_F);

          **((int **) (val)) = *pfirstint(element);
          break;
        }
    case _tLPSTR:
    case _tLPCTSTR:
    case _tLPTSTR:
        {
          char       *str;

          if ((kind(element) == chartype) ||
              (kind(element) == phrasetype) ||
              ((kind(element) == inttype) && (*pfirstint(element) == 0)) ||
              (element == Null)) {  /* allow a null */
            if (tally(element))
              str = pfirstchar(fetch_array(arr, index));  /* safe: no allocation */
            else if (kind(element) == inttype)
              str = NULL;
            else
              str = "";

            *((int *) val) = (int) (str ? strdup(str) : NULL);

          }
          else
            return (-index);
          break;
        }
    case _tCHAR:
    case _tSHORT:
    case _tWCHAR:
        {                    /* Bogus...needs to be fixed */
          if ((kind(element) != chartype) &&
              (kind(element) != inttype))
            return (-index);
          if (kind(element) == chartype)
            *((int *) val) = (int) (char) *pfirstchar(element);
          else
            *((int *) val) = (char) *pfirstint(element);
          break;
        }
    case _tFLOAT:
    case _tDOUBLE:
        {
          if ((kind(element) != realtype) && (kind(element) != inttype))
            return (-index);
          if (kind(element) == realtype)
            (*((double *) val)) = (double) *pfirstreal(element);
          else
            (*((double *) val)) = (double) *pfirstint(element);
          (*intsUsed)++;

          break;
        }
  }
  return (1);
}

int
DLLclearflags()
{
  int         pos;

  /* no clean up if no table has been created yet */
  if (tally(dlllist) == 0)
    return (1);

  /* */
  for (pos = 0; pos < tally(dlllist); pos++)
    freeEntry(pos);          /* unload the library and mark it */

  return (1);
}

/* Unload all of the libraries and free up the table */
int
DLLcleanup()
{
  int         pos = 0;

  /* no clean up if no table has been created yet */
  if (tally(dlllist) == 0)
    return (1);

  DLLclearflags();

  decrrefcnt(dlllist);
  freeup(dlllist);
  dlllist = Null;
  incrrefcnt(dlllist);
  return (1);
}

int
freeEntry(int pos)
{
  /* make sure we have a valid reference here */
  if ((pos > 0) && (pos < tally(dlllist))) {
    nialptr     current = fetch_array(dlllist, pos);

    /* unload the library only if it was loaded */
    if (get_isloaded(current)) {
      FreeLibrary(get_handle(current));
      set_isloaded(current, false);
    }
  }
  return (1);
}


/* the definition of this routine implies that the "*" option
   must be the first option
*/

static bool
IsVarArg(char *str)
{
  return (str[0] == '*');
}


/* IsPointer
   This routine will detect a & in either the first or second position
   in the string.  IT will also check to see if the TYPE is a native
   pointer already.  Thus &LPSTR is redundant but safe.
*/

static bool
IsPointer(char *str)
{
  return (
          (str[0] == '&') ||
          (str[1] == '&') ||
          StringToIsNativePointer(str)
    );
}

/* StringToIsNativePointer
   This will tell us if the base type of the specified type
   is a native pointer type. (by looking in the table)
 */
bool
StringToIsNativePointer(char *str)
{
  int         pos = 0;
  int         offset = 0;

  /* Check to see if we have a pointer type variable */
  if (str[offset] == '*')
    offset++;
  if (str[offset] == '&')
    offset++;
  while (typetable[pos].name) {
    if (strcmpi(str + offset, typetable[pos].name) == 0)
      return (typetable[pos].ispointer);
    pos++;
  }
  return (false);
}

ResType
StringToTypeID(char *str)
{
  int         pos = 0;
  int         offset = 0;

  /* Check to see if we have a pointer type variable */
  if (str[offset] == '*')
    offset++;
  if (str[offset] == '&')
    offset++;
  while (typetable[pos].name) {
    if (strcmpi(str + offset, typetable[pos].name) == 0)
      return (typetable[pos].type);
    pos++;
  }
  return (-1);
}


/* 
   This routine will return the raw DLLlist or a mapping list
   for the types. 
   
   This is a low level routine that is used by the Nial level 
   routine to create a nicely formatted list that uses the
   original phrases for type names instead of the internal
   numbers (obtained with the arg = 0 form of this routine).
  
   When called with a 0 arg:
     It generates a list of int/phrase pairs that represents the
     type values used in the table and the phrase used in the
     call to registerdllfun. 
  
   When called with a 1 arg:
     The raw Dlllist array is written.  This included all of the
     flags and integer values. 
*/

void
iDllList(void)
{
  int         i,
              pos = 0;
  nialptr     z = apop();
  nialptr     maplist;

  if (kind(z) != inttype) {
    apush(makefault("?Must supply an integer argument to _dlllist"));
    freeup(z);
    return;
  }

  switch (*pfirstint(z)) {
    case 0:                  /* push the register list */
        while (typetable[pos++].name);
        pos--;

        /* create container for the map list */
        maplist = new_create_array(atype, 1, 0, &pos);

        for (i = 0; i < pos; i++) {
          pair(createint(typetable[i].type), makephrase(typetable[i].name));
          store_array(maplist, i, apop());
        }
        apush(maplist);
        freeup(z);
        break;
    case 1:                  /* push the mapping list */
    default:                 /* default is also to push the mapping list */
        apush(dlllist);
        freeup(z);
        break;

  }
  return;
}

int
inlist(char *name)
{
  int         i = 0;

  /*-- if the dlllist has any size (not empty) */
  if (tally(dlllist)) {
    int         pos;

    /*-- check each element of the list for a match */
    for (pos = 0; pos < tally(dlllist); pos++)
      if (strcmp(name, get_nialname(fetch_array(dlllist, pos))) == 0)
        return (pos);
  }
  /*-- -1 means NOT FOUND */
  return (-1);
}

#endif             /* CALLDLL */
