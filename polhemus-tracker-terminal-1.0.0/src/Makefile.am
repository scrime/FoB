AM_CPPFLAGS= \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_DATA_ROOT_DIR_PITERM=\""$(datarootdir)/PiTerm"\" \
	-DPACKAGE_LOCAL_STATE_DIR_PITERM=\""$(localstatedir)/PiTerm"\" \
	-DPACKAGE_SOURCE_DIR=\""$(abs_srcdir)"\" \
	@PACKAGE_CFLAGS@

bin_PROGRAMS=PiTerm 
dist_dataroot_DATA=PiTermIcon.png
dist_localstate_DATA=PiTerm_params.cnx
PiTerm_SOURCES= PingPong.cpp  PingPong.h  PiTerm.cpp  PiTerm.h  PiTracker.cpp  PiTracker.h




PiTerm_LDADD = @PACKAGE_LIBS@ -lpthread -lusb-1.0 $(INTLLIBS) 
