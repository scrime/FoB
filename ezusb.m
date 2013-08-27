/*
 * Copyright (c) 2001 Stephen Williams (steve@icarus.com)
 * Copyright (c) 2001-2002 David Brownell (dbrownell@users.sourceforge.net)
 * Copyright (c) 2008 Roger Williams (rawqux@users.sourceforge.net)
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

#include <mach/mach.h>
#include <Foundation/Foundation.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/IOCFPlugIn.h>

#import "ezusb.h"

struct ezusb_s {
  IOUSBDeviceInterface** dev;
};

/*
 * These are the requests (bRequest) that the bootstrap loader is expected
 * to recognize.  The codes are reserved by Cypress, and these values match
 * what EZ-USB hardware, or "Vend_Ax" firmware (2nd stage loader) uses.
 * Cypress' "a3load" is nice because it supports both FX and FX2, although
 * it doesn't have the EEPROM support (subset of "Vend_Ax").
 */
#define RW_INTERNAL 0xA0        /* hardware implements this one */
#define RW_EEPROM   0xA2
#define RW_MEMORY   0xA3
#define GET_EEPROM_SIZE 0xA5

/*
 * For writing to RAM using a first (hardware) or second (software)
 * stage loader and 0xA0 or 0xA3 vendor requests
 */
typedef enum
{
    _undef = 0,
    internal_only,      /* hardware first-stage loader */
    skip_internal,      /* first phase, second-stage loader */
    skip_external       /* second phase, second-stage loader */
} ram_mode;

struct ram_poke_context
{
    IOUSBDeviceInterface** dev;
    ram_mode mode;
    uint32_t total;
    uint32_t count;
};

/*
 * For writing to EEPROM using a 2nd stage loader
 */
struct eeprom_poke_context
{
    IOUSBDeviceInterface** dev;
    uint16_t ee_addr;    /* next free address */
    BOOL last;
};


#define RETRY_LIMIT 5

uint8_t verbose = 0;

/*
 * This file contains functions for downloading firmware into Cypress
 * EZ-USB microcontrollers. These chips use control endpoint 0 and vendor
 * specific commands to support writing into the on-chip SRAM. They also
 * support writing into the CPUCS register, which is how we reset the
 * processor after loading firmware (including the reset vector).
 *
 * A second stage loader must be used when writing to off-chip memory,
 * or when downloading firmare into the bootstrap I2C EEPROM which may
 * be available in some hardware configurations.
 *
 * These Cypress devices are 8-bit 8051 based microcontrollers with
 * special support for USB I/O.  They come in several packages, and
 * some can be set up with external memory when device costs allow.
 * Note that the design was originally by AnchorChips, so you may find
 * references to that vendor (which was later merged into Cypress).
 * The Cypress FX parts are largely compatible with the Anchorhip ones.
 */

static int ram_poke (void* context, const uint16_t addr,  const BOOL external,
    const uint8_t *data, const size_t len);

static int eeprom_poke (void* context, const uint16_t addr, const BOOL external,
    const uint8_t* data, const size_t len);

static int parse_ihex (FILE* image, void* context,
                BOOL (*is_external)(const uint16_t addr, const size_t len),
                int (*poke) (void *context, const uint16_t addr, BOOL external,
                    const uint8_t *data, const size_t len));


static BOOL fx_is_external(const uint16_t addr, const size_t len)
{
    /* with 8KB RAM, 0x0000-0x1b3f can be written
     * we can't tell if it's a 4KB device here
     */
    if (addr <= 0x1b3f)
    {
        return ((addr + len) > 0x1b40);
    }

    /* there may be more RAM; unclear if we can write it.
     * some bulk buffers may be unused, 0x1b3f-0x1f3f
     * firmware can set ISODISAB for 2KB at 0x2000-0x27ff
     */
    return TRUE;
}

/*
 * return true iff [addr,addr+len) includes external RAM
 * for Cypress EZ-USB FX2
 */
static BOOL fx2_is_external (const uint16_t addr, const size_t len)
{
    /* 1st 8KB for data/code, 0x0000-0x1fff */
    if (addr <= 0x1fff)
    {
        return ((addr + len) > 0x2000);
    }

    /* and 512 for data, 0xe000-0xe1ff */
    else if(addr >= 0xe000 && addr <= 0xe1ff)
    {
            return ((addr + len) > 0xe200);
    }

    /* otherwise, it's certainly external */
    else
    {
        return TRUE;
    }
}

/*
 * return true iff [addr,addr+len) includes external RAM
 * for Cypress EZ-USB FX2LP
 */
