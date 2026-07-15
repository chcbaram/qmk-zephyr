#include "host.h"
#include "host_driver.h"
#include "report.h"
#include "usb_hid/usb_hid.h"

/*
 * USB 출력용 host_driver_t.
 * host.c 의 host_*_send() 가 활성 드라이버의 함수 포인터로 dispatch 하며,
 * 여기서 usbd_next HID(usb_hid.c) 전송 API 로 연결한다.
 * Phase 5: ble_driver 추가 후 host_set_driver() 로 USB/BLE 전환.
 */

static uint8_t usb_keyboard_leds(void)
{
  return usbHidGetKbdLeds();
}

static void usb_send_keyboard(report_keyboard_t *report)
{
  usbHidSendReport((uint8_t *)report, KEYBOARD_REPORT_SIZE);
}

static void usb_send_nkro(report_nkro_t *report)
{
  // Phase 1: NKRO 미사용(6KRO boot). 필요 시 별도 HID 인터페이스로 확장.
  (void)report;
}

static void usb_send_mouse(report_mouse_t *report)
{
  // Phase 1: 마우스 리포트 미연결.
  (void)report;
}

static void usb_send_extra(report_extra_t *report)
{
  // System/Consumer control (Phase 2)
  usbHidSendReportEXK((uint8_t *)report, sizeof(report_extra_t));
}

host_driver_t usb_driver = {
  usb_keyboard_leds,
  usb_send_keyboard,
  usb_send_nkro,
  usb_send_mouse,
  usb_send_extra,
};
