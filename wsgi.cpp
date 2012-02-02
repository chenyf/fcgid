#include <Python.h>
#include <node.h>

#include <openssl/md5.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>

#include <string>
#include <list>
#include <map>

#include "trace.h"
#include "util.h"
#include "httpd.h"
#include "wsgi.h"

using namespace std;

namespace fcgid
{

typedef struct {
        PyObject_HEAD
        const char *target;
			IRequest *r;
        int level;
        char *s;
        int l;
        int expired;

} LogObject;

static void Log_call(LogObject *self, const char *s, int l)
{
    if (self->r) {
   	self->r->PutDataErr(s, l); 
		self->r->FlushErr();
	}
}

static void Log_dealloc(LogObject *self)
{
    if (self->s) {
        if (!self->expired)
            Log_call(self, self->s, self->l);
        free(self->s);
    }
    PyObject_Del(self);
}

static PyObject *Log_flush(LogObject *self, PyObject *args)
{
    if (self->expired) {
        PyErr_SetString(PyExc_RuntimeError, "log object has expired");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, ":flush"))
        return NULL;

    if (self->s) {
        Log_call(self, self->s, self->l);
        free(self->s);
        self->s = NULL;
        self->l = 0;
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Log_close(LogObject *self, PyObject *args)
{
    PyObject *result = NULL;
    if (!PyArg_ParseTuple(args, ":close"))
        return NULL;

    if (!self->expired)
        result = Log_flush(self, args);

    Py_XDECREF(result);
    self->r = NULL;
    self->expired = 1;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Log_isatty(LogObject *self, PyObject *args)
{
    //PyObject *result = NULL;
    if (!PyArg_ParseTuple(args, ":isatty"))
        return NULL;
    Py_INCREF(Py_False);
    return Py_False;
}

static void Log_queue(LogObject *self, const char *msg, int len)
{
    const char *p = NULL;
    const char *q = NULL;
    const char *e = NULL;

    p = msg;
    e = p + len;

    /*
     * Break string on newline. This is on assumption
     * that primarily textual information being logged.
     */

    q = p;
    while (q != e) {
        if (*q == '\n')
            break;
        q++;
    }

    while (q != e) {
        /* Output each complete line. */

        if (self->s) {
            /* Need to join with buffered value. */

            int m = 0;
            int n = 0;
            char *s = NULL;

            m = self->l;
            n = m+q-p+1;

            s = (char *)malloc(n);
            memcpy(s, self->s, m);
            memcpy(s+m, p, q-p);
            s[n-1] = '\0';

            free(self->s);
            self->s = NULL;
            self->l = 0;

            Log_call(self, s, n-1);
            free(s);
        }
        else {
            int n = 0;
            char *s = NULL;

            n = q-p+1;

            s = (char *)malloc(n);
            memcpy(s, p, q-p);
            s[n-1] = '\0';

            Log_call(self, s, n-1);
            free(s);
        }

        p = q+1;

        /* Break string on newline. */

        q = p;
        while (q != e) {
            if (*q == '\n')
                break;
            q++;
        }
    }

    if (p != e) {
        /* Save away incomplete line. */

        if (self->s) {
            /* Need to join with buffered value. */

            int m = 0;
            int n = 0;

            m = self->l;
            n = m+e-p+1;

            self->s = (char *)realloc(self->s, n);
            memcpy(self->s+m, p, e-p);
            self->s[n-1] = '\0';
            self->l = n-1;
        }
        else {
            int n = 0;

            n = e-p+1;

            self->s = (char *)malloc(n);
            memcpy(self->s, p, n-1);
            self->s[n-1] = '\0';
            self->l = n-1;
        }
    }
}

static PyObject *Log_write(LogObject *self, PyObject *args)
{
    const char *msg = NULL;
    int len = -1;

    if (self->expired) {
        PyErr_SetString(PyExc_RuntimeError, "log object has expired");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "s#:write", &msg, &len))
        return NULL;
    Log_queue(self, msg, len);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Log_writelines(LogObject *self, PyObject *args)
{
    PyObject *sequence = NULL;
    PyObject *iterator = NULL;
    PyObject *item = NULL;
    //const char *msg = NULL;

    if (self->expired) {
        PyErr_SetString(PyExc_RuntimeError, "log object has expired");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "O:writelines", &sequence))
        return NULL;

    iterator = PyObject_GetIter(sequence);
    if (iterator == NULL) {
        PyErr_SetString(PyExc_TypeError,
                        "argument must be sequence of strings");
        return NULL;
    }

    while ((item = PyIter_Next(iterator))) {
        PyObject *result = NULL;
        result = Log_write(self, item);
        if (!result) {
            Py_DECREF(iterator);
            PyErr_SetString(PyExc_TypeError,
                            "argument must be sequence of strings");
            return NULL;
        }
    }

    Py_DECREF(iterator);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Log_closed(LogObject *self, void *closure)
{
    Py_INCREF(Py_False);
    return Py_False;
}

static PyMethodDef Log_methods[] = {
    { "flush",      (PyCFunction)Log_flush,      METH_VARARGS, 0 },
    { "close",      (PyCFunction)Log_close,      METH_VARARGS, 0 },
    { "isatty",     (PyCFunction)Log_isatty,     METH_VARARGS, 0 },
    { "write",      (PyCFunction)Log_write,      METH_VARARGS, 0 },
    { "writelines", (PyCFunction)Log_writelines, METH_VARARGS, 0 },
    { NULL, NULL}
};

static PyGetSetDef Log_getset[] = {
    { "closed", (getter)Log_closed, NULL, 0 },
    { NULL },
};

static PyTypeObject Log_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "mod_wsgi.Log",         /*tp_name*/
    sizeof(LogObject),      /*tp_basicsize*/
    0,                      /*tp_itemsize*/
    /* methods */
    (destructor)Log_dealloc, /*tp_dealloc*/
    0,                      /*tp_print*/
    0,                      /*tp_getattr*/
    0,                      /*tp_setattr*/
    0,                      /*tp_compare*/
    0,                      /*tp_repr*/
    0,                      /*tp_as_number*/
    0,                      /*tp_as_sequence*/
    0,                      /*tp_as_mapping*/
    0,                      /*tp_hash*/
    0,                      /*tp_call*/
    0,                      /*tp_str*/
    0,                      /*tp_getattro*/
    0,                      /*tp_setattro*/
    0,                      /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,     /*tp_flags*/
    0,                      /*tp_doc*/
    0,                      /*tp_traverse*/
    0,                      /*tp_clear*/
    0,                      /*tp_richcompare*/
    0,                      /*tp_weaklistoffset*/
    0,                      /*tp_iter*/
    0,                      /*tp_iternext*/
    Log_methods,            /*tp_methods*/
    0,                      /*tp_members*/
    Log_getset,             /*tp_getset*/
    0,                      /*tp_base*/
    0,                      /*tp_dict*/
    0,                      /*tp_descr_get*/
    0,                      /*tp_descr_set*/
    0,                      /*tp_dictoffset*/
    0,                      /*tp_init*/
    0,                      /*tp_alloc*/
    0,                      /*tp_new*/
    0,                      /*tp_free*/
    0,                      /*tp_is_gc*/
};

static PyObject *newLogObject(IRequest *r, int level, const char *target)
{
    LogObject *self;
    self = PyObject_New(LogObject, &Log_Type);
    if (self == NULL)
        return NULL;

    self->target = target;
    self->r = r;
    self->level = level;////APLOG_NOERRNO|level;
    self->s = NULL;
    self->l = 0;
    self->expired = 0;
    return (PyObject *)self;
}

typedef struct {
        PyObject_HEAD
			IRequest *r;
        int init;
        int done;
        char *buffer;
        int size;
        int offset;
        int length;

} InputObject;

static void Input_dealloc(InputObject *self)
{
    if (self->buffer)
        free(self->buffer);
    PyObject_Del(self);
}

static PyObject *Input_close(InputObject *self, PyObject *args)
{
	TRACE0("enter input close\n");
    if (!self->r) {
        PyErr_SetString(PyExc_RuntimeError, "request object has expired");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, ":close"))
        return NULL;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Input_read(InputObject *self, PyObject *args)
{
	TRACE0("enter input_read\n");
    long size = -1;

    PyObject *result = NULL;
    char *buffer = NULL;
    int length = 0;
    int init = 0;

    int n;

    if (!self->r) {
        PyErr_SetString(PyExc_RuntimeError, "request object has expired");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "|l:read", &size))
        return NULL;

    init = self->init;

    if (!self->init) {
        self->init = 1;
    }

    /* No point continuing if no more data to be consumed. */

    if (self->done && self->length == 0)
        return PyString_FromString("");

    /*
     * If requested size is zero bytes, then still need to pass
     * this through to Apache input filters so that any
     * 100-continue response is triggered. Only do this if very
     * first attempt to read data. Note that this will cause an
     * assertion failure in HTTP_IN input filter when Apache
     * maintainer mode is enabled. It is arguable that the
     * assertion check, which prohibits a zero length read,
     * shouldn't exist, as why should a zero length read be not
     * allowed if input filter processing still works when it
     * does occur.
     */

    if (size == 0) {
        return PyString_FromString("");
    }

    /*
     * First deal with case where size has been specified. After
     * that deal with case where expected that all remaining
     * data is to be read in and returned as one string.
     */

    if (size > 0) {
        /* Allocate string of the exact size required. */

        result = PyString_FromStringAndSize(NULL, size);

        if (!result)
            return NULL;

        buffer = PyString_AS_STRING((PyStringObject *)result);

        /* Copy any residual data from use of readline(). */

        if (self->buffer && self->length) {
            if (size >= self->length) {
                length = self->length;
                memcpy(buffer, self->buffer + self->offset, length);
                self->offset = 0;
                self->length = 0;
            }
            else {
                length = size;
                memcpy(buffer, self->buffer + self->offset, length);
                self->offset += length;
                self->length -= length;
            }
        }

        /* If all data residual buffer consumed then free it. */

        if (!self->length) {
            free(self->buffer);
            self->buffer = NULL;
        }

        /* Read in remaining data required to achieve size. */

        if (length < size) {
            while (length != size) {
					 n = self->r->GetData(buffer + length, size - length);

                if (n == -1) {
                    PyErr_SetString(PyExc_IOError, "request data read error");
                    Py_DECREF(result);
                    return NULL;
                }
                else if (n == 0) {
                    /* Have exhausted all the available input data. */

                    self->done = 1;
                    break;
                }
                length += n;
            }

            /*
             * Resize the final string. If the size reduction is
             * by more than 25% of the string size, then Python
             * will allocate a new block of memory and copy the
             * data into it.
             */

            if (length != size) {
                if (_PyString_Resize(&result, length))
                    return NULL;
            }
        }
    }
    else {
    }
    return result;
}


static PyObject *Input_readline(InputObject *self, PyObject *args)
{
	return NULL;
}

static PyObject *Input_readlines(InputObject *self, PyObject *args)
{
	return NULL;
}

static PyMethodDef Input_methods[] = {
    { "close",     (PyCFunction)Input_close,     METH_VARARGS, 0 },
    { "read",      (PyCFunction)Input_read,      METH_VARARGS, 0 },
    { "readline",  (PyCFunction)Input_readline,  METH_VARARGS, 0 },
    { "readlines", (PyCFunction)Input_readlines, METH_VARARGS, 0 },
    { NULL, NULL}
};

static PyObject *Input_iter(InputObject *self)
{
	TRACE0("enter Input_iter\n");
    if (!self->r) {
        PyErr_SetString(PyExc_RuntimeError, "request object has expired");
        return NULL;
    }
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *Input_iternext(InputObject *self)
{
	TRACE0("enter Input_iternext\n");
    PyObject *line = NULL;
    PyObject *rlargs = NULL;

    if (!self->r) {
        PyErr_SetString(PyExc_RuntimeError, "request object has expired");
        return NULL;
    }

    rlargs = PyTuple_New(0);

    if (!rlargs)
      return NULL;

    line = Input_readline(self, rlargs);

    Py_DECREF(rlargs);

    if (!line)
        return NULL;

    if (PyString_GET_SIZE(line) == 0) {
        PyErr_SetObject(PyExc_StopIteration, Py_None);
        Py_DECREF(line);
        return NULL;
    }
    return line;
}


static PyTypeObject Input_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "mod_wsgi.Input",       /*tp_name*/
    sizeof(InputObject),    /*tp_basicsize*/
    0,                      /*tp_itemsize*/
    /* methods */
    (destructor)Input_dealloc, /*tp_dealloc*/
    0,                      /*tp_print*/
    0,                      /*tp_getattr*/
    0,                      /*tp_setattr*/
    0,                      /*tp_compare*/
    0,                      /*tp_repr*/
    0,                      /*tp_as_number*/
    0,                      /*tp_as_sequence*/
    0,                      /*tp_as_mapping*/
    0,                      /*tp_hash*/
    0,                      /*tp_call*/
    0,                      /*tp_str*/
    0,                      /*tp_getattro*/
    0,                      /*tp_setattro*/
    0,                      /*tp_as_buffer*/
#if defined(Py_TPFLAGS_HAVE_ITER)
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_ITER, /*tp_flags*/
#else
    Py_TPFLAGS_DEFAULT,     /*tp_flags*/
#endif
    0,                      /*tp_doc*/
    0,                      /*tp_traverse*/
    0,                      /*tp_clear*/
    0,                      /*tp_richcompare*/
    0,                      /*tp_weaklistoffset*/
    (getiterfunc)Input_iter, /*tp_iter*/
    (iternextfunc)Input_iternext, /*tp_iternext*/
    Input_methods,          /*tp_methods*/
    0,                      /*tp_members*/
    0,                      /*tp_getset*/
    0,                      /*tp_base*/
    0,                      /*tp_dict*/
    0,                      /*tp_descr_get*/
    0,                      /*tp_descr_set*/
    0,                      /*tp_dictoffset*/
    0,                      /*tp_init*/
    0,                      /*tp_alloc*/
    0,                      /*tp_new*/
    0,                      /*tp_free*/
    0,                      /*tp_is_gc*/
};

static InputObject *newInputObject(IRequest *r)
{
    InputObject *self;

    self = PyObject_New(InputObject, &Input_Type);
    if (self == NULL)
        return NULL;

    self->r = r;
    self->init = 0;
    self->done = 0;
    self->buffer = NULL;
    self->size = 0;
    self->length = 0;
    self->offset = 0;
    return self;
}



typedef struct {
        PyObject_HEAD
        int result;
        IRequest *r;

        InputObject *input;
        PyObject    *log;
        int status;
        const char *status_line;
        PyObject *headers;
        PyObject *sequence;
        int content_length_set;
        off_t content_length;
        off_t output_length;

} AdapterObject;

static void Adapter_dealloc(AdapterObject *self)
{
    Py_XDECREF(self->headers);
    Py_XDECREF(self->sequence);
    Py_DECREF(self->input);
    Py_DECREF(self->log);
    PyObject_Del(self);
}

static PyObject *Adapter_start_response(AdapterObject *self, PyObject *args)
{
    const char *status = NULL;
    PyObject *headers = NULL;
    PyObject *exc_info = NULL;

    PyObject *item = NULL;
    PyObject *latin_item = NULL;
    char* value = NULL;

    if (!self->r) {
        PyErr_SetString(PyExc_RuntimeError, "request object has expired");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "OO|O:start_response",
        &item, &headers, &exc_info)) {
        return NULL;
    }

