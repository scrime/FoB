/*

  PiTrkPerm
  Copyright  ©  2009  Polhemus, Inc.


    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    *************************************************************************

    This Program uses the libusb library version 1.0
    libusb Copyright © 2007-2008 Daniel Drake <dsd@gentoo.org>
    libusb Copyright © 2001 Johannes Erdfelt <johannes@erdfelt.com>
    Licensed under the GNU Lesser General Public License version 2.1 or later.
*/






#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>



int main(){

  int cnt;
  libusb_device **devs;
  char path[200];
  int i,rv;
  int r=libusb_init(NULL);
  if (r<0){
    fprintf(stderr,"Unable to init libusb\n");
    return -1;
  }

  cnt = libusb_get_device_list(NULL, &devs);
  if (cnt < 0)
    return (int) cnt;

  for (i=0;i<cnt;i++){
    struct libusb_device_descriptor desc;
    int r = libusb_get_device_descriptor(devs[i], &desc);
    if (r < 0) {
      fprintf(stderr, "failed to get device descriptor");
      return 1;
    }

    if ((desc.idVendor==0x0f44) && ((desc.idProduct==0xff20)||(desc.idProduct==0xff12)||
				    (desc.idProduct==0xef12)||(desc.idProduct==0x0002))){
      sprintf(path,"/dev/bus/usb/%.03d/%.03d",libusb_get_bus_number(devs[i]),libusb_get_device_address(devs[i]));
      rv=chmod(path,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    }

  }

  libusb_free_device_list(devs,1);

  return 0;
}

