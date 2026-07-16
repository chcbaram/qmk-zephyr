#ifndef USB_HID_H_
#define USB_HID_H_

#include "hw_def.h"



bool usbHidInit(void);

bool    usbHidSendReport(uint8_t *data, uint16_t length);
bool    usbHidSendReportEXK(uint8_t *data, uint16_t length);
bool    usbHidSendReportVia(uint8_t *data, uint16_t length);
uint8_t usbHidGetKbdLeds(void);
// 호스트가 키보드 인터페이스를 구성했는지(=USB 로 전송 가능한지)
bool    usbHidIsReady(void);
void    usbHidSetViaReceiveFunc(void (*func)(uint8_t *data, uint8_t length));


#endif
