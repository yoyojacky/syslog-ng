/*
 * Copyright (c) 2014 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2014 Gergely Nagy <algernon@balabit.hu>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "python-dest.h"
#include "python-globals.h"
#include "python-value-pairs.h"
#include "python-logmsg.h"
#include "logthrdestdrv.h"
#include "stats/stats.h"
#include "misc.h"

#ifndef SCS_PYTHON
#define SCS_PYTHON 0
#endif

typedef struct
{
  LogThrDestDriver super;

  gchar *class;
  GList *imports;

  LogTemplateOptions template_options;
  ValuePairs *vp;

  struct
  {
    PyObject *module;
    PyObject *class;
    PyObject *instance;
    PyObject *is_opened;
    PyObject *send;
  } py;
} PythonDestDriver;

/** Setters & config glue **/

void
python_dd_set_class(LogDriver *d, gchar *filename)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  g_free(self->class);
  self->class = g_strdup(filename);
}

void
python_dd_set_value_pairs(LogDriver *d, ValuePairs *vp)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  if (self->vp)
    value_pairs_free(self->vp);
  self->vp = vp;
}

void
python_dd_set_imports(LogDriver *d, GList *imports)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  string_list_free(self->imports);
  self->imports = imports;
}

LogTemplateOptions *
python_dd_get_template_options(LogDriver *d)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  return &self->template_options;
}

/** Helpers for stats & persist_name formatting **/

static gchar *
python_dd_format_stats_instance(LogThrDestDriver *d)
{
  PythonDestDriver *self = (PythonDestDriver *)d;
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name),
             "python,%s",
             self->class);
  return persist_name;
}

static gchar *
python_dd_format_persist_name(LogThrDestDriver *d)
{
  PythonDestDriver *self = (PythonDestDriver *)d;
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name),
             "python(%s)",
             self->class);
  return persist_name;
}

/** Python calling helpers **/
static const gchar *
_py_get_callable_name(PyObject *callable, gchar *buf, gsize buf_len)
{
  PyObject *name = PyObject_GetAttrString(callable, "__name__");

  if (name)
    {
      g_strlcpy(buf, PyString_AsString(name), buf_len);
    }
  else
    {
      PyErr_Clear();
      g_strlcpy(buf, "<unknown>", buf_len);
    }
  Py_XDECREF(name);
  return buf;
}

static const gchar *
_py_format_exception_text(gchar *buf, gsize buf_len)
{
  PyObject *exc, *value, *tb, *str;

  PyErr_Fetch(&exc, &value, &tb);
  if (!exc)
    {
      g_strlcpy(buf, "None", buf_len);
      return buf;
    }
  PyErr_NormalizeException(&exc, &value, &tb);

  str = PyObject_Str(value);
  if (str)
    {
      g_snprintf(buf, buf_len, "%s: %s", ((PyTypeObject *) exc)->tp_name, PyString_AsString(str));
    }
  else
    {
      g_strlcpy(buf, "<unknown>", buf_len);
    }
  Py_XDECREF(exc);
  Py_XDECREF(value);
  Py_XDECREF(tb);
  Py_XDECREF(str);
  return buf;
}

static PyObject *
_py_invoke_function(PythonDestDriver *self, PyObject *func, PyObject *arg)
{
  PyObject *ret;

  ret = PyObject_CallFunctionObjArgs(func, arg, NULL);
  if (!ret)
    {
      gchar buf1[256], buf2[256];

      msg_error("Exception while calling a Python function",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("script", self->class),
                evt_tag_str("function", _py_get_callable_name(func, buf1, sizeof(buf1))),
                evt_tag_str("exception", _py_format_exception_text(buf2, sizeof(buf2))),
                NULL);
      return NULL;
    }
  return ret;
}

static void
_py_invoke_void_function(PythonDestDriver *self, PyObject *func, PyObject *arg)
{
  PyObject *ret = _py_invoke_function(self, func, arg);
  Py_XDECREF(ret);
}

static gboolean
_py_invoke_bool_function(PythonDestDriver *self, PyObject *func, PyObject *arg)
{
  PyObject *ret;
  gboolean result = FALSE;

  ret = _py_invoke_function(self, func, arg);
  if (ret)
    result = PyObject_IsTrue(ret);
  Py_XDECREF(ret);
  return result;
}

static PyObject *
_py_do_import(PythonDestDriver *self, const gchar *modname)
{
  PyObject *module, *modobj;

  module = PyUnicode_FromString(modname);
  if (!module)
    {
      msg_error("Error allocating Python string",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("string", modname),
                NULL);
      return NULL;
    }

  modobj = PyImport_Import(module);
  Py_DECREF(module);
  if (!modobj)
    {
      gchar buf[256];

      msg_error("Error loading Python module",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("module", modname),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))),
                NULL);
      return NULL;
    }
  return modobj;
}

