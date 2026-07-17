#pragma once

#include <stdint.h>

/*
 * 전력 설정(idle/sleep 타임아웃) — EEPROM 저장 + VIA 채널 핸들러를 자기가 소유한다.
 * (baram-qmk 의 debounce_cfg.c / kill_switch.c 와 같은 구성. via_port.c 는 라우팅만 한다)
 */

// VIA 값 ID. VIA 의 dropdown 은 **1바이트**라 단위를 초/분으로 잡아 0~255 에 맞춘다.
// (RGB 를 분 단위로 두는 이유: 초 단위면 4분 15초가 한계라 5분을 못 넣는다. sleep 과도 일관된다)
enum
{
  id_qmk_power_idle_timeout  = 1,   // 초  (0 = 비활성)
  id_qmk_power_sleep_timeout = 2,   // 분  (0 = 안 잠)
  id_qmk_power_rgb_timeout   = 3,   // 분  (0 = 안 끔 — ZMK 와 같은 동작)
};

// EEPROM 값을 읽어 activity 에 적용. activityInit() 뒤에 호출할 것.
void power_cfg_init(void);

// VIA 커스텀 채널(ID_QMK_POWER_CHANNEL) 처리.
void via_qmk_power_command(uint8_t *data, uint8_t length);
