#ifndef EVENTLOG_TAGS_H_INCLUDED
#define EVENTLOG_TAGS_H_INCLUDED 1

#include "syslog-ng.h"

#include <evtlog.h>

EVTTAG *evt_tag_socket_error(const char *name, int value);

#endif
