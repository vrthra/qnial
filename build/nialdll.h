/*==============================================================

  NIALDLL.H:  

  COPYRIGHT NIAL Systems Limited  1983-2005

  This file declares functions and Macros needed to easily
  use the Nial Data Engine DLL in Windows Programs

================================================================*/

//-----------------------------------------------------------
//-- To Use the WINNIAL.DLL
//   1) include this file after other Windows includes in
//      any file that will call any of the DLL functions.
//   2) Call the function "NialDLL_LoadDLL" to find and load
//      the DLL file and all of the functions in the file.
//   3) Call the function "NialDLL_StartNial" to start the
//      the Nial Data Engine.  This can be passed argc, argv
//      parameters in the same way as if you started Nial
//      on the command line.
//   4) Now any of the DLL functions can be used directly:
//         NialDLL_CommandInterpret,
//         NialDll_GetBuf,
//         etc
//      These functions have specific argument list and result
//      types.  These are documented below
//-----------------------------------------------------------

//-----------------------------------------------------------
// DLL Function Syntax and description:
//-----------------------------------------------------------
//   char *NialDll_LoadDLL(void)
//   void  NialDLL_StartNial(int argc, char **argv)
//   char *NialDLL_NialInterpret(char *input,
//                               int screenwidth,
//                               int triggered,
//                               int sketch,
//                               int decor)
//   char *NialDLL_CommandInterpret(char *input,
//                                  int screenwidth,
//                                  int triggered,
//                                  int sketch,
//                                  int decor)
//   void  NialDLL_ResetBuf()
//   char *NialDLL_GetBuf(void)
//-----------------------------------------------------------
#ifndef _NIALDLL_
#define _NIALDLL_

//-- Global variables,types and macros to hold the actual DLL functions
typedef     VOID(*STARTNIAL) (int, char **);
STARTNIAL   NialDLL_StartNial_fun;

#define   NialDLL_StartNial         (NialDLL_StartNial_fun)

typedef char *(*COMMANDINTERPRET) (char *, int, int, int, int);
COMMANDINTERPRET NialDLL_CommandInterpret_fun;

#define   NialDLL_CommandInterpret  (NialDLL_CommandInterpret_fun)

typedef char *(*NIALINTERPRET) (char *, int, int, int, int);
NIALINTERPRET NialDLL_NialInterpret_fun;

#define   NialDLL_NialInterpret     (NialDLL_NialInterpret_fun)

typedef char *(*RESETBUF) (VOID);
RESETBUF    NialDLL_ResetBuf_fun;

#define   NialDLL_ResetBuf          (NialDLL_ResetBuf_fun)

typedef char *(*GETBUF) (VOID);
GETBUF      NialDLL_GetBuf_fun;

#define   NialDLL_GetBuf            (NialDLL_GetBuf_fun)



//----------------------------------------------------------------
//-- This function will load in the DLL and connect the functions
//-- to the variables/macros defined above
//-- RETURN VALUE:
//--    On Success:   NULL
//--    On Failure:   an error string
//--                  (if result is non-NULL, then calling function
//--                   must free result)
//----------------------------------------------------------------
char       *
NialDLL_LoadDLL()
{
  HINSTANCE   hinstLib;

  //-- attempt to load the DLL
  hinstLib = LoadLibrary("WINNIAL.DLL");

  //-- if we have found the library, then load the functions
  if (hinstLib != NULL) {
    NialDLL_StartNial_fun =
      (STARTNIAL) GetProcAddress(hinstLib, "_nialmain");
    NialDLL_CommandInterpret_fun =
      (COMMANDINTERPRET) GetProcAddress(hinstLib, "_CommandInterpret");
    NialDLL_NialInterpret_fun =
      (NIALINTERPRET) GetProcAddress(hinstLib, "_NialInterpret");
    NialDLL_ResetBuf_fun =
      (RESETBUF) GetProcAddress(hinstLib, "_resetWinBuffer");
    NialDLL_GetBuf_fun =
      (GETBUF) GetProcAddress(hinstLib, "_getWinBuffer");
    return (NULL);
  }
  else {
    //-- failed to epen DLL, so produce an Error message
    char       *error = new char[100];

    wsprintf(error, "DLL load error num: %d", GetLastError());
    return (error);
  }
}

#else
//-- If this file has been loaded once already, then just declare
//-- the globals as externs
extern STARTNIAL NialDLL_StartNial_fun;
extern COMMANDINTERPRET NialDLL_CommandInterpret_fun;
extern NIALINTERPRET NialDLL_NialInterpret_fun;
extern RESETBUF NialDLL_ResetBuf_fun;
extern INITBUF NialDLL_InitBuf_fun;
extern GETBUF NialDLL_GetBuf_fun;
extern ENDBUF NialDLL_EndBuf_fun;

extern char NialDLL_LoadDLL();

#endif
