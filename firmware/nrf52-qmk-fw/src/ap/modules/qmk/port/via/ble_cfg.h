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
};

void via_qmk_ble_command(uint8_t *data, uint8_t length);
