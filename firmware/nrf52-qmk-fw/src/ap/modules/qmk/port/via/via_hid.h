#pragma once

#include <stdint.h>

// VIA raw HID 브릿지. Phase 3 에서 usb_hid VIA out-report 콜백과 연결한다.
void via_hid_init(void);
