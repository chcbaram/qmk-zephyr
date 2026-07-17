#include "qmk.h"
#include "log.h"   // logPrintf (콘솔 비활성 빌드에선 no-op)
#include "host.h"
#include "eeprom.h"

// settle-flush(100ms) 가 확실히 돌도록 그보다 여유 있게 깨운다.
#define EEPROM_FLUSH_WAIT_MS   20
#include "via_hid.h"
#include "via_port.h"
#include "port/activity.h"
#ifdef RGB_MATRIX_ENABLE
#include "rgb_matrix.h"
#endif
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
static bool usb_suspended = false;
static bool suspended     = false;

static void qmk_usb_suspend_cb(bool s)
{
  logPrintf("[  ] usb %s\n", s ? "SUSPEND" : "RESUME");
  usb_suspended = s;   // 실제 반영은 qmkSuspendUpdate() 가 한다 (조건이 둘이라 합쳐야 한다)
}

/*
 * RGB/인디케이터 소등의 **단일 결정 지점**.
 *
 * 끄는 이유가 둘이다 — 호스트 PC 가 잠(USB SUSPEND), 사용자가 자리를 뜸(activity IDLE).
 * 각자 suspend_power_down_quantum() 을 부르면 서로를 모른 채 "내 조건이 풀렸다"고 켜버린다
 * (예: PC 가 깨어났지만 여전히 idle → RGB 가 켜진 채 배터리를 태운다). OR 로 합쳐서
 * **전이할 때만** 부른다.
 *
 * activityGetState() 가 아니라 activityIsIdle() 을 쓴다 — activityUpdate() 는 idle 분기에서만
 * 불려서, 키가 눌리는 동안 state 는 IDLE 로 낡아 있다. 그걸 보면 RGB 가 영영 안 돌아온다.
 *
 * suspend_wakeup_init_quantum() -> led_wakeup() -> led_set(host_keyboard_leds()) 는 활성
 * host_driver 를 타므로, qmkUpdate() 에선 output_select_task() **뒤에** 불려야 한다.
 */
bool qmkIsSuspended(void)
{
  return suspended;
}

void qmkSuspendUpdate(void)
{
  bool want = usb_suspended || activityIsIdle();

  if (want == suspended)
  {
    return;
  }
  // [순서 계약] suspend_power_down_quantum() **전에** 세운다. 그 안의
  // rgb_matrix_set_suspend_state(true) 가 검은 프레임을 flush 하는데, 우리 flush 가
  // qmkIsSuspended() 로 "레일을 내려도 되나"를 판정하기 때문이다.
  suspended = want;

  logPrintf("[  ] %s (usb_susp=%d idle=%d)\n", want ? "power down" : "wakeup",
            usb_suspended, activityIsIdle());

  if (want)
  {
    suspend_power_down_quantum();   // RGB + 인디케이터(led_suspend) 둘 다
  }
  else
  {
    suspend_wakeup_init_quantum();  // led_wakeup() 이 호스트 LED 상태를 복구한다
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

  usbSetSuspendFunc(qmk_usb_suspend_cb);   // 호스트 PC 가 자면 RGB 소등
  usbHidSetKbdLedFunc(qmkWake);            // LED 리포트 도착 -> led_task 가 돌도록 깨움   // EEPROM 에 저장된 전력 설정을 activity 에 적용 (activityInit 뒤여야 한다)

  logPrintf("[OK] qmkInit()\n");
  logPrintf("     MATRIX %d x %d, DEBOUNCE %d\n", MATRIX_ROWS, MATRIX_COLS, DEBOUNCE);
  return true;
}

uint32_t qmkGetIdleWaitMs(void)
{
  uint32_t wait_ms = activityGetWaitMs();

  /*
   * EEPROM 에 아직 안 쓴 변경이 있으면 오래 자면 안 된다.
   * settle-flush 는 "마지막 쓰기 후 EE_FLUSH_DELAY_MS 조용하면 flush" 인데, 그 판정을
   * eeprom_task() 가 한다 → 루프가 자면 flush 도 멈춘다. VIA 저장 직후 루프가 수십 초
   * 블록해버려 그 사이 전원이 꺼지면 유실된다(실제로 겪음).
   */
  if (eeprom_is_dirty())
  {
    if (wait_ms == 0 || wait_ms > EEPROM_FLUSH_WAIT_MS)
    {
      wait_ms = EEPROM_FLUSH_WAIT_MS;
    }
  }

#ifdef RGB_MATRIX_ENABLE
  /*
   * RGB 가 켜져 있으면 애니메이션이 돌아야 한다. activity 데드라인(수십 초)까지 자버리면 멈춘다.
   *
   * [주의] 여기서 RGB_MATRIX_LED_FLUSH_LIMIT(16ms)를 쓰면 안 된다. 그건 **프레임 주기**지
   * task 주기가 아니다. rgb_matrix_task() 는 상태머신이고 RENDERING 이 청크 단위라
   * (RGB_MATRIX_LED_PROCESS_LIMIT), 한 프레임에 여러 번 불려야 한다. 16ms 로 깨우면
   * 프레임당 ~100ms 가 걸려 눈에 띄게 끊긴다(실제로 겪음).
   * → **task 주기로 깨운다.** 프레임 주기 제한은 rgb_task_sync() 가 알아서 건다.
   *
   * 전력: RGB 자체가 10mA 단위라 이 웨이크업 비용(≈1mA)은 묻힌다. RGB 를 끄면 원래대로 돌아간다.
   */
  if (rgb_matrix_is_enabled() && !suspended)
  {
    if (wait_ms == 0 || wait_ms > QMK_TASK_PERIOD_MS)
    {
      wait_ms = QMK_TASK_PERIOD_MS;
    }
  }
#endif

  return wait_ms;
}

void qmkUpdate(void)
{
  /*
   * [순서 주의] output_select_task() 가 **먼저**다.
   *
   * keyboard_task() 안의 led_task() 는 host_keyboard_leds() 를 읽는데, 그건 활성 host_driver 를
   * 탄다. 전환을 뒤에 하면 이번 회차의 led_task 는 **이전 드라이버**의 LED 상태를 읽는다.
   * 부팅 시 cur_driver 는 usb_driver 라, BLE 전용 연결에서 첫 회차가 USB 의 0 을 읽고
   * 그 뒤 BLE 로 전환된다. 그리고 루프가 잠들면(호스트의 주기 리포트는 값이 같아 qmkWake 를
   * 부르지 않는다) **idle 데드라인 30초가 지나서야** LED 가 켜졌다(실제로 겪음).
   * 전환을 먼저 하면 같은 회차의 led_task 가 올바른 드라이버를 읽는다.
   */
  output_select_task();

  // 활성 구간에서 idle 이 풀리는(=키 눌림) 순간의 복귀도 여기서 잡는다.
  // output_select_task() 뒤여야 한다 — led_wakeup() 이 활성 드라이버의 LED 상태를 읽는다.
  qmkSuspendUpdate();

  keyboard_task();
  eeprom_task();
}