static void
_foreach_import(gpointer data, gpointer user_data)
{
  PythonDestDriver *self = (PythonDestDriver *) user_data;
  gchar *modname = (gchar *) data;
  PyObject *mod;

  mod = _py_do_import(self, modname);
  Py_XDECREF(mod);
}

static void
_py_perform_imports(PythonDestDriver *self)
{
  g_list_foreach(self->imports, _foreach_import, self);
}

static PyObject *
_py_get_attr_or_null(PyObject *o, const gchar *attr)
{
  PyObject *result;

  if (!attr)
    return NULL;

  result = PyObject_GetAttrString(o, attr);
  if (!result)
    {
      PyErr_Clear();
      return NULL;
    }
  return result;
}

static PyObject *
_py_get_method(PythonDestDriver *self, PyObject *o, const gchar *method_name)
{
  PyObject *method = _py_get_attr_or_null(self->py.instance, method_name);
  if (!method)
    {
      gchar buf[256];

      msg_error("Missing Python method in the driver class",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("method", method_name),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))),
                NULL);
      return NULL;
    }
  return method;
}

static void
_py_invoke_void_method_by_name(PythonDestDriver *self, PyObject *instance, const gchar *method_name)
{
  PyObject *method = _py_get_method(self, instance, method_name);
  if (method)
    {
      _py_invoke_void_function(self, method, NULL);
      Py_DECREF(method);
    }
}

static gboolean
_py_invoke_bool_method_by_name(PythonDestDriver *self, PyObject *instance, const gchar *method_name)
{
  gboolean result = FALSE;
  PyObject *method = _py_get_method(self, instance, method_name);

  if (method)
    {
      result = _py_invoke_bool_function(self, method, NULL);
      Py_DECREF(method);
    }
  return result;
}

static gboolean
_py_invoke_is_opened(PythonDestDriver *self)
{
  if (!self->py.is_opened)
    return TRUE;

  return _py_invoke_bool_function(self, self->py.is_opened, NULL);
}

static gboolean
_py_invoke_open(PythonDestDriver *self)
{
  return _py_invoke_bool_method_by_name(self, self->py.instance, "open");
}

static void
_py_invoke_close(PythonDestDriver *self)
{
  _py_invoke_void_method_by_name(self, self->py.instance, "close");
}

static gboolean
_py_invoke_send(PythonDestDriver *self, PyObject *dict)
{
  return _py_invoke_bool_function(self, self->py.send, dict);
}

static gboolean
_py_invoke_init(PythonDestDriver *self)
{
  return _py_invoke_bool_method_by_name(self, self->py.instance, "init");
}

static void
_py_invoke_deinit(PythonDestDriver *self)
{
  _py_invoke_void_method_by_name(self, self->py.instance, "deinit");
}

static gboolean
_split_fully_qualified_name(const gchar *input, gchar **module, gchar **class)
{
  const gchar *p;

  for (p = input + strlen(input) - 1; p > input && *p != '.'; p--)
    ;

  if (p > input)
    {
      *module = g_strndup(input, (p - input));
      *class = g_strdup(p + 1);
      return TRUE;
    }
  return FALSE;
}

static gboolean
_py_init_bindings_from_module_and_class(PythonDestDriver *self, const gchar *module, const gchar *class)
{
  self->py.module = _py_do_import(self, module);
  if (!self->py.module)
    return FALSE;

  self->py.class = _py_get_attr_or_null(self->py.module, class);
  if (!self->py.class)
    return FALSE;

  self->py.instance = _py_invoke_function(self, self->py.class, NULL);
  if (!self->py.instance)
    {
      gchar buf[256];

      msg_error("Error instantiating Python driver class",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("module", module),
                evt_tag_str("class", class),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))),
                NULL);
      return FALSE;
    }

  /* these are fast paths, store references to be faster */
  self->py.is_opened = _py_get_attr_or_null(self->py.instance, "is_opened");
  self->py.send = _py_get_attr_or_null(self->py.instance, "send");
  if (!self->py.send)
    {
      msg_error("Error initializing Python destination, class does not have a send() method",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("module", module),
                evt_tag_str("class", class),
                NULL);
    }
  return self->py.send != NULL;
}

static gboolean
_py_init_bindings(PythonDestDriver *self)
{
  gchar *module, *class;
  gboolean result;

  if (!_split_fully_qualified_name(self->class, &module, &class))
    return FALSE;

  result = _py_init_bindings_from_module_and_class(self, module, class);
  g_free(module);
  g_free(class);
  return result;
}

static void
_py_free_bindings(PythonDestDriver *self)
{
  Py_CLEAR(self->py.module);
  Py_CLEAR(self->py.class);
  Py_CLEAR(self->py.instance);
  Py_CLEAR(self->py.is_opened);
  Py_CLEAR(self->py.send);
}

static gboolean
_py_init_object(PythonDestDriver *self)
{
  if (!_py_invoke_init(self))
    {
      msg_error("Error initializing Python driver object, init() returned FALSE",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("class", self->class),
                NULL);
      return FALSE;
    }
  return TRUE;
}

