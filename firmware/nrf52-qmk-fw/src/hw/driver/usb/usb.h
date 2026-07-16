#ifndef USB_H_
#define USB_H_

#include "hw_def.h"



bool usbInit(void);

// USB 전원(VBUS) 연결 여부. 저전력 정책 판단용(ZMK 의 zmk_usb_is_powered 상당).
// USB 가 있으면 개발/디버그 중으로 보고 콘솔·CLI 를 살리고, 배터리 only 면 끈다.
bool usbIsVbusPresent(void);


#endif 
