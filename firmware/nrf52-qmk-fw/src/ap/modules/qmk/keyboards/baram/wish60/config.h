#pragma once


#define KBD_NAME                    "WISH60"

// VIA/USB 식별자 (usb.c 디스크립터가 이 값을 사용, VIA JSON/info.json 과 일치)
// PID 는 다른 baram 보드와 겹치지 않게: 0x5201~0x5207,0x5210 사용중 → wish60 = 0x5208
#define USB_VID                     0x0483
#define USB_PID                     0x5208


// eeprom — emu-eeprom(DTS eeprom0 size) 및 port/platforms/eeprom.c PORT_EEPROM_SIZE 와 일치
#define TOTAL_EEPROM_BYTE_COUNT     4096

// 우리 설정(전력 타임아웃 등)을 담을 사용자 영역. baram-qmk/VENOM 과 동일하게 512B 고정.
// 항목별 오프셋은 port/port.h 의 맵 참고. [주의] 이 크기를 바꾸면 키맵 주소가 밀린다 —
// 그래서 넉넉히 한 번 잡고 그 안에서만 늘린다.
#define EECONFIG_USER_DATA_SIZE     512

#define DYNAMIC_KEYMAP_LAYER_COUNT  8

// 매트릭스 (DTS kscan/keys 노드의 row/col 개수와 반드시 일치: wish60 = 5 x 15)
#define MATRIX_ROWS                 5
#define MATRIX_COLS                 15

#define DEBOUNCE                    10

// QMK 처리 주기(ms). board DTS 의 kbd_matrix poll-period-ms 와는 별개 노브다(qmk.h 주석 참고).
// 실측(키 누른 채): 1ms=3.40mA / 2ms=2.43mA / 4ms=1.40mA. USB 리포트율은 1000/N Hz.
// 2 = 전력/지연 균형점. BLE 는 연결 간격 11.25ms 라 이 값과 무관하다.
#define QMK_TASK_PERIOD_MS          2

// 언더글로우 — DTS led_strip 의 chain-length / HW_WS2812_MAX_CH 와 반드시 일치.
#define RGB_MATRIX_LED_COUNT        16
// 무선 키보드라 밝기를 제한한다. 16개 풀 화이트는 수백 mA 로 배터리를 즉시 말린다.
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 120
#define RGB_MATRIX_DEFAULT_VAL      60
// 켜진 상태로 부팅하지 않는다. ramune60(유선)은 default on 이지만 우리는 **무선**이라 다르다 —
// 16개 언더글로우는 배터리에서 수십 mA 라 사용자가 켜기로 선택해야 한다.
// (ZMK wish60 의 RGB_UNDERGLOW_ON_START=n 과 같은 판단)
#define RGB_MATRIX_DEFAULT_ON       false
// idle/서스펜드 시 LED 소등 훅(rgb_matrix_set_suspend_state). ramune60 도 sleep:True.
// Phase 8-C 에서 activity 상태머신이 이걸 호출한다.
#define RGB_MATRIX_SLEEP
// 효과 — baram 의 upstream QMK 보드 ramune60 과 동일한 17개(keyboard.json 의 rgb_matrix.animations).
// 47개 전부 넣으면 플래시만 먹고, 언더글로우에선 키 반응형(reactive) 효과가 의미 없다.
#define ENABLE_RGB_MATRIX_GRADIENT_UP_DOWN
#define ENABLE_RGB_MATRIX_GRADIENT_LEFT_RIGHT
#define ENABLE_RGB_MATRIX_BREATHING
#define ENABLE_RGB_MATRIX_BAND_VAL
#define ENABLE_RGB_MATRIX_BAND_PINWHEEL_VAL
#define ENABLE_RGB_MATRIX_BAND_SPIRAL_VAL
#define ENABLE_RGB_MATRIX_CYCLE_ALL
#define ENABLE_RGB_MATRIX_CYCLE_LEFT_RIGHT
#define ENABLE_RGB_MATRIX_CYCLE_UP_DOWN
#define ENABLE_RGB_MATRIX_RAINBOW_MOVING_CHEVRON
#define ENABLE_RGB_MATRIX_CYCLE_OUT_IN
#define ENABLE_RGB_MATRIX_CYCLE_OUT_IN_DUAL
#define ENABLE_RGB_MATRIX_CYCLE_PINWHEEL
#define ENABLE_RGB_MATRIX_CYCLE_SPIRAL
#define ENABLE_RGB_MATRIX_RAINBOW_BEACON
#define ENABLE_RGB_MATRIX_RAINBOW_PINWHEELS
#define ENABLE_RGB_MATRIX_PIXEL_FLOW

#define GRAVE_ESC_ENABLE
