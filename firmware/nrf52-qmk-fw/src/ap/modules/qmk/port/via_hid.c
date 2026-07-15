#include "via_hid.h"
#include "raw_hid.h"

/*
 * VIA raw HID 브릿지.
 * Phase 0: 링크만 되도록 스텁. (호스트→디바이스 수신 콜백/전송 미연결)
 * Phase 3: usbHidSetViaReceiveFunc(via_hid_receive) 등록 + raw_hid_send 를
 *          usb_hid VIA in-report 전송으로 실체화한다.
 */

void via_hid_init(void)
{
}

// QMK via.c 가 응답을 보낼 때 호출한다. Phase 3에서 usb_hid VIA IN 전송으로 연결.
__attribute__((weak)) void raw_hid_send(uint8_t *data, uint8_t length)
{
  (void)data;
  (void)length;
}
