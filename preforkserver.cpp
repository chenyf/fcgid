#include <Python.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <netinet/in.h>
#include <setjmp.h>

#include <list>
#include <map>
#include <fcgio.h>

#include "trace.h"
#include "preforkserver.h"
#include "wsgi.h"
#include "httpd.h"

using namespace std;

namespace fcgid
{

static bool g_keepGoing = false;
static bool g_hupReceived = false;

static void _hupHandler(int sig) {
	TRACE0("receive hup\n");
	g_keepGoing = false;
	g_hupReceived = true;
}

static void alrmHandler(int sig) {
}


typedef struct _child
{
	int  file;
	bool avail;
} child_t;

child_t *child_new(int fd, bool avail=false)
{
	child_t *c = (child_t*)malloc(sizeof(child_t));
	c->file = fd;
	c->avail = avail;
	return c;
}

void child_free(child_t *c)
{
	free(c);
}

class FCGIRequest: public IRequest
{
public:
	FCGIRequest(FCGX_Request *r) {
		m_req = r;
		m_ok = true;
	}

	virtual ~FCGIRequest() {
	}

	int  GetData(char *buf, int len) {
		return FCGX_GetStr(buf, len, m_req->in);
	}
		 
	int  PutData(const char *buf, int len) {
		return FCGX_PutStr(buf, len, m_req->out);
	}

	int  PutStr(const char *str) {
		return FCGX_PutS(str, m_req->out);
	}
	 
	int  PutDataErr(const char *buf, int len) {
		return FCGX_PutStr(buf, len, m_req->err);
	}

	int  PutStrErr(const char *str) {
		return FCGX_PutS(str, m_req->err);
	}

	int  FlushOut() {
		return FCGX_FFlush(m_req->out);
	}

	int FlushErr() {
		return FCGX_FFlush(m_req->err);
	}

	int FPrintF(const char *fmt, ...) {
    int result;
    va_list ap;
    va_start(ap, fmt);
    result = FCGX_VFPrintF(m_req->out, fmt, ap);
    va_end(ap);
    return result;	
	}

	int FPrintFErr(const char *fmt, ...) {
    int result;
    va_list ap;
    va_start(ap, fmt);
    result = FCGX_VFPrintF(m_req->err, fmt, ap);
		//FCGX_FFlush(m_req->err);
    va_end(ap);
    return result;	
	}

	char **GetEnv() {
		return m_req->envp;
	}
	char *GetParam(const char *key) {
		return FCGX_GetParam(key, m_req->envp);
	}

private:
	FCGX_Request *m_req;
	bool m_ok;

};

bool PreforkServer::_daemonize() 
{
	/*close(0);
	close(1);
	close(2);*/
	if (0 != fork()) exit(0);
	if (0 != fork()) exit(0);		
	return true;	
}
		
	bool PreforkServer::run(int sock, bool bDaemon) 
	{
		g_keepGoing = true;
		g_hupReceived = false;

		if(bDaemon) {
			TRACE0("be daemonize...\n");
			if(!_daemonize())
				return false;
		}
		
		_installSignalHandlers();
		_setBlocking(sock, false);
		_setCloseOnExec(sock);	

		struct timeval timeout;
		pid_t pid;
		child_t *x;
		map<pid_t, child_t*>::iterator idx;

		fd_set rfs;
		
		time_t last_exit = time(NULL);
		uint32_t abnormal_exit = 0;

		while(g_keepGoing) {
			while(m_children.size() < m_maxSpare) {
				if(!_spawnChild(sock))
					break;
			}

			uint32_t count = 0;
			FD_ZERO(&rfs);
			int max_fd = 0;
			for(idx = m_children.begin(); idx!= m_children.end(); idx++) {
				pid = idx->first;
				x   = idx->second;
				if(x->file != -1) {
					FD_SET(x->file, &rfs);
					if(max_fd < x->file)
						max_fd = x->file;
					count++;
				}
			}
			
			if(count == m_children.size()) {
				timeout.tv_sec = 1;
				timeout.tv_usec = 0;
			} else {
				timeout.tv_sec = 2;
				timeout.tv_usec = 0;
			}

			int ret = select(max_fd+1, &rfs, NULL, NULL, &timeout);
			//TRACE0("select result: %d, %d\n", ret, errno);
			if(ret < 0) {
				if(errno != EINTR)
					break;
				continue;
			} else if(ret == 0)
				;//continue;


			for(idx = m_children.begin(); idx!= m_children.end() && ret > 0; idx++) {
				pid = idx->first;
				x = idx->second;
				if(x->file == -1)
					continue;

				if(FD_ISSET(x->file, &rfs)) {
					ret--;

					char state;
					ret = recv(x->file, &state, 1, 0);

					if(ret == 1) {
						//child is not avaiable 
						if(state != '\x00') {
							x->avail = true;
						} else {
							x->avail = false;
						}
					} else {
						TRACE0("recv failed with child %d, %d\n", pid, ret);
						close(x->file);
						x->file = -1;
						x->avail = false;
						
						time_t now = time(NULL);
						if((now - last_exit) <= 2)
							abnormal_exit++;
						else
							abnormal_exit = 0;
						last_exit = now;

						if(abnormal_exit > 10) {
							TRACE0("Too many worker process exit abnormally, please check!!!\n");
							g_keepGoing = false;
							break;
						}
					}
				}
			}

			_reapChildren();

			uint32_t avail = 0;
			std::list<pid_t> avail_list;
	
			for(idx = m_children.begin(); idx!= m_children.end(); idx++) {
				x = idx->second;
				if(x->avail) {
					avail++;
					avail_list.push_back(idx->first);
				}
			}

			if(!g_keepGoing)
				break;

			if(avail < m_minSpare) {
				while(avail < m_minSpare && m_children.size() < m_maxChildren) {
					if(!_spawnChild(sock))
						break;
					avail++;
				}
			} else if(avail > m_maxSpare) {
				//too many avaiable work process, kill some of them.
				avail_list.sort();
				std::list<pid_t>::iterator idx2;
				for(idx2 = avail_list.begin(); idx2 != avail_list.end(); idx2++) {
					if(avail <= m_maxSpare)
						break;

					idx = m_children.find(*idx2);
					if(idx != m_children.end()) {
						avail--;
					
						x = idx->second;
						close(x->file);
						x->file = -1;
						x->avail = false;
					}
				}
			}
		}
		TRACE0("main process quit\n");	
		_cleanupChildren();
		_restoreSignalHandlers();
		TRACE0("main process quit 2\n");	
		return g_hupReceived;
	}	