    if (!PyString_Check(item)) {
        PyErr_Format(PyExc_TypeError, "expected byte string object for "
                     "status, value of type %.200s found",
                     item->ob_type->tp_name);
        Py_XDECREF(latin_item);
        return NULL;
    }

    status = PyString_AsString(item);

    if (!PyList_Check(headers)) {
        PyErr_SetString(PyExc_TypeError, "response headers must be a list");
        Py_XDECREF(latin_item);
        return NULL;
    }

    if (exc_info && exc_info != Py_None) {
        if (self->status_line && !self->headers) {
            PyObject *type = NULL;
            PyObject *value = NULL;
            PyObject *traceback = NULL;

            if (!PyArg_ParseTuple(exc_info, "OOO", &type,
                                  &value, &traceback)) {
                Py_XDECREF(latin_item);
                return NULL;
            }

            Py_INCREF(type);
            Py_INCREF(value);
            Py_INCREF(traceback);
            PyErr_Restore(type, value, traceback);
            Py_XDECREF(latin_item);
            return NULL;
        }
    }
    else if (self->status_line && !self->headers) {
        PyErr_SetString(PyExc_RuntimeError, "headers have already been sent");
        Py_XDECREF(latin_item);
        return NULL;
    }

   self->status_line = strdup(status);
	//printf("status_line: %s\n", status);
	value = ap_getword(&status, ' ');
    errno = 0;
    self->status = strtol(value, &value, 10);
    if (*value || errno == ERANGE) {
        PyErr_SetString(PyExc_TypeError, "status value is not an integer");
        Py_XDECREF(latin_item);
        return NULL;
    }
    if (!*status) {
        PyErr_SetString(PyExc_ValueError, "status message was not supplied");
        Py_XDECREF(latin_item);
        return NULL;
    }

