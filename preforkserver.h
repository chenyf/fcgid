#ifndef __PREFORK_SERVER_H__
#define __PREFORK_SERVER_H__

#include <map>
#include <boost/program_options.hpp>
#include <fcgio.h>

using namespace std;
using namespace boost;

namespace fcgid
{

typedef struct _child child_t;

class PreforkServer
{
public:
	PreforkServer(program_options::variables_map &cfgvm) : m_cfgvm(cfgvm) {
		if(cfgvm.count("min_spare"))
			m_minSpare    = cfgvm["min_spare"].as<uint32_t>();
		else
			m_minSpare = 1;

		if(cfgvm.count("max_spare"))
			m_maxSpare    = cfgvm["max_spare"].as<uint32_t>();
		else
			m_maxSpare = 5;

		if(cfgvm.count("max_children"))
			m_maxChildren = cfgvm["max_children"].as<uint32_t>();
		else
			m_maxChildren = 50;

		if(cfgvm.count("max_requests"))
			m_maxRequests = cfgvm["max_requests"].as<uint32_t>();
		else
			m_maxRequests = 50;

		if(cfgvm.count("max_execute_time"))
			m_maxExecTime = cfgvm["max_execute_time"].as<uint32_t>();
		else
			m_maxExecTime = 60;

		m_pid = -1;
	}

	~PreforkServer() {
	}
		
	bool run(int sock, bool bDaemon=false);

private:
	void _setPid(pid_t id) {
		m_pid = id;
	}

	bool _daemonize(); 
	void _setBlocking(int sock, bool flag);
	void _setCloseOnExec(int sock);
	bool _handleRequest(FCGX_Request &request, uint32_t timeout);
	void _childLoop(int sock, int parent);
	bool _spawnChild(int sock);
	void _installSignalHandlers();
	void _restoreSignalHandlers();
	void _reapChildren();
	void _cleanupChildren();
	bool _isClientAllowed(struct sockaddr_in *paddr);
	bool _notifyParent(int parent, char msg);
			
private:
	uint32_t m_minSpare;
	uint32_t m_maxSpare;
	uint32_t m_maxChildren;
	uint32_t m_maxRequests;
	uint32_t m_maxExecTime;
	program_options::variables_map &m_cfgvm;

	pid_t    m_pid;
	map<pid_t, child_t*> m_children;
};

}

#endif

