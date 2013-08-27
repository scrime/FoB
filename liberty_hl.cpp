/* FoB - GUI for 3D Trackers
   Copyright (C) 2007, 2008, 2009 SCRIME, universite' Bordeaux 1

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
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include "ezusb.h"
#include "PiTracker.h"
#include "liberty_hl.h"

// A bird record should be: [header: 8 bytes, x: 4 bytes, y: 4 bytes,
// z: 4 bytes, za: 4 bytes, ya: 4 bytes, xa: 4 bytes].
#define BIRD_RECORD_SIZE 32
#define RECORD_SIZE (NUMBER_OF_BIRDS * BIRD_RECORD_SIZE)

static const int vendor_id = 0x0f44;
static const int raw_product_id = 0xff21;
static const int configured_product_id = 0xff20;
static const int write_endpoint = 0x04;
static const int read_endpoint = 0x88;

typedef void (*fill_bird_record_t) (bird_record_t record, unsigned char* uc);

struct liberty_s {
  int fd;
  fd_set input_fd_set;

  unsigned char buf[1024];
  size_t pos;
  struct bird_record_s bird_records[NUMBER_OF_BIRDS];
  int sync;

  unsigned long syncs;
  unsigned long tagSuccesses;
  unsigned long stationNumberErrors;
  unsigned char stations[256];
  unsigned long errorIndicators;
  unsigned long errorIndicatorCounts[256];
  unsigned long sizeErrors;

  fill_bird_record_t fill_bird_record;

  // For USB.
  PiTracker* tracker;
  int usb_transfer;
  int sockfd;
  pthread_mutex_t usb_mutex;
  pthread_t usb_read_thread;
  pthread_t usb_write_thread;

  // For RS232.
  struct termios initialAtt;
  struct termios newAtt;
};

static char empty_path[] = { 0 };
static char* firmware_path = empty_path;

void liberty_set_firmware_path (const char* path) {
  if (firmware_path != empty_path) free (firmware_path);
  firmware_path = strdup (path);
}

static int usb_check () {
  ezusb_t ezusb = 0;

  if (0) {
    // Using fork and executing fxload.
    printf ("Trying to exec fxload in %s\n", firmware_path);
    pid_t newPID = fork();

    if(newPID == 0) {
      chdir (firmware_path);
      execlp ("./fxload", "./fxload",
              "-t", "fx2lp",
              "-D", "0f44:ff21",
              "-I", "LbtyUsbHS.hex",
              "-s", "a3load.hex",
              NULL);

      printf ("Failed to exec fxload (err = %d: %s)\n", errno, strerror (errno));
    }
    else if(newPID == -1) {
      printf ("Fork failed to make new process for driver (err = %d: %s)\n", errno, strerror (errno));
    }

    // FIXME: unfinished.  Using our own integrated version of fxload
    // code was found more flexible.
    return 0;
  }

  // Using our own integrated version of fxload code.
  int status = 0;

  // Try the already configured device first.
  status = ezusb_new (&ezusb, vendor_id, configured_product_id);
  if (!status) {
    ezusb_free (ezusb);
    return 1;
  }

  // Configured device absent.  Find the raw device and upload
  // firmware.
  status = ezusb_new (&ezusb, vendor_id, raw_product_id);
  if (status) {
    printf ("Can't find Liberty raw USB device\n");
    return 0;
  }

  chdir (firmware_path);

  // First stage loader.
  status = ezusb_load_ram (ezusb, "a3load.hex", ptFX2LP, 0);
  if (status) {
    printf ("Can't upload first stage loader\n");
    return 0;
  }

  // Second stage firmware.
  status = ezusb_load_ram (ezusb, "LbtyUsbHS.hex", ptFX2LP, 1);
  if (status) {
    printf ("Can't upload second stage loader\n");
    return 0;
  }

  ezusb_free (ezusb);

  // Get the configured device.
  for (int i = 0; i < 5; i++) {
    sleep (1);
    status = ezusb_new (&ezusb, vendor_id, configured_product_id);
    if (!status) {
      ezusb_free (ezusb);
      return 1;
    }
  }

  printf("Can't find Liberty USB device\n");
  return 0;
}

static void fill_bird_record_big_endian (bird_record_t record, unsigned char* uc) {
  float* res = &record->x;
  for (int i = 0; i < 6; i++, res++, uc += 4) {
    unsigned char* res_ = (unsigned char*) res;
    res_[0] = uc[3];
    res_[1] = uc[2];
    res_[2] = uc[1];
    res_[3] = uc[0];
  }
}

static void fill_bird_record_little_endian (bird_record_t record, unsigned char* uc) {
  float* res = &record->x;
  for (int i = 0; i < 6; i++, res++, uc += 4) {
    unsigned char* res_ = (unsigned char*) res;
    res_[0] = uc[0];
    res_[1] = uc[1];
    res_[2] = uc[2];
    res_[3] = uc[3];
  }
}

static void liberty_init (liberty_t liberty) {
  liberty->fd = -1;
  liberty->sockfd = -1;
  liberty->usb_transfer = 0;

  FD_ZERO (&liberty->input_fd_set);
  memset (liberty->buf, 0, sizeof (liberty->buf));
  memset (liberty->bird_records, 0, sizeof (liberty->bird_records));
  liberty->pos = 0;
  liberty->sync = 0;

  liberty->tagSuccesses = 0;
  liberty->stationNumberErrors = 0;
  memset (liberty->stations, 0, sizeof (liberty->stations));
  liberty->errorIndicators = 0;
  memset (liberty->errorIndicatorCounts, 0, sizeof (liberty->errorIndicatorCounts));
  liberty->sizeErrors = 0;
  liberty->syncs = 0;

  long one = 1;
  int big_endian = !(*((char*) &one));
  liberty->fill_bird_record = big_endian ?
    fill_bird_record_big_endian :
    fill_bird_record_little_endian;
}

static void debug_buffer (const char* str, unsigned char* buf, int len) {
  printf("%s: %d bytes\n", str, len);
  for (int i = 0; i < len; i++) {
    printf ("%02x ", buf[i]);
    printf ("%c ", isprint(buf[i]) ? buf[i] : '.');
    if ((i + 1) % 8 == 0) printf("\n");
  }
  printf("\n");
}

static void* usb_read_thread (void* data) {
  printf("USB read thread start\n");
  liberty_t liberty = (liberty_t) data;
  size_t total = 0;

  while (liberty->usb_transfer) {
    unsigned char buf[512];
    int len = liberty->tracker->ReadTrkData (buf, sizeof (buf));
    if (len <= 0) {
      usleep (2000);
      continue;
    }

    /*
    static int ndisplays = 0;
    if (ndisplays < 10) {
      ndisplays++;
      debug_buffer ("usb_read_thread", buf, len);
    }
    */

    pthread_mutex_lock (&liberty->usb_mutex);
    write (liberty->sockfd, buf, len);
    pthread_mutex_unlock (&liberty->usb_mutex);
    total += len;
  }

  printf("USB read thread end: %lu bytes read\n", total);
  return 0;
}