    Py_XDECREF(self->headers);
    self->headers = headers;
    Py_INCREF(self->headers);
    Py_XDECREF(latin_item);
    return PyObject_GetAttrString((PyObject *)self, "write");
}

static int Adapter_output(AdapterObject *self, const char *data, int length,
                          int exception_when_aborted)
{
    int i = 0;
    int n = 0;
    IRequest *r;

    if (!self->status_line) {
        PyErr_SetString(PyExc_RuntimeError, "response has not been started");
        return 0;
    }

    r = self->r;

    /* Have response headers yet been sent. */
    if (self->headers) {
			list< pair<char*, char*> > headers;

        for (i = 0; i < PyList_Size(self->headers); i++) {
            PyObject *tuple = NULL;

            PyObject *object1 = NULL;
            PyObject *object2 = NULL;

            char *name = NULL;
            char *value = NULL;

            tuple = PyList_GetItem(self->headers, i);

            if (!PyTuple_Check(tuple)) {
                PyErr_Format(PyExc_TypeError, "list of tuple values "
                             "expected, value of type %.200s found",
                             tuple->ob_type->tp_name);
                return 0;
            }

            if (PyTuple_Size(tuple) != 2) {
                PyErr_Format(PyExc_ValueError, "tuple of length 2 "
                             "expected, length is %d",
                             (int)PyTuple_Size(tuple));
                return 0;
            }

            object1 = PyTuple_GetItem(tuple, 0);
            object2 = PyTuple_GetItem(tuple, 1);

            if (PyString_Check(object1)) {
                name = PyString_AsString(object1);
            }
            else {
                PyErr_Format(PyExc_TypeError, "expected byte string object "
                             "for header name, value of type %.200s "
                             "found", object1->ob_type->tp_name);
                return 0;
            }

            if (PyString_Check(object2)) {
                value = PyString_AsString(object2);
            }
            else {
                PyErr_Format(PyExc_TypeError, "expected byte string object "
                             "for header value, value of type %.200s "
                             "found", object2->ob_type->tp_name);
                return 0;
            }

            if (strchr(name, '\n') != 0 || strchr(value, '\n') != 0) {
                PyErr_Format(PyExc_ValueError, "embedded newline in "
                             "response header with name '%s' and value '%s'",
                             name, value);
                return 0;
            }

            if (!strcasecmp(name, "Content-Length")) {
                char *v = value;
                long l = 0;

                errno = 0;
                l = strtol(v, &v, 10);
                if (*v || errno == ERANGE || l < 0) {
                    PyErr_SetString(PyExc_ValueError,
                                    "invalid content length");
                    return 0;
                }
                self->content_length_set = 1;
                self->content_length = l;
            }
				pair<char*, char*> tmp(name, value);
				headers.push_back(tmp);
        }

		string output("");
		list< pair<char*, char*> >::iterator idx;
		for(idx = headers.begin(); idx != headers.end(); idx++) {
			char *key = (*idx).first;
			char *val = (*idx).second;
			output += key;
			output += ": ";
			output += val;
			output += "\r\n";
		}
		output += "\r\n";
		r->PutStr(output.c_str());
		r->FlushOut();

        /*
         * Reset flag indicating whether '100 Continue' response
         * expected. If we don't do this then if an attempt to read
         * input for the first time is after headers have been
         * sent, then Apache is wrongly generate the '100 Continue'
         * response into the response content. Not sure if this is
         * a bug in Apache, or that it truly believes that input
         * will never be read after the response headers have been
         * sent.
         */

        /* No longer need headers now that they have been sent. */

        Py_DECREF(self->headers);
        self->headers = NULL;
    }

    /*
     * If content length was specified, ensure that we don't
     * actually output more data than was specified as being
     * sent as otherwise technically in violation of HTTP RFC.
     */

    if (length) {
        int output_length = length;

        if (self->content_length_set) {
            if (self->output_length < self->content_length) {
                if (self->output_length + length > self->content_length) {
                    length = self->content_length - self->output_length;
                }
            }
            else
                length = 0;
        }
        self->output_length += output_length;
    }

    /* Now output any data. */

    if (length) {
        /*
         * In Apache 1.3, the bucket brigade system doesn't exist,
         * so have no choice but to use ap_rwrite()/ap_rflush().
         * It is not believed that Apache 1.3 suffers the memory
         * accumulation problem when streaming lots of data.
         */

        n = r->PutData(data, length);
        if (n == -1) {
            PyErr_SetString(PyExc_IOError, "failed to write data");
            return 0;
        }

        n = r->FlushOut();
        if (n == -1) {
            PyErr_SetString(PyExc_IOError, "failed to flush data");
            return 0;
        }
		//	TRACE0("output (%s) (%d)\n", data, length);
    }

    /*
     * Check whether aborted connection was found when data
     * being written, otherwise will not be flagged until next
     * time that data is being written. Early detection is
     * better as it may have been the last data block being
     * written and application may think that data has all
     * been written. In a streaming application, we also want
     * to avoid any additional data processing to generate any
     * successive data.
     */
	/*
    if (r->connection->aborted) {
        if (!exception_when_aborted) {
            ap_log_rerror(APLOG_MARK, WSGI_LOG_DEBUG(0), self->r,
                          "mod_wsgi (pid=%d): Client closed connection.",
                          getpid());
        }
        else
            PyErr_SetString(PyExc_IOError, "client connection closed");

        return 0;
    }
	*/
    return 1;
}

