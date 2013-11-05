#include <evtlog.h>

EVTTAG *
evt_tag_socket_error(const char *name, int value)
{
#ifdef __WIN32
  return evt_tag_win32_error(name, value);
#else
  return evt_tag_errno(name, value);
#endif
}
