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

/*
 * 호스트가 키보드 LED 리포트(CapsLock 등)를 보냈을 때 알림.
 *
 * QMK 의 led_task() 는 host_keyboard_leds() 를 **폴링**하는데 리포트는 비동기로 온다.
 * 메인 루프가 idle 로 블록 중이면 반영이 안 되므로(LED 가 안 켜짐) 깨워야 한다.
 * hw 가 QMK 를 직접 부르지 않게 콜백만 노출한다(usbSetSuspendFunc 와 같은 패턴).
 */
void    usbHidSetKbdLedFunc(void (*func)(void));


#endif
