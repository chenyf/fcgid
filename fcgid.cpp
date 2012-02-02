#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <netinet/in.h>

#include <boost/program_options.hpp>
#include <boost/typeof/std/string.hpp>
#include <boost/any.hpp>

#include "preforkserver.h"

using namespace std;
using namespace boost;
using namespace fcgid;


program_options::variables_map g_cfgvm;

int main(int argc, char **argv)
{
	program_options::options_description cmdopts("Usage: fcgid [options]");
	cmdopts.add_options()
		("help,h", "display this help and exit")
		("version,v", "output version information and exit")
		("daemon,d", "run fcgid as daemon")
		("port,p",   program_options::value<uint16_t>(), "specify which port to listen")
		("config,c", program_options::value<string>(), "specify which config file to read")
		;

	program_options::variables_map cmdvm;
	
	BOOST_AUTO(pr, 
		program_options::command_line_parser(argc, argv).
			options(cmdopts).
			allow_unregistered().
			run());
	store(pr, cmdvm);
	//program_options::store(program_options::parse_command_line(argc, argv, cmdopts), cmdvm);

	if(cmdvm.size() == 0) {
		cout << cmdopts << endl;
		return 0;
	}
	if(cmdvm.count("help")) {
		cout << cmdopts << endl;
		return 0;
	}
	if(cmdvm.count("version")) {
		cout << "fcgid 1.0.0.0" << endl;
		return 0;
	}

	if(cmdvm.count("config") == 0) {
		cout << "Please specify fcgid config file" << endl;
		return 1;
	}

	if(cmdvm.count("port") == 0) {
		cout << "Please specify port" << endl;
		return 1;
	}

	uint16_t port = cmdvm["port"].as<uint16_t>();	
	if(port <= 1024) {
		cout << "'port' should > 1024" << endl;
		return 1;
	}

	bool daemon = false;
	if(cmdvm.count("daemon")) {
		daemon = true;
	}

	program_options::options_description cfgopts("fastcgid config options");
	cfgopts.add_options()
		("min_spare", program_options::value<uint32_t>()->default_value(1))
		("max_spare", program_options::value<uint32_t>()->default_value(1))
		("max_children", program_options::value<uint32_t>()->default_value(1))
		("max_requests", program_options::value<uint32_t>()->default_value(1))
		("max_execute_time", program_options::value<uint32_t>()->default_value(10))
		;
	program_options::store(program_options::parse_config_file<char>(cmdvm["config"].as<string>().c_str(), cfgopts, true), g_cfgvm);

	cout << "=============config file optoins=================" << endl;
	cout << "min_spare: " << g_cfgvm["min_spare"].as<uint32_t>() << endl;
	cout << "max_spare: "	<< g_cfgvm["max_spare"].as<uint32_t>() << endl; 
	cout << "max_children: " << g_cfgvm["max_children"].as<uint32_t>() << endl;
	cout << "max_requests: " << g_cfgvm["max_requests"].as<uint32_t>() << endl;
	cout << "max_execute_time: " << g_cfgvm["max_execute_time"].as<uint32_t>() << endl;
	cout << "=================================================" << endl;
	
	if(g_cfgvm["min_spare"].as<uint32_t>() < 1 || g_cfgvm["min_spare"].as<uint32_t>() > 100) {
		cout << "'min_spare' should within [1, 100]" << endl;
		return 1;
	}
	if(g_cfgvm["max_spare"].as<uint32_t>() < 1 || g_cfgvm["max_spare"].as<uint32_t>() > 100) {
		cout << "'max_spare' should within [1, 100]" << endl;
		return 1;
	}
	if(g_cfgvm["max_children"].as<uint32_t>() < 1 || g_cfgvm["max_children"].as<uint32_t>() > 100) {
		cout << "'max_children' should within [1, 100]" << endl;
		return 1;
	}
	if(g_cfgvm["max_requests"].as<uint32_t>() > 1024) {
		cout << "'max_requets' should <= 1024" << endl;
		return 1;
	}
	if(g_cfgvm["max_execute_time"].as<uint32_t>() > 3600) {
		cout << "'max_execute_time' should <= 3600" << endl;
		return 1;
	}

   char normpath[PATH_MAX];
   if(realpath(argv[0], normpath) == NULL) { 
		cout << "readlink failed " << errno << endl;
		return 1;
   } 

	char *sep = strrchr(normpath, '/');
	sep++;
	*sep = 0;
	const any a = string(normpath);	
	pair<string, program_options::variable_value> item(string("exe_path"), program_options::variable_value(a, false));
	g_cfgvm.insert(item);
	cout << "execute path: " << g_cfgvm["exe_path"].as<string>() << endl;
		
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	int on = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	struct sockaddr_in soin;
	bzero(&soin, sizeof(soin));
	soin.sin_family = AF_INET;
	soin.sin_addr.s_addr = htonl(INADDR_ANY);
	soin.sin_port = htons(port);

	if(bind(sock, (struct sockaddr *) &soin, sizeof(soin)) != 0) {
		printf("bind failed\n");
		return -1;
	}

	listen(sock, 5);
	PreforkServer server(g_cfgvm);
	server.run(sock, daemon);
	return 0;
}

