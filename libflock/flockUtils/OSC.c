/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, universite' Bordeaux 1

   OSC.c - loose implementation of CNMAT's Open Sound Control
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <assert.h>

#include "OSC.h"

#define REPORT(x) x

#define OSC_PADDING 4
#define OSC_STRING_BOUNDARY(s) (((strlen (s) / OSC_PADDING) + 1) * OSC_PADDING)

// Types of OSC arguments.

struct OSC_argtype_s {
  unsigned char id;
  /* Returns the number of bytes that the OSC argument takes in the
     raw OSC packet. */
  int (* size) (const void * value);
  /* Writes the OSC argument as a raw sequence of bytes at a given
     location.  Converts from host order to network order. */
  void (* put) (const void * value, void * dest);
};


typedef struct OSC_argtype_s * OSC_argtype_t;

static int
OSC_argtype_4bytes_size (const void * value)
{
  return 4;
}


static void
OSC_argtype_4bytes_put (const void * value, void * dest)
{
  *((unsigned long *) dest) = htonl (*((unsigned long *) value));
}


static struct OSC_argtype_s OSC_argtype_int = {
  'i', OSC_argtype_4bytes_size, OSC_argtype_4bytes_put
};


static struct OSC_argtype_s OSC_argtype_float = {
  'f', OSC_argtype_4bytes_size, OSC_argtype_4bytes_put
};


static int
OSC_argtype_string_size (const void * value)
{
  return OSC_STRING_BOUNDARY (value);
}


static void
OSC_argtype_string_put (const void * value, void * dest)
{
  strncpy (dest, value, OSC_argtype_string_size (value));
}


static struct OSC_argtype_s OSC_argtype_string = {
  's', OSC_argtype_string_size, OSC_argtype_string_put
};

static int
OSC_argtype_blob_size (const void * value)
{
  int size;

  size = sizeof (size) + *((int *) value);
  size = ((size / OSC_PADDING) + 1) * OSC_PADDING;
  return size;
}


static void
OSC_argtype_blob_put (const void * value, void * dest)
{
  strncpy (dest, value, OSC_argtype_blob_size (value));
  *((unsigned long *) dest) = htonl (*((unsigned long *) value));
}

static struct OSC_argtype_s OSC_argtype_blob = {
  'b', OSC_argtype_blob_size, OSC_argtype_blob_put
};


#define MAX_NUMBER_OF_TYPES 256


static OSC_argtype_t types[MAX_NUMBER_OF_TYPES]; /* Type's id is key. */


static inline void
OSC_argtype_init (void)
{
  int i;

  for (i = 0; i < MAX_NUMBER_OF_TYPES; i++)
    types[i] = NULL;

  types['i'] = &OSC_argtype_int;
  types['f'] = &OSC_argtype_float;
  types['s'] = &OSC_argtype_string;
  types['b'] = &OSC_argtype_blob;
  /* TODO: implement the other (uncommon) types of arguments. */
}


// OSC messages.

struct OSC_message_s {
  struct OSC_message_packet_s packet;
};


OSC_message_t
OSC_message_make (char * name, char * typetag, ...)
{
  OSC_message_t m;
  va_list ap;
  int allocated;
  char * t;

  if (typetag[0] != ',')
    {
      REPORT (fprintf (stderr, "Type tag does not start with a comma.\n"));
      return NULL;
    }

  m = (OSC_message_t) malloc (sizeof (*m));
  assert (m);

  {
    int name_boundary = OSC_STRING_BOUNDARY (name);
    int typetag_boundary = OSC_STRING_BOUNDARY (typetag);

    m->packet.size =  name_boundary + typetag_boundary;
    allocated = m->packet.size * 2;
    m->packet.buffer = (char *) malloc (allocated * sizeof (char));
    strncpy (m->packet.buffer, name, name_boundary);
    strncpy (m->packet.buffer + name_boundary, typetag, typetag_boundary);
  }

  t = typetag + 1;
  va_start (ap, typetag);

  while (*t != '\0')
    {
      OSC_argtype_t type;
      void * value;
      int size;

      type = types[*(unsigned char *) t];

      if ((type == NULL) || (type->id != *t))
        {
          REPORT (fprintf (stderr, "Unhandled OSC argument type: %c.\n", *t));
          OSC_message_free (m);
          return NULL;
        }

      value = va_arg (ap, void *);
      size = type->size (value);

      if (m->packet.size + size > allocated)
        {
          allocated = (m->packet.size + size) * 2;
          m->packet.buffer = (char *)
            realloc (m->packet.buffer, allocated * sizeof (char));
          assert (m->packet.buffer);
        }

      type->put (value, m->packet.buffer + m->packet.size);
      m->packet.size += size;
      t++;
    }

  va_end (ap);

  return m;
}


