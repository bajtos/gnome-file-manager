#ifdef TRACE
#undef TRACE
#endif
#ifdef TRACEM
#undef TRACEM
#endif

/* 
 * debugging macros (print some info to stderr)
 * shall be the first include
 * e.g.:
 *  #ifndef DEBUG
 *  #define DEBUG 1
 *  #endif
 *  #include "debug.h"
 *
 * TRACE -- prints just position (file,function,line)
 * TRACEM -- prints position + custom message (formated through printf)
 */

#if DEBUG
#define TRACE fprintf(stderr,"Trace "G_STRLOC"\n");
#define TRACEM(M...) fprintf(stderr,"Trace "G_STRLOC"\t\t"M);
//#define TRACEP(M,P...) printf("Trace %-30s %-40s [%d] 	* "M"\n", __FILE__, __PRETTY_FUNCTION__, __LINE__, P...);
#else
#define TRACE 
#define TRACEM(M...)
//#define TRACP(M,P...)
#endif
