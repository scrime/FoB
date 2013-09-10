#ifndef __ezusb_H
#define __ezusb_H
#ifdef __cplusplus

#include <stdint.h>
extern "C" {
#endif

/*
 * Copyright (c) 2001 Stephen Williams (steve@icarus.com)
 * Copyright (c) 2002 David Brownell (dbrownell@users.sourceforge.net)
 * Copyright (c) 2009 Jon Nall (jon.nall@gmail.com)
 * Copyright (c) 2009 Anthony Beurive (anthony.beurive@free.fr)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

typedef struct ezusb_s * ezusb_t;

typedef enum {
  ptUNDEF = 0,
  ptAN21,
  ptFX,
  ptFX2,
  ptFX2LP
} part_type;

/* Creates a new ezusb structure for the USB device of given vendor
   and product IDs.  Returns a non-zero error code on failure. */
extern int ezusb_new
(ezusb_t* ezusb,
 int vendor_id,
 int product_id);

/* Removes the given ezusb object from memory. */
extern void ezusb_free
(ezusb_t ezusb);

/*
 * This function loads the firmware from the given file into RAM.
 * The file is assumed to be in Intel HEX format.  If fx2 is set, uses
 * appropriate reset commands.  Stage == 0 means this is a single stage
 * load (or the first of two stages).  Otherwise it's the second of
 * two stages; the caller preloaded the second stage loader.
 *
 * The target processor is reset at the end of this download.
 */
extern int ezusb_load_ram
(ezusb_t ezusb,              /* usb device handle */
 const char* hexfilePath,    /* path to hexfile */
 const part_type partType,   /* the part type to program */
 const int stage);           /* TRUE if this is the second stage */


/*
 * This function stores the firmware from the given file into EEPROM.
 * The file is assumed to be in Intel HEX format.  This uses the right
 * CPUCS address to terminate the EEPROM load with a reset command,
 * where FX parts behave differently than FX2 ones.  The configuration
 * byte is as provided here (zero for an21xx parts) and the EEPROM
 * type is set so that the microcontroller will boot from it.
 *
 * The caller must have preloaded a second stage loader that knows
 * how to respond to the EEPROM write request.
 */
extern int ezusb_load_eeprom
(ezusb_t ezusb,              /* usb device handle */
 const char* hexfilePath,    /* path to hexfile */
 const part_type partType,   /* fx, fx2, fx2lp, an21 */
 uint8_t config);            /* config byte for fx/fx2; else zero */


#ifdef __cplusplus
}
#endif
#endif
