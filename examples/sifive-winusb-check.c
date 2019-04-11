/*
* sifive-winusb-installer.c: Console Driver Installer for a single USB device
* Copyright (c) 2010-2018 Pete Batard <pete@akeo.ie>
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 3 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _MSC_VER
#include "getopt/getopt.h"
#else
#include <getopt.h>
#endif
#include "libwdi.h"

#if defined(_PREFAST_)
/* Disable "Banned API Usage:" errors when using WDK's OACR/Prefast */
#pragma warning(disable:28719)
/* Disable "Consider using 'GetTickCount64' instead of 'GetTickCount'" when using WDK's OACR/Prefast */
#pragma warning(disable:28159)
#endif

#define oprintf(...) do {if (!opt_silent) printf(__VA_ARGS__);} while(0)

#include "../usb_devices.h"

#define UPDATE_OLIMEX 0x1
#define UPDATE_DIGILENT 0x2

#define WINUSB_TAG "=> WinUSB"

void usage(void)
{
	printf("\n");
	printf("-v, --verbose              display verbose info");
	printf("-h, --help                 display usage\n");
	printf("\n"); 
}

int isMatch(struct wdi_device_info *ldev, struct wdi_device_info *dev) {
	if ((ldev->vid == dev->vid)
		&& (ldev->pid == dev->pid)
		&& (ldev->mi == dev->mi)
		&& (ldev->is_composite == dev->is_composite)) {
		return TRUE;
	}
	return FALSE;
}

int isDriverInstallNeeded(struct wdi_device_info *ldev) {
	return (ldev->driver == NULL || strcmp(ldev->driver, "WinUSB"));
}


// from http://support.microsoft.com/kb/124103/
HWND GetConsoleHwnd(void)
{
	HWND hwndFound;
	char pszNewWindowTitle[128];
	char pszOldWindowTitle[128];

	GetConsoleTitleA(pszOldWindowTitle, 128);
	wsprintfA(pszNewWindowTitle, "%d/%d", GetTickCount(), GetCurrentProcessId());
	SetConsoleTitleA(pszNewWindowTitle);
	Sleep(40);
	hwndFound = FindWindowA(NULL, pszNewWindowTitle);
	SetConsoleTitleA(pszOldWindowTitle);
	return hwndFound;
}

static int opt_silent = 1, log_level = WDI_LOG_LEVEL_WARNING;

int checkDrivers() {
	static struct wdi_device_info *ldev;
	static struct wdi_device_info *dev = NULL;
	static BOOL matching_device_found;
	static struct wdi_options_create_list ocl = { 1, 0, 0 };
	ocl.list_all = TRUE;
	ocl.list_hubs = TRUE;
	ocl.trim_whitespaces = TRUE;

	int return_code = WDI_SUCCESS;

	matching_device_found = FALSE;
	char *tag;
	if (wdi_create_list(&ldev, &ocl) == WDI_SUCCESS) {
		for (; (ldev != NULL); ldev = ldev->next) {
			tag = "";
			int print = FALSE;
			if (isMatch(ldev, &olim_dev)) {
				matching_device_found = TRUE;
				print = TRUE;
				if (isDriverInstallNeeded(ldev)) {
					tag = WINUSB_TAG;
					return_code |= UPDATE_OLIMEX;
				}
			}
			else if (isMatch(ldev, &arty_dev)) {
				matching_device_found = TRUE;
				print = TRUE;
				if (isDriverInstallNeeded(ldev)) {
					tag = WINUSB_TAG;
					return_code |= UPDATE_DIGILENT;
				}
			}
			if (print) {
				oprintf("Device: %04x:%04x:%x %12s %-14s %d %s\n", ldev->vid,
					ldev->pid, ldev->mi, ldev->driver, tag, ldev->is_composite,
					ldev->desc);
			}
		}
	}

	if (!matching_device_found) {
		// No device connected
		return_code = -1;
	}

	oprintf("Return code: %d\n", return_code);
	exit(return_code);
}


int __cdecl main(int argc, char** argv)
{
	int c;

	static struct option long_options[] = {
		{ "help", no_argument, 0, 'h'},
		{ "verbose" , no_argument, 0, 'v' },
		{0, 0, 0, 0}
	};

	while (1) {
		c = getopt_long(argc, argv, "hv", long_options, NULL);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'v':
			opt_silent = 0;
			break;
		case -1:
			exit(0);
		case 'h':
		default:
			usage();
			exit(0);
		}
	}

	wdi_set_log_level(log_level);

	checkDrivers();


}
