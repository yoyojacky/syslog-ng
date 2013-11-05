compatincludedir		= ${pkgincludedir}/compat

compatinclude_HEADERS		= 	\
	lib/compat/compat.h		\
	lib/compat/getutent.h		\
	lib/compat/glib.h		\
	lib/compat/lfs.h		\
	lib/compat/pio.h		\
	lib/compat/socket.h		\
	lib/compat/string.h		\
	lib/compat/time.h		\
	lib/compat/win32.h

compat_sources			= 	\
	lib/compat/getutent.c		\
	lib/compat/clock_gettime.c	\
	lib/compat/inet_aton.c		\
	lib/compat/localtime_r.c	\
	lib/compat/memrchr.c		\
	lib/compat/nanosleep.c		\
	lib/compat/pio.c		\
	lib/compat/strcasestr.c		\
	lib/compat/strptime.c		\
	lib/compat/strtok_r.c

include lib/compat/tests/Makefile.am
