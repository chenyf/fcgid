PREFIX=/usr/local
PYTHON_VERSION=2.6

INC_DIR=\
	-I${PREFIX}/include \
	-I${PREFIX}/include/python${PYTHON_VERSION}

LIB_DIR=\
	-L${PREFIX}/lib 

CPP_FLAGS=-g -Wall 
LDFLAGS=-Wl,-E 
LIBS=-ldl -lrt -lutil -lssl -lboost_program_options -lfcgi -lfcgi++

STATIC_LIBS=\
	/usr/local/lib/libpython${PYTHON_VERSION}.a 

all: fcgid
fcgid: trace.cpp util.cpp fcgid.cpp preforkserver.cpp wsgi.cpp
	 g++ ${CPP_FLAGS}  ${LDFLAGS} ${INC_DIR} ${LIB_DIR} -o fcgid trace.cpp util.cpp preforkserver.cpp wsgi.cpp fcgid.cpp ${LIBS} ${STATIC_LIBS} 


test: trace.cpp util.cpp test_wsgi.cpp wsgi.cpp
	 g++ ${CPP_FLAGS}  ${LDFLAGS} ${INC_DIR} ${LIB_DIR} -o test_wsgi trace.cpp util.cpp wsgi.cpp test_wsgi.cpp ${LIBS} ${STATIC_LIBS} 


clean:
	rm -f fcgid test_wsgi