static void* usb_write_thread (void* data) {
  printf("USB write thread start\n");
  liberty_t liberty = (liberty_t) data;
  size_t total = 0;

  fd_set read_fd_set;
  FD_ZERO(&read_fd_set);
  FD_SET (liberty->sockfd, &read_fd_set);

  while (liberty->usb_transfer) {
    unsigned char buf[512];

    fd_set read_fd_set_ = read_fd_set;
    struct timeval tv = { 0, 100000 };
    int sel = select (liberty->sockfd + 1, &read_fd_set_, 0, 0, &tv);
    if (sel <= 0) continue;

    pthread_mutex_lock (&liberty->usb_mutex);
    int len = read (liberty->sockfd, buf, sizeof (buf));
    pthread_mutex_unlock (&liberty->usb_mutex);
    if (len <= 0) continue;

    /*
    static int ndisplays = 0;
    if (ndisplays < 10) {
      ndisplays++;
      debug_buffer ("usb_write_thread", buf, len);
    }
    */

    liberty->tracker->WriteTrkData (buf, len);
    total += len;
  }

  printf("USB write thread end: %lu bytes written\n", total);
  return 0;
}

liberty_t liberty_new () {
  liberty_t liberty = (liberty_t) calloc (1, sizeof (*liberty));
  liberty_init (liberty);
  return liberty;
}

