#pragma once

#include <stdint.h>

/*
 * 시스템 채널 — 부트로더 진입 / EEPROM 초기화 (baram-qmk 의 port/sys_port.c 와 동일 구성).
 *
 * EEPROM 초기화는 되돌릴 수 없으므로 **토글 3개를 모두 켜야** 실행된다(오조작 방지).
 * VIA UI 쪽 라벨: Confirm 1 / Confirm 2 / Are you sure?
 */

enum
{
  id_qmk_system_dfu           = 1,   // 부트로더(UF2) 진입
  id_qmk_system_eep_reset_0   = 2,   // 확인 1
  id_qmk_system_eep_reset_1   = 3,   // 확인 2
  id_qmk_system_eep_reset_done = 4,  // 확인 3 → 3개 다 켜지면 실행
};

void via_qmk_system_command(uint8_t *data, uint8_t length);