static BOOL fx2lp_is_external (const uint16_t addr, const size_t len)
{
    /* 1st 16KB for data/code, 0x0000-0x3fff */
    if (addr <= 0x3fff)
    {
        return ((addr + len) > 0x4000);
    }

    /* and 512 for data, 0xe000-0xe1ff */
    else if (addr >= 0xe000 && addr <= 0xe1ff)
    {
        return ((addr + len) > 0xe200);
    }

    /* otherwise, it's certainly external */
    else
    {
        return TRUE;
    }
}

static inline int ctrl_msg(IOUSBDeviceInterface** dev, const UInt8 requestType,
    const UInt8 request, const UInt16 value, const UInt16 index,
    void* data, const size_t length)
{
    if(length > USHRT_MAX)
    {
        NSLog(@"length (%d) too big (max = %d)", length, USHRT_MAX);
        return -EINVAL;
    }

    IOUSBDevRequest req;
    req.bmRequestType = requestType;
    req.bRequest = request;
    req.wValue = value;
    req.wIndex = index;
    req.wLength = (UInt16)length;
    req.pData = data;
    req.wLenDone = 0;

    IOReturn rc = (*dev)->DeviceRequest(dev, &req);

    if(rc != kIOReturnSuccess)
    {
        return -1;
    }

    return req.wLenDone;
}

/*
 * Issues the specified vendor-specific read request.
 */
static int ezusb_read (IOUSBDeviceInterface** dev, NSString* label,
    const uint8_t opcode, const uint16_t addr, const uint8_t* data,
    const size_t len)
{
    if(verbose)
    {
        NSLog(@"%@, addr 0x%04x len %4zd (0x%04zx)", label, addr, len, len);
    }

    const int status = ctrl_msg(dev,
        USBmakebmRequestType(kUSBIn, kUSBVendor, kUSBDevice),
        opcode, addr, 0, (uint8_t*)data, len);
    if(status != len)
    {
        NSLog(@"%@: %d", label, status);
    }

    return status;
}

/*
 * Issues the specified vendor-specific write request.
 */
static int ezusb_write (IOUSBDeviceInterface** dev, NSString* label,
    const uint8_t opcode, const uint16_t addr, const uint8_t* data,
    const size_t len)
{
    if(verbose)
    {
        NSLog(@"%@, addr 0x%04x len %4zd (0x%04zx)", label, addr, len, len);
    }

    const int status = ctrl_msg(dev,
        USBmakebmRequestType(kUSBOut, kUSBVendor, kUSBDevice),
        opcode, addr, 0, (uint8_t*)data, len);
    if(status != len)
    {
        NSLog(@"%@: %d", label, status);
    }

    return status;
}

/*
 * Modifies the CPUCS register to stop or reset the CPU.
 * Returns false on error.
 */
static BOOL ezusb_cpucs (IOUSBDeviceInterface** dev, const uint16_t addr,
    const BOOL doRun)
{
    uint8_t data = doRun ? 0 : 1;

    if(verbose)
    {
        NSLog(@"%@", data ? @"stop CPU" : @"reset CPU");
    }

    const int status = ctrl_msg(dev,
        USBmakebmRequestType(kUSBOut, kUSBVendor, kUSBDevice),
        RW_INTERNAL, addr, 0, &data, 1);
    if(status != 1)
    {
        NSLog(@"Can't modify CPUCS");
        return FALSE;
    }

    // Successful
    return TRUE;
}


/*
 * Returns the size of the EEPROM (assuming one is present).
 * *data == 0 means it uses 8 bit addresses (or there is no EEPROM),
 * *data == 1 means it uses 16 bit addresses
 */
static inline int ezusb_get_eeprom_type (IOUSBDeviceInterface** dev, uint8_t* data)
{
    return ezusb_read (dev, @"get EEPROM size", GET_EEPROM_SIZE, 0, data, 1);
}


/*
 * Load an Intel HEX file into target RAM. The fd is the open "usbfs"
 * device, and the path is the name of the source file. Open the file,
 * parse the bytes, and write them in one or two phases.
 *
 * If stage == 0, this uses the first stage loader, built into EZ-USB
 * hardware but limited to writing on-chip memory or CPUCS.  Everything
 * is written during one stage, unless there's an error such as the image
 * holding data that needs to be written to external memory.
 *
 * Otherwise, things are written in two stages.  First the external
 * memory is written, expecting a second stage loader to have already
 * been loaded.  Then file is re-parsed and on-chip memory is written.
 */
