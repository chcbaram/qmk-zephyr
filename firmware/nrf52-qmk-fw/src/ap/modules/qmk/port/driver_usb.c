#include "host.h"
#include "host_driver.h"
#include "report.h"

/*
 * USB 출력용 host_driver_t.
 * Phase 0: 링크만 되도록 스텁(아무것도 전송 안 함).
 * Phase 1: 각 send_* 를 usbHidSendReport()/usbHidSendReportEXK() 로 실체화한다.
 */

static uint8_t usb_keyboard_leds(void)
{
  return 0;
}

static void usb_send_keyboard(report_keyboard_t *report)
{
  (void)report;
}

static void usb_send_nkro(report_nkro_t *report)
{
  (void)report;
}

static void usb_send_mouse(report_mouse_t *report)
{
  (void)report;
}

static void usb_send_extra(report_extra_t *report)
{
  (void)report;
}

host_driver_t usb_driver = {
  usb_keyboard_leds,
  usb_send_keyboard,
  usb_send_nkro,
  usb_send_mouse,
  usb_send_extra,
};
