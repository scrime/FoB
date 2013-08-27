SUBDIRS = flock flockUtils tests

m4macros = cdebug.m4 cwarnings.m4 flock_report_errors.m4

EXTRA_DIST = SCRIME $(m4macros) libflock.m4 TODO THANKS

acinclude.m4: $(m4macros)
	cat $(m4macros) > acinclude.m4