void liberty_free (liberty_t liberty) {
  liberty_close (liberty);
  free (liberty);
}

int liberty_open (liberty_t liberty, const char* file) {
  liberty_close (liberty);

  if (usb_check ()) {
    // Using USB.
    if (!liberty->tracker) liberty->tracker = new PiTracker ();

    if (liberty->tracker->UsbConnect (vendor_id, configured_product_id, write_endpoint, read_endpoint))
      return LIBERTY_ERROR_OPEN_DEVICE;

    int fds[2] = { -1, -1 };
    if (socketpair (AF_UNIX, SOCK_STREAM, 0, fds)) {
      perror ("socketpair");
      liberty_close (liberty);
      return LIBERTY_ERROR_OPEN_DEVICE;
    }

    liberty->sockfd = fds[0];
    liberty->fd = fds[1];

    /*
    if ((fcntl (liberty->sockfd, F_SETFL, O_NONBLOCK)) == -1) {
      perror ("fcntl sockfd");
      liberty_close (liberty);
      return LIBERTY_ERROR_OPEN_DEVICE;
    }
    */

    if ((fcntl (liberty->fd, F_SETFL, O_NONBLOCK)) == -1) {
      perror ("fcntl fd");
      liberty_close (liberty);
      return LIBERTY_ERROR_OPEN_DEVICE;
    }

    liberty->usb_transfer = 1;
    pthread_mutex_init (&liberty->usb_mutex, 0);
    pthread_create (&liberty->usb_write_thread, 0, usb_write_thread, (void*) liberty);
    pthread_create (&liberty->usb_read_thread, 0, usb_read_thread, (void*) liberty);
  }

  else {
    // Using RS232.
    liberty->fd = open (file, O_RDWR | O_NOCTTY | O_NDELAY);
    if (liberty->fd == -1) {
      return LIBERTY_ERROR_OPEN_DEVICE;
    }

    // Set up terminal for raw data.
    if (tcgetattr (liberty->fd, &liberty->initialAtt)) {
      liberty_close (liberty);
      return LIBERTY_ERROR_GET_TERMINAL_ATTRIBUTES;
    }

    liberty->newAtt = liberty->initialAtt;

    // Seem to be important settings (especially on Mac OS X), taken
    // from libflock.
    liberty->newAtt.c_iflag = IGNBRK | IGNPAR;
    liberty->newAtt.c_oflag = 0;
    liberty->newAtt.c_cflag = CS8 | CLOCAL | CREAD;
    liberty->newAtt.c_lflag = NOFLSH;
    liberty->newAtt.c_cc[VMIN] = 0;
    liberty->newAtt.c_cc[VTIME] = 0;
    liberty->newAtt.c_ispeed = B115200;
    liberty->newAtt.c_ospeed = B115200;

    cfmakeraw (&liberty->newAtt);
    cfsetspeed (&liberty->newAtt, B115200);

    if (tcsetattr (liberty->fd, TCSANOW, &liberty->newAtt)) {
      liberty_close (liberty);
      return LIBERTY_ERROR_SET_TERMINAL_ATTRIBUTES;
    }

    tcflush (liberty->fd, TCIOFLUSH);
  }

  // Establish comm and clear out any residual trash data.
  int len = 0;
  int count = 0;
  unsigned char buf[64];

  while (1) {
    write (liberty->fd, "\r", 1);
    usleep (100000);
    len = read (liberty->fd, buf, sizeof (buf));
    if (len > 0) break;
    if (count++ >= 100) break;
  }

  if (len <= 0) {
    liberty_close (liberty);
    return LIBERTY_ERROR_READ_DEVICE;
  }

  if (liberty->tracker) {
    while (1) {
      int len = read (liberty->fd, buf, sizeof (buf));
      if (len <= 0) break;
    }
  }
  else
    tcflush (liberty->fd, TCIOFLUSH);

  // Configure device.
  write (liberty->fd, "F1\r", 3); // Binary output.
  write (liberty->fd, "U1\r", 3); // Centimeters.
  write (liberty->fd, "O*,2,4\r", 7); // Position and angles.
  write (liberty->fd, "C\r", 2); // Continuous mode.

  FD_SET (liberty->fd, &liberty->input_fd_set);
  return LIBERTY_ERROR_NO_ERROR;
}

