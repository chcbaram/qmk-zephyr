#include "qmk.h"
#include "log.h"   // logPrintf (콘솔 비활성 빌드에선 no-op)
#include "host.h"
#include "eeprom.h"
#include "via_hid.h"
#include "via_port.h"
#include "port/activity.h"
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

/*
 * USB 서스펜드 -> QMK 서스펜드 훅.
 *
 * PC 가 자면 RGB 를 꺼야 한다. QMK 에 이미 경로가 있다:
 *   suspend_power_down_quantum() -> rgb_matrix_set_suspend_state(true) -> LED 소등 + flush
 *   -> 우리 flush 가 ext-power 레일까지 내린다(rgb_matrix_drivers.c)
 * 그 훅을 아무도 안 부르고 있었다 — usbd_next 의 SUSPEND/RESUME 메시지를 여기 연결한다.
 *
 * BLE 쪽 idle 은 activity 상태머신이 따로 담당한다(port/activity.c).
 */
static void qmk_usb_suspend_cb(bool suspended)
{
  if (suspended)
  {
    suspend_power_down_quantum();
  }
  else
  {
    suspend_wakeup_init_quantum();
  }
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

  activityInit();
  viaPortInit();

  usbSetSuspendFunc(qmk_usb_suspend_cb);   // 호스트 PC 가 자면 RGB 소등   // EEPROM 에 저장된 전력 설정을 activity 에 적용 (activityInit 뒤여야 한다)

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
