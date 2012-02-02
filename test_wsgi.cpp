#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "wsgi.h"

using namespace fcgid;

char *g_envp[] = {
	"DOCUMENT_ROOT=/home/bce/chenyifei/lighttpd/htdocs/osqa.baidu.com",
	"HTTP_HOST=osqa.baidu.com",
	"REQUEST_URI=/",
   "HTTP_ACCEPT=*/*",
   "SCRIPT_FILENAME=/home/bce/chenyifei/lighttpd/htdocs/osqa.baidu.com/index.py",
   "SCRIPT_NAME=index.py",
  	"REQUEST_METHOD=GET",
	NULL
};

class MockRequest: public IRequest
{
public:
	MockRequest() {
		m_envp = g_envp;
	}

	virtual ~MockRequest() {
	}

	int  GetData(char *buf, int len) {
		return len;
	}
		 
	int  PutData(const char *buf, int len) {
		char *s = strndup(buf, len);
		printf("output1: (%s) \n", s);
		free(s);
		return len;
	}

	int  PutStr(const char *str) {
		printf("output2: (%s)\n", str);
		return strlen(str);
	}

	int FPrintF(const char *fmt, ...) {
    int result;
    va_list ap;
    va_start(ap, fmt);
    result = printf(fmt, ap);
    va_end(ap);
    return result;
	}

	int FPrintFErr(const char *fmt, ...) {
    int result;
    va_list ap;
    va_start(ap, fmt);
    result = fprintf(stderr, fmt, ap);
    va_end(ap);
    return result;
	}

	int  FlushOut() {
		return 0;
	}
			 
	int  PutDataErr(const char *buf, int len) {
		char *s = strndup(buf, len);
		fprintf(stderr, "output1: (%s) \n", s);
		free(s);
		return len;
	}

	int  PutStrErr(const char *str) {
		fprintf(stderr, "output2: (%s)\n", str);
		return strlen(str);
	}

	int  FlushErr() {
		return 0;
	}

	char **GetEnv() {
		return m_envp;
	}



	char* GetParam(const char *name) {
    int len;
    char **p;

	if (name == NULL || m_envp == NULL) return NULL;
    len = strlen(name);

    for (p = m_envp; *p; ++p) {
        if((strncmp(name, *p, len) == 0) && ((*p)[len] == '=')) {
            return *p+len+1;
        }
    }
    return NULL;
	}

	
private:
	char **m_envp;
};

int main(int argc, char **argv)
{
	wsgi_python_init();

	int count = 1;
	
	if(argc > 1) {
		count = atoi(argv[1]);
	}

	for(int i = 0; i < count; i++) {
	printf("=======================%d test===========================\n", i);	
	const char *script = "/home/bce/chenyifei/lighttpd/htdocs/osqa.baidu.com/index.py";
	MockRequest *r = new MockRequest();
	wsgi_execute_script(r, script);
	delete r;
	printf("=========================================================\n");	
	}

	return 0;
}