static worker_insert_result_t
python_dd_insert(LogThrDestDriver *d, LogMessage *msg)
{
  PythonDestDriver *self = (PythonDestDriver *)d;
  gboolean success;
  PyObject *msg_object;
  PyGILState_STATE gstate;

  gstate = PyGILState_Ensure();
  if (!_py_invoke_is_opened(self))
    {
      return WORKER_INSERT_RESULT_NOT_CONNECTED;
    }
  if (self->vp)
    {
      success = py_value_pairs_apply(self->vp, &self->template_options, self->super.seq_num, msg, &msg_object);
      if (!success && (self->template_options.on_error & ON_ERROR_DROP_MESSAGE))
        {
          goto exit;
        }
    }
  else
    {
      msg_object = py_log_message_new(msg);
    }

  success = _py_invoke_send(self, msg_object);
  if (!success)
    {
      msg_error("Python send() method returned failure, suspending destination for time_reopen()",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("class", self->class),
                evt_tag_int("time_reopen", self->super.time_reopen),
                NULL);
    }

 exit:
  PyGILState_Release(gstate);
  if (success)
    return WORKER_INSERT_RESULT_SUCCESS;
  else
    return WORKER_INSERT_RESULT_ERROR;
}

static void
python_dd_open(PythonDestDriver *self)
{
  PyGILState_STATE gstate;

  gstate = PyGILState_Ensure();
  if (!_py_invoke_is_opened(self))
    _py_invoke_open(self);

  PyGILState_Release(gstate);
}

static void
python_dd_close(PythonDestDriver *self)
{
  PyGILState_STATE gstate;

  gstate = PyGILState_Ensure();
  if (_py_invoke_is_opened(self))
    _py_invoke_close(self);
  PyGILState_Release(gstate);
}

static void
python_dd_worker_init(LogThrDestDriver *d)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  python_dd_open(self);
}

static void
python_dd_worker_deinit(LogThrDestDriver *d)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  python_dd_close(self);
}

static void
python_dd_disconnect(LogThrDestDriver *d)
{
  PythonDestDriver *self = (PythonDestDriver *) d;

  python_dd_close(self);
}

static gboolean
python_dd_init(LogPipe *d)
{
  PythonDestDriver *self = (PythonDestDriver *)d;
  GlobalConfig *cfg = log_pipe_get_config(d);
  PyGILState_STATE gstate;

  if (!self->class)
    {
      msg_error("Error initializing Python destination: no script specified!",
                evt_tag_str("driver", self->super.super.super.id),
                NULL);
      return FALSE;
    }

  if (!log_dest_driver_init_method(d))
    return FALSE;

  log_template_options_init(&self->template_options, cfg);
  self->super.time_reopen = 1;

  gstate = PyGILState_Ensure();

  _py_perform_imports(self);
  if (!_py_init_bindings(self) ||
      !_py_init_object(self))
    goto fail;

  PyGILState_Release(gstate);

  msg_verbose("Python destination initialized",
              evt_tag_str("driver", self->super.super.super.id),
              evt_tag_str("class", self->class),
              NULL);

  return log_threaded_dest_driver_start(d);

 fail:
  PyGILState_Release(gstate);
  return FALSE;
}

static gboolean
python_dd_deinit(LogPipe *d)
{
  PythonDestDriver *self = (PythonDestDriver *)d;
  PyGILState_STATE gstate;

  gstate = PyGILState_Ensure();
  _py_invoke_deinit(self);
  PyGILState_Release(gstate);

  return log_threaded_dest_driver_deinit_method(d);
}

static void
python_dd_free(LogPipe *d)
{
  PythonDestDriver *self = (PythonDestDriver *)d;
  PyGILState_STATE gstate;

  log_template_options_destroy(&self->template_options);

  gstate = PyGILState_Ensure();
  _py_free_bindings(self);
  PyGILState_Release(gstate);

  g_free(self->class);

  if (self->vp)
    value_pairs_free(self->vp);

  log_threaded_dest_driver_free(d);
}

LogDriver *
python_dd_new(GlobalConfig *cfg)
{
  PythonDestDriver *self = g_new0(PythonDestDriver, 1);

  log_threaded_dest_driver_init_instance(&self->super, cfg);
  log_template_options_defaults(&self->template_options);

  self->super.super.super.super.init = python_dd_init;
  self->super.super.super.super.deinit = python_dd_deinit;
  self->super.super.super.super.free_fn = python_dd_free;

  self->super.worker.thread_init = python_dd_worker_init;
  self->super.worker.thread_deinit = python_dd_worker_deinit;
  self->super.worker.disconnect = python_dd_disconnect;
  self->super.worker.insert = python_dd_insert;

  self->super.format.stats_instance = python_dd_format_stats_instance;
  self->super.format.persist_name = python_dd_format_persist_name;
  self->super.stats_source = SCS_PYTHON;

  return (LogDriver *)self;
}