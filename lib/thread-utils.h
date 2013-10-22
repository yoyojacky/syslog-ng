#ifndef THREAD_UTILS_H_INCLUDED
#define THREAD_UTILS_H_INCLUDED

#include "syslog-ng.h"
#include <pthread.h>

#ifdef _WIN32
typedef DWORD ThreadId;
#else
typedef pthread_t ThreadId;
#endif

static inline ThreadId
get_thread_id()
{
#ifndef _WIN32
  return pthread_self();
#else
  return GetCurrentThreadId();
#endif
}

static inline int
threads_equal(ThreadId thread_a, ThreadId thread_b)
{
#ifndef _WIN32
  return pthread_equal(thread_a, thread_b);
#else
  return thread_a == thread_b;
#endif
}

#endif
