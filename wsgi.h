#ifndef __WSGI_H__
#define __WSGI_H__

namespace fcgid
{

class IRequest
{
public:
	virtual int  GetData(char *buf, int len) = 0;
	virtual int  PutData(const char *buf, int len) = 0;
	virtual int  PutStr(const char *str) = 0;
	virtual int  FlushOut() = 0;
	virtual int  PutDataErr(const char *buf, int len) = 0;
	virtual int  PutStrErr(const char *str) = 0;
	virtual int  FlushErr() = 0;
	virtual int  FPrintF(const char *fmt, ...) = 0;
	virtual int  FPrintFErr(const char *fmt, ...) = 0;

	virtual char** GetEnv() = 0;
	virtual char* GetParam(const char* key) = 0;
};

int  wsgi_python_init();
void wsgi_python_cleanup();
int  wsgi_execute_script(IRequest *r, const char *script);

}

#endif