static PyObject *Adapter_environ(AdapterObject *self)
{
    IRequest *r = NULL;

    PyObject *vars = NULL;
    PyObject *object = NULL;

    /* Create the WSGI environment dictionary. */
    vars = PyDict_New();

    /* Merge the CGI environment into the WSGI environment. */

    r = self->r;
	 char **envp = r->GetEnv();
	 for(; *envp; ++envp) {
		char *sep = strchr(*envp, '=');
		if(sep == NULL)
			continue;
		char *key = strndup(*envp, sep-*envp);
		char *val = sep+1;
		object = PyString_FromString(val);
		PyDict_SetItemString(vars, key, object);
      Py_DECREF(object);
		/*
		if(!strcmp(key, "REQUEST_URI")) {
			object = PyString_FromString(val);
			PyDict_SetItemString(vars, "PATH_INFO", object);
	      Py_DECREF(object);
		}
		*/

		free(key);
	}	

    /////PyDict_DelItemString(vars, "PATH");

    /* Now setup all the WSGI specific environment values. */
    object = Py_BuildValue("(ii)", 1, 1);
    PyDict_SetItemString(vars, "wsgi.version", object);
    Py_DECREF(object);

    object = PyBool_FromLong(0);
    PyDict_SetItemString(vars, "wsgi.multithread", object);
    Py_DECREF(object);

    object = PyBool_FromLong(1);
    PyDict_SetItemString(vars, "wsgi.multiprocess", object);
    Py_DECREF(object);

    PyDict_SetItemString(vars, "wsgi.run_once", Py_False);

    object = PyString_FromString("http");
    PyDict_SetItemString(vars, "wsgi.url_scheme", object);
    Py_DECREF(object);

    /*
     * Setup log object for WSGI errors. Don't decrement
     * reference to log object as keep reference to it.
     */

    object = (PyObject *)self->log;
    PyDict_SetItemString(vars, "wsgi.errors", object);

    /* Setup input object for request content. */

    object = (PyObject *)self->input;
    PyDict_SetItemString(vars, "wsgi.input", object);
    return vars;
}