void
OSC_message_free (OSC_message_t m)
{
  assert (m);
  assert (m->packet.buffer);
  free (m->packet.buffer);
  free (m);
}


//const
OSC_message_packet_t
OSC_message_packet (OSC_message_t m)
{
  assert (m);
  return &m->packet;
}


// OSC spaces and methods.

struct OSC_space_s {
  OSC_method_t * methods;
  int allocated;
  int nmethods;
};


struct OSC_method_s {
  char * name;
  char * typetag;
  OSC_method_callback_t callback;
  void * callback_data;
};


static int parse_bundle (OSC_space_t space, const char * buffer, int maxsize);
static int parse_leaf (OSC_space_t space, const char * buffer, int maxsize);
static int parse_args (OSC_space_t space, const char * buffer, int maxsize);


static inline int
parse_bundle (OSC_space_t space, const char * buffer, int maxsize)
{
  int pos;

  assert (strcmp (buffer, "#bundle") == 0);

  /* FIXME: interpret time tag. */
  /* Skip message name and time tag. */
  pos = 8 + 8;

  while (pos + 4 < maxsize)
    {
      unsigned long esize;

      /* Get size of bundle element. */
      esize = ntohl (*((unsigned long *) (buffer + pos)));

      /* FIXME: doesn't really consider the simultaneity of messages
         in a bundle. */
      /* Parse element.  Might be a bundle, too. */
      if (OSC_space_parse (space, buffer + pos + 4, esize) == -1)
        return -1;

      /* Go to next element. */
      pos += 4 + esize;
    }

  if (pos > maxsize)
    {
      REPORT (fprintf (stderr, "Incomplete OSC bundle.\n"));
      return -1;
    }

  return 0;
}


static inline int
parse_args (OSC_space_t space, const char * buffer, int maxsize)
{
  int pos;
  int i;

  assert (buffer[0] == ',' && strlen (buffer) < maxsize);

  pos = OSC_STRING_BOUNDARY (buffer);
  i = 1;

  while ((pos < maxsize) && (buffer[i] != '\0'))
    {
      OSC_argtype_t type;

      type = types [(unsigned int) buffer[i]];

      if ((type == NULL) || (type->id != buffer[i]))
        {
          REPORT (fprintf (stderr, "Unhandled OSC argument type: %c.\n",
                           buffer[i]));
          return -1;
        }

      /* FIXME: partially received strings may lead to parsing beyond
         (buffer + maxsize). */
      pos += type->size (buffer + pos);
      i++;
    }

  if (pos > maxsize)
    {
      REPORT (fprintf (stderr, "Incomplete OSC arguments.\n"));
      return -1;
    }

  return 0;
}


