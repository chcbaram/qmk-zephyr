#pragma once

// QMK core references QMK_BUILDDATE (via.c EEPROM magic 등). 값 자체는 임의.
#define QMK_VERSION     "qmk-zephyr"
/*
 * VIA EEPROM 매직의 근거(quantum/via.c: via_eeprom_is_valid).
 * 실제 빌드 날짜가 아니라 **고정 문자열**이다 — 그래야 펌웨어를 새로 올려도 사용자의
 * 키맵이 살아남는다(baram-qmk 원본도 동일한 방식).
 *
 * [규칙] EEPROM 레이아웃이 바뀌면 반드시 이 값을 올릴 것. 안 올리면 EEPROM 이 "유효"로
 * 판정돼 옛 데이터를 **어긋난 주소에서** 읽는다(키맵이 전부 틀어짐). 레이아웃을 바꾸는 것:
 *   VIA_EEPROM_CUSTOM_CONFIG_SIZE, VIA_EEPROM_LAYOUT_OPTIONS_SIZE,
 *   DYNAMIC_KEYMAP_LAYER_COUNT, MATRIX_ROWS/COLS, 매크로 버퍼 크기 등
 * 반대로 로직만 바뀌는 일반 업데이트에선 절대 올리지 말 것(키맵이 날아간다).
 *
 * 2026-07-17: VIA_EEPROM_CUSTOM_CONFIG_SIZE 16 추가로 dynamic keymap 주소가 밀림
 */
#define QMK_BUILDDATE   "2026-07-17-00:00:00"