static int Adapter_run(AdapterObject *self, PyObject *object)
{
    PyObject *vars = NULL;
    PyObject *start = NULL;
    PyObject *args = NULL;

    PyObject *iterator = NULL;
    PyObject *close = NULL;

    const char *msg = NULL;
    int length = 0;

    vars = Adapter_environ(self);
    start = PyObject_GetAttrString((PyObject *)self, "start_response");
    args = Py_BuildValue("(OO)", vars, start);
  
	self->sequence = PyEval_CallObject(object, args);

    if (self->sequence != NULL) {
         {
            int aborted = 0;
            iterator = PyObject_GetIter(self->sequence);
            if (iterator != NULL) {
                PyObject *item = NULL;

                while ((item = PyIter_Next(iterator))) {
                    if (!PyString_Check(item)) {
                        PyErr_Format(PyExc_TypeError, "sequence of byte "
                                    "string values expected, value of "
                                     "type %.200s found",
                                     item->ob_type->tp_name);
                        Py_DECREF(item);
                        break;
                    }

                    msg = PyString_AsString(item);
                    length = PyString_Size(item);

                    if (!msg) {
                        Py_DECREF(item);
                        break;
                    }
                    if (length && !Adapter_output(self, msg, length, 0)) {
                        if (!PyErr_Occurred()) {
                            aborted = 1;
									TRACE0("aborted set to 1\n");
								}
                        Py_DECREF(item);
                        break;
                    }
                    Py_DECREF(item);
                }
            }

            if (!PyErr_Occurred() && !aborted) {
                if (Adapter_output(self, "", 0, 0))
                    self->result = HTTP_OK;
            }

            Py_XDECREF(iterator);
        }

        if (PyErr_Occurred()) {
            /*
             * Response content has already been sent, so cannot
             * return an internal server error as Apache will
             * append its own error page. Thus need to return OK
             * and just truncate the response.
             */

            if (self->status_line && !self->headers)
                self->result = HTTP_OK;

            ////wsgi_log_python_error(self->r, self->log, self->r->filename);
        }

        if (PyObject_HasAttrString(self->sequence, "close")) {
            PyObject *args = NULL;
            PyObject *data = NULL;

            close = PyObject_GetAttrString(self->sequence, "close");
            args = Py_BuildValue("()");
            data = PyEval_CallObject(close, args);
            Py_DECREF(args);
            Py_XDECREF(data);
            Py_DECREF(close);
        }

        ////if (PyErr_Occurred())
        ////    wsgi_log_python_error(self->r, self->log, self->r->filename);

        Py_XDECREF(self->sequence);
        self->sequence = NULL;
    } else {
      if (PyErr_Occurred())
			PyErr_Print(); 
		}

    Py_DECREF(args);
    Py_DECREF(start);
    Py_DECREF(vars);
    return self->result;
}

