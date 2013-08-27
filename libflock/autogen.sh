#!/bin/sh

m4macros="cdebug.m4 cwarnings.m4 flock_report_errors.m4"

cat $m4macros > acinclude.m4
aclocal
autoheader
libtoolize --automake
automake --add-missing --gnu > /dev/null 2>&1
autoconf
