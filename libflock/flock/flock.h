/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flock.h - low level operations on flock of birds.

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

#ifndef __FLOCK_H__
#define __FLOCK_H__

#include <flock/flock_command.h>

/* Should be called first before calling other functions in the
   library. */
extern void flock_init (void);

/* Abstract data type for flock of birds. */
typedef struct flock_s * flock_t;

/* Allocates a new flock handler.  Returns a valid pointer on success,
   NULL on failure.  The 'filename' argument is the name of a file
   corresponding to the serial device (for example "/dev/ttyS0") to
   which the master bird is connected.  The file is opened with
   open(2) for reading and writing, also taking care of the optional
   'flags' (0 if normal blocking behavior, O_NDELAY otherwise).  The
   file is kept open until the handler is deleted.  The 'birds'
   argument is the number of birds (individual units) in the flock. */
extern flock_t flock_open (const char * filename, int flags, int birds);

/* Deletes a flock handler from memory.  Closes the related file. */
extern void flock_close (flock_t flock);

/* Returns the file descriptor of the open file associated to a flock.
   This may be useful if the caller wants to control I/O with
   functions like select(2). */
extern int flock_get_file_descriptor (flock_t flock);

/* Flushes both output and input streams of the file associated to a
   flock, and sends a raw command to the flock.  Returns 0 if the
   write is successful, -1 otherwise.  If the flock outputs data as a
   response to the command, the response can be accessed by calling
   'flock_read' (see below).  The 'size' argument is a hint that the
   caller should give, because in the current implementation the size
   of every flock command cannot be guessed in all cases. */
extern int flock_write (flock_t flock,
                        const flock_command_t command,
                        int size);

/* Concrete data type for raw responses from a flock. */
typedef struct flock_response_s * flock_response_t;
struct flock_response_s {
  int size; /* size of the following array */
  const unsigned char * data; /* a pointer to an array of 'size' bytes */
};

/* Reads raw data from a flock, likely the response to the last
   command sent to it.  The 'size' member of the response can either
   be -1, 0, or greater than 0.  A 'size' of -1 means that the flock
   was not given enough time to respond to the last command; the
   function should be called again later on.  If 'size' is 0, then the
   last command sent to the flock implied no response.  If 'size' is
   greater than 0, the 'data' member of the response points to an
   array of 'size' bytes containing the data from the flock, which
   should be copied (if necessary) before calling any function in the
   library.  The 'expected_size' argument is a hint that the caller
   should give, because in the current implementation the size of a
   command response cannot be guessed in all cases.  Returns NULL if
   an error occurs. */
extern flock_response_t flock_read (flock_t flock, int expected_size);

#endif
