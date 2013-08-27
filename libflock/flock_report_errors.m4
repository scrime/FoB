dnl autoconf macro to set a preprocessor variable for error printing
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

dnl --------------------------------------------------------------------------------
dnl FLOCK_REPORT_ERRORS

dnl This macro defines a preprocessor variable, FLOCK_REPORT_ERRORS,
dnl which can be set by a configure option.  By default, the variable is
dnl not set.

AC_DEFUN(FLOCK_REPORT_ERRORS_CHECK,
[
  define([ENABLE_FLOCK_REPORT_ERRORS_DEFAULT], ifelse($1, yes, yes, no))

  AC_MSG_CHECKING(flock-report-errors)

  AC_ARG_ENABLE(flock-report-errors,\
changequote(<<, >>)\
<<  --enable-flock-report-errors
                          set preprocessor variable for error printing [default=>>ENABLE_FLOCK_REPORT_ERRORS_DEFAULT],
changequote([, ])
  [
    case "${enableval}" in
      yes | no)
        flock_report_errors=${enableval}
        ;;
      *)
        AC_MSG_ERROR(bad value ${enableval} for --enable-flock-report-errors)
        ;;
    esac],
  flock_report_errors=ENABLE_FLOCK_REPORT_ERRORS_DEFAULT)

  if test "$flock_report_errors" = "yes"
  then
    AC_DEFINE(FLOCK_REPORT_ERRORS)
  fi

  AC_MSG_RESULT($flock_report_errors)
])