static inline int
parse_leaf (OSC_space_t space, const char * buffer, int maxsize)
{
  int name_boundary;
  int typetag_boundary;
  int found;
  int i;

  assert (strlen (buffer) < maxsize);

  name_boundary = OSC_STRING_BOUNDARY (buffer);

  if ((name_boundary >= maxsize) || (buffer[name_boundary] != ','))
    {
      REPORT (fprintf (stderr, "Incomplete OSC message.\n"));
      return -1;
    }

  found = 0;

  for (i = name_boundary + 1; i < maxsize; i++)
    if (buffer[i] == '\0')
      {
        found = 1;
        break;
      }

  if (!found)
    {
      REPORT (fprintf (stderr, "Incomplete OSC type tag.\n"));
      return -1;
    }

  typetag_boundary = OSC_STRING_BOUNDARY (buffer + name_boundary);

  if (parse_args (space,
                  buffer + name_boundary,
                  maxsize - name_boundary) == -1)
    return -1;

  for (i = 0; i < space->nmethods; i++)
    {
      /* FIXME: could be more efficient, for example with a hash
         table. */
      /* FIXME: manage regular expressions. */
      /* Find method with appropriate type tag. */
      if (strcmp (buffer, space->methods[i]->name) != 0)
        continue;

      if (strcmp (buffer + name_boundary, space->methods[i]->typetag) != 0)
        continue;

      /* Everything seems ok.  Invoke the method. */
      return space->methods[i]->callback
        (buffer + name_boundary + typetag_boundary,
         space->methods[i]->callback_data);
    }

  /* No method found for this message. */
  return 0;
}


OSC_space_t
OSC_space_make (void)
{
  OSC_space_t space;

  space = (OSC_space_t) malloc (sizeof (*space));
  assert (space);
  space->allocated = 16;
  space->methods = (OSC_method_t *)
    malloc (space->allocated * sizeof (*space->methods));
  assert (space->methods);
  space->nmethods = 0;

  return space;
}


void
OSC_space_free (OSC_space_t space)
{
  int i;

  assert (space);

  for (i = 0; i < space->nmethods; i++)
    OSC_method_free (space->methods[i]);

  free (space->methods);
  free (space);
}


void
OSC_space_register_method (OSC_space_t space, OSC_method_t m)
{
  int i;

  assert (space);
  assert (m);

  for (i = 0; i < space->nmethods; i++)
    {
      /* FIXME: could be more efficient, for example with a hash
         table. */
      if (strcmp (m->name, space->methods[i]->name) != 0)
        continue;

      if (strcmp (m->typetag, space->methods[i]->typetag) != 0)
        continue;

      REPORT (fprintf (stderr, "Method already registered: %s, %s.\n",
                       m->name, m->typetag));
      return;
    }

  if (space->nmethods == space->allocated)
    {
      space->allocated *= 2;
      space->methods = (OSC_method_t *)
        realloc (space->methods, space->allocated * sizeof (*space->methods));
      assert (space->methods);
    }

  space->methods[space->nmethods++] = m;
}


int
OSC_space_parse (OSC_space_t space, const char * buffer, int maxsize)
{
  int found;
  int i;

  found = 0;

  for (i = 0; i < maxsize; i++)
    if (buffer[i] == '\0')
      {
        found = 1;
        break;
      }

  if (!found)
    {
      REPORT (fprintf (stderr, "Incomplete OSC message.\n"));
      return -1;
    }

  if (strcmp (buffer, "#bundle") == 0)
    return parse_bundle (space, buffer, maxsize);

  return parse_leaf (space, buffer, maxsize);
}


OSC_method_t
OSC_method_make (char * name,
                 char * typetag,
                 OSC_method_callback_t callback,
                 void * callback_data)
{
  OSC_method_t m;

  assert (name && typetag && callback);

  m = (OSC_method_t) malloc (sizeof (*m));
  assert (m);

  m->name = (char *) malloc ((strlen (name) + 1) * sizeof (char));
  assert (m->name);
  strcpy (m->name, name);

  m->typetag = (char *) malloc ((strlen (typetag) + 1) * sizeof (char));
  assert (m->typetag);
  strcpy (m->typetag, typetag);

  m->callback = callback;
  m->callback_data = callback_data;

  return m;
}


void
OSC_method_free (OSC_method_t m)
{
  assert (m);
  free (m->name);
  free (m->typetag);
  free (m);
}


// Init function.

void
OSC_init (void)
{
  OSC_argtype_init ();
}

