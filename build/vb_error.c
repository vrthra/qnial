/*==============================================================

  MODULE   VB_ERROR.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  The error message interface to Visual Basic use of the
  Data Engine DLL's

  This file, when complied with the correct version of messages.c,
  will output to stdout VB code that allow easy extraction and
  checking for Error messages and string when using the
  NIAL Data Engine.

  Eventually, this should be incorporated into the API.

================================================================*/

#define IOLIB
#include "sysincl.h"

#include "..\newsrc\messages.h"


#define ERROR_TABLE_STRING "ErrorCodeNumbersTable"
#define TYPE_TABLE_NUMBER "ErrorTypeNumbersTable"
#define TYPE_TABLE_STRING "ErrorTypeStringsTable"

main()
{
  int         i = 0,
              size = 0;

  printf("Rem -- This file automatically generated\n");
  printf("Rem -- DO NOT EDIT\n\n");
  while (error_strings[size++].error_code);
  printf("Global %s(%d) As String\n\n", ERROR_TABLE_STRING, size + 2);
  printf("Global %s(%d) As Long\n\n", TYPE_TABLE_NUMBER, size + 2);
  while (error_type_strings[size++].error_type_name);
  printf("Global %s(%d) As String\n\n", TYPE_TABLE_STRING, size + 2);

  printf("Sub BuildErrors\n");
  while (error_strings[i].error_code) {
    printf("    %s(%d) = \"%s\"\n", ERROR_TABLE_STRING, error_strings[i].error_code, error_strings[i].error_message);
    printf("    %s(%d) = %d\n", TYPE_TABLE_NUMBER, error_strings[i].error_code, error_strings[i].error_type);
    i++;
  }
  i = 0;
  while (error_type_strings[i].error_type) {
    printf("    %s(%d) = \"%s\"\n", TYPE_TABLE_STRING, error_type_strings[i].error_type, error_type_strings[i].error_type_name);
    i++;
  }
  printf("End Sub\n\n");

  printf("Function IsTerminate(errcode As Long) As Long\n");
  printf("  thetype = %s(errcode)\n", TYPE_TABLE_NUMBER);
  printf("  If thetype = %d Then\n", NC_TERMINATE);
  printf("     IsTerminate = 1\n");
  printf("  Else\n");
  printf("     IsTerminate = 0\n");
  printf("  Endif\n");
  printf("End Function\n\n");

  printf("Function ErrorTypeNumber(errcode As Long) As Long\n");
  printf("  ErrorTypeNumber = %s(errcode)\n", TYPE_TABLE_NUMBER);
  printf("End Function\n\n");

  printf("Function ErrorCodeString(errcode As Long) As String\n");
  printf("  ErrorCodeString = %s(errcode)\n", ERROR_TABLE_STRING);
  printf("End Function\n\n");

  printf("Function ErrorTypeString(errcode As Long) As String\n");
  printf("  ErrorTypeString = %s(%s(errcode))\n", TYPE_TABLE_STRING, TYPE_TABLE_NUMBER);
  printf("End Function\n");

}