static int ezusb_load_ram_ (IOUSBDeviceInterface** dev, const char* hexfilePath,
    const part_type partType, const int stage)
{
    FILE* image;
    uint16_t cpucs_addr;
    BOOL (*is_external)(const uint16_t off, const size_t len);
    struct ram_poke_context ctx;
    uint32_t status;

    image = fopen (hexfilePath, "r");
    if (image == 0)
    {
        NSLog(@"%@: unable to open for input.", hexfilePath);
        return -2;
    }
    else if (verbose)
    {
        NSLog(@"open RAM hexfile image %@", hexfilePath);
    }

    /* EZ-USB original/FX and FX2 devices differ, apart from the 8051 core */
    if (partType == ptFX2LP)
    {
        cpucs_addr = 0xe600;
        is_external = fx2lp_is_external;
    }
    else if (partType == ptFX2)
    {
        cpucs_addr = 0xe600;
        is_external = fx2_is_external;
    }
    else
    {
        cpucs_addr = 0x7f92;
        is_external = fx_is_external;
    }

    /* use only first stage loader? */
    if (stage == FALSE)
    {
        ctx.mode = internal_only;

        /* don't let CPU run while we overwrite its code/data */
        if (ezusb_cpucs (dev, cpucs_addr, 0) == FALSE)
        {
            return -1;
        }

        /* 2nd stage, first part? loader was already downloaded */
    }
    else
    {
        ctx.mode = skip_internal;

        /* let CPU run; overwrite the 2nd stage loader later */
        if (verbose)
        {
            NSLog(@"2nd stage:  write external memory");
        }
    }

    /* scan the image, first (maybe only) time */
    ctx.dev = dev;
    ctx.total = ctx.count = 0;
    status = parse_ihex (image, &ctx, is_external, ram_poke);
    if (status < 0)
    {
        NSLog(@"unable to download %@", hexfilePath);
        return status;
    }

    /* second part of 2nd stage: rescan */
    if (stage == TRUE)
    {
        ctx.mode = skip_external;

        /* don't let CPU run while we overwrite the 1st stage loader */
        if (ezusb_cpucs (dev, cpucs_addr, 0) == FALSE)
        {
            return -1;
        }

        /* at least write the interrupt vectors (at 0x0000) for reset! */
        rewind (image);
        if (verbose)
        {
            NSLog(@"2nd stage:  write on-chip memory");
        }

        status = parse_ihex (image, &ctx, is_external, ram_poke);
        if (status < 0)
        {
            NSLog(@"unable to completely download %@", hexfilePath);
            return status;
        }
    }

    if (verbose)
    {
        NSLog(@"... WROTE: %d bytes, %d segments, avg %d",
            ctx.total, ctx.count, ctx.total / ctx.count);
    }

    /* now reset the CPU so it runs what we just downloaded */
    if (ezusb_cpucs (dev, cpucs_addr, 1) == FALSE)
    {
        return -1;
    }

    return 0;
}

/*
 * Load an Intel HEX file into target (large) EEPROM, set up to boot from
 * that EEPROM using the specified microcontroller-specific config byte.
 * (Defaults:  FX2 0x08, FX 0x00, AN21xx n/a)
 *
 * Caller must have pre-loaded a second stage loader that knows how
 * to handle the EEPROM write requests.
 */