static PyObject *Adapter_write(AdapterObject *self, PyObject *args)
{
    PyObject *item = NULL;
    const char *data = NULL;
    int length = 0;

    if (!self->r) {
        PyErr_SetString(PyExc_RuntimeError, "request object has expired");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "O:write", &item))
        return NULL;

    if (!PyString_Check(item)) {
        PyErr_Format(PyExc_TypeError, "byte string value expected, value "
                     "of type %.200s found", item->ob_type->tp_name);
        Py_DECREF(item);
        return NULL;
    }

    data = PyString_AsString(item);
    length = PyString_Size(item);

    if (!Adapter_output(self, data, length, 1))
        return NULL;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef Adapter_methods[] = {
    { "start_response", (PyCFunction)Adapter_start_response, METH_VARARGS, 0 },
    { "write",          (PyCFunction)Adapter_write, METH_VARARGS, 0 },
    { NULL, NULL}
};

static PyTypeObject Adapter_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "mod_wsgi.Adapter",     /*tp_name*/
    sizeof(AdapterObject),  /*tp_basicsize*/
    0,                      /*tp_itemsize*/
    /* methods */
    (destructor)Adapter_dealloc, /*tp_dealloc*/
    0,                      /*tp_print*/
    0,                      /*tp_getattr*/
    0,                      /*tp_setattr*/
    0,                      /*tp_compare*/
    0,                      /*tp_repr*/
    0,                      /*tp_as_number*/
    0,                      /*tp_as_sequence*/
    0,                      /*tp_as_mapping*/
    0,                      /*tp_hash*/
    0,                      /*tp_call*/
    0,                      /*tp_str*/
    0,                      /*tp_getattro*/
    0,                      /*tp_setattro*/
    0,                      /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,     /*tp_flags*/
    0,                      /*tp_doc*/
    0,                      /*tp_traverse*/
    0,                      /*tp_clear*/
    0,                      /*tp_richcompare*/
    0,                      /*tp_weaklistoffset*/
    0,                      /*tp_iter*/
    0,                      /*tp_iternext*/
    Adapter_methods,        /*tp_methods*/
    0,                      /*tp_members*/
    0,                      /*tp_getset*/
    0,                      /*tp_base*/
    0,                      /*tp_dict*/
    0,                      /*tp_descr_get*/
    0,                      /*tp_descr_set*/
    0,                      /*tp_dictoffset*/
    0,                      /*tp_init*/
    0,                      /*tp_alloc*/
    0,                      /*tp_new*/
    0,                      /*tp_free*/
    0,                      /*tp_is_gc*/
};

