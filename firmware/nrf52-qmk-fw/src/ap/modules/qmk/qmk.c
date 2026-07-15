#include "qmk.h"
#include "host.h"
#include "eeprom.h"
#include "via_hid.h"
#include "cli.h"

// USB 출력 드라이버(port/driver_usb.c). Phase 5에서 ble_driver 추가 후
// host_set_driver() 로 USB/BLE 전환.
extern host_driver_t usb_driver;

bool qmkInit(void)
{
  eeprom_init();
  via_hid_init();

  host_set_driver(&usb_driver);

  keyboard_setup();
  keyboard_init();

  logPrintf("[OK] qmkInit()\n");
  logPrintf("     MATRIX %d x %d, DEBOUNCE %d\n", MATRIX_ROWS, MATRIX_COLS, DEBOUNCE);
  return true;
}

void qmkUpdate(void)
{
  keyboard_task();
  eeprom_task();
}
