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
#ifndef COMPAT_TIME_H_INCLUDED
#define COMPAT_TIME_H_INCLUDED 1

#include "compat/compat.h"

#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC CLOCK_REALTIME
#endif

#ifdef _WIN32
#define sleep(x) Sleep(x * 1000)
#endif

#ifndef HAVE_STRPTIME
char *strptime(const char *buf, const char *fmt, struct tm *tm);
#endif

#ifndef HAVE_CLOCK_GETTIME

#define CLOCK_REALTIME 1

int clock_gettime(int clk_id, struct timespec *tp);
#endif

#ifndef HAVE_LOCALTIME_R
struct tm *localtime_r(const time_t *timer, struct tm *result);
#endif

#ifndef HAVE_NANOSLEEP
int nanosleep(const struct timespec *request, struct timespec *remain);
#endif

#endif