static char* liberty_error_string (int error) {
  char* result = "unknown error";

  switch (error) {
  case 0x00: result = "no error"; break;
  case 0x01: result = "invalid command"; break;
  case 0x02: result = "invalid station"; break;
  case 0x03: result = "invalid parameter"; break;
  case 0x04: result = "too few parameters"; break;
  case 0x05: result = "too many parameters"; break;
  case 0x06: result = "parameter below limit"; break;
  case 0x07: result = "parameter above limit"; break;
  case 0x08: result = "communication failure with sensor processor board"; break;
  case 0x09: result = "error initiating sensor processor 1"; break;
  case 0x0a: result = "error initiating sensor processor 2"; break;
  case 0x0b: result = "error initiating sensor processor 3"; break;
  case 0x0c: result = "error initiating sensor processor 4"; break;
  case 0x0d: result = "no sensor processors detected"; break;
  case 0x0e: result = "error initiating source processor"; break;
  case 0x0f: result = "memory allocation error"; break;
  case 0x10: result = "excessive command characters entered"; break;
  case 0x11: result = "you must exist UTH mode to send this command"; break;
  case 0x12: result = "error reading source prom, using defaults"; break;
  case 0x13: result = "this is a read only command"; break;
  case 0x14: result = "non-fatal text message"; break;
  case 0x15: result = "error loading map"; break;
  case 0x16: result = "error synchronizing sensors"; break;
  case 0x17: result = "firmware incompatible with IO proc code, upgrade required"; break;
  case 0x18: result = "outdated IO processor code, upgrade recommanded"; break;
  case 0x20: result = "no error"; break;
  case 0x41: result = "source fail x + bit errors"; break;
  case 0x42: result = "source fail y + bit errors"; break;
  case 0x43: result = "source fail xy + bit errors"; break;
  case 0x44: result = "source fail z + bit errors"; break;
  case 0x45: result = "source fail xz + bit errors"; break;
  case 0x46: result = "source fail yz + bit errors"; break;
  case 0x47: result = "source fail xyz + bit errors"; break;
  case 0x49: result = "bit errors"; break;
  case 0x61: result = "source fail x"; break;
  case 0x62: result = "source fail y"; break;
  case 0x63: result = "source fail xy"; break;
  case 0x64: result = "source fail z"; break;
  case 0x65: result = "source fail xz"; break;
  case 0x66: result = "source fail yz"; break;
  case 0x67: result = "source fail xyz"; break;
  case 0x75: result = "position outside of mapped area"; break;
  default: break;
  }

  return result;
}

void liberty_close (liberty_t liberty) {
  if (liberty->fd < 0) return;

  if (liberty->tracker) {

    printf("Liberty syncs: %lu\n", liberty->syncs);
    printf("Liberty tag successes: %lu\n", liberty->tagSuccesses);

    printf("Liberty station number errors: %lu\n", liberty->stationNumberErrors);
    printf("  stations found:");
    for (int i = 0; i < 256; i++) {
      if (liberty->stations[i] != 0) {
        printf(" #%d", i);
      }
    }
    printf("\n");

    printf("Liberty record error indicators: %lu\n", liberty->errorIndicators);
    for (int i = 0; i < 256; i++) {
      if (liberty->errorIndicatorCounts[i] != 0) {
        printf("  %s: %lu\n", liberty_error_string(i), liberty->errorIndicatorCounts[i]);
      }
    }

    printf("Liberty size errors: %lu\n", liberty->sizeErrors);

    liberty->usb_transfer = 0;
    close (liberty->sockfd);
    close (liberty->fd);
    pthread_join (liberty->usb_read_thread, 0);
    pthread_join (liberty->usb_write_thread, 0);
    pthread_mutex_destroy (&liberty->usb_mutex);
    delete liberty->tracker;
    liberty->tracker = 0;
  }
  else {
    tcflush (liberty->fd, TCIOFLUSH);
    tcsetattr (liberty->fd, TCSANOW, &liberty->initialAtt);
    close (liberty->fd);
  }

  liberty_init (liberty);
}