static int ezusb_load_eeprom_ (IOUSBDeviceInterface** dev, const char* hexfilePath,
    const part_type partType, uint8_t config)
{
    FILE* image;
    uint16_t cpucs_addr;
    BOOL (*is_external)(const uint16_t off, const size_t len);
    struct eeprom_poke_context ctx;
    uint32_t status;
    uint8_t value, first_byte;

    if (ezusb_get_eeprom_type (dev, &value) != 1 || value != 1)
    {
        NSLog(@"don't see a large enough EEPROM");
        return -1;
    }

    image = fopen (hexfilePath, "r");
    if (image == 0)
    {
        NSLog(@"%@: unable to open for input.", hexfilePath);
        return -2;
    }
    else if (verbose)
    {
        NSLog(@"open EEPROM hexfile image %@", hexfilePath);
    }

    if (verbose)
    {
        NSLog(@"2nd stage:  write boot EEPROM");
    }

    /* EZ-USB family devices differ, apart from the 8051 core */
    if(partType == ptFX2)
    {
        first_byte = 0xC2;
        cpucs_addr = 0xe600;
        is_external = fx2_is_external;
        ctx.ee_addr = 8;
        config &= 0x4f;
        NSLog(@
            "FX2:  config = 0x%02x, %sconnected, I2C = %d KHz",
            config,
            (config & 0x40) ? "dis" : "",
            // NOTE:  old chiprevs let CPU clock speed be set
            // or cycle inverted here.  You shouldn't use those.
            // (Silicon revs B, C?  Rev E is nice!)
            (config & 0x01) ? 400 : 100
            );
    }
    else if(partType == ptFX2LP)
    {
        first_byte = 0xC2;
        cpucs_addr = 0xe600;
        is_external = fx2lp_is_external;
        ctx.ee_addr = 8;
        config &= 0x4f;
        fprintf (stderr,
            "FX2LP:  config = 0x%02x, %sconnected, I2C = %d KHz",
            config,
            (config & 0x40) ? "dis" : "",
            (config & 0x01) ? 400 : 100
            );
    }
    else if(partType == ptFX)
    {
        first_byte = 0xB6;
        cpucs_addr = 0x7f92;
        is_external = fx_is_external;
        ctx.ee_addr = 9;
        config &= 0x07;
        NSLog(@
            "FX:  config = 0x%02x, %d MHz%s, I2C = %d KHz",
            config,
            ((config & 0x04) ? 48 : 24),
            (config & 0x02) ? " inverted" : "",
            (config & 0x01) ? 400 : 100
            );
    }
    else if(partType == ptAN21)
    {
        first_byte = 0xB2;
        cpucs_addr = 0x7f92;
        is_external = fx_is_external;
        ctx.ee_addr = 7;
        config = 0;
        NSLog(@"AN21xx:  no EEPROM config byte");
    }
    else
    {
        NSLog(@"?? Unrecognized microcontroller type %@ ??", partType);
        return -1;
    }

    /* make sure the EEPROM won't be used for booting,
     * in case of problems writing it
     */
    value = 0x00;
    status = ezusb_write (dev, @"mark EEPROM as unbootable",
        RW_EEPROM, 0, &value, sizeof value);
    if (status < 0)
    {
        return status;
    }

    /* scan the image, write to EEPROM */
    ctx.dev = dev;
    ctx.last = 0;
    status = parse_ihex (image, &ctx, is_external, eeprom_poke);
    if (status < 0)
    {
        NSLog(@"unable to write EEPROM %@", hexfilePath);
        return status;
    }

    /* append a reset command */
    value = 0;
    ctx.last = 1;
    status = eeprom_poke (&ctx, cpucs_addr, 0, &value, sizeof value);
    if (status < 0)
    {
        NSLog(@"unable to append reset to EEPROM %@", hexfilePath);
        return status;
    }

    /* write the config byte for FX, FX2 */
    else if(partType == ptAN21)
    {
        value = config;
        status = ezusb_write (dev, @"write config byte",
            RW_EEPROM, 7, &value, sizeof value);
        if (status < 0)
        {
            return status;
        }
    }

    /* EZ-USB FX has a reserved byte */
    else if(partType == ptFX)
    {
        value = 0;
        status = ezusb_write (dev, @"write reserved byte",
            RW_EEPROM, 8, &value, sizeof value);
        if (status < 0)
        {
            return status;
        }
    }

    /* make the EEPROM say to boot from this EEPROM */
    status = ezusb_write (dev, @"write EEPROM type byte",
        RW_EEPROM, 0, &first_byte, sizeof first_byte);
    if (status < 0)
    {
        return status;
    }

    /* Note:  VID/PID/version aren't written.  They should be
     * written if the EEPROM type is modified (to B4 or C0).
     */

    return 0;
}

/*
 * Parse an Intel HEX image file and invoke the poke() function on the
 * various segments to implement policies such as writing to RAM (with
 * a one or two stage loader setup, depending on the firmware) or to
 * EEPROM (two stages required).
 *
 * image    - the hex image file
 * context  - for use by poke()
 * is_external  - if non-null, used to check which segments go into
 *        external memory (writable only by software loader)
 * poke     - called with each memory segment; errors indicated
 *        by returning negative values.
 *
 * Caller is responsible for halting CPU as needed, such as when
 * overwriting a second stage loader.
 */
