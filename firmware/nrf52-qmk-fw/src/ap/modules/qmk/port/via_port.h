#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * VIA 커스텀 채널 — 무선/전력 설정을 VIA UI 에 노출한다.
 *
 * quantum/via.h 의 via_channel_id enum 은 건드리지 않는다(QMK 무수정 원칙).
 * 채널 ID 는 숫자로만 통하므로 여기서 정의하면 충분하다.
 * 8~14 는 baram-qmk 가 이미 쓰고 있어(via.h) 15 부터 시작한다.
 *
 * 프로토콜(QMK via.c): data = [command_id, channel_id, value_id, value...]
 *   id_custom_set_value(0x07) / id_custom_get_value(0x08) / id_custom_save(0x09)
 * 우리는 weak 인 via_custom_value_command_kb() 를 오버라이드해서 받는다.
 */

#define ID_BARAM_POWER_CHANNEL      15

enum
{
  ID_POWER_IDLE_TIMEOUT  = 1,   // 초 단위 (VIA UI 가 다루기 쉬운 단위)
  ID_POWER_SLEEP_TIMEOUT = 2,   // 분 단위
};

// EEPROM 에 저장된 설정을 읽어 activity 에 적용. qmkInit() 에서 호출.
void viaPortInit(void);
