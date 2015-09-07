/*
 * Copyright (c) 2015 BalaBit
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

#include "correllate-parser.h"
#include "correllation.h"
#include "correllation-context.h"
#include "synthetic-message.h"
#include "messages.h"
#include "misc.h"
#include <iv.h>

typedef struct _CorrellateParser
{
  StatefulParser super;
  GStaticMutex lock;
  struct iv_timer tick;
  TimerWheel *timer_wheel;
  GTimeVal last_tick;
  CorrellationState *correllation;
  LogTemplate *context_id_template;
  gint context_timeout;
  gint context_scope;
  SyntheticMessage *synthetic_message;
} CorrellateParser;

static NVHandle context_id_handle = 0;

void
correllate_set_context_id_template(LogParser *s, LogTemplate *context_id_template)
{
  CorrellateParser *self = (CorrellateParser *) s;

  log_template_unref(self->context_id_template);
  self->context_id_template = log_template_ref(context_id_template);
}

void
correllate_set_context_timeout(LogParser *s, gint context_timeout)
{
  CorrellateParser *self = (CorrellateParser *) s;

  self->context_timeout = context_timeout;
}

void
correllate_set_synthetic_message(LogParser *s, SyntheticMessage *message)
{
  CorrellateParser *self = (CorrellateParser *) s;

  if (self->synthetic_message)
    synthetic_message_free(self->synthetic_message);
  self->synthetic_message = message;
}



/* NOTE: lock should be acquired for writing before calling this function. */
void
correllate_set_time(CorrellateParser *self, const LogStamp *ls)
{
  GTimeVal now;

  /* clamp the current time between the timestamp of the current message
   * (low limit) and the current system time (high limit).  This ensures
   * that incorrect clocks do not skew the current time know by the
   * correllation engine too much. */

  cached_g_current_time(&now);
  self->last_tick = now;

  if (ls->tv_sec < now.tv_sec)
    now.tv_sec = ls->tv_sec;

  timer_wheel_set_time(self->timer_wheel, now.tv_sec);
  msg_debug("Advancing correllate() current time because of an incoming message",
            evt_tag_long("utc", timer_wheel_get_time(self->timer_wheel)),
            NULL);
}

/*
 * This function can be called any time when pattern-db is not processing
 * messages, but we expect the correllation timer to move forward.  It
 * doesn't need to be called absolutely regularly as it'll use the current
 * system time to determine how much time has passed since the last
 * invocation.  See the timing comment at pattern_db_process() for more
 * information.
 */
void
_correllate_timer_tick(CorrellateParser *self)
{
  GTimeVal now;
  glong diff;

  g_static_mutex_lock(&self->lock);
  cached_g_current_time(&now);
  diff = g_time_val_diff(&now, &self->last_tick);

  if (diff > 1e6)
    {
      glong diff_sec = diff / 1e6;

      timer_wheel_set_time(self->timer_wheel, timer_wheel_get_time(self->timer_wheel) + diff_sec);
      msg_debug("Advancing correllate() current time because of timer tick",
                evt_tag_long("utc", timer_wheel_get_time(self->timer_wheel)),
                NULL);
      /* update last_tick, take the fraction of the seconds not calculated into this update into account */

      self->last_tick = now;
      g_time_val_add(&self->last_tick, -(diff - diff_sec * 1e6));
    }
  else if (diff < 0)
    {
      /* time moving backwards, this can only happen if the computer's time
       * is changed.  We don't update patterndb's idea of the time now, wait
       * another tick instead to update that instead.
       */
      self->last_tick = now;
    }
  g_static_mutex_unlock(&self->lock);
}

static void
correllate_timer_tick(gpointer s)
{
  CorrellateParser *self = (CorrellateParser *) s;

  _correllate_timer_tick(self);
  iv_validate_now();
  self->tick.expires = iv_now;
  self->tick.expires.tv_sec++;
  iv_timer_register(&self->tick);
}

static void
correllate_emit_synthetic(CorrellateParser *self, CorrellationContext *context)
{
  GString *buffer = g_string_sized_new(256);
  LogMessage *msg;

  /* FIXME: RAC_MSG_ should be part of synthetic-message and be configurable */
  msg = synthetic_message_generate_with_context(self->synthetic_message, RAC_MSG_INHERIT_CONTEXT, context, buffer);
  stateful_parser_emit_synthetic(&self->super, msg);
  log_msg_unref(msg);
  g_string_free(buffer, TRUE);
}

static void
correllate_expire_entry(TimerWheel *wheel, guint64 now, gpointer user_data)
{
  CorrellationContext *context = user_data;
  CorrellateParser *self = (CorrellateParser *) timer_wheel_get_associated_data(wheel);

  msg_debug("Expiring correllate() correllation context",
            evt_tag_long("utc", timer_wheel_get_time(wheel)),
            NULL);
  correllate_emit_synthetic(self, context);
  g_hash_table_remove(self->correllation->state, &context->key);

  /* correllation_context_free is automatically called when returning from
     this function by the timerwheel code as a destroy notify
     callback. */
}


static gchar *
correllate_format_persist_name(CorrellateParser *self)
{
  static gchar persist_name[512];

  g_snprintf(persist_name, sizeof(persist_name), "correllation()");
  return persist_name;
}

