#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>

#include "trace.h"

namespace fcgid
{

int g_nKLLogLevel = 7;
static FILE* s_fpLog = stderr;

static void Trace(const char *fmt, ...)
{
	va_list	ap;
    if (s_fpLog) {
       fprintf(s_fpLog, "[%d]", getpid());
	    va_start(ap, fmt);
	    vfprintf(s_fpLog, fmt, ap);
	    va_end(ap);
		fflush(s_fpLog);
	}
}

TRACE_FUNC g_fTrace = Trace;

void KLSetLog(FILE* fpLog, int nLogLevel, TRACE_FUNC f)
{
	s_fpLog = fpLog;
	g_nKLLogLevel = nLogLevel;
	if (NULL != f)
		g_fTrace = f;
}

}

