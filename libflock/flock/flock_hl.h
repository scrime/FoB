/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flock_hl.h - higher level operations on flock of birds.

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

#ifndef __FLOCK_HL_H__
#define __FLOCK_HL_H__

#include <flock/flock.h>
#include <flock/flock_bird_record.h>

/* A high-level function to open a flock (in non-blocking mode), check
   its status, and configure it so that all birds are set in the given
   record mode, and the flock itself operates (or not) in group mode
   and stream mode.  Uses some of the other high-level functions
   declared below.  Returns NULL on error. */
extern flock_t flock_hl_open (char * device,
			      int number_of_birds,
			      flock_bird_record_mode_t mode,
			      int group_mode,
			      int stream_mode);

/* Closes a flock.  Stops eventual stream mode and checks the flock's
   status before closing the device and freeing the memory. */
extern void flock_hl_close (flock_t flock);

/* Checks the status of a flock.  The flock's system status and each
   bird's status are checked, together with error codes.  Returns 1 if
   there is no error and status is correct with regard to the software
   flock data structure, 0 otherwise.  The caller should be aware that
   the function puts the process asleep for some time, waiting for the
   flock to execute commands, as required in the "installation and
   operation guide".  It is not a good idea to call this function
   after every operation on a flock.  It should rather be called on
   initiliazation, or when an error is suspected. */
extern int flock_check_status (flock_t flock);

/* Processes auto configuration of a flock.  Returns 1 on success, 0
   otherwise.  The caller should be aware that the function puts the
   process asleep for some time, waiting for the flock to execute
   commands, as required in the "installation and operation guide". */
extern int flock_auto_configure (flock_t flock);

/* Puts a flock in group mode ('value' is 1) or standalone mode
   ('value' is 0).  Returns 1 on success, 0 otherwise.  By default, a
   flock is in standalone mode. */
extern int flock_set_group_mode (flock_t flock, int value);

/* Puts a flock in stream mode ('value' is 1) or point mode ('value'
   is 0).  Returns 1 on success, 0 otherwise.  By default, a flock is
   in point mode.  A flock should be put in group mode before being
   put in stream mode. */
extern int flock_set_stream_mode (flock_t flock, int value);

/* Puts a bird in the given record mode.  Returns 1 on success, 0
   otherwise.  By default, a bird is in position/angles mode. */
extern int flock_set_record_mode (flock_t flock,
				  int bird,
				  flock_bird_record_mode_t mode);

/* Puts all birds in the given record mode.  Returns 1 on success, 0
   otherwise. */
extern int flock_set_record_mode_all_birds (flock_t flock,
					    flock_bird_record_mode_t mode);

/* Gets a new record from a bird.  Returns 1 on success, 0 otherwise.
   The response sent by the bird will be available through a call to
   'flock_get_record'.  If the flock is in group mode, the bird should
   be the master (address 1) and the response will contain every
   bird's record.  This function can block indefinitely. */
extern int flock_next_record (flock_t flock, int bird);

/* Returns the address of the last received bird's record as read by
   'flock_next_record'.  If necessary, the contents of the record
   should be copied before calling any function in the library.  The
   result is unspecified if 'flock_next_record' is not called just
   before this function. */
extern flock_bird_record_t flock_get_record (flock_t flock, int bird);

#endif
