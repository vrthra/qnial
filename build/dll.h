/*==============================================================

  DLL.H:  HEADER FOR DLL.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains the prototypes of the DLL support functions

================================================================*/

#ifdef CALLDLL

/* There is a limit to the number of arguments to a DLL
   call.  This is only because of the switch statement used is
   icalldllfun where we have to have a case for each number of
   arguments when using the PASCAL calling convention.  IF we
   has safe code to push the arguments on the stack that could
   be generalized, then this limit could be removed.
 */
#define MAX_ARGS 10

/* The two types of DLL calls */
enum {
  PASCAL_CALL, C_CALL
};


typedef int ResType;

typedef FARPROC PASCAL_CALLt;
typedef double (*C_CALLt_64) ();
typedef int (*C_CALLt_32) ();
typedef int bool;

#define STRING_CHECK(arr,pos) if ((tally(fetch_array(arr,pos)) != 0) && \
                                  (kind(fetch_array(arr,pos))!=phrasetype) &&\
                                  (kind(fetch_array(arr,pos)) != chartype)) {\
                                char tt[256];\
                                sprintf(tt,"?Arg %d, must be a string or phrase",pos);\
                                apush(makefault(tt));\
                                freeup(arr);\
                                return;\
                         }

#define STRING_CHECK_FREEUP(arr,pos,other) if ((tally(fetch_array(arr,pos)) != 0) && \
                                  (kind(fetch_array(arr,pos))!=phrasetype) &&\
                                  (kind(fetch_array(arr,pos)) != chartype)) {\
                                char tt[256];\
                                sprintf(tt,"?Arg %d, must be a string or phrase",pos);\
                                apush(makefault(tt));\
                                freeup(other);\
                                return;\
                         }

#define STRING_GET(arr,pos) ((tally(fetch_array(arr,pos)) == 0)?"":pfirstchar(fetch_array(arr,pos)))

/* Routines to access the same structure, but stored in the workspace */

/* [int,int,int,int,char,char,char,int,int,int,int[],bool[],int,int,boo[]] */

extern int _dllentrysize_ ;

#define create_new_dll_entry  new_create_array(atype,1,0,&_dllentrysize_)

#define setup_dll_entry(x) {\
 store_array(x,0,createint(-1));\
 store_array(x,1,createint(-1));\
 store_array(x,2,createint(-1));\
 store_array(x,3,createint(-1));\
 store_array(x,4,Null);\
 store_array(x,5,Null);\
 store_array(x,6,Null);\
 store_array(x,7,createint(-1));\
 store_array(x,8,createint(-1));\
 store_array(x,9,createint(-1));\
 store_array(x,10,createint(0));\
 store_array(x,11,createbool(0));\
 store_array(x,12,createint(-1));\
 store_array(x,13,createint(-1));\
 store_array(x,14,createbool(0));}\


#define get_handle(x)              ((HINSTANCE) (*pfirstint(fetch_array(x,0))))
#define get_callingconvention(x)   ((int) (*pfirstint(fetch_array(x,1))))
#define get_function_p(x)          ((PASCAL_CALLt)(*pfirstint(fetch_array(x,2))))
#define get_function_c32(x)          ((C_CALLt_32)(*pfirstint(fetch_array(x,3))))
#define get_function_c64(x)          ((C_CALLt_64)(*pfirstint(fetch_array(x,3))))
#define get_nialname(x)            (pfirstchar(fetch_array(x,4)))
#define get_dllname(x)             (pfirstchar(fetch_array(x,5)))
#define get_library(x)             (pfirstchar(fetch_array(x,6)))
#define get_isloaded(x)            ((int) (*pfirstint(fetch_array(x,7))))
#define get_resulttype(x)          ((int) (*pfirstint(fetch_array(x,8))))
#define get_argcount(x)            ((int) (*pfirstint(fetch_array(x,9))))
#define get_argtypes(x,i)          ((int) (fetch_int(fetch_array(x,10),i)))
#define get_ispointer(x,i)         ((int) (fetch_bool(fetch_array(x,11),i)))
#define get_varargcount(x)         ((int) (*pfirstint(fetch_array(x,12))))
#define get_ispointercount(x)      ((int) (*pfirstint(fetch_array(x,13))))
#define get_vararg(x,i)            ((int) (fetch_bool(fetch_array(x,14),i)))

#define set_handle(x,v)              (replace_array(x,0,createint((int)v)))
#define set_callingconvention(x,v)   (replace_array(x,1,createint(v)))
#define set_function_p(x,v)          (replace_array(x,2,createint((int)v)))
#define set_function_c(x,v)          (replace_array(x,3,createint((int)v)))
#define set_nialname(x,v)            replace_array(x,4,makestring(v))
#define set_dllname(x,v)             replace_array(x,5,makestring(v))
#define set_library(x,v)             replace_array(x,6,makestring(v))
#define set_isloaded(x,v)            (replace_array(x,7,createint(v)))
#define set_resulttype(x,v)          (replace_array(x,8,createint(v)))
#define set_argcount(x,v)            (replace_array(x,9,createint(v)))
#define set_argtypes(x,i,v)          (store_int(fetch_array(x,10),i,v))
#define set_ispointer(x,i,v)         (store_bool(fetch_array(x,11),i,v))
#define set_varargcount(x,v)         (store_int(fetch_array(x,12),0,(int)v))
#define set_ispointercount(x,v)      (store_int(fetch_array(x,13),0,(int)v))
#define set_vararg(x,i,v)            (store_bool(fetch_array(x,14),i,v))

enum {
  _tCHAR, _tSHORT, _tLONG, _tWCHAR, _tLPSTR, _tHANDLE, _tDWORD, _tWORD, _tBOOL,
  _tHWND, _tUINT, _tWPARAM, _tLPARAM, _tLRESULT,
  _tHMENU, _tLPCTSTR, _tLPTSTR, _tINT, _tLPDWORD,
  _tUCHAR, _tSCHAR, _tSDWORD, _tSWORD, _tUDWORD, _tUWORD,
  _tSLONG, _tSSHORT, _tULONG, _tUSHORT, _tDOUBLE, _tFLOAT
};


void        iregisterdllfun(void);
void        icalldllfun(void);
int
MapNialArg(nialptr arr,
           int index,
           ResType type,
           void *val,
           bool isvararg, bool ispointer, int *intsUsed);
int         DLLcleanup();
int         DLLclearflags();
int         freeEntry(int pos);
ResType     StringToTypeID(char *str);
bool        StringToIsNativePointer(char *str);
void        MapAndPush(ResType type, int arg, bool ispointer, int *intsUsed);
void        LinkStack(int num);
int         inlist(char *);

#endif             /* CALLDLL  */