static AdapterObject *newAdapterObject(IRequest *r)
{
    AdapterObject *self;
    self = PyObject_New(AdapterObject, &Adapter_Type);
    if (self == NULL)
        return NULL;

    self->result = HTTP_INTERNAL_SERVER_ERROR;

    self->r = r;
    self->status = HTTP_INTERNAL_SERVER_ERROR;
    self->status_line = NULL;
    self->headers = NULL;
    self->sequence = NULL;

    self->content_length_set = 0;
    self->content_length = 0;
    self->output_length = 0;

    self->input = newInputObject(r);
    self->log   = newLogObject(r, 1, NULL);
    return self;
}

/*
 * Code for importing a module from source by absolute path.
 */
static PyObject *wsgi_load_source(IRequest *r,
                                  const char *name, int exists,
                                  const char* filename)
{
    FILE *fp = NULL;
    PyObject *m = NULL;
    PyObject *co = NULL;
    struct _node *n = NULL;

    fp = fopen(filename, "r");
    if (!fp) {
			TRACE0("Failed to open '%s'.\n", filename);
			r->FPrintFErr("Failed to open '%s'.\n", filename);
        return NULL;
    }

    n = PyParser_SimpleParseFile(fp, filename, Py_file_input);
    fclose(fp);
    if (!n) {
			TRACE0("Failed to parse WSGI script file '%s'.\n", filename);
			r->FPrintFErr("Failed to parse WSGI script file '%s'.\n", filename);
        return NULL;
    }

    co = (PyObject *)PyNode_Compile(n, filename);
    PyNode_Free(n);

	 if(co == NULL) {
		TRACE0("Failed to compile WSGI script file '%s'.\n", filename);
		r->FPrintFErr("Failed to compile WSGI script file '%s'.\n", filename);
		return NULL;
	}

	m = PyImport_ExecCodeModuleEx((char *)name, co, (char *)filename);
	if(m == NULL) {
		TRACE0("Failed to import '%s'.\n", filename);
		r->FPrintFErr("Failed to import '%s'.\n", filename);
      if (PyErr_Occurred())
			PyErr_Print(); 
	}
    Py_XDECREF(co);
    if (m) {
        PyObject *object = NULL;

            struct stat finfo;
            if (stat(filename, &finfo) == -1) {
                object = PyLong_FromLongLong(0);
            }
            else {
                object = PyLong_FromLongLong(finfo.st_mtime);
            }

        PyModule_AddObject(m, "__mtime__", object);
	}
    return m;
}

static int wsgi_reload_required(const char *filename, PyObject *module)
{
    PyObject *dict = NULL;
    PyObject *object = NULL;
    time_t mtime = 0;

    dict = PyModule_GetDict(module);
    object = PyDict_GetItemString(dict, "__mtime__");

    if (object) {
        mtime = PyLong_AsLongLong(object);
            struct stat finfo;
            if (stat(filename, &finfo) == -1) {
                return 1;
            }
            else if (mtime != finfo.st_mtime) {
                return 1;
            }
    		return 0;
    }
    else
        return 1;
    return 0;
}

static char *wsgi_module_name(const char *filename)
{
    /*
     * Calculate a name for the module using the MD5 of its full
     * pathname. This is so that different code files with the
     * same basename are still considered unique. Note that where
     * we believe a case insensitive file system is being used,
     * we always change the file name to lower case so that use
     * of different case in name doesn't result in duplicate
     * modules being loaded for the same file.
     */

	 unsigned char md5[16];
	 MD5((const unsigned char*)filename, strlen(filename), md5);
	 char *file = (char*)malloc(60);
	 strcpy(file, "_mod_wsgi_");
	 char *p = file + strlen("_mod_wsgi_");
	 for(int i = 0; i < 16; i++) {
		sprintf(p + i*2, "%02X", md5[i]);
	 }
	 return file;
}

/*
void wsgi_build_environment(IRequest *r)
{
	if(r->GetParam("PATH_INFO") == NULL) {
		r->PutParam("PATH_INFO", "");
	}
}
*/

