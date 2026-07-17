#pragma once

#include <stdint.h>

/*
 * BLE 프로파일 VIA 채널 — 프로파일 선택 + 본딩 삭제.
 * (기능이 자기 VIA 핸들러를 소유한다. via_port.c 는 라우팅만 — power_cfg.c 와 같은 구성)
 *
 * 영속화는 port/ble.c 가 settings(NVS)로 이미 한다 → 여기엔 EEPROM 이 없다.
 */

enum
{
  id_qmk_ble_profile   = 1,   // dropdown : 활성 프로파일 (0~4)
  id_qmk_ble_bond_0    = 2,   // toggle   : 본딩 유무. 끄면 그 프로파일 본딩 삭제
  id_qmk_ble_bond_1    = 3,
  id_qmk_ble_bond_2    = 4,
  id_qmk_ble_bond_3    = 5,
  id_qmk_ble_bond_4    = 6,
  id_qmk_ble_clear_all = 7,   // button   : 전 프로파일 본딩 삭제
  id_qmk_ble_tx_power  = 8,   // dropdown : TX power (dBm 이 아니라 **인덱스**)
};

/*
 * TX power 를 dBm 이 아니라 인덱스로 주고받는 이유:
 * VIA 의 dropdown 값은 **1바이트 부호없음**이라 -40dBm 같은 음수를 그대로 못 싣는다.
 * JSON 의 options 순서와 아래 표가 **반드시 일치**해야 한다.
 */
#define BLE_TX_POWER_TBL   { -40, -20, -12, -8, -4, 0, 4, 8 }
#define BLE_TX_POWER_DEF   5     // 0 dBm

// EEPROM 의 TX power 를 읽어 적용. bleInit() 뒤에 호출.
void ble_cfg_init(void);

void via_qmk_ble_command(uint8_t *data, uint8_t length);