	void PreforkServer::_setBlocking(int sock, bool flag) 
	{
		int flags = fcntl(sock, F_GETFL, 0);
		if(flag)
			flags = flags;
		else
			flags = flags | O_NONBLOCK;
		fcntl(sock, F_SETFL, flags);
	}

	void PreforkServer::_setCloseOnExec(int sock) 
	{
		fcntl(sock, F_SETFD, FD_CLOEXEC);
	}

sigjmp_buf buffer;
static bool recv_alarm = false;

static void timeout_handler(int sig) 
{
	TRACE0("enter alarm handler\n");
	if(!recv_alarm) {
		recv_alarm = true;
		siglongjmp(buffer, 1);
	}
}

static void _timeout_error(FCGX_Request &request)
{
	FCGX_PutS("HTTP/1.0 404 Not Found\r\n", request.out);
	FCGX_PutS("Content-type: text/html\r\n\r\n", request.out);
	FCGX_PutS("<html><body><h1>Requst timeout.</h1></body></html>\n", request.out);
	FCGX_FFlush(request.out);
}


	bool PreforkServer::_handleRequest(FCGX_Request &request, uint32_t timeout) 
	{
		FCGIRequest *fcgi_req = new FCGIRequest(&request);	
		sighandler_t old_handler = SIG_ERR;

		if(timeout > 0) {
			recv_alarm = false;
			old_handler = signal(SIGALRM, timeout_handler);
			if(sigsetjmp(buffer, 1) != 0) {
				if(old_handler != SIG_ERR)
					signal(SIGALRM, old_handler);
				_timeout_error(request);
				delete fcgi_req;
				return false;
			}
			alarm(timeout);
		}

		wsgi_execute_script(fcgi_req, FCGX_GetParam("SCRIPT_FILENAME", request.envp));
		if(timeout > 0) {
			alarm(0);
			if(old_handler != SIG_ERR)
				signal(SIGALRM, old_handler);
		}

		delete fcgi_req;
		return true;
	}

	void PreforkServer::_childLoop(int sock, int parent) 
	{
		signal(SIGPIPE, SIG_IGN);

		fd_set rfs;
		uint32_t requestCount = 0;
		int max_fd = sock;
		if(parent > sock)
			max_fd = parent;
		
		FCGX_Init();
		if(wsgi_python_init() != 0) {
			return;
		}

		PyThreadState *my_tstate = PyThreadState_GET();

		while(g_keepGoing) {
			TRACE0("loop ..., %d, %d\n", parent, sock);
			FD_ZERO(&rfs);
			FD_SET(sock, &rfs);
			FD_SET(parent, &rfs);	
			int ret = select(max_fd+1, &rfs, NULL, NULL, NULL);	
			TRACE0("select result %d\n", ret);
			
			if(ret <= 0)
				break;
			
			if(FD_ISSET(parent, &rfs)) {
				TRACE0("parent ask me to quit\n");
				break;
			}

			if(!FD_ISSET(sock, &rfs))
				continue;

			FCGX_Request request;
			FCGX_InitRequest(&request, sock, 0);

			ret = FCGX_Accept_r(&request);
			if(ret == 0) {
				TRACE0("accept new connection, %d\n", request.ipcFd);
				_notifyParent(parent, '\x00');
				//_setCloseOnExec(request.ipcFd);
				requestCount++;
				bool result = _handleRequest(request, m_maxExecTime);
				
				FCGX_Finish_r(&request);	
				FCGX_Free(&request, true);

				//»Ö¸´ stdin, stdout, stderr
				TRACE0("finish response\n");
				if(!result) {
					if(PyThreadState_GET() == NULL) {
						(void) PyThreadState_Swap(my_tstate);
					}
				}

				if(!_notifyParent(parent, '\xff'))
					break;
			} else {
				TRACE0("accept result %d\n", ret);
			}

			if(m_maxRequests > 0) {
				if(requestCount >= m_maxRequests)
					break;
			}
		} 
		wsgi_python_cleanup();
	}

