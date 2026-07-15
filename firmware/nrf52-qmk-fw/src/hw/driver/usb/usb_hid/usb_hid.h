#ifndef USB_HID_H_
#define USB_HID_H_

#include "hw_def.h"



bool usbHidInit(void);

bool    usbHidSendReport(uint8_t *data, uint16_t length);
bool    usbHidSendReportEXK(uint8_t *data, uint16_t length);
bool    usbHidSendReportVia(uint8_t *data, uint16_t length);
uint8_t usbHidGetKbdLeds(void);
void    usbHidSetViaReceiveFunc(void (*func)(uint8_t *data, uint8_t length));


#endif
