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

#ifdef _WIN32

#define MAX_SLEEP_IN_MS         4294967294UL

#define POW10_2     INT64_C(100)
#define POW10_3     INT64_C(1000)
#define POW10_4     INT64_C(10000)
#define POW10_6     INT64_C(1000000)
#define POW10_7     INT64_C(10000000)
#define POW10_9     INT64_C(1000000000)


int nanosleep(const struct timespec *request, struct timespec *remain)
{
    unsigned long ms, rc = 0;
    unsigned __int64 u64, want, real;

    union {
        unsigned __int64 ns100;
        FILETIME ft;
    }  _start, _end;

    if (request->tv_sec < 0 || request->tv_nsec < 0 || request->tv_nsec >= POW10_9) {
        errno = EINVAL;
        return -1;
    }

    if (remain != NULL) GetSystemTimeAsFileTime(&_start.ft);

    want = u64 = request->tv_sec * POW10_3 + request->tv_nsec / POW10_6;
    while (u64 > 0 && rc == 0) {
        if (u64 >= MAX_SLEEP_IN_MS) ms = MAX_SLEEP_IN_MS;
        else ms = (unsigned long) u64;

        u64 -= ms;
        rc = SleepEx(ms, TRUE);
    }

    if (rc != 0) { /* WAIT_IO_COMPLETION */
        if (remain != NULL) {
            GetSystemTimeAsFileTime(&_end.ft);
            real = (_end.ns100 - _start.ns100) / POW10_4;

            if (real >= want) u64 = 0;
            else u64 = want - real;

            remain->tv_sec = u64 / POW10_3;
            remain->tv_nsec = (long) (u64 % POW10_3) * POW10_6;
        }

        errno = EINTR;
        return -1;
    }

    return 0;
}
#endif
