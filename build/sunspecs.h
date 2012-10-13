/*==============================================================

  SUNSPECS.H:  

  COPYRIGHT NIAL Systems Limited  1983-2005

  System Specific Macros for SUN SPARC 32 bit architecture.

  A file like this must be prepared for each specific hardware/software architecture. 
   
  The particular version used is included by switches.h

================================================================*/

/* constants that may be machine dependent  */

#define ALLBITSON (-1)
 /* assumes -1 is 32 1 bits */


/* the range for characters */

#define LOWCHAR 0
#define HIGHCHAR 255


#define DEFEDITOR "vi"

#define LARGEINT    2147483647.0  /* max int as a double */
#define SMALLINT    -2147483648.0 /* min int as a double */


#define collatingseq "0123456789 _&aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ-~.,;:()[]{}<>+?!#$%'=^|\\\"/@`*"

#define LARGESTNEG    -2147483648 /* min int as integer */

typedef int nialint;
typedef int nialptr;


