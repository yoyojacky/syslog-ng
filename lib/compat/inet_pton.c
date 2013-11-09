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
#include "compat/socket.h"

#ifndef HAVE_INET_PTON
#include <string.h>
#include <stdlib.h>

int
inet_pton(int af, const char *src, void *dst)
{
  struct addrinfo hints, *res, *ressave;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = af;

  if (getaddrinfo(src, NULL, &hints, &res) != 0)
    {
      return -1;
    }

  ressave = res;

  while (res)
    {
      switch (af)
        {
        case AF_INET:
          memcpy(dst, &(((struct sockaddr_in *)res->ai_addr)->sin_addr), sizeof(struct in_addr));
          break;
        case AF_INET6:
          memcpy(dst, &(((struct sockaddr_in6 *)res->ai_addr)->sin6_addr), sizeof(struct in6_addr));
          break;
        default:
          abort();
          break;
        }
      res = res->ai_next;
    }

  freeaddrinfo(ressave);
  return 0;
}
#endif
