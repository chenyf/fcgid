#ifndef _TRACE_H_
#define _TRACE_H_

#include <stdio.h>

namespace fcgid
{

#ifndef DISABLE_TRACE
#   ifndef __TRACE__
#       define __TRACE__        7
#   endif
#else
#   undef   __TRACE__
#   define  __TRACE__           0
#endif

#ifndef __TRACE__
#   define  TRACE0(...)
#   define  TRACE1(...)
#   define  TRACE2(...)
#   define  TRACE3(...)
#   define  TRACE4(...)
#   define  TRACE5(...)
#   define  TRACE6(...)
#   define  TRACE7(...)
#elif   __TRACE__ >= 7
#   define  TRACE0(...)      { g_fTrace(__VA_ARGS__); }
#   define  TRACE1(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE2(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE3(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE4(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE5(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE6(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE7(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#elif   __TRACE__ >= 6
#   define  TRACE0(...)      { g_fTrace(__VA_ARGS__); }
#   define  TRACE1(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE2(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE3(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE4(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE5(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE6(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE7(...)
#elif   __TRACE__ >= 5
#   define  TRACE0(...)      { g_fTrace(__VA_ARGS__); }
#   define  TRACE1(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE2(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE3(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE4(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE5(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE6(...)
#   define  TRACE7(...)
#elif   __TRACE__ >= 4
#   define  TRACE0(...)      { g_fTrace(__VA_ARGS__); }
#   define  TRACE1(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE2(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE3(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE4(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE5(...)
#   define  TRACE6(...)
#   define  TRACE7(...)
#elif   __TRACE__ >= 3
#   define  TRACE0(...)      { g_fTrace(__VA_ARGS__); }
#   define  TRACE1(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE2(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE3(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE4(...)
#   define  TRACE5(...)
#   define  TRACE6(...)
#   define  TRACE7(...)
#elif   __TRACE__ >= 2
#   define  TRACE0(...)      { g_fTrace(__VA_ARGS__); }
#   define  TRACE1(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE2(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE3(...)
#   define  TRACE4(...)
#   define  TRACE5(...)
#   define  TRACE6(...)
#   define  TRACE7(...)
#elif   __TRACE__ >= 1
#   define  TRACE0(...)      { g_fTrace(__VA_ARGS__); }
#   define  TRACE1(...)      if (g_nKLLogLevel>=1) { g_fTrace(__VA_ARGS__); }
#   define  TRACE2(...)
#   define  TRACE3(...)
#   define  TRACE4(...)
#   define  TRACE5(...)
#   define  TRACE6(...)
#   define  TRACE7(...)
#elif   __TRACE__ == 0
#   define  TRACE0(...)      { g_fTrace(__VA_ARGS__); }
#   define  TRACE1(...)
#   define  TRACE2(...)
#   define  TRACE3(...)
#   define  TRACE4(...)
#   define  TRACE5(...)
#   define  TRACE6(...)
#   define  TRACE7(...)
#endif

typedef void (*TRACE_FUNC)(const char *fmt, ...);
extern int g_nKLLogLevel;
extern TRACE_FUNC g_fTrace;
extern void KLSetLog(FILE* fpLog, int nLogLevel, TRACE_FUNC f);

}

#endif

