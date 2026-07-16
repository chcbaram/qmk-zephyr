#include "qmk.h"
#include "host.h"
#include "eeprom.h"
#include "via_hid.h"
#include "ble.h"
#include "cli.h"
#include "usb_hid/usb_hid.h"

// 출력 드라이버 2종. 전환은 host_set_driver() 로만 이뤄진다(QMK 네이티브 outputselect).
extern host_driver_t usb_driver;   // port/driver_usb.c
extern host_driver_t ble_driver;   // port/driver_ble.c

static host_driver_t *cur_driver = NULL;

// USB 가 붙어있으면 USB, 아니면 BLE(연결 시). 전환 시 직전 드라이버로 빈 리포트를 보내
// stuck key 를 방지한다.
static void output_select_task(void)
{
  host_driver_t *want;

  if (usbHidIsReady())
  {
    want = &usb_driver;
  }
  else if (bleIsConnected())
  {
    want = &ble_driver;
  }
  else
  {
    return;   // 둘 다 없으면 현재 유지(전송은 어차피 각 드라이버가 무시)
  }

  if (want == cur_driver)
  {
    return;
  }

  if (cur_driver != NULL)
  {
    report_keyboard_t empty = {0};
    (*cur_driver->send_keyboard)(&empty);   // 이전 transport 의 눌림 상태 해제
  }

  host_set_driver(want);
  cur_driver = want;
  logPrintf("[  ] output -> %s\n", (want == &usb_driver) ? "USB" : "BLE");
}

bool qmkInit(void)
{
  eeprom_init();
  via_hid_init();

  bleInit();

  // 기본은 USB. 연결 상태에 따라 output_select_task() 가 전환한다.
  host_set_driver(&usb_driver);
  cur_driver = &usb_driver;

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
  output_select_task();
}
