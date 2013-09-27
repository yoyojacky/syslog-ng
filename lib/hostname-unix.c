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
#include "hostname.h"
#include <netdb.h>

gchar *
get_dnsname(void)
{
  struct hostent *host = NULL;
  gchar *result = NULL;

  G_LOCK(resolv_lock);
  host = gethostbyname(get_cached_longhostname());
  if (host)
    {
       result = g_strdup(host->h_name);
    }
  G_UNLOCK(resolv_lock);
  return result;
}