int parse_ihex (FILE* image,
                void* context,
                BOOL (*is_external)(const uint16_t addr, const size_t len),
                int (*poke) (void *context, const uint16_t addr, BOOL external,
                    const uint8_t *data, const size_t len)
)
{
    uint8_t data [1023];
    uint16_t data_addr = 0;
    size_t data_len = 0;
    uint32_t first_line = 1;
    BOOL external = FALSE;
    int rc;

    /* Read the input file as an IHEX file, and report the memory segments
     * as we go.  Each line holds a max of 16 bytes, but downloading is
     * faster (and EEPROM space smaller) if we merge those lines into larger
     * chunks.  Most hex files keep memory segments together, which makes
     * such merging all but free.  (But it may still be worth sorting the
     * hex files to make up for undesirable behavior from tools.)
     *
     * Note that EEPROM segments max out at 1023 bytes; the download protocol
     * allows segments of up to 64 KBytes (more than a loader could handle).
     */
    for (;;) {
        char        buf [512], *cp;
        char        tmp, type;
        size_t      len;
        unsigned    idx, off;

        cp = fgets(buf, sizeof buf, image);
        if (cp == 0) {
            NSLog(@"EOF without EOF record!");
            break;
        }

        /* EXTENSION: "# comment-till-end-of-line", for copyrights etc */
        if (buf[0] == '#')
            continue;

        if (buf[0] != ':') {
            NSLog(@"not an ihex record: %s", buf);
            return -2;
        }

        /* ignore any newline */
        cp = strchr (buf, '\n');
        if (cp)
            *cp = 0;

        if (verbose >= 3)
            NSLog(@"** LINE: %s", buf);

        /* Read the length field (up to 16 bytes) */
        tmp = buf[3];
        buf[3] = 0;
        len = strtoul(buf+1, 0, 16);
        buf[3] = tmp;

        /* Read the target offset (address up to 64KB) */
        tmp = buf[7];
        buf[7] = 0;
        off = strtoul(buf+3, 0, 16);
        buf[7] = tmp;

   /* Initialize data_addr */
    if (first_line) {
        data_addr = off;
        first_line = 0;
    }

    /* Read the record type */
    tmp = buf[9];
    buf[9] = 0;
    type = strtoul(buf+7, 0, 16);
    buf[9] = tmp;

    /* If this is an EOF record, then make it so. */
    if (type == 1) {
        if (verbose >= 2)
        NSLog(@"EOF on hexfile");
        break;
    }

    if (type != 0) {
        NSLog(@"unsupported record type: %u", type);
        return -3;
    }

    if ((len * 2) + 11 > strlen(buf)) {
        NSLog(@"record too short?");
        return -4;
    }

    // FIXME check for _physically_ contiguous not just virtually
    // e.g. on FX2 0x1f00-0x2100 includes both on-chip and external
    // memory so it's not really contiguous

    /* flush the saved data if it's not contiguous,
     * or when we've buffered as much as we can.
     */
     if (data_len != 0
             && (off != (data_addr + data_len)
             // || !merge
             || (data_len + len) > sizeof data)) {
         if (is_external)
         external = is_external (data_addr, data_len);
         rc = poke (context, data_addr, external, data, data_len);
         if (rc < 0)
         return -1;
         data_addr = off;
         data_len = 0;
     }

     /* append to saved data, flush later */
     for (idx = 0, cp = buf+9 ;  idx < len ;  idx += 1, cp += 2) {
         tmp = cp[2];
         cp[2] = 0;
         data [data_len + idx] = strtoul(cp, 0, 16);
         cp[2] = tmp;
     }
     data_len += len;
     }


     /* flush any data remaining */
     if (data_len != 0) {
     if (is_external)
         external = is_external (data_addr, data_len);
     rc = poke (context, data_addr, external, data, data_len);
     if (rc < 0)
         return -1;
     }
     return 0;
 }

static int ram_poke (void* context, const uint16_t addr,  const BOOL external,
    const uint8_t *data, const size_t len)
{
     struct ram_poke_context *ctx = context;
     int rc = 0;
     uint32_t retry = 0;

     switch (ctx->mode)
     {
         case internal_only:     /* CPU should be stopped */
         {
             if (external)
             {
                 NSLog(@"can't write %zd bytes external memory at 0x%04x", len, addr);
                 return -EINVAL;
             }

             break;
         }
         case skip_internal:     /* CPU must be running */
         {
             if (!external)
             {
                 if (verbose >= 2)
                 {
                     NSLog(@"SKIP on-chip RAM, %zd bytes at 0x%04x", len, addr);
                 }

                 return 0;
             }
             break;
         }
         case skip_external:     /* CPU should be stopped */
         {
             if (external)
             {
                  if (verbose >= 2)
                  {
                      NSLog(@"SKIP external RAM, %zd bytes at 0x%04x", len, addr);
                  }

                  return 0;
              }
              break;
         }
         default:
         {
             NSLog(@"bug");
             return -EDOM;
         }
     }

     ctx->total += len;
     ++ctx->count;

     /* Retry this till we get a real error. Control messages are not
      * NAKed (just dropped) so time out means is a real problem.
      */
     while ((rc = ezusb_write (ctx->dev,
             external ? @"write external" : @"write on-chip",
             external ? RW_MEMORY : RW_INTERNAL,
             addr, data, len)) < 0
         && retry < RETRY_LIMIT)
     {
        if (errno != ETIMEDOUT)
        {
            break;
        }

        retry += 1;
     }
     return (rc < 0) ? -errno : 0;
 }

static int eeprom_poke (void* context, const uint16_t addr, const BOOL external,
    const uint8_t* data, const size_t len)
{
    struct eeprom_poke_context* ctx = context;
    int rc = 0;
    uint8_t header [4];

    if (external)
    {
        NSLog(@"EEPROM can't init %zd bytes external memory at 0x%04x", len, addr);
        return -EINVAL;
    }

    if (len > 1023)
    {
        NSLog(@"not fragmenting %zd bytes", len);
        return -EDOM;
    }

    /* NOTE:  No retries here.  They don't seem to be needed;
    * could be added if that changes.
    */

    /* write header */
    header [0] = len >> 8;
    header [1] = len;
    header [2] = addr >> 8;
    header [3] = addr;
    if (ctx->last)
    {
        header [0] |= 0x80;
    }

    if ((rc = ezusb_write (ctx->dev, @"write EEPROM segment header",
         RW_EEPROM,
         ctx->ee_addr, header, 4)) < 0)
    {
        return rc;
    }

    /* write code/data */
    if ((rc = ezusb_write (ctx->dev, @"write EEPROM segment",
         RW_EEPROM,
         ctx->ee_addr + 4, data, len)) < 0)
    {
        return rc;
    }

    /* next shouldn't overwrite it */
    ctx->ee_addr += 4 + len;

    return 0;
    }


