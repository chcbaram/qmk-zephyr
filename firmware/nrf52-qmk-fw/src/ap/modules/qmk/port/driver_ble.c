#include "host.h"
#include "host_driver.h"
#include "report.h"
#include "ble.h"

/*
 * BLE 출력용 host_driver_t.
 * USB(driver_usb.c)와 동일한 인터페이스라, 전환은 host_set_driver(&ble_driver) 한 줄이면 된다.
 * (QMK 네이티브 outputselect 방식 — 상위 QMK 로직은 transport 를 모른다)
 */

static uint8_t ble_keyboard_leds(void)
{
  return bleGetKbdLeds();
}

static void ble_send_keyboard(report_keyboard_t *report)
{
  bleSendKeyboard(report);
}

static void ble_send_nkro(report_nkro_t *report)
{
  // 6KRO boot 리포트만 사용(USB 쪽과 동일).
  (void)report;
}

static void ble_send_mouse(report_mouse_t *report)
{
  // TODO: 마우스 리포트(ID 2)를 report map 에 추가 후 연결.
  (void)report;
}

static void ble_send_extra(report_extra_t *report)
{
  bleSendExtra(report);   // System(ID3)/Consumer(ID4) 는 report_id 로 분기
}

host_driver_t ble_driver = {
  ble_keyboard_leds,
  ble_send_keyboard,
  ble_send_nkro,
  ble_send_mouse,
  ble_send_extra,
};
