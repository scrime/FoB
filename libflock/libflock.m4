dnl autoconf macros to check libflock installation
dnl Copyright (C) 2002 SCRIME, université Bordeaux 1
dnl
dnl Author: Anthony Beurivé <beurive@labri.u-bordeaux.fr>
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

define([DEFAULT_FLOCK_PREFIX], "/usr")

dnl --------------------------------------------------------------------------------
dnl FLOCK_CHECK_PREFIX

dnl This macro redefines the value of FLOCK_PREFIX, which represents the
dnl directory where libflock is to be searched for.  A configure option is 
dnl provided to specify a different directory for the search.

AC_DEFUN(FLOCK_CHECK_PREFIX,
[
  AC_ARG_WITH(flock-prefix,\
changequote(<<, >>)\
<<  --with-flock-prefix=DIR flock is in DIR subdirectories [default=>>DEFAULT_FLOCK_PREFIX],
changequote([, ])
  flock_prefix="$withval",
  flock_prefix=DEFAULT_FLOCK_PREFIX)

  AC_SUBST(flock_prefix)
])

dnl --------------------------------------------------------------------------------
dnl FLOCK_CHECK_LIB

dnl This macro defines a new variable, flock_library, which value is the
dnl directory where libflock was found, or "".  The value of LIBS and LDFLAGS
dnl are set accordingly.  A configure option is provided to specify a different
dnl directory for the search.

AC_DEFUN(FLOCK_CHECK_LIB,
[
  dnl flock library dir setting option.
  AC_ARG_WITH(flock-library,\
changequote(<<, >>)\
<<  --with-flock-library=DIR
                          flock library is in DIR [default=>>DEFAULT_FLOCK_PREFIX/lib],
changequote([, ])
  flock_library="$withval",
  flock_library=ifelse($flock_prefix, "", DEFAULT_FLOCK_PREFIX, $flock_prefix)/lib)

  AC_CHECK_LIB(flock, flock_init,, flock_library="", "-L$flock_library")

  dnl Checks for libflock.
  if test "$flock_library" != ""
  then
    (echo $LDFLAGS | grep -q "\-L$flock_library") || LDFLAGS="-L$flock_library $LDFLAGS"
  fi

  AC_SUBST(flock_library)
])

dnl --------------------------------------------------------------------------------
dnl FLOCK_CHECK_HEADERS

dnl This macro defines a new variable, flock_includes, which value is the
dnl directory where flock/flock.h was found, or "".  The value of CPPFLAGS is
dnl set accordingly.  A configure option is provided to specify a different
dnl directory for the search.

AC_DEFUN(FLOCK_CHECK_HEADERS,
[
  dnl flock include dir setting option.
  AC_ARG_WITH(flock-includes,\
changequote(<<, >>)\
<<  --with-flock-includes=DIR
                          flock include files are in DIR [default=>>DEFAULT_FLOCK_PREFIX<</include]
                          Looking there for flock/flock.h.>>,
changequote([, ])
  flock_includes="$withval",
  flock_includes=ifelse($flock_prefix, "", DEFAULT_FLOCK_PREFIX, $flock_prefix)/include)

  AC_MSG_CHECKING(for flock headers in $flock_includes)

  dnl Checks for flock/flock.h.
  if test -f "$flock_includes/flock/flock.h"
  then
    AC_MSG_RESULT(found)
    (echo $CPPFLAGS | grep -q "$flock_includes") || CPPFLAGS="-I$flock_includes $CPPFLAGS"
  else
    AC_MSG_RESULT(not found)
    flock_includes=""
  fi

  AC_SUBST(flock_includes)
])

