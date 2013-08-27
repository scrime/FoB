/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, universite' Bordeaux 1

   OSC.h - loose implementation of CNMAT's Open Sound Control
   protocol.

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

#ifndef __OSC_H__
#define __OSC_H__

#include <sys/types.h>
#include <sys/socket.h>

/* Should be called before the other functions. */
void OSC_init (void);

/* Abstract data type for OSC address spaces.  An OSC address space is
   a set of OSC methods. */
typedef struct OSC_space_s * OSC_space_t;

/* Abstract data type for OSC methods.  An OSC method is a function
   which can be invoked at the reception of a conforming OSC
   message. */
typedef struct OSC_method_s * OSC_method_t;

/* Abstract data type for OSC messages (but currently not bundles).
   An OSC message is made of a name, a type tag, and corresponding
   arguments.  The message produces a raw vector of bytes (in network
   order) that can be sent to an OSC server. */
typedef struct OSC_message_s * OSC_message_t;

/* Concrete data type for the raw vectors of bytes corresponding to
   OSC messages. */
struct OSC_message_packet_s {
  char * buffer;
  int size;
};

typedef struct OSC_message_packet_s * OSC_message_packet_t;

/* Returns an allocated OSC message with given name, type tag and
   arguments.  OSC arguments like integers or float should be given as
   generic pointers (void*) to their values (in host order), while
   string arguments should be given "as is" (char*).  All arguments
   will be safely copied into the message data structure.  Returns
   NULL on failure, which can mean that one of the type tags is not
   handled in the current implementation. */
extern OSC_message_t OSC_message_make (char * name, char * typetag, ...);

/* Deletes an OSC message from memory.  The corresponding vector of
   bytes is also deleted. */
extern void OSC_message_free (OSC_message_t m);

/* Returns a pointer to the vector of bytes corresponding to the
   message. */
//extern const OSC_message_packet_t OSC_message_packet (OSC_message_t m);
extern OSC_message_packet_t OSC_message_packet (OSC_message_t m);

/* Returns 1 if the given OSC argument type is managed in the current
   implementation, 0 otherwise. */
extern int OSC_type_is_managed (char type);

/* Allocates and returns an OSC address space, with no registered
   methods. */
extern OSC_space_t OSC_space_make (void);

/* Deletes an existing OSC address space and every methods it
   contains. */
extern void OSC_space_free (OSC_space_t space);

/* Registers an OSC method in an OSC address space. */
extern void OSC_space_register_method (OSC_space_t space, OSC_method_t m);

/* Parses an OSC message contained in 'buffer'.  The parsing will not
   go further than 'maxsize' bytes after 'buffer'.  If the name of a
   registered method of the address space is found in the message, the
   method is invoked with appropriate arguments.  Returns 0 on
   success, -1 otherwise. */
extern int OSC_space_parse (OSC_space_t space,
                            const char * buffer,
                            int maxsize);

typedef int (* OSC_method_callback_t) (const char * args, void * data);

/* Allocates and returns an OSC method.  The method is made of a name
   (the OSC message name that implies the invocation of the method), a
   type tag, and a callback (the function that is called when the
   method is invoked) and optional callback data.  On invocation, the
   callback is called with a pointer to the raw bytes containing the
   arguments of the message (still in network order) and the optional
   callback data.  The callback should return 0 on success, -1
   otherwise. */
extern OSC_method_t OSC_method_make (char * name,
                                     char * typetag,
                                     OSC_method_callback_t callback,
                                     void * callback_data);

/* Deletes an OSC method from memory. */
extern void OSC_method_free (OSC_method_t m);

#endif