//- From Apple's examples ---------------------------------------------------------------------

static void
MyCallBackFunction(void *dummy, IOReturn result, void *arg0)
{
	//  UInt8	inPipeRef = (UInt32)dummy;
    
    printf("MyCallbackfunction: %d, %d, %d\n", (int)dummy, (int)result, (int)arg0);
    CFRunLoopStop(CFRunLoopGetCurrent());
}


static void transferData(IOUSBInterfaceInterface245 **intf, UInt8 inPipeRef, UInt8 outPipeRef)
{
    IOReturn			err;
    CFRunLoopSourceRef		cfSource;
    int				i;
    static char			outBuf[8096];
    
    err = (*intf)->CreateInterfaceAsyncEventSource(intf, &cfSource);
    if (err)
    {
	printf("transferData: unable to create event source, err = %08x\n", err);
	return;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), cfSource, kCFRunLoopDefaultMode);
    for (i=0; i < 12; i++)
	outBuf[i] = 'R';
    err = (*intf)->WritePipeAsync(intf, outPipeRef, outBuf, 12, (IOAsyncCallback1)MyCallBackFunction, (void*)(UInt32)inPipeRef);
    if (err)
    {
	printf("transferData: WritePipeAsyncFailed, err = %08x\n", err);
	CFRunLoopRemoveSource(CFRunLoopGetCurrent(), cfSource, kCFRunLoopDefaultMode);
	return;
    }
    printf("transferData: calling CFRunLoopRun\n");
    CFRunLoopRun();
    printf("transferData: returned from  CFRunLoopRun\n");
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), cfSource, kCFRunLoopDefaultMode);
}


static void dealWithPipes_(IOUSBInterfaceInterface245 **intf, UInt8 numPipes)
{
    int					i;
    IOReturn				err;			
    UInt8				inPipeRef = 0;
    UInt8				outPipeRef = 0;
    UInt8				direction, number, transferType, interval;
    UInt16				maxPacketSize;
    
    // pipes are one based, since zero is the default control pipe
    for (i=1; i <= numPipes; i++)
    {
	err = (*intf)->GetPipeProperties(intf, i, &direction, &number, &transferType, &maxPacketSize, &interval);
	if (err)
	{
	    printf("dealWithPipes: unable to get pipe properties for pipe %d, err = %08x\n", i, err);
	    return;
	}
	if (transferType != kUSBBulk)
	{
	    printf("dealWithPipes: skipping pipe %d because it is not a bulk pipe\n", i);
	    continue;
	}
	if ((direction == kUSBIn) && !inPipeRef)
	{
	    printf("dealWithPipes: grabbing BULK IN pipe index %d, number %d\n",i, number);
	    inPipeRef = i;
	}
	if ((direction == kUSBOut) && !outPipeRef)
	{
	    printf("dealWithPipes: grabbing BULK OUT pipe index %d, number %d\n", i, number);
	    outPipeRef = i;
	}
    }
//    if (inPipeRef && outPipeRef)
//	transferData(intf, inPipeRef, outPipeRef);
}

static void dealWithPipes(IOUSBInterfaceInterface245 **intf, UInt8 numPipes)
{
  for (int pipeRef = 1; pipeRef <= numPipes; pipeRef++)
    {
      IOReturn	kr2;
      UInt8	direction;
      UInt8	number;
      UInt8	transferType;
      UInt16	maxPacketSize;
      UInt8	interval;
      char	*message;

      kr2 = (*intf)->GetPipeProperties(intf, pipeRef, &direction, &number, &transferType, &maxPacketSize, &interval);
      if (kIOReturnSuccess != kr2)
        printf("unable to get properties of pipe %d (%08x)\n", pipeRef, kr2);
      else {
        printf("pipeRef %d: ", pipeRef);

        switch (direction) {
        case kUSBOut:
          message = "out";
          break;
        case kUSBIn:
          message = "in";
          break;
        case kUSBNone:
          message = "none";
          break;
        case kUSBAnyDirn:
          message = "any";
          break;
        default:
          message = "???";
        }
        printf("direction %s, ", message);
        switch (transferType) {
        case kUSBControl:
          message = "control";
          break;
        case kUSBIsoc:
          message = "isoc";
          break;
        case kUSBBulk:
          message = "bulk";
          break;
        case kUSBInterrupt:
          message = "interrupt";
          break;
        case kUSBAnyType:
          message = "any";
          break;
        default:
          message = "???";
        }
        printf("transfer type %s, maxPacketSize %d\n", message, maxPacketSize);
      }
    }
}

