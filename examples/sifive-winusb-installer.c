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
#define ARTY_DESC        "Digilent USB Device (Interface 0)"
#define ARTY_VID         0x0403
#define ARTY_PID         0x6010
#define ARTY_INF_NAME    "sifive_arty_digilent.inf"
#define ARTY_DEFAULT_DIR "sifive_art_digilent_driver"

#define OLIM_DESC        "Olimex OpenOCD JTAG ARM-USB-TINY-H (Interface 0)"
#define OLIM_VID         0x15ba
#define OLIM_PID         0x002a
#define OLIM_INF_NAME    "sifive_olimex_winusb.inf"
#define OLIM_DEFAULT_DIR "sifive_olimex_winusb_driver"


void usage(void)
{
	printf("\n");
	printf("-o, --olimex               install olimex winusb driver\n");
	printf("-a, --arty                 install arty winusb driver\n");
	printf("-h, --help                 display usage\n");
	printf("\n");
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

int __cdecl main(int argc, char** argv)
{
	static struct wdi_device_info *ldev;
	static struct wdi_device_info arty_dev = { NULL, ARTY_VID, ARTY_PID, FALSE, 0, ARTY_DESC, NULL, NULL, NULL };
	static struct wdi_device_info olim_dev = { NULL, OLIM_VID, OLIM_PID, FALSE, 0, OLIM_DESC, NULL, NULL, NULL };
	static struct wdi_device_info dev;

	static struct wdi_options_create_list ocl = { 0 };
	static struct wdi_options_prepare_driver opd = { 0 };
	static struct wdi_options_install_driver oid = { 0 };
	static struct wdi_options_install_cert oic = { 0 };
	static int opt_silent = 0, opt_extract = 0, log_level = WDI_LOG_LEVEL_WARNING;
	static BOOL matching_device_found;
	int c, r;
	char *inf_name;
	char *ext_dir;
	char *cert_name = NULL;

	static struct option long_options[] = {
		{ "help", no_argument, 0, 'h'},
		{ "olimex", no_argument, 0, 'o' },
		{ "arty" , no_argument, 0, 'a' },
		{0, 0, 0, 0}
	};

	ocl.list_all = TRUE;
	ocl.list_hubs = TRUE;
	ocl.trim_whitespaces = TRUE;
	opd.driver_type = WDI_WINUSB;

	c = getopt_long(argc, argv, "h:o:a", long_options, NULL);
	switch(c) {
	case 'o':
		inf_name = OLIM_INF_NAME;
		ext_dir = OLIM_DEFAULT_DIR;
		memcpy(&dev, &olim_dev, sizeof(olim_dev));
		break;
	case 'a':
		inf_name = ARTY_INF_NAME;
		ext_dir = ARTY_DEFAULT_DIR;
		memcpy(&dev, &arty_dev, sizeof(arty_dev));
		break;
	case -1:
	case 'h':
	default:
		usage();
		exit(0);
	}
	
	wdi_set_log_level(log_level);

	oprintf("Extracting WinUSB driver...\n");
	r = wdi_prepare_driver(&dev, ext_dir, inf_name, &opd);
	oprintf("  %s\n", wdi_strerror(r));
	if ((r != WDI_SUCCESS) || (opt_extract))
		return r;

	if (cert_name != NULL) {
		oprintf("Installing certificate '%s' as a Trusted Publisher...\n", cert_name);
		r = wdi_install_trusted_certificate(cert_name, &oic);
		oprintf("  %s\n", wdi_strerror(r));
	}

	oprintf("Installing driver...\n");

	// Try to match against a plugged device to avoid device manager prompts
	matching_device_found = FALSE;
	if (wdi_create_list(&ldev, &ocl) == WDI_SUCCESS) {
		r = WDI_SUCCESS;
		for (; (ldev != NULL) && (r == WDI_SUCCESS); ldev = ldev->next) {
			if ( (ldev->vid == dev.vid) && (ldev->pid == dev.pid) && (ldev->mi == dev.mi) &&(ldev->is_composite == dev.is_composite) ) {
				
				dev.hardware_id = ldev->hardware_id;
				dev.device_id = ldev->device_id;
				matching_device_found = TRUE;
				oprintf("  %s: ", dev.hardware_id);
				fflush(stdout);
				r = wdi_install_driver(&dev, ext_dir, inf_name, &oid);
				oprintf("%s\n", wdi_strerror(r));
			}
		}
	}

	// No plugged USB device matches this one -> install driver
	if (!matching_device_found) {
		r = wdi_install_driver(&dev, ext_dir, inf_name, &oid);
		oprintf("  %s\n", wdi_strerror(r));
	}

	return r;
}
