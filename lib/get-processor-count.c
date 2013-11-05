#include "get-processor-count.h"

#ifndef _WIN32
gint
get_processor_count(void)
{
#ifdef _SC_NPROCESSORS_ONLN
  return sysconf(_SC_NPROCESSORS_ONLN);
#else
  return -1;
#endif /*_SC_NPROCESSORS_ONLN*/
}

#else

gint
get_processor_count(void)
{
  SYSTEM_INFO system_info;
  GetSystemInfo(&system_info);
  return (gint) system_info.dwNumberOfProcessors;
}
#endif
