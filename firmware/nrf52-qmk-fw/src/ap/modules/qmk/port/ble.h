#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "report.h"

/*
 * BLE HID (HOG) — NCS BT_HIDS 기반.
 * 리포트 구성(USB exk 와 동일한 Report ID 규약):
 *   ID 1 : 키보드 (mods + reserved + keys[6], 8B) + LED output
 *   ID 3 : System control (usage16)
 *   ID 4 : Consumer control (usage16)
 * (마우스는 ID 2 로 확장 예정)
 */

bool bleInit(void);

// 현재 BLE 로 리포트를 보낼 수 있는 상태인지(연결 + 알림 활성)
bool bleIsConnected(void);

// QMK host_driver(port/driver_ble.c) 가 호출하는 전송 API
bool bleSendKeyboard(report_keyboard_t *report);
bool bleSendExtra(report_extra_t *report);

// 호스트가 보낸 LED 상태(CapsLock 등)
uint8_t bleGetKbdLeds(void);