static void dealWithInterface(io_service_t usbInterfaceRef)
{
    IOReturn					err;
    IOCFPlugInInterface 		**iodev;		// requires <IOKit/IOCFPlugIn.h>
    IOUSBInterfaceInterface245 	**intf;
    SInt32						score;
    UInt8						numPipes;


    err = IOCreatePlugInInterfaceForService(usbInterfaceRef, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, &iodev, &score);
    if (err || !iodev)
    {
		printf("dealWithInterface: unable to create plugin. ret = %08x, iodev = %p\n", err, iodev);
		return;
    }
    err = (*iodev)->QueryInterface(iodev, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID245), (LPVOID)&intf);
	IODestroyPlugInInterface(iodev);				// done with this
	
    if (err || !intf)
    {
		printf("dealWithInterface: unable to create a device interface. ret = %08x, intf = %p\n", err, intf);
		return;
    }
    err = (*intf)->USBInterfaceOpen(intf);
    if (err)
    {
		printf("dealWithInterface: unable to open interface. ret = %08x\n", err);
		return;
    }
    err = (*intf)->GetNumEndpoints(intf, &numPipes);
    if (err)
    {
		printf("dealWithInterface: unable to get number of endpoints. ret = %08x\n", err);
		(*intf)->USBInterfaceClose(intf);
		(*intf)->Release(intf);
		return;
    }
    
    printf("dealWithInterface: found %d pipes\n", numPipes);
    if (numPipes == 0)
    {
		// try alternate setting 1
		err = (*intf)->SetAlternateInterface(intf, 1);
		if (err)
		{
			printf("dealWithInterface: unable to set alternate interface 1. ret = %08x\n", err);
			(*intf)->USBInterfaceClose(intf);
			(*intf)->Release(intf);
			return;
		}
		err = (*intf)->GetNumEndpoints(intf, &numPipes);
		if (err)
		{
			printf("dealWithInterface: unable to get number of endpoints - alt setting 1. ret = %08x\n", err);
			(*intf)->USBInterfaceClose(intf);
			(*intf)->Release(intf);
			return;
		}
		numPipes = 13;  		// workaround. GetNumEndpoints does not work after SetAlternateInterface
    }
    
    if (numPipes)
	dealWithPipes(intf, numPipes);
	
    err = (*intf)->USBInterfaceClose(intf);
    if (err)
    {
		printf("dealWithInterface: unable to close interface. ret = %08x\n", err);
		return;
    }
    err = (*intf)->Release(intf);
    if (err)
    {
		printf("dealWithInterface: unable to release interface. ret = %08x\n", err);
		return;
    }
}


static int dealWithDevice(IOUSBDeviceInterface **dev)
{
    IOReturn						err;
    UInt8							numConf;
    IOUSBConfigurationDescriptorPtr	confDesc;
    IOUSBFindInterfaceRequest		interfaceRequest;
    io_iterator_t					iterator;
    io_service_t					usbInterfaceRef;
    
    err = (*dev)->USBDeviceOpen(dev);
    if (err)
    {
		printf("dealWithDevice: unable to open device. ret = %08x\n", err);
		return err;
    }
    err = (*dev)->GetNumberOfConfigurations(dev, &numConf);
    if (err || !numConf)
    {
		printf("dealWithDevice: unable to obtain the number of configurations. ret = %08x\n", err);
        (*dev)->USBDeviceClose(dev);
        (*dev)->Release(dev);
		return err;
    }
    printf("dealWithDevice: found %d configurations\n", numConf);
    err = (*dev)->GetConfigurationDescriptorPtr(dev, 0, &confDesc);			// get the first config desc (index 0)
    if (err)
    {
		printf("dealWithDevice:unable to get config descriptor for index 0\n");
        (*dev)->USBDeviceClose(dev);
        (*dev)->Release(dev);
		return err;
    }
    err = (*dev)->SetConfiguration(dev, confDesc->bConfigurationValue);
    if (err)
    {
		printf("dealWithDevice: unable to set the configuration\n");
        (*dev)->USBDeviceClose(dev);
        (*dev)->Release(dev);
		return err;
    }
    
    interfaceRequest.bInterfaceClass = kIOUSBFindInterfaceDontCare;		// requested class
    interfaceRequest.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;		// requested subclass
    interfaceRequest.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;		// requested protocol
    interfaceRequest.bAlternateSetting = kIOUSBFindInterfaceDontCare;		// requested alt setting
    
    err = (*dev)->CreateInterfaceIterator(dev, &interfaceRequest, &iterator);
    if (err)
    {
		printf("dealWithDevice: unable to create interface iterator\n");
        (*dev)->USBDeviceClose(dev);
        (*dev)->Release(dev);
		return err;
    }
    
    while ( (usbInterfaceRef = IOIteratorNext(iterator)) )
    {
		printf("found interface: %p\n", (void*)usbInterfaceRef);
		dealWithInterface(usbInterfaceRef);
		IOObjectRelease(usbInterfaceRef);				// no longer need this reference
    }
    
    IOObjectRelease(iterator);
    iterator = 0;

    err = (*dev)->USBDeviceClose(dev);
    if (err)
    {
		printf("dealWithDevice: error closing device - %08x\n", err);
		(*dev)->Release(dev);
		return err;
    }

    /*
    err = (*dev)->Release(dev);
    if (err)
    {
		printf("dealWithDevice: error releasing device - %08x\n", err);
		return err;
    }
    */

    return 0;
}