/*
	insert application path into sys.path
*/
static int _setup_exec_environment(const char* approot) 
{
	PyObject *pName = PyString_FromString("sys");
	PyObject *pSysModule = PyImport_Import(pName);
	Py_DECREF(pName);

	if(pSysModule == NULL) {
		TRACE0("load sys module failed\n");
		return 0;
	}

	// sys.path.insert()
	PyObject *pSysPath = PyObject_GetAttrString(pSysModule, "path");
	if(!PySequence_Contains(pSysPath, PyString_FromString(approot))) {
		PyObject *pAppPath = PyString_FromString(approot);
		PyList_Insert(pSysPath, 0, pAppPath);
		Py_DECREF(pAppPath);
	}
	Py_DECREF(pSysPath);
	Py_DECREF(pSysModule);
	return 1;
}

static void _cleanup_exec_environment() 
{
}

static void _output_error(IRequest *r)
{
	r->PutStr("HTTP/1.0 500 INTERNAL SERVER ERROR\r\n");
	r->PutStr("Content-type: text/html\r\n");
	r->PutStr("\r\n");
	r->PutStr("<html><body><h1>Internal error.</h1></body></html>\n");
	r->FlushOut();
}

int wsgi_execute_script(IRequest *r, const char *script)
{
	char *document_root = r->GetParam("DOCUMENT_ROOT");
	if(document_root == NULL) {
		TRACE0("Missing 'DOCUMENT_ROOT'\n");
		r->FPrintFErr("Missing 'DOCUMENT_ROOT'\n");
		_output_error(r);
		return HTTP_INTERNAL_SERVER_ERROR;
	}

	//TRACE0("document root: %s\n", document_root);
	if(!_setup_exec_environment(document_root)) {
		TRACE0("setup python env failed\n");
		r->FPrintFErr("setup python env failed\n");
		_output_error(r);
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	
	PyObject *modules = NULL;
	PyObject *module = NULL;
	char *name = NULL;
	int exists = 0;

	int status;

	name = wsgi_module_name(script);
   TRACE0("script filename (%s), module name (%s)\n", script, name);
	modules = PyImport_GetModuleDict();
	module = PyDict_GetItemString(modules, name);
	Py_XINCREF(module);
	if (module)
		exists = 1;
   
	if (module) {
        if (wsgi_reload_required(script, module)) {
            Py_DECREF(module);
            module = NULL;
				PyDict_DelItemString(modules, name);
        }
    }
	/* Load module if not already loaded. */
	if (!module) {
		module = wsgi_load_source(r, name, exists, script);
		if(module == NULL) {
			TRACE0("load module (%s) failed\n", name);
			r->FPrintFErr("load module (%s) failed\n", name);
		}
	}

	status = HTTP_INTERNAL_SERVER_ERROR;
	/* Determine if script exists and execute it. */
	if (module) {
		PyObject *module_dict = NULL;
		PyObject *object = NULL;

		module_dict = PyModule_GetDict(module);
		const char *funcname = "application";
		object = PyDict_GetItemString(module_dict, funcname);
		
		if (object) {
			AdapterObject *adapter = NULL;
			adapter = newAdapterObject(r);

			if (adapter) {
				PyObject *method = NULL;
				PyObject *args = NULL;

				Py_INCREF(object);
				status = Adapter_run(adapter, object);
				Py_DECREF(object);

				/*
				 * Wipe out references to Apache request object
				 * held by Python objects, so can detect when an
				 * application holds on to the transient Python
				 * objects beyond the life of the request and
				 * thus raise an exception if they are used.
				 */

				adapter->r = NULL;
				adapter->input->r = NULL;

				/* Close the log object so data is flushed. */
				method = PyObject_GetAttrString(adapter->log, "close");
				if (!method) {
					PyErr_Format(PyExc_AttributeError,
								 "'%s' object has no attribute 'close'",
								 adapter->log->ob_type->tp_name);
				}
				else {
					args = PyTuple_New(0);
					object = PyEval_CallObject(method, args);
					Py_DECREF(args);
				}
				/////Py_XDECREF(object);  /// don't XDECREF(object) here
				Py_XDECREF(method);
			}
			Py_XDECREF((PyObject *)adapter);
		}
		else {
			TRACE0("'application' object not found\n");
			r->FPrintFErr("'application' object not found\n");
			_output_error(r);
			status = HTTP_NOT_FOUND;
		}
	} else {
		_output_error(r);
	}

	Py_XDECREF(module);
	free(name);
	_cleanup_exec_environment();
	return status;
}

void wsgi_python_cleanup()
{
	Py_Finalize();
}

int wsgi_python_init()
{
    static int initialized = 0;

    /* Perform initialisation if required. */
    if (!Py_IsInitialized() || !initialized) {

            ////Py_OptimizeFlag = 0;
            ////Py_SetPythonHome((char *)wsgi_server_config->python_home);

        /* Initialise Python. */
        	initialized = 1;
        	Py_Initialize();
			
			PyType_Ready(&Log_Type);
			PyType_Ready(&Input_Type);
			PyType_Ready(&Adapter_Type);

        /* Initialise threading. */
        //PyEval_InitThreads();
        //PyThreadState_Swap(NULL);
    }
	return 0;
}

}

