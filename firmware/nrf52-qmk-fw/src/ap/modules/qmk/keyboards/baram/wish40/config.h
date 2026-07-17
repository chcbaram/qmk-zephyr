#pragma once


#define KBD_NAME                    "WISH40"

// VIA/USB 식별자 (usb.c 디스크립터가 이 값을 사용, VIA JSON/info.json 과 일치)
// PID 는 다른 baram 보드와 겹치지 않게: 0x5201~0x5209,0x5210 사용중 → wish40 = 0x520A
#define USB_VID                     0x0483
#define USB_PID                     0x520A


// eeprom — emu-eeprom(DTS eeprom0 size) 및 port/platforms/eeprom.c PORT_EEPROM_SIZE 와 일치
#define TOTAL_EEPROM_BYTE_COUNT     4096

// 우리 설정(전력 타임아웃 등)을 담을 사용자 영역. baram-qmk/VENOM 과 동일하게 512B 고정.
// 항목별 오프셋은 port/port.h 의 맵 참고. [주의] 이 크기를 바꾸면 키맵 주소가 밀린다 —
// 그래서 넉넉히 한 번 잡고 그 안에서만 늘린다.
#define EECONFIG_USER_DATA_SIZE     512

#define DYNAMIC_KEYMAP_LAYER_COUNT  8

// 매트릭스 (DTS kbd_matrix 노드의 개수와 반드시 일치: wish40 = 4 x 12)
// row2col 이라 Zephyr 의 구동측(col-gpios) = QMK row 다 — wish60 과 같은 방향이므로
// MATRIX_DRIVE_IS_QMK_COL 은 **필요 없다**(그건 wish65 처럼 col2row 인 보드용).
#define MATRIX_ROWS                 4
#define MATRIX_COLS                 12

#define DEBOUNCE                    10

// QMK 처리 주기(ms). board DTS 의 kbd_matrix poll-period-ms 와는 별개 노브다(qmk.h 주석 참고).
// 실측(키 누른 채): 1ms=3.40mA / 2ms=2.43mA / 4ms=1.40mA. USB 리포트율은 1000/N Hz.
// 2 = 전력/지연 균형점. BLE 는 연결 간격 11.25ms 라 이 값과 무관하다.
#define QMK_TASK_PERIOD_MS          2

// 언더글로우 — DTS led_strip 의 chain-length 와 반드시 일치(BUILD_ASSERT 가 본다).
#define RGB_MATRIX_LED_COUNT        42
// 기본값은 (COUNT+4)/5 개씩 나눠 렌더 → 한 프레임에 task 호출이 여러 번 필요하다.
// 한 번에 렌더해 호출 수를 줄이고 프레임을 빨리 완성시킨다(끊김 방지).
// [미확인] 42개는 wish60(16)의 2.6배라 렌더 1회가 그만큼 길다. 타이핑에 영향이 보이면
// 기본값(분할 렌더)으로 되돌릴 것.
#define RGB_MATRIX_LED_PROCESS_LIMIT RGB_MATRIX_LED_COUNT
// 밝기 상한 — **보드가 정하는 전력 예산**이다(그래서 QMK 가 이걸 키보드 config.h 에 뒀다).
// VIA 는 0~255 를 scale8(v, MAXIMUM_BRIGHTNESS) 로 스케일하므로 슬라이더는 항상
// "이 보드가 허용하는 범위의 0~100%" 를 뜻한다. 낮춰도 정상 동작이다.
//
// **wish40 은 상한을 낮춘다 — LED 가 42개라 열어두면 전류가 감당이 안 된다**(사용자 결정: 128).
//
// 42개 풀 화이트는 개당 ~60mA 라 상한 255 면 **2.5A** 로, 배터리로는 불가능하고 보호회로가
// 걸린다. 128(50%)이어도 ~1.25A 라 여전히 크다 — 실측 후 더 낮출 수 있다(§6.10 의 "보드별
// 전력 예산 노브" 가 정확히 이 용도다). 참고로 wish60(16개, 상한 255)과 같은 최악 전류가
// 되는 값은 ~96 이다.
//
// VIA 슬라이더는 0~255 를 scale8(v, MAXIMUM_BRIGHTNESS) 로 스케일하므로 사용자에겐 항상
// "이 보드가 허용하는 범위의 0~100%" 로 보인다 — 낮춰도 UI 가 어색해지지 않는다.
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 128
#define RGB_MATRIX_DEFAULT_VAL      60
// 켜진 상태로 부팅하지 않는다. ramune60(유선)은 default on 이지만 우리는 **무선**이라 다르다 —
// 42개 언더글로우는 배터리에서 수백 mA 라 사용자가 켜기로 선택해야 한다.
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

// HOLD_ON_OTHER_KEY_PRESS 를 **런타임 콜백**으로 받는다 (config.cmake 의 HOLD_OKP_RUNTIME).
// 이게 없으면 action_tapping.c 의 TAP_GET_HOLD_ON_OTHER_KEY_PRESS 가 상수 false 로 굳어
// **VIA 토글이 조용히 무효가 된다**. 구현은 port/via/hold_okp.c 의 get_hold_on_other_key_press().
#define HOLD_ON_OTHER_KEY_PRESS_PER_KEY

#define GRAVE_ESC_ENABLE
