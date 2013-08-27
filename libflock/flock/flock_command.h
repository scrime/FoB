/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flock_command.h - encapsulating information about flock commands.

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

#ifndef __FLOCK_COMMAND_H__
#define __FLOCK_COMMAND_H__

#include <ctype.h>
#include <stdio.h>

/* Concrete data type for flock commands.  A flock command is an array
   of bytes, with first byte being the command byte and subsequent
   bytes the command data (unless command and data are mixed in first
   byte for some special command). */
typedef unsigned char flock_command_t[];

/* Returns the number of data bytes that should follow the command
   byte in a flock command.  Returns -1 if this number cannot be
   guessed from the command.  (This is because the current
   implementation of the library is incomplete with regard to the
   specification of flock of birds.)  The number of data bytes depends
   on the first byte of the command, and sometimes on the second byte
   (e.g. the CHANGE VALUE command) or current operating mode of a
   flock (which is not implemented in the library right now).  The
   function is useful when the caller only knows the first one or two
   bytes of a command, and wants to check how many bytes should
   follow. */
extern int flock_command_size_of_data (const flock_command_t command);

/* Returns the number of bytes that a flock sends as a response to a
   flock command.  Returns -1 if this number cannot be guessed from
   the command.  (This is because the current implementation of the
   library is incomplete with regard to the specification of flock of
   birds.)  The number of bytes received depends on the first byte of
   the command, and sometimes on the second byte (e.g. the EXAMINE
   VALUE command) or current operating mode of a flock (which is not
   implemented in the library right now).  The function is useful when
   the caller only knows the first one or two bytes of a command, and
   wants to check how many bytes the response will be. */
extern int flock_command_size_of_response (const flock_command_t command);

/* Returns 1 if the specified command can change the operating mode of
   a flock, 0 otherwise.  Examples of such commands are 'CHANGE VALUE
   GROUP MODE 1' or 'ANGLES'.  The operating mode is an important
   information in the computation of command data and response
   sizes. */
extern int flock_command_can_change_mode (const flock_command_t command);

/* Outputs a readable representation of a flock command on the
   specified stream, followed by a newline character.  (For debugging
   purposes only.) */
extern void flock_command_display (FILE * stream,
                                   const flock_command_t command);

/* Known command bytes. */
#define FLOCK_COMMAND_ANGLES                                       0x57
#define FLOCK_COMMAND_ANGLE_ALIGN                                  0x4a
#define FLOCK_COMMAND_BUTTON_MODE                                  0x4d
#define FLOCK_COMMAND_BUTTON_READ                                  0x4e
#define FLOCK_COMMAND_CHANGE_VALUE                                 0x50
#define FLOCK_COMMAND_EXAMINE_VALUE                                0x4f
#define FLOCK_COMMAND_FACTORY_TEST                                 0x7a
#define FLOCK_COMMAND_HEMISPHERE                                   0x4c
#define FLOCK_COMMAND_MATRIX                                       0x58
#define FLOCK_COMMAND_NEXT_MASTER                                  0x30
#define FLOCK_COMMAND_NEXT_TRANSMITTER                             0x30
#define FLOCK_COMMAND_POINT                                        0x42
#define FLOCK_COMMAND_POSITION                                     0x56
#define FLOCK_COMMAND_POSITION_ANGLES                              0x59
#define FLOCK_COMMAND_POSITION_MATRIX                              0x5a
#define FLOCK_COMMAND_POSITION_QUATERNION                          0x5d
#define FLOCK_COMMAND_QUATERNION                                   0x5c
#define FLOCK_COMMAND_REFERENCE_FRAME                              0x48
#define FLOCK_COMMAND_RS232_TO_FBB                                 0xf0
#define FLOCK_COMMAND_RS232_TO_FBB_EXPANDED                        0xe0
#define FLOCK_COMMAND_REPORT_RATE_CYCLE_DIVISOR_1                  0x51
#define FLOCK_COMMAND_REPORT_RATE_CYCLE_DIVISOR_2                  0x52
#define FLOCK_COMMAND_REPORT_RATE_CYCLE_DIVISOR_8                  0x53
#define FLOCK_COMMAND_REPORT_RATE_CYCLE_DIVISOR_32                 0x54
#define FLOCK_COMMAND_RUN                                          0x46
#define FLOCK_COMMAND_SLEEP                                        0x47
#define FLOCK_COMMAND_STREAM                                       0x40
#define FLOCK_COMMAND_SYNC                                         0x41
#define FLOCK_COMMAND_XON                                          0x11
#define FLOCK_COMMAND_XOFF                                         0x13

/* Known parameter bytes for CHANGE VALUE and EXAMINE VALUE
   commands. */
#define FLOCK_PARAMETER_BIRD_SYSTEM_STATUS                         0x00
#define FLOCK_PARAMETER_SOFTWARE_REVISION_NUMBER                   0x01
#define FLOCK_PARAMETER_BIRD_COMPUTER_CRYSTAL_SPEED                0x02
#define FLOCK_PARAMETER_POSITION_SCALING                           0x03
#define FLOCK_PARAMETER_FILTER_ON_OFF_STATUS                       0x04
#define FLOCK_PARAMETER_DC_FILTER_CONSTANT_TABLE_ALPHA_MIN         0x05
#define FLOCK_PARAMETER_BIRD_MEASUREMENT_RATE                      0x06
#define FLOCK_PARAMETER_DISABLE_ENABLE_DATA_READY_OUTPUT           0x08
#define FLOCK_PARAMETER_CHANGES_DATA_READY_CHARACTER               0x09
#define FLOCK_PARAMETER_ERROR_CODE                                 0x0a
#define FLOCK_PARAMETER_ERROR_DETECT_MASK                          0x0b
#define FLOCK_PARAMETER_DC_FILTER_TABLE_VM                         0x0c
#define FLOCK_PARAMETER_DC_FILTER_CONSTANT_TABLE_ALPHA_MAX         0x0d
#define FLOCK_PARAMETER_SUDDEN_OUTPUT_CHANGE_LOCK                  0x0e
#define FLOCK_PARAMETER_SYSTEM_MODEL_IDENTIFICATION                0x0f
#define FLOCK_PARAMETER_EXPANDED_ERROR_CODE                        0x10
#define FLOCK_PARAMETER_XYZ_REFERENCE_FRAME                        0x11
#define FLOCK_PARAMETER_FBB_HOST_RESPONSE_DELAY                    0x20
#define FLOCK_PARAMETER_FBB_CONFIGURATION                          0x21
#define FLOCK_PARAMETER_FBB_ARM                                    0x22
#define FLOCK_PARAMETER_GROUP_MODE                                 0x23
#define FLOCK_PARAMETER_FLOCK_SYSTEM_STATUS                        0x24
#define FLOCK_PARAMETER_FBB_AUTO_CONFIGURATION                     0x32

#endif
