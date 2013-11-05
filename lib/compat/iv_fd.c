/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#include "compat/iv_fd.h"

#ifdef _WIN32

struct iv_fd_thr_info
{
  struct iv_fd  *handled_socket;
};

static struct iv_tls_user iv_fd_tls_user =
{
  .sizeof_state = sizeof(struct iv_fd_thr_info),
};

static void iv_fd_tls_init(void) __attribute__((constructor));
static void iv_fd_tls_init(void)
{
  iv_tls_user_register(&iv_fd_tls_user);
}

static int iv_fd_set_event_mask(struct iv_fd *this)
{
  if (this->fd == INVALID_SOCKET)
    return -1;

  return WSAEventSelect(this->fd, this->handle.handle,
           this->event_mask);
}

static void iv_fd_got_event(void *_s)
{
  struct iv_fd_thr_info *tinfo =
    iv_tls_user_ptr(&iv_fd_tls_user);
  struct iv_fd *this = (struct iv_fd *)_s;
  WSANETWORKEVENTS ev;
  int ret;
  int i;

  ret = WSAEnumNetworkEvents(this->fd, this->handle.handle, &ev);
  if (ret) {
    iv_fatal("iv_fd_got_event: WSAEnumNetworkEvents "
       "returned %d", ret);
  }

  tinfo->handled_socket = this;
  for (i = 0; i < FD_MAX_EVENTS; i++) {
    if (ev.lNetworkEvents & (1 << i)) {
      this->handler[i](this->handle.cookie, i, ev.iErrorCode[i]);
      if (tinfo->handled_socket == this && ev.iErrorCode[i] != 0 && this->handler_err)
        {
          this->handler_err(this->cookie);
        }
      if (tinfo->handled_socket == NULL)
        return;
    }
  }
  tinfo->handled_socket = NULL;

  iv_fd_set_event_mask(this);
}

int _iv_fd_register(struct iv_fd *this)
{
  HANDLE hnd;
  int i;

  hnd = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (hnd == NULL)
    return -1;

  IV_HANDLE_INIT(&this->handle);
  this->handle.handle = hnd;
  this->handle.cookie = this;
  this->handle.handler = iv_fd_got_event;
  this->event_mask = 0;

  for (i = 0; i < FD_MAX_EVENTS; i++) {
    if (this->handler[i] != NULL)
      this->event_mask |= 1 << i;
  }
  /*
   * Call WSAEventSelect() even if the event mask is zero,
   * as it implicitly sets the socket to nonblocking mode.
   */
  if (iv_fd_set_event_mask(this) != 0)
    {
      CloseHandle(this->handle.handle);
      return -1;
    }

  iv_handle_register(&this->handle);
  return 0;
}

void iv_fd_register(struct iv_fd *this)
{
  if (_iv_fd_register(this))
    iv_fatal("iv_fd_regiter: can't create event for socket");
  return;
}

int iv_fd_register_try(struct iv_fd *this)
{
  return _iv_fd_register(this);
}

int iv_fd_registered(struct iv_fd *this)
{
  return iv_handle_registered(&this->handle);
}

void iv_fd_unregister(struct iv_fd *this)
{
  struct iv_fd_thr_info *tinfo =
    iv_tls_user_ptr(&iv_fd_tls_user);

  if (tinfo->handled_socket == this)
    tinfo->handled_socket = NULL;

  if (this->event_mask) {
    this->event_mask = 0;
    iv_fd_set_event_mask(this);
    this->handler_err = NULL;
  }

  iv_handle_unregister(&this->handle);
  CloseHandle(this->handle.handle);
}

void iv_fd_set_handler(struct iv_fd *this, int event,
             void (*handler)(void *, int, int))
{
  struct iv_fd_thr_info *tinfo =
    iv_tls_user_ptr(&iv_fd_tls_user);
  long old_mask;

  if (event >= FD_MAX_EVENTS) {
    iv_fatal("iv_fd_set_handler: called with "
       "event == %d", event);
  }

  old_mask = this->event_mask;
  if (this->handler[event] == NULL && handler != NULL)
    this->event_mask |= 1 << event;
  else if (this->handler[event] != NULL && handler == NULL)
    this->event_mask &= ~(1 << event);

  this->handler[event] = handler;

  if (tinfo->handled_socket != this && old_mask != this->event_mask)
    iv_fd_set_event_mask(this);
}

void IV_FD_INIT(struct iv_fd *this)
{
  int i;

  this->fd = INVALID_SOCKET;
  this->cookie = NULL;
  for (i = 0; i < FD_MAX_EVENTS; i++)
    this->handler[i] = NULL;
  IV_HANDLE_INIT(&this->handle);
}

static void iv_sock_handler_in(void *cookie, int event, int error)
{
  struct iv_fd *sock = cookie;

  sock->handler_in(sock->cookie);
}


void iv_fd_set_handler_in(struct iv_fd *this, void (*handler_in)(void *))
{
  void (*h)(void *, int, int);

  this->handler_in = handler_in;

  h = (handler_in != NULL) ? iv_sock_handler_in : NULL;
  iv_fd_set_handler(this, FD_READ_BIT, h);
  iv_fd_set_handler(this, FD_OOB_BIT, h);
  iv_fd_set_handler(this, FD_ACCEPT_BIT, h);
}

static void iv_sock_handler_out(void *cookie, int event, int error)
{
  struct iv_fd *sock = cookie;

  sock->handler_out(sock->cookie);
}


void iv_fd_set_handler_out(struct iv_fd * this, void (*handler_out)(void *))
{
  void (*h)(void *, int, int);

  this->handler_out = handler_out;

  h = (handler_out != NULL) ? iv_sock_handler_out : NULL;
  iv_fd_set_handler(this, FD_WRITE_BIT, h);
  iv_fd_set_handler(this, FD_CONNECT_BIT, h);
}

void iv_sock_handler_err(void *cookie, int event, int error)
{
  struct iv_fd *sock = cookie;

  sock->handler_err(sock->cookie);
}

void iv_fd_set_handler_err(struct iv_fd *this, void (*handler_err)(void *))
{
  void (*h)(void *, int, int);

  this->handler_err = handler_err;

  h = (handler_err != NULL) ? iv_sock_handler_err : NULL;
  iv_fd_set_handler(this, FD_CLOSE_BIT, h);
}

#endif
