/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flock_command.c - encapsulating information about flock commands.

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
#include <stdlib.h>
#include <assert.h>

#include "flock_common.h"

#include "flock.h"
#include "flock_command.h"

typedef struct command_info_s * command_info_t;
struct command_info_s {
  const char * name;
  unsigned char command_byte;
  int data_size;
  int response_size;
  int can_change_mode;
};

typedef struct parameter_info_s * parameter_info_t;
struct parameter_info_s {
  const char * name;
  unsigned char number;
  int changeable;
  int data_size;
  int response_size;
  int can_change_mode;
};

/* Set by flock_command_init. */
static command_info_t * commands = NULL;
static parameter_info_t * parameters = NULL;

int
flock_command_size_of_data (const flock_command_t command)
{
  unsigned char command_byte;
  int size;

  command_byte = command[0];

  if (commands[command_byte] == NULL)
    return -1;

  size = commands[command_byte]->data_size;

  if (command_byte == FLOCK_COMMAND_CHANGE_VALUE)
    {
      /* Specific to this command: variable number of data bytes. */
      unsigned char parameter;

      parameter = command[1];

      if (parameters[parameter] == NULL ||
          !parameters[parameter]->changeable)
        return -1;

      /* FIXME: may depend on addressing mode. */
      size = 1 + parameters[parameter]->data_size;
    }

  return size;
}

int
flock_command_size_of_response (const flock_command_t command)
{
  unsigned char command_byte;
  int size;

  command_byte = command[0];

  if (commands[command_byte] == NULL)
    return -1;

  size = commands[command_byte]->response_size;

  if (command_byte == FLOCK_COMMAND_EXAMINE_VALUE)
    {
      /* Specific to this command: variable number of response bytes. */
      unsigned char parameter;

      parameter = command[1];

      if (parameters[parameter] == NULL)
        return -1;

      /* FIXME: may depend on addressing mode. */
      size = parameters[parameter]->response_size;
    }

  return size;
}

int
flock_command_can_change_mode (const flock_command_t command)
{
  unsigned char command_byte;
  int can_change_mode;

  command_byte = command[0];

  if (commands[command_byte] == NULL)
    return 0;

  can_change_mode = commands[command_byte]->can_change_mode;

  if (command_byte == FLOCK_COMMAND_CHANGE_VALUE)
    {
      /* Specific to this command: some parameter may change the mode. */
      unsigned char parameter;

      parameter = command[1];

      if (parameters[parameter] == NULL)
        return 0;

      can_change_mode = parameters[parameter]->can_change_mode;
    }

  return can_change_mode;
}

void
flock_command_display (FILE * stream, const flock_command_t command)
{
  unsigned char command_byte;

  command_byte = command[0];
  if (commands[command_byte] == NULL)
    {
      fprintf (stderr,
               "flock_command_display: warning: bad command byte 0x%.2x\n",
               command_byte);
      return;
    }

  if (command_byte == FLOCK_COMMAND_CHANGE_VALUE ||
      command_byte == FLOCK_COMMAND_EXAMINE_VALUE)
    {
      unsigned char parameter;

      parameter = command[1];

      if (parameters[parameter] == NULL)
        {
          fprintf (stderr,
                   "flock_command_display: warning: bad parameter 0x%.2x\n",
                   parameter);
          return;
        }

      fprintf (stream, "%s %s (command: 0x%.2x, parameter: 0x%.2x, data size: %d, response size: %d)\n",
               commands[command_byte]->name,
               parameters[parameter]->name,
               command_byte,
               parameter,
               flock_command_size_of_data (command) - 1,
               flock_command_size_of_response (command));
    }
  else
    fprintf (stream, "%s (command: 0x%.2x, data size: %d, response size: %d)\n",
             commands[command_byte]->name,
             command_byte,
             flock_command_size_of_data (command),
             flock_command_size_of_response (command));
}


static struct command_info_s _commands [] = {
  {"ANGLES", FLOCK_COMMAND_ANGLES, 0, 0, 1},
  {"ANGLE ALIGN", FLOCK_COMMAND_ANGLE_ALIGN, 12, 0, 0},
  {"BUTTON MODE", FLOCK_COMMAND_BUTTON_MODE, 1, 0, 1},
  {"BUTTON READ", FLOCK_COMMAND_BUTTON_READ, 0, 1},

  /* Special: variable data size (depends on parameter). */
  /* Some parameter can also change the operating mode. */
  {"CHANGE VALUE", FLOCK_COMMAND_CHANGE_VALUE, -1, 0, -1},

  /* Special variable response size (depends on parameter). */
  {"EXAMINE VALUE", FLOCK_COMMAND_EXAMINE_VALUE, 1, -1, 0},

  /* Not considered. */
  {"FACTORY TEST", FLOCK_COMMAND_FACTORY_TEST, 0, 0, 0},

  {"HEMISPHERE", FLOCK_COMMAND_HEMISPHERE, 2, 0, 0},
  {"MATRIX", FLOCK_COMMAND_MATRIX, 0, 0, 1},

