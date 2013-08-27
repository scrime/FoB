INCLUDES = -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include/ -I/usr/X11R6/include
CFLAGS = -L/usr/X11R6/lib

bin_PROGRAMS = flockOutput flockNoise flockMoves flockBumps \
               flockSpeed flockOSC netBump2Sound flockOSC_seb \
               flockVisu flockOSC_vinc

flockOutput_SOURCES = flockOutput.c
flockOutput_LDADD = ../flock/libflock.la

flockNoise_SOURCES = flockNoise.c
flockNoise_LDADD = ../flock/libflock.la
flockNoise_LDFLAGS = -lm

flockMoves_SOURCES = flockMoves.c
flockMoves_LDADD = ../flock/libflock.la

flockBumps_SOURCES = flockBumps.c
flockBumps_LDADD = ../flock/libflock.la

flockSpeed_SOURCES = flockSpeed.c
flockSpeed_LDADD = ../flock/libflock.la

flockOSC_SOURCES = flockOSC.c OSC.c OSC.h
flockOSC_LDADD = ../flock/libflock.la
flockOSC_LDFLAGS = -lm

netBump2Sound_SOURCES = netBump2Sound.c


flockOSC_seb_SOURCES = flockOSC_seb.c OSC.c OSC.h
flockOSC_seb_LDADD = ../flock/libflock.la
flockOSC_seb_LDFLAGS = -lm -lglib-2.0

flockOSC_vinc_SOURCES = flockOSC_vinc.c OSC.c OSC.h
flockOSC_vinc_LDADD = ../flock/libflock.la
flockOSC_vinc_LDFLAGS = -lm -lglib-2.0
flockVisu_SOURCES = flockVisu.c

#flockVisu_CFLAGS = -L/usr/X11R6/lib #-I/usr/X11R6/include
flockVisu_LDFLAGS = -lglut -lGLU -lGL -lm -lXmu -lglib-2.0 #-L/usr/X11R6/lib


# install-exec-local:
# 	chmod +s $(DESTDIR)$(bindir)/flockOutput
# 	chmod +s $(DESTDIR)$(bindir)/flockNoise
# 	chmod +s $(DESTDIR)$(bindir)/flockMoves
# 	chmod +s $(DESTDIR)$(bindir)/flockBumps
# 	chmod +s $(DESTDIR)$(bindir)/flockSpeed
# 	chmod +s $(DESTDIR)$(bindir)/flockOSC
# 	chmod +s $(DESTDIR)$(bindir)/flockOSC2