	bool PreforkServer::_spawnChild(int sock) 
	{
		int sv[2];
		socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
		int parent = sv[0];
		int child = sv[1];
	
		_setBlocking(parent, false);
		_setCloseOnExec(parent);
		_setBlocking(child, false);
		_setCloseOnExec(child);
		
		pid_t pid = fork();
		if(pid < 0) {
			if(errno == EAGAIN || errno == ENOMEM)
				return false;
			return false;
		}

		if(pid == 0) {
			close(child);
			pid = getpid();
			_setPid(pid);

			setpgid(pid, pid);
			_restoreSignalHandlers();
		
			map<pid_t, child_t*>::iterator idx;
			child_t *x;
			for(idx = m_children.begin(); idx != m_children.end(); idx++) {
				x = idx->second;
				if(x->file != -1) 
					close(x->file);
			}
			_childLoop(sock, parent);
			exit(0);
		}

		TRACE0("fork child: %d, %d, %d\n", pid, parent, child);
		close(parent);
		child_t *c = child_new(child, true);
		pair<pid_t, child_t*> item(pid, c);
		m_children.insert(item);
		return true;
	}

	void PreforkServer::_installSignalHandlers() 
	{
		signal(SIGINT,  _hupHandler);
		signal(SIGTERM, _hupHandler);
		signal(SIGHUP,  _hupHandler);
	}

	void PreforkServer::_restoreSignalHandlers() 
	{
		signal(SIGINT,  SIG_DFL);
		signal(SIGTERM, SIG_DFL);
		signal(SIGHUP,  SIG_DFL);
	}

	void PreforkServer::_reapChildren() 
	{
		while(g_keepGoing) {
			pid_t pid = waitpid(-1, NULL, WNOHANG);
			//TRACE0("waitpid result: %d\n", pid);
			if(pid < 0) {
				if(errno == ECHILD)
					break;
				return;
			} else if(pid == 0) // no child avaiable this time
				break;
			
			map<pid_t, child_t*>::iterator idx;
			idx = m_children.find(pid);
			if(idx != m_children.end()) {
				TRACE0("child %d quit\n", pid);
				child_t *x = idx->second;
				if(x->file != -1) {
					close(x->file);
				}
				m_children.erase(idx);
				child_free(x);
			} else {
				TRACE0("child %d not mine\n", pid);
			}
		}	
	}

	void PreforkServer::_cleanupChildren() 
	{
		pid_t pid;
		child_t *x;
		map<pid_t, child_t*>::iterator idx;

		// let all cihlden know it's time to go
		for(idx = m_children.begin(); idx != m_children.end(); idx++) {
			pid = idx->first;
			x   = idx->second;
			if(x->file != -1) {
				close(x->file);
				x->file = -1;
			}
			if(!x->avail) {
				TRACE0("kill child %d\n", pid);
				kill(pid, SIGINT);
			}
		}

		sighandler_t oldSIGALRM = signal(SIGALRM, alrmHandler);
		alarm(10);
	
		// wait for all children to die
		while(m_children.size() > 0) {
			pid = wait(NULL);
			if(pid < 0) {
				if(errno == ECHILD || errno == EINTR)
					break;
			}

			TRACE0("wait child %d\n", pid);
			idx = m_children.find(pid);
			if(idx != m_children.end()) {
				x = idx->second;
				m_children.erase(idx);
				child_free(x);
			}
		}

		if(oldSIGALRM != SIG_ERR)
			signal(SIGALRM, oldSIGALRM);
		
		//forcelly kill any remaining children
		for(idx = m_children.begin(); idx != m_children.end(); idx++) {
			pid = idx->first;
			kill(pid, SIGKILL);
			TRACE0("fork kill child %d\n", pid);
		}
	}

	bool PreforkServer::_isClientAllowed(struct sockaddr_in *paddr) 
	{
		return true;
	}

	bool PreforkServer::_notifyParent(int parent, char msg) 
	{
		while(1) {
			ssize_t ret = send(parent, &msg, 1, 0);
			if(ret <= 0) {
				if(errno == EPIPE) 
					return false;
				if(errno == EAGAIN) 
					continue;
				else
					return false;
			} else 
				return true;
		}
	}
}


