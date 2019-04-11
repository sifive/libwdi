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