static gboolean
correllate_with_state(CorrellateParser *self, LogMessage *msg)
{
  GString *buffer = g_string_sized_new(32);
  CorrellationContext *context = NULL;

  g_static_mutex_lock(&self->lock);
  correllate_set_time(self, &msg->timestamps[LM_TS_STAMP]);
  if (self->context_id_template)
    {
      CorrellationKey key;

      log_template_format(self->context_id_template, msg, NULL, LTZ_LOCAL, 0, NULL, buffer);
      log_msg_set_value(msg, context_id_handle, buffer->str, -1);

      correllation_key_setup(&key, self->context_scope, msg, buffer->str);
      context = g_hash_table_lookup(self->correllation->state, &key);
      if (!context)
        {
          msg_debug("Correllation context lookup failure, starting a new context",
                    evt_tag_str("context", buffer->str),
                    evt_tag_int("context_timeout", self->context_timeout),
                    evt_tag_int("context_expiration", timer_wheel_get_time(self->timer_wheel) + self->context_timeout),
                    NULL);
          context = correllation_context_new(&key);
          g_hash_table_insert(self->correllation->state, &context->key, context);
          g_string_steal(buffer);
        }
      else
        {
          msg_debug("Correllation context lookup successful",
                    evt_tag_str("context", buffer->str),
                    evt_tag_int("context_timeout", self->context_timeout),
                    evt_tag_int("context_expiration", timer_wheel_get_time(self->timer_wheel) + self->context_timeout),
                    evt_tag_int("num_messages", context->messages->len),
                    NULL);
        }

      g_ptr_array_add(context->messages, log_msg_ref(msg));

      if (context->timer)
        {
          timer_wheel_mod_timer(self->timer_wheel, context->timer, self->context_timeout);
        }
      else
        {
          context->timer = timer_wheel_add_timer(self->timer_wheel, self->context_timeout, correllate_expire_entry, correllation_context_ref(context), (GDestroyNotify) correllation_context_unref);
        }
    }
  else
    {
      context = NULL;
    }

  g_static_mutex_unlock(&self->lock);

  if (context)
    log_msg_write_protect(msg);

  g_string_free(buffer, TRUE);
  return TRUE;
}

static gboolean
correllate_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const char *input, gsize input_len)
{
  CorrellateParser *self = (CorrellateParser *) s;

  log_msg_make_writable(pmsg, path_options);
  return correllate_with_state(self, *pmsg);
}

static gboolean
correllate_init(LogPipe *s)
{
  CorrellateParser *self = (CorrellateParser *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  self->correllation = cfg_persist_config_fetch(cfg, correllate_format_persist_name(self));
  if (!self->correllation)
    {
      self->correllation = correllation_state_new();
    }
  iv_validate_now();
  IV_TIMER_INIT(&self->tick);
  self->tick.cookie = self;
  self->tick.handler = correllate_timer_tick;
  self->tick.expires = iv_now;
  self->tick.expires.tv_sec++;
  self->tick.expires.tv_nsec = 0;
  iv_timer_register(&self->tick);
  return TRUE;
}

static gboolean
correllate_deinit(LogPipe *s)
{
  CorrellateParser *self = (CorrellateParser *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (iv_timer_registered(&self->tick))
    {
      iv_timer_unregister(&self->tick);
    }

  cfg_persist_config_add(cfg, correllate_format_persist_name(self), self->correllation, (GDestroyNotify) correllation_state_free, FALSE);
  self->correllation = NULL;
  return TRUE;
}

static LogPipe *
correllate_clone(LogPipe *s)
{
  LogParser *clone;
  CorrellateParser *self = (CorrellateParser *) s;

  /* FIXME: share state between clones! */
  clone = correllate_new(s->cfg);
  correllate_set_context_id_template(clone, self->context_id_template);
  correllate_set_context_timeout(clone, self->context_timeout);
  return &clone->super;
}

static void
correllate_free(LogPipe *s)
{
  CorrellateParser *self = (CorrellateParser *) s;

  g_static_mutex_free(&self->lock);
  log_template_unref(self->context_id_template);
  if (self->synthetic_message)
    synthetic_message_free(self->synthetic_message);
  timer_wheel_free(self->timer_wheel);
  stateful_parser_free_method(s);
}

LogParser *
correllate_new(GlobalConfig *cfg)
{
  CorrellateParser *self = g_new0(CorrellateParser, 1);

  stateful_parser_init_instance(&self->super, cfg);
  self->super.super.super.free_fn = correllate_free;
  self->super.super.super.init = correllate_init;
  self->super.super.super.deinit = correllate_deinit;
  self->super.super.super.clone = correllate_clone;
  self->super.super.process = correllate_process;
  g_static_mutex_init(&self->lock);
  self->context_scope = RCS_GLOBAL;
  self->timer_wheel = timer_wheel_new();
  timer_wheel_set_associated_data(self->timer_wheel, self, NULL);
  cached_g_current_time(&self->last_tick);
  return &self->super.super;
}

void
correllate_global_init(void)
{
  context_id_handle = log_msg_get_value_handle(".classifier.context_id");
}
