sources = flock.c flock_bird.c flock_bird_record.c flock_command.c flock_error.c \
	  flock_mode.c flock_hl.c

flock_headers = flock.h flock_bird_record.h flock_command.h flock_error.h \
		flock_mode.h flock_hl.h

headers = $(flock_headers) flock_private.h flock_common.h flock_bird.h \
	  flock_bird_private.h flock_command_private.h

lib_LTLIBRARIES = libflock.la
flockincludedir = $(includedir)/flock
flockinclude_HEADERS = $(flock_headers)

libflock_la_SOURCES = $(sources) $(headers)
libflock_la_LDFLAGS = -version-info 0:3:0

EXTRA_DIST =
