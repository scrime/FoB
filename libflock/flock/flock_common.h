/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flock_common.h - common library definitions.

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

#ifndef __FLOCK_COMMON_H__
#define __FLOCK_COMMON_H__

#include <config.h>

#ifdef FLOCK_REPORT_ERRORS
#define REPORT(e) (e)
#else
#define REPORT(e)
#endif

#define NANOSLEEP(s, ns)                                        \
{                                                               \
  struct timespec tspec;                                        \
  struct timespec rem;                                          \
  tspec.tv_sec = s;                                             \
  tspec.tv_nsec = ns;                                           \
  while ((nanosleep (&tspec, &rem) == -1) &&                    \
         (errno == EINTR))                                      \
    tspec = rem;                                                \
}

#endif
