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
#define ARTY_INSTALL_MSG "INSTALL_DIGILENT(0x2)"
#define ARTY_EXIST_MSG   "DIGILENT_EXISTS(0x2)"

#define OLIM_DESC        "SiFive Olimex OpenOCD JTAG ARM-USB-TINY-H (Interface 0)"
#define OLIM_VID         0x15ba
#define OLIM_PID         0x002a
#define OLIM_INF_NAME    "sifive_olimex_winusb.inf"
#define OLIM_DEFAULT_DIR "sifive_olimex_winusb_driver"
#define OLIM_INSTALL_MSG "INSTALL_OLIMEX(0x1)"
#define OLIM_EXIST_MSG   "OLIMEX_EXISTS(0x1)"

#define HF2_DESC        "SiFive HiFive2 USB (Interface 0)"
#define HF2_VID         0x0403
#define HF2_PID         0x6011
#define HF2_INF_NAME    "sifive_hifive2_winusb.inf"
#define HF2_DEFAULT_DIR "sifive_hifive2_winusb_driver"
#define HF2_INSTALL_MSG "INSTALL_HIFIVE2(0x4)"
#define HF2_EXIST_MSG   "HIFIVE2_EXISTS(0x4)"


static struct wdi_device_info arty_dev = { NULL, ARTY_VID, ARTY_PID, TRUE, 0, ARTY_DESC, NULL, NULL, NULL };
static struct wdi_device_info olim_dev = { NULL, OLIM_VID, OLIM_PID, TRUE, 0, OLIM_DESC, NULL, NULL, NULL };
static struct wdi_device_info hf2_dev = { NULL, HF2_VID, HF2_PID, TRUE, 0, HF2_DESC, NULL, NULL, NULL };

#define FLAG_OLIMEX   0x1
#define FLAG_DIGILENT 0x2
#define FLAG_HIFIVE2  0x4

#define WINUSB_TAG "=> WinUSB"

#define CHECK_DRIVER 1
#define CHECK_EXIST 2

static int opt_silent = 1, opt_listall = 0;
static int checkType = CHECK_EXIST;