  /* Special: mixed command/data. */
  {"NEXT MASTER", FLOCK_COMMAND_NEXT_MASTER, 0, 0, 0},

  {"NEXT TRANSMITTER", FLOCK_COMMAND_NEXT_TRANSMITTER, 1, 0, 0},

  /* Special: variable response size (depends on group and record mode). */
  {"POINT", FLOCK_COMMAND_POINT, 0, -1, 0}, // FIXME

  {"POSITION", FLOCK_COMMAND_POSITION, 0, 0, 1},
  {"POSITION ANGLES", FLOCK_COMMAND_POSITION_ANGLES, 0, 0, 1},
  {"POSITION MATRIX", FLOCK_COMMAND_POSITION_MATRIX, 0, 0, 1},
  {"POSITION QUATERNION", FLOCK_COMMAND_POSITION_QUATERNION, 0, 0, 1},
  {"QUATERNION", FLOCK_COMMAND_QUATERNION, 0, 0, 1},
  {"REFERENCE FRAME", FLOCK_COMMAND_REFERENCE_FRAME, 12, 0, 0},

  /* Special: mixed command/data. */
  {"RS232 TO FBB", FLOCK_COMMAND_RS232_TO_FBB, 0, 0, 0},
  {"EXPANDED RS232 TO FBB", FLOCK_COMMAND_RS232_TO_FBB_EXPANDED, 0, 0, 0},

  {"REPORT RATE 1 CYCLE", FLOCK_COMMAND_REPORT_RATE_CYCLE_DIVISOR_1, 0, 0, 0},
  {"REPORT RATE 2 CYCLE", FLOCK_COMMAND_REPORT_RATE_CYCLE_DIVISOR_2, 0, 0, 0},
  {"REPORT RATE 8 CYCLE", FLOCK_COMMAND_REPORT_RATE_CYCLE_DIVISOR_8, 0, 0, 0},
  {"REPORT RATE 32 CYCLE", FLOCK_COMMAND_REPORT_RATE_CYCLE_DIVISOR_32, 0, 0, 0},
  {"RUN", FLOCK_COMMAND_RUN, 0, 0, 1},
  {"SLEEP", FLOCK_COMMAND_SLEEP, 0, 0, 1},

  /* Special: starts continuous stream of records from the flock. */
  {"STREAM", FLOCK_COMMAND_STREAM, 0, -1, 1}, // FIXME

  {"SYNC", FLOCK_COMMAND_SYNC, 1, 0, 0},
  {"XON", FLOCK_COMMAND_XON, 0, 0, 1},
  {"XOFF", FLOCK_COMMAND_XOFF, 0, 0, 1}
};

static struct parameter_info_s _parameters[] = {
  {"BIRD SYSTEM STATUS", FLOCK_PARAMETER_BIRD_SYSTEM_STATUS, 0, 0, 2, 0},
  {"SOFTWARE REVISION NUMBER", FLOCK_PARAMETER_SOFTWARE_REVISION_NUMBER, 0, 0, 2, 0},
  {"BIRD COMPUTER CRYSTAL SPEED", FLOCK_PARAMETER_BIRD_COMPUTER_CRYSTAL_SPEED, 0, 0, 2, 0},
  {"POSITION SCALING", FLOCK_PARAMETER_POSITION_SCALING, 1, 2, 2, 0},
  {"FILTER ON OFF STATUS", FLOCK_PARAMETER_FILTER_ON_OFF_STATUS, 1, 2, 2, 0},
  {"DC FILTER CONSTANT TABLE ALPHA MIN", FLOCK_PARAMETER_DC_FILTER_CONSTANT_TABLE_ALPHA_MIN, 1, 14, 14, 0},
  {"BIRD MEASUREMENT RATE", FLOCK_PARAMETER_BIRD_MEASUREMENT_RATE, 1, 2, 2, 0},
  {"DISABLE ENABLE DATA READY OUTPUT", FLOCK_PARAMETER_DISABLE_ENABLE_DATA_READY_OUTPUT, 1, 1, 1, 1},
  {"CHANGES DATA READY CHARACTER", FLOCK_PARAMETER_CHANGES_DATA_READY_CHARACTER, 1, 1, 1, 0},
  {"ERROR CODE", FLOCK_PARAMETER_ERROR_CODE, 0, 0, 1, 0},
  {"ERROR DETECT MASK", FLOCK_PARAMETER_ERROR_DETECT_MASK, 1, 1, 1, 0},
  {"DC FILTER TABLE VM", FLOCK_PARAMETER_DC_FILTER_TABLE_VM, 1, 14, 14, 0},
  {"DC FILTER CONSTANT TABLE ALPHA MAX", FLOCK_PARAMETER_DC_FILTER_CONSTANT_TABLE_ALPHA_MAX, 1, 14, 14, 0},
  {"SUDDEN OUTPUT CHANGE LOCK", FLOCK_PARAMETER_SUDDEN_OUTPUT_CHANGE_LOCK, 1, 1, 1, 0},
  {"SYSTEM MODEL IDENTIFICATION", FLOCK_PARAMETER_SYSTEM_MODEL_IDENTIFICATION, 0, 0, 10, 0},
  {"EXPANDED ERROR CODE", FLOCK_PARAMETER_EXPANDED_ERROR_CODE, 0, 0, 2, 0},
  {"XYZ REFERENCE FRAME", FLOCK_PARAMETER_XYZ_REFERENCE_FRAME, 1, 1, 1, 0},
  {"FBB HOST RESPONSE DELAY", FLOCK_PARAMETER_FBB_HOST_RESPONSE_DELAY, 1, 2, 2, 0},
  /* Special: change_data or examine_response are 5 bytes in normal
     addressing mode, 7 bytes in expanded addressing mode (old flock
     interface, disappeared since then). */
  {"FBB CONFIGURATION", FLOCK_PARAMETER_FBB_CONFIGURATION, 1, -1, -1, 1}, // FIXME
  /* Special: change_data or examine_response are 14 bytes in normal
     addressing mode, 30 bytes in expanded addressing mode (old flock
     interface, disappeared since then). */
  {"FBB ARM", FLOCK_PARAMETER_FBB_ARM, 1, -1, -1, 1}, // FIXME
  {"GROUP MODE", FLOCK_PARAMETER_GROUP_MODE, 1, 1, 1, 1},
  /* Special: examine_response is 14 bytes in normal addressing mode,
     30 bytes in expanded addressing mode, 126 in super-expanded
     mode. */
  {"FLOCK SYSTEM STATUS", FLOCK_PARAMETER_FLOCK_SYSTEM_STATUS, 0, 0, -1, 0}, // FIXME
  /* Special: examine_response is 5 bytes in normal addressing mode, 7
     bytes in expanded addressing mode, 19 in super-expanded mode. */
  {"FBB AUTO CONFIGURATION", FLOCK_PARAMETER_FBB_AUTO_CONFIGURATION, 1, 1, -1, 1} // FIXME
};

