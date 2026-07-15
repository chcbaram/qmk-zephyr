#include "via_hid.h"
#include "raw_hid.h"
#include "usb_hid/usb_hid.h"

/*
 * VIA raw HID 브릿지 (usb_hid ↔ QMK via.c).
 *  - 수신: usb_hid VIA OUT 리포트 → via_hid_receive → QMK raw_hid_receive(via.c 처리)
 *  - 송신: QMK raw_hid_send → usb_hid VIA IN 리포트
 */

// 호스트→디바이스 32바이트. usb_hid 가 OUT 리포트 수신 시 호출.
static void via_hid_receive(uint8_t *data, uint8_t length)
{
  raw_hid_receive(data, length);
}

void via_hid_init(void)
{
  usbHidSetViaReceiveFunc(via_hid_receive);
}

// QMK via.c 가 응답을 보낼 때 호출. 디바이스→호스트 VIA IN 전송.
void raw_hid_send(uint8_t *data, uint8_t length)
{
  usbHidSendReportVia(data, length);
}
