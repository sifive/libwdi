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

/*
 * Change these values according to your device if
 * you don't want to provide parameters
 */
#define ARTY_DESC        "SiFive Digilent USB Device (Interface 0)"
#define ARTY_VID         0x0403
#define ARTY_PID         0x6010
#define ARTY_INF_NAME    "sifive_arty_digilent.inf"
#define ARTY_DEFAULT_DIR "sifive_art_digilent_driver"

#define OLIM_DESC        "SiFive Olimex OpenOCD JTAG ARM-USB-TINY-H (Interface 0)"
#define OLIM_VID         0x15ba
#define OLIM_PID         0x002a
#define OLIM_INF_NAME    "sifive_olimex_winusb.inf"
#define OLIM_DEFAULT_DIR "sifive_olimex_winusb_driver"

static struct wdi_device_info arty_dev = { NULL, ARTY_VID, ARTY_PID, TRUE, 0, ARTY_DESC, NULL, NULL, NULL };
static struct wdi_device_info olim_dev = { NULL, OLIM_VID, OLIM_PID, TRUE, 0, OLIM_DESC, NULL, NULL, NULL };

static int opt_silent = 1;


void usage(void)
{
	printf("SiFive WinUSB Utility\n");
	printf("  -c, --check                check devices to see what needs to be updated\n");
	printf("  -o, --olimex               install olimex winusb driver\n");
	printf("  -d, --digelent             install digilent winusb driver\n");
	printf("  -v, --verbose              be verbose about it (must be first param)\n");
	printf("  -h, --help                 display usage\n");
	printf("\nAt least one of --olimex or --digilent is required.\n\n");
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

BOOL IsElevated() {
	BOOL fRet = FALSE;
	HANDLE hToken = NULL;
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
		TOKEN_ELEVATION Elevation;
		DWORD cbSize = sizeof(TOKEN_ELEVATION);
		if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize)) {
			fRet = Elevation.TokenIsElevated;
		}
	}
	if (hToken) {
		CloseHandle(hToken);
	}
	return fRet;
}

// from http://support.microsoft.com/kb/124103/
HWND GetConsoleHwnd(void)
{
	HWND hwndFound;
	char pszNewWindowTitle[128];
	char pszOldWindowTitle[128];

	GetConsoleTitleA(pszOldWindowTitle, 128);
	wsprintfA(pszNewWindowTitle,"%d/%d", GetTickCount(), GetCurrentProcessId());
	SetConsoleTitleA(pszNewWindowTitle);
	Sleep(40);
	hwndFound = FindWindowA(NULL, pszNewWindowTitle);
	SetConsoleTitleA(pszOldWindowTitle);
	return hwndFound;
}

#define UPDATE_OLIMEX 0x1
#define UPDATE_DIGILENT 0x2

#define WINUSB_TAG "=> WinUSB"

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

	oprintf("\nFor reference:\n");
	oprintf("Installation of Olimex driver is required if bit 0 of return code is set (%s).\n", (return_code&0x1)?"it is":"it is not");
	oprintf("Installation of Digilent driver is required if bit 1 of return code is set (%s).\n", (return_code & 0x2) ? "it is" : "it is not");
	oprintf("Return code: %d\n", return_code);
	exit(return_code);
}

int __cdecl main(int argc, char** argv)
{
	static struct wdi_device_info *ldev;
	static struct wdi_device_info *dev;

	static struct wdi_options_create_list ocl = { 1, 0, 0 };
	static struct wdi_options_prepare_driver opd = { 0 };
	static struct wdi_options_install_driver oid = { 0 };
	static struct wdi_options_install_cert oic = { 0 };
	static BOOL driverInstalled = FALSE;
	static opt_extract = 0, log_level = WDI_LOG_LEVEL_WARNING;
	int c, r;
	char *inf_name;
	char *ext_dir;
	char *cert_name = NULL;

	static struct option long_options[] = {
		{ "help", no_argument, 0, 'h'},
		{ "olimex", no_argument, 0, 'o' },
		{ "digilent" , no_argument, 0, 'd' },
		{ "verbose" , no_argument, 0, 'v' },
		{ "check" , no_argument, 0, 'c' },
		{0, 0, 0, 0}
	};

	ocl.list_all = TRUE;
	ocl.list_hubs = TRUE;
	ocl.trim_whitespaces = TRUE;
	opd.driver_type = WDI_WINUSB;

	BOOL isElevated = IsElevated();

	while (1) {
		c = getopt_long(argc, argv, "chodv", long_options, NULL);
		switch (c) {
		case 'c':
			checkDrivers();
			break;
		case 'o':
			inf_name = OLIM_INF_NAME;
			ext_dir = OLIM_DEFAULT_DIR;
			dev = &olim_dev;
			oprintf("Checking WinUSB driver for Olimex, ");
			break;
		case 'd':
			inf_name = ARTY_INF_NAME;
			ext_dir = ARTY_DEFAULT_DIR;
			dev = &arty_dev;
			oprintf("Checking WinUSB driver for Digilent, ");
			break;
		case 'v':
			opt_silent = 0;
			continue;
			break;
		case -1:
			if (dev == NULL) {
				usage();
			}
			exit(0);
		default:
			oprintf("Unknown argument: %c\n", c);
		case 'h':
			usage();
			exit(0);
		}

		wdi_set_log_level(log_level);

		//oprintf("Extracting WinUSB driver...\n");
		r = wdi_prepare_driver(dev, ext_dir, inf_name, &opd);

		if ((r != WDI_SUCCESS) || (opt_extract)) {
			oprintf("  %s\n", wdi_strerror(r));
			return r;
		}

		if (cert_name != NULL) {
			r = wdi_install_trusted_certificate(cert_name, &oic);
			if (r != WDI_SUCCESS) {
				oprintf("Attempted to install certificate '%s' as a Trusted Publisher...\n", cert_name);
				oprintf("Error: %s\n", wdi_strerror(r));
			}
		}

		// Try to match against a plugged device to avoid device manager prompts
		if (wdi_create_list(&ldev, &ocl) == WDI_SUCCESS) {
			r = WDI_SUCCESS;
			for (; (ldev != NULL) && (r == WDI_SUCCESS); ldev = ldev->next) {
				if ((ldev->vid == dev->vid) 
					&& (ldev->pid == dev->pid) 
					&& (ldev->mi == dev->mi) 
					&& (ldev->is_composite == dev->is_composite)) {
					if (ldev->driver == NULL || strcmp(ldev->driver, "WinUSB") != 0) {
						dev->hardware_id = ldev->hardware_id;
						dev->device_id = ldev->device_id;
						fflush(stdout);
						if (!isElevated) {
							printf("driver needs to be installed, but this program must be run from an elevated shell in order to install a driver, skipping\n");
						}
						else {
							oprintf("installing...\n");
							r = wdi_install_driver(dev, ext_dir, inf_name, &oid);
							driverInstalled = TRUE;
							if (r != WDI_SUCCESS) {
								oprintf("%s\n", wdi_strerror(r));
								return r;
							}
							oprintf("finished\n");
						}
					}
					else {
						oprintf("already installed, skipping\n");
						break;
					}
				}
			}
		}
	}

	if (!driverInstalled) {
		oprintf("No drivers were installed");
	}
	return WDI_SUCCESS;
}