int liberty_get_file_descriptor (liberty_t liberty) {
  return liberty->fd;
}

static void liberty_push (liberty_t liberty, unsigned char* buf, size_t len) {
  size_t rest = sizeof (liberty->buf) - liberty->pos;
  if (rest < len) len = rest;
  memcpy (liberty->buf + liberty->pos, buf, len);
  liberty->pos += len;

  if (0) {
    static size_t max_fill = 0;
    if (liberty->pos > max_fill) {
      max_fill = liberty->pos;
      printf ("max_fill = %lu\n", max_fill);
    }
  }
}

static inline int liberty_sync_check_header (liberty_t liberty, unsigned char* buf, int bird) {
  if (buf[0] != 'L' || buf[1] != 'Y')
    return 0;
  liberty->tagSuccesses++;

  liberty->stations[buf[2]] = 1;
  if (buf[2] != (unsigned char) (bird + 1)) {
    liberty->stationNumberErrors++;
    return 0;
  }

  if (buf[4] != 0x00 && buf[4] != 0x20) {
    liberty->errorIndicators++;
    liberty->errorIndicatorCounts[buf[4]]++;
  }

  size_t size = ((size_t) buf[6]) + (((size_t) buf[7]) << 8);
  if (size + 8 != BIRD_RECORD_SIZE) {
    liberty->sizeErrors++;
    return 0;
  }

  return 1;
}

static int liberty_sync (liberty_t liberty) {
  if (liberty->sync) return 1;

  size_t shift = 0;
  int ok = 0;

  for (size_t i = 0; i + RECORD_SIZE <= liberty->pos; i++) {
    shift = i;
    ok = 1;

    for (int bird = 0; bird < NUMBER_OF_BIRDS; bird++) {
      unsigned char* buf = liberty->buf + i + (bird * BIRD_RECORD_SIZE);
      int bird_ok = liberty_sync_check_header (liberty, buf, bird);

      if (bird_ok) {
        bird_record_t birdRecord = &liberty->bird_records[bird];
        liberty->fill_bird_record (birdRecord, buf + 8);
      }

      ok = ok && bird_ok;
      if (!ok) break;
    }

    if (ok) {
      liberty->sync = 1;
      liberty->syncs++;
      shift += RECORD_SIZE;
      break;
    }
  }

  if (shift > 0) {
    size_t diff = liberty->pos - shift;
    memmove (liberty->buf, liberty->buf + shift, diff);
    liberty->pos = diff;
  }

  return ok;
}

static void liberty_unsync (liberty_t liberty) {
  liberty->sync = 0;
}

void liberty_read_next_record (liberty_t liberty) {
  liberty_unsync (liberty);
  int tries = 0;

  while (!liberty_sync (liberty)) {
    unsigned char buf[128];
    fd_set read_fd_set = liberty->input_fd_set;
    struct timeval tv = { 0, 1000 };

    int sel = select (liberty->fd + 1, &read_fd_set, 0, 0, &tv);
    if (sel == -1) return;
    if (tries++ > 20) return;
    if (sel == 0) continue;

    int len = read (liberty->fd, buf, sizeof (buf));
    if (len <= 0) return;

    /*
    static int ndisplays = 0;
    if (ndisplays < 10) {
      ndisplays++;
      debug_buffer ("READ", buf, len);
    }
    */

    liberty_push (liberty, buf, len);
  }

}

void liberty_fill_bird_record (liberty_t liberty, int bird, bird_record_t record) {
  *record = liberty->bird_records[bird - 1];
}