#define MAX_NUMBER_OF_COMMANDS 256
#define MAX_NUMBER_OF_PARAMETERS 256

/* Allocating and setting information tables. */
void
flock_command_init (void)
{
  int i;

  if (commands == NULL)
    {
      commands = (command_info_t *) malloc (MAX_NUMBER_OF_COMMANDS * sizeof (command_info_t));
      assert (commands);

      for (i = 0; i < MAX_NUMBER_OF_COMMANDS; i++)
        commands[i] = NULL;

      for (i = 0; i < (sizeof (_commands) / sizeof (_commands[0])); i++)
        {
          unsigned char command_byte;

          command_byte = _commands[i].command_byte;
          commands[command_byte] = &_commands[i];

          /* Special cases: mixed command/data. */

          if (command_byte == FLOCK_COMMAND_NEXT_MASTER)
            {
              int j;

              /* Address 0 is illegal for a master. */
              commands[FLOCK_COMMAND_NEXT_MASTER] = NULL;

              for (j = 1; j < 16; j++)
                commands[FLOCK_COMMAND_NEXT_MASTER + j] =
                  &_commands[i];
            }

          /* Normal addressing mode RS232 TO FBB. */
          if (command_byte == FLOCK_COMMAND_RS232_TO_FBB)
            {
              int j;

              /* We go up to 15 because address 15 is illegal for a
                 bird in normal addressing mode but not in
                 expanded. */
              for (j = 0; j < 16; j++)
                commands[FLOCK_COMMAND_RS232_TO_FBB + j] =
                  &_commands[i];
            }

          /* Expanded addressing mode RS232 TO FBB. */
          if (command_byte == FLOCK_COMMAND_RS232_TO_FBB_EXPANDED)
            {
              int j;

              for (j = 0; j < 16; j++)
                commands[FLOCK_COMMAND_RS232_TO_FBB_EXPANDED + j] =
                  &_commands[i];

              /* Address 31 is illegal for a bird.  Note: 15 here is
                 not a typo.  In expanded addressing mode, the low 4
                 bits of command 0xe0 mean birds from 16 to 30. */
              commands[FLOCK_COMMAND_RS232_TO_FBB + 15] = NULL;
            }

          /* FIXME: other cases depend on specific operating modes.
             How can we manage this, apart from a simulation of the
             machine?  (e.g. the "stream" mode gives a special meaning
             to the POINT command.) */
        }
    }

  if (parameters == NULL)
    {
      parameters = (parameter_info_t *)
        malloc (MAX_NUMBER_OF_PARAMETERS * sizeof (parameter_info_t));
      assert (parameters);

      for (i = 0; i < MAX_NUMBER_OF_PARAMETERS; i++)
        parameters[i] = NULL;

      for (i = 0; i < (sizeof (_parameters) / sizeof (_parameters[0])); i++)
        {
          unsigned char number;

          number = _parameters[i].number;
          parameters[number] = &_parameters[i];
        }
    }
}

