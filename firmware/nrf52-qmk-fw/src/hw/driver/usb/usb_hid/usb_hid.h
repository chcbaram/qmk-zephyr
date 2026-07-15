#ifndef USB_HID_H_
#define USB_HID_H_

#include "hw_def.h"



bool usbHidInit(void);

bool    usbHidSendReport(uint8_t *data, uint16_t length);
bool    usbHidSendReportEXK(uint8_t *data, uint16_t length);
uint8_t usbHidGetKbdLeds(void);


#endif
