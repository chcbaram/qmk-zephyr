#pragma once

#include <stdint.h>

/*
 * VIA 커스텀 채널 라우터.
 *
 * quantum/via.c 의 weak via_custom_value_command_kb() 를 오버라이드해 채널별로 넘긴다.
 * 값 처리/EEPROM 은 각 기능이 소유한다(power_cfg.c, sys_port.c) — baram-qmk 와 같은 구성.
 *
 * quantum/via.h 의 via_channel_id enum 은 건드리지 않는다(QMK 무수정 원칙).
 * 채널 ID 는 숫자로만 통하므로 여기서 정의하면 충분하다.
 * 8~14 는 baram-qmk 가 이미 쓰고 있어(via.h) 겹치지 않게 잡는다.
 *
 * 프로토콜: data = [command_id, channel_id, value_id, value_data...]
 *   id_custom_set_value(0x07) / id_custom_get_value(0x08) / id_custom_save(0x09)
 *
 * [값 폭] 컨트롤 타입이 정한다 (공식 스펙: https://caniusevia.com/docs/custom_ui)
 *   toggle   : 1B (0/1)
 *   range    : max<=255 면 1B, 아니면 2B 빅엔디안   ← 최댓값이 폭을 바꾼다
 *   dropdown : 1B (인덱스 또는 지정값)
 *   button   : 1B
 *   color    : 2B (hue, sat)
 *   keycode  : 2B 빅엔디안
 */

#define ID_QMK_SYSTEM_CHANNEL   9    // baram-qmk 와 동일 번호 (DFU / EEPROM clean)
#define ID_QMK_POWER_CHANNEL    15   // 전력 타임아웃 (신규)
#define ID_QMK_BLE_CHANNEL      16   // BLE 프로파일 (신규)
#define ID_QMK_DEBOUNCE_CHANNEL 17   // 디바운스 시간 (신규)
#define ID_QMK_HOLD_OKP_CHANNEL 18   // HOLD_ON_OTHER_KEY_PRESS (신규)

// EEPROM 설정을 읽어 적용. qmkInit() 에서 activityInit() 뒤에 호출.
void viaPortInit(void);
