/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flock_error.c - description of flock error codes.

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

#include <stdio.h>
#include <assert.h>

#include "flock_common.h"

#include "flock_private.h"
#include "flock_error.h"

/* FIXME: current implementation only deals with errors due to system
   calls. */

int
flock_error (flock_t flock)
{
  int error;

  assert (flock);

  error = flock->error;
  flock->error = FLOCK_NO_ERROR;
  return error;
}

static const char * flock_errors[] = {
  "no error",
  "ram failure",
  "non-volatile storage write failure",
  "PCB configuration data corrupt",
  "transmitter configuration data corrupt",
  "sensor configuration data corrupt",
  "invalid RS232 command",
  "not an FBB master",
  "no birds accessible in device list",
  "bird is not initialized",
  "FBB receive error - intra bird bus",
  "RS232 receive overrun on framing error",
  "FBB receive error - FBB host bus",
  "no FBB command response",
  "invalid FBB host command",
  "FBB run time error",
  "invalid CPU speed",
  "no data error",
  "illegal baud rate error",
  "slave acknowledge error",
  "unused_INT4",
  "unused_INT5",
  "unused_INT6",
  "unused_INT7",
  "unused_INT9",
  "unused_INT10",
  "unused_INT11",
  "unused_INT16",
  "CRT synchronization error",
  "transmitter not accessible error",
  "extended range transmitter not attached error",
  "CPU time overflow error",
  "sensor saturated error",
  "slave configuration error",
  "watch dog error",
  "over temperature error"
};

static const char * flock_system_call_error = "system call error";
static const char * flock_unimplemented_error = "unimplemented error";

const char *
flock_strerror (int error_code)
{
  if (error_code == FLOCK_SYSTEM_CALL_ERROR)
    return flock_system_call_error;

  if (error_code == FLOCK_UNIMPLEMENTED_ERROR)
    return flock_unimplemented_error;

  if (error_code < 0 ||
      error_code >= sizeof (flock_errors) / sizeof (flock_errors[0]))
    {
      REPORT (fprintf (stderr, "flock_strerror: "));
      REPORT (fprintf (stderr, "warning: unknown error code %d.\n",
		       error_code));
      error_code = 0;
    }

  return flock_errors[error_code];
}