void usage(void)
{
	printf("SiFive WinUSB Utility\n");
	printf("    -h, --help                 display usage\n\n");
	printf("  These must be specified first, if used:\n");
	printf("    -l, --list-all             list all connected devices, implies -v, use with -c, -e\n");
	printf("    -v, --verbose              be verbose about it, -c and -e will not be silent\n\n");
	printf("  These options query information, but do not install drivers:\n");
	printf("    -c, --check-driver         check devices to see what needs to be updated\n");
	printf("    -e, --check-connected      check the connection status for each device\n");
	printf("    (without -v or -l this process is silent and the return code can be used to determine\n");
	printf("    what actions needs to be taken)\n\n");
	printf("  These options will install drivers (but required an elevated shell):\n");
	printf("    -o, --olimex               install winusb driver to olimex (interface 0)\n");
	printf("    -d, --digelent             install winusb driver to digelent (interface 0)\n");
	printf("    -2, --hifive2              install winusb driver to hifive2 (interface 0)\n\n");

	printf("\nAt least one of --olimex, --hifive2, or --digilent is required unless using -c or -e.\n\n");
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

BOOL isDeviceFlaggedFor(int checkType, struct wdi_device_info *ldev) {
	if (checkType == CHECK_EXIST) {
		/*
		  IF we got this far, then the device certainly exists
		 */
		return TRUE;
	}
	/*
	  For the CHECK_DRIVER case we need to examine the driver details.
	 */
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


int checkDrivers(int checkType) {
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
			if (opt_listall == 1) {
				print = TRUE;
			}

			if (isMatch(ldev, &olim_dev)) {
				matching_device_found = TRUE;
				print = TRUE;
				if (isDeviceFlaggedFor(checkType, ldev)) {
					tag = WINUSB_TAG;
					return_code |= FLAG_OLIMEX;
				}
			}
			else if (isMatch(ldev, &arty_dev)) {
				matching_device_found = TRUE;
				print = TRUE;
				if (isDeviceFlaggedFor(checkType, ldev)) {
					tag = WINUSB_TAG;
					return_code |= FLAG_DIGILENT;
				}
			}
			else if (isMatch(ldev, &hf2_dev)) {
				matching_device_found = TRUE;
				print = TRUE;
				if (isDeviceFlaggedFor(checkType, ldev)) {
					tag = WINUSB_TAG;
					return_code |= FLAG_HIFIVE2;
				}
			}

			if (print) {
				oprintf("Device: %04x:%04x:%x:%x %12s %9s %s\n", ldev->vid,
					ldev->pid, ldev->mi, ldev->is_composite, ldev->driver, tag,
					ldev->desc);
			}
		}
	}

	//if (!matching_device_found) {
		// No device connected
	//	return_code = -1;
	//}

	char* o_leader = OLIM_EXIST_MSG;
	char* d_leader = ARTY_EXIST_MSG;
	char* h2_leader = HF2_EXIST_MSG;

	if (CHECK_DRIVER == checkType) {
		o_leader = OLIM_INSTALL_MSG;
		d_leader = ARTY_INSTALL_MSG;
		h2_leader = HF2_INSTALL_MSG;
	}

	oprintf("Return code: %d", return_code);
	if (return_code == 0) {
		oprintf("\n");
	}
	else {
		oprintf(" (");
		if (return_code & 0x1) {
			oprintf(o_leader);
			if (return_code & 0x2) {
				oprintf(" | ");
			}
		}
		if (return_code & 0x2) {
			oprintf(d_leader);
		}
		oprintf(")\n");
	}

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
		{ "hifive2" , no_argument, 0, '2' },
		{ "verbose" , no_argument, 0, 'v' },
		{ "check-driver" , no_argument, 0, 'c' },
		{ "check-exist" , no_argument, 0, 'e' },
		{0, 0, 0, 0}
	};

	ocl.list_all = TRUE;
	ocl.list_hubs = TRUE;
	ocl.trim_whitespaces = TRUE;
	opd.driver_type = WDI_WINUSB;

	BOOL isElevated = IsElevated();

	while (1) {
		c = getopt_long(argc, argv, "cehodvl2", long_options, NULL);
		switch (c) {
		case 'c':
			// This will exit the program.
			checkDrivers(CHECK_DRIVER);
			break;
		case 'e':
			// This will exit the program.
			checkDrivers(CHECK_EXIST);
			break;
		case 'l':
			// This will exit the program.
			opt_silent = 0;
			opt_listall = 1;
			continue;
			break;
		case 'o':
			inf_name = OLIM_INF_NAME;
			ext_dir = OLIM_DEFAULT_DIR;
			dev = &olim_dev;
			oprintf("Checking Olimex  : ");
			break;
		case 'd':
			inf_name = ARTY_INF_NAME;
			ext_dir = ARTY_DEFAULT_DIR;
			dev = &arty_dev;
			oprintf("Checking Digilent: ");
			break;
		case '2':
			inf_name = HF2_INF_NAME;
			ext_dir = HF2_DEFAULT_DIR;
			dev = &hf2_dev;
			oprintf("Checking HiFive2: ");
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
			int deviceConnected = FALSE;
			int needsInstalling = FALSE;
			r = WDI_SUCCESS;
			for (; (ldev != NULL) && (r == WDI_SUCCESS) && (deviceConnected == FALSE); ldev = ldev->next) {
				//oprintf("Examining device: %04x:%04x:%x:%x %12s %s\n", ldev->vid, ldev->pid, ldev->mi, ldev->is_composite, ldev->driver, ldev->desc);
				if ((ldev->vid == dev->vid) 
					&& (ldev->pid == dev->pid) 
					&& (ldev->mi == dev->mi) 
					&& (ldev->is_composite == dev->is_composite)) {
					deviceConnected = TRUE;
					if (ldev->driver == NULL || strcmp(ldev->driver, "WinUSB") != 0) {
						needsInstalling = TRUE;
						dev->hardware_id = ldev->hardware_id;
						dev->device_id = ldev->device_id;
						fflush(stdout);
					}
					else {
						oprintf("WinUSB already installed, skipping\n");
						break;
					}
				}
			}
 			if (!deviceConnected || needsInstalling) {
				if (!deviceConnected) {
					printf("not connected, please connect before installing driver.\n");
					needsInstalling = FALSE;
				}
				else {
					printf("device found, ");
				}

				if (needsInstalling) {
					if (!isElevated) {
						printf("but this program must be run from an elevated shell in order to install a driver, skipping\n");
					}
					else {
						oprintf("installing driver...");
						r = wdi_install_driver(dev, ext_dir, inf_name, &oid);
						if (r != WDI_SUCCESS) {
							oprintf("failed: %s\n", wdi_strerror(r));
							return r;
						}
						else {
							oprintf("finished\n");
						}
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
