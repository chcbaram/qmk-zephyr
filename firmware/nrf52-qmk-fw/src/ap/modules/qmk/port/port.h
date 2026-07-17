#pragma once

// QMK quantum.h -> platform_deps.h -> port.h.
// baram 포크는 여기서 kill_switch/kkuk/chattering 등 확장을 끌어왔으나,
// 이 프로젝트(Zephyr 포팅)에서는 QMK 순정 코어만 쓰므로 최소로 유지한다.

// config.h(EECONFIG_USER_DATA_SIZE 등)는 CMake 의 -include 로 모든 TU 맨 앞에 들어간다.
// eeconfig.h 보다 먼저여야 하기 때문 — qmk/CMakeLists.txt 의 주석 참고.
#include "version.h"
#include "eeconfig.h"

/*
 * EEPROM 사용자 영역 맵 (baram-qmk port/port.h 와 동일한 방식).
 *
 * [왜 이렇게 하나]
 * QMK 의 EEPROM 주소는 앞 영역 크기의 누적이다:
 *   EECONFIG_BASE_SIZE + EECONFIG_KB_DATA_SIZE + EECONFIG_USER_DATA_SIZE = EECONFIG_SIZE
 *     -> VIA_EEPROM_MAGIC_ADDR -> ... -> DYNAMIC_KEYMAP_EEPROM_START
 * 즉 **앞쪽 크기를 바꾸면 키맵 주소가 통째로 밀린다**(사용자 키맵이 어긋나거나 날아간다).
 *
 * 그래서 EECONFIG_USER_DATA_SIZE 를 **넉넉히 한 번(512B, config.h) 잡아 고정**하고,
 * 설정은 그 안에서 **직접 오프셋**으로 배치한다. 새 설정은 아래 빈 오프셋을 쓰면 되므로
 * 크기가 안 바뀌고 -> 주소가 안 밀리고 -> 키맵이 살아남는다.
 * (VIA_EEPROM_CUSTOM_CONFIG_SIZE 로 잡으면 항목을 추가할 때마다 크기가 바뀌어 키맵이 밀린다)
 *
 * [규칙]
 *  - 배정된 오프셋은 **재사용/이동 금지**. 새 항목은 뒤의 빈 자리에 추가한다.
 *  - EECONFIG_USER_DATA_SIZE 를 바꾸면 QMK_BUILDDATE(port/version.h)도 반드시 올릴 것.
 *  - eeconfig_init_user_datablock() 은 이 영역을 **0 으로 민다**. 그러므로 각 항목은
 *    "저장된 적 있음"을 스스로 판별해야 한다(magic 등). 0 을 유효값으로 쓰면 안 된다.
 */

//                                                                        offset  size
#define EECONFIG_USER_POWER   ((void *)((uint32_t)EECONFIG_USER_DATABLOCK +  0))  // 4B  idle/sleep 타임아웃
#define EECONFIG_USER_BLE     ((void *)((uint32_t)EECONFIG_USER_DATABLOCK +  4))  // 4B  TX power
// 다음 빈 오프셋: 8   (예정: 런타임 디바운스)
