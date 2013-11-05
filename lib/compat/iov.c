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
#include "compat/iov.h"

#ifdef _WIN32

#ifndef SSIZE_MAX
#define SSIZE_MAX SHRT_MAX
#endif


ssize_t
readv(int fd, const struct iovec *vector, int count)
{
  size_t byte_size = 0;
  int i = 0;
  char *buffer = NULL;
  size_t to_copy = 0;
  char *data = NULL;
  ssize_t byte_read = 0;

  for ( i = 0; i < count; ++i)
    {
      if (SSIZE_MAX - byte_size < vector[i].iov_len)
        {
          SetLastError(EINVAL);
	  return -1;
        }
      byte_size += vector[i].iov_len;
    }

  buffer = _malloca(byte_size);

  byte_read = read(fd, buffer, byte_size);

  if(byte_read < 0)
    return -1;

  to_copy = byte_read;
  data = buffer;

  for (i = 0; i < count; ++i)
    {
       size_t copy = min (vector[i].iov_len, to_copy);
       memcpy(vector[i].iov_base, data, copy);
       to_copy -= copy;
       data -= copy;

       if (to_copy == 0)
         break;
    }
  _freea(buffer);
  return byte_read;
}

ssize_t
writev(int fd, const struct iovec *vector, int count)
{
  size_t byte_size = 0;
  int i = 0;
  char *buffer = NULL;
  size_t to_copy = 0;
  char *ret = NULL;
  ssize_t bytes_written = 0;

  for ( i = 0; i < count; ++i)
    {
      if (SSIZE_MAX - byte_size < vector[i].iov_len)
        {
          SetLastError(EINVAL);
	  return -1;
        }
      byte_size += vector[i].iov_len;
    }

  buffer = _malloca(byte_size);

  to_copy = byte_size;
  ret = buffer;

  for (i = 0; i < count; ++i)
    {
       size_t copy = min (vector[i].iov_len, to_copy);
       ret = memcpy(ret, vector[i].iov_base, copy);
       to_copy -= copy;
       ret += copy;
       if (to_copy == 0)
         break;
    }

  bytes_written = write(fd, buffer, byte_size);
  _freea(buffer);
  return bytes_written;
}

#endif
