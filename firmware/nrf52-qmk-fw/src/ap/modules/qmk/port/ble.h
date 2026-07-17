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

// 호스트 프로파일 개수. ZMK 기본값과 동일(CONFIG_BT_MAX_PAIRED 와 맞출 것).
#define BLE_PROFILE_COUNT   5

bool bleInit(void);

// 현재 BLE 로 리포트를 보낼 수 있는 상태인지(활성 프로파일이 연결됨)
bool bleIsConnected(void);

// QMK host_driver(port/driver_ble.c) 가 호출하는 전송 API
bool bleSendKeyboard(report_keyboard_t *report);
bool bleSendExtra(report_extra_t *report);

// 호스트가 보낸 LED 상태(CapsLock 등)
uint8_t bleGetKbdLeds(void);


/*
 * 프로파일 — 호스트 5대 전환 (ZMK app/src/ble.c 패턴).
 *
 * ZMK 와 동일하게 **여러 호스트가 동시에 연결된 채로 있고**, 리포트는 활성 프로파일의
 * 연결로만 나간다. 전환 시 재연결을 기다리지 않아 즉시 바뀐다.
 * 프로파일 인덱스와 각 프로파일의 peer 주소는 settings(NVS)에 저장돼 재부팅 후에도 유지된다.
 */

uint8_t bleProfileGetActive(void);
bool    bleProfileSelect(uint8_t index);
bool    bleProfileNext(void);
bool    bleProfilePrev(void);

// 해당 프로파일의 본딩을 지운다.
bool    bleProfileClear(uint8_t index);
// 활성 프로파일의 본딩을 지운다 (ZMK BT_CLR).
bool    bleProfileClearActive(void);
// 전 프로파일의 본딩을 지우고 프로파일 0 으로 돌아간다 (ZMK BT_CLR_ALL).
void    bleProfileClearAll(void);

// 프로파일에 본딩된 호스트가 없다 = 새 호스트를 받을 수 있는 상태.
bool    bleProfileIsOpen(uint8_t index);
// 해당 프로파일의 호스트가 지금 연결돼 있나.
bool    bleProfileIsConnected(uint8_t index);


/*
 * TX power (dBm).
 *
 * Zephyr **표준** VS HCI(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL, 0xfc0e)를 쓴다 — NCS 전용 API 가
 * 아니다. NCS SoftDevice Controller 도 이 명령을 그대로 구현하므로(hci_internal.c) 순수 Zephyr
 * 컨트롤러로 옮겨도 동작한다. `CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL=y` 가 전제.
 *
 * 컨트롤러가 지원하지 않는 값은 **가장 가까운 지원값으로 클램프**된다(요청값과 다를 수 있다).
 */
bool   bleSetTxPower(int8_t dbm);
int8_t bleGetTxPower(void);
