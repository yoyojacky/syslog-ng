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
#include "compat/time.h"

#ifndef HAVE_LOCALTIME_R
#include <pthread.h>

pthread_mutex_t localtime_mutex = PTHREAD_MUTEX_INITIALIZER;

struct tm *
localtime_r(const time_t *timer, struct tm *result)
{
  pthread_mutex_lock(&localtime_mutex);
  struct tm *tmp = localtime(timer);

  if (tmp)
    {
      *result = *tmp;
      pthread_mutex_unlock(&localtime_mutex);
      return result;
    }
  pthread_mutex_unlock(&localtime_mutex);
  return tmp;
}
#endif