//----------------------------------------------------------------------

int ezusb_new
(ezusb_t* ezusb,
 int vendor_id,
 int product_id)
{
    *ezusb = NULL;
    int rc = 0;
    mach_port_t masterPort = 0;
    kern_return_t err = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if(err)
    {
        return -1;
    }

    NSMutableDictionary* matchingDict = (NSMutableDictionary*)IOServiceMatching(kIOUSBDeviceClassName);
    if(matchingDict == nil)
    {
        mach_port_deallocate(mach_task_self(), masterPort);
        return -1;
    }

    NSNumber* vid = [NSNumber numberWithInt:vendor_id];
    NSNumber* pid = [NSNumber numberWithInt:product_id];
    [matchingDict setValue:vid forKey:[NSString stringWithCString:kUSBVendorID encoding:NSASCIIStringEncoding]];
    [matchingDict setValue:pid forKey:[NSString stringWithCString:kUSBProductID encoding:NSASCIIStringEncoding]];

    io_iterator_t iterator = 0;
    err = IOServiceGetMatchingServices(masterPort, (CFMutableDictionaryRef)matchingDict, &iterator);

    // This adds a reference we must release below
    io_service_t usbDeviceRef = IOIteratorNext(iterator);
    IOObjectRelease(iterator);

    if(usbDeviceRef == 0)
    {
        NSLog(@"Couldn't find USB device with %x:%x",
            [vid unsignedIntValue], [pid unsignedIntValue]);
        rc = -1;
    }

    // Now, get the locationID of this device. In order to do this, we need to
    // create an IOUSBDeviceInterface for our device. This will create the
    // necessary connections between our userland application and the kernel
    // object for the USB Device.
    else
    {
        SInt32 score;
        IOCFPlugInInterface** plugInInterface = 0;

        err = IOCreatePlugInInterfaceForService(usbDeviceRef,
                                                kIOUSBDeviceUserClientTypeID,
                                                kIOCFPlugInInterfaceID,
                                                &plugInInterface, &score);
        if (err != kIOReturnSuccess || plugInInterface == 0)
        {
            NSLog(@"IOCreatePlugInInterfaceForService returned 0x%08x.", err);
            rc = -2;
        }
        else
        {
            IOUSBDeviceInterface** dev = 0;
            // Use the plugin interface to retrieve the device interface.
            err = (*plugInInterface)->QueryInterface(plugInInterface,
                                                     CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
                                                     (LPVOID*)&dev);

            // Now done with the plugin interface.
            (*plugInInterface)->Release(plugInInterface);

            if (err || dev == NULL)
            {
               NSLog(@"QueryInterface returned %d.", err);
               rc = -3;
            }
            else {
              err = 0; //dealWithDevice(dev);

              if (err || dev == NULL)
                {
                  NSLog(@"dealWithDevice returned %d.", err);
                  rc = -3;
                }
              else {
                ezusb_t ezusb_ = calloc (1, sizeof(*ezusb_));
                ezusb_->dev = dev;
                *ezusb = ezusb_;
              }
            }
        }
    }

    IOObjectRelease(usbDeviceRef);
    mach_port_deallocate(mach_task_self(), masterPort);

    return rc;
}

void ezusb_free
(ezusb_t ezusb) {
  if (!ezusb) return;
  if (ezusb->dev) (*(ezusb->dev))->Release(ezusb->dev);
  free (ezusb);
}

int ezusb_load_ram
(ezusb_t ezusb,
 const char* hexfilePath,
 const part_type partType,
 const int stage) {

  return ezusb_load_ram_ (ezusb->dev, hexfilePath, partType, stage);
}

int ezusb_load_eeprom
(ezusb_t ezusb,
 const char* hexfilePath,
 const part_type partType,
 uint8_t config) {

  return ezusb_load_eeprom_ (ezusb->dev, hexfilePath, partType, config);
}

