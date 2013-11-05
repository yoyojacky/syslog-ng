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
#ifndef COMPAT_IV_FD_H_INCLUDED
#define COMPAT_IV_FD_H_INCLUDED 1

#include "compat/compat.h"

#ifdef _WIN32

struct iv_fd {
  SOCKET  fd;
  void    *cookie;
  void    (*handler[FD_MAX_EVENTS])(void *, int, int);
  struct iv_handle handle;
  void    (*handler_in)(void *cookie);
  void    (*handler_out)(void *cookie);
  void    (*handler_err)(void *cookie);
  long    event_mask;
};

void IV_FD_INIT(struct iv_fd *);
void iv_fd_register(struct iv_fd *);
int iv_fd_register_try(struct iv_fd *);
void iv_fd_unregister(struct iv_fd *);
int iv_fd_registered(struct iv_fd *);
void iv_fd_set_handler_in(struct iv_fd *, void (*)(void *));
void iv_fd_set_handler_out(struct iv_fd *, void (*)(void *));
void iv_fd_set_handler_err(struct iv_fd *, void (*)(void *));

#endif

#endif
