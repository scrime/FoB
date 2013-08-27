/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flock_error.h - description of flock error codes.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef __FLOCK_ERROR_H__
#define __FLOCK_ERROR_H__

#include <limits.h>

#include <flock/flock.h>

/* Several functions in the library put an error code in a flock_t
   data structure when something goes wrong.  The following function
   returns the error code of the last operation on a flock. */
extern int flock_error (flock_t flock);

/* Returns a string describing the given flock error code. */
extern const char * flock_strerror (int error_code);

#define FLOCK_NO_ERROR 0

/* The error code when a system call has just failed in the library.
   Generally, this means that 'errno' should be examined. */
#define FLOCK_SYSTEM_CALL_ERROR (INT_MAX)

/* The error code associated to real but currently unimplemented flock
   errors. */
#define FLOCK_UNIMPLEMENTED_ERROR (INT_MAX - 1)

#endif
