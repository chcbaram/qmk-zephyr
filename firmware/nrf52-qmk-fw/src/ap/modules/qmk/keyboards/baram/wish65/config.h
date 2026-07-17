#pragma once


#define KBD_NAME                    "WISH65"

// VIA/USB 식별자 (usb.c 디스크립터가 이 값을 사용, VIA JSON/info.json 과 일치)
// PID 는 다른 baram 보드와 겹치지 않게: 0x5201~0x5208,0x5210 사용중 → wish65 = 0x5209
#define USB_VID                     0x0483
#define USB_PID                     0x5209


// eeprom — emu-eeprom(DTS eeprom0 size) 및 port/platforms/eeprom.c PORT_EEPROM_SIZE 와 일치
#define TOTAL_EEPROM_BYTE_COUNT     4096

// 우리 설정(전력 타임아웃 등)을 담을 사용자 영역. baram-qmk/VENOM 과 동일하게 512B 고정.
// 항목별 오프셋은 port/port.h 의 맵 참고. [주의] 이 크기를 바꾸면 키맵 주소가 밀린다 —
// 그래서 넉넉히 한 번 잡고 그 안에서만 늘린다.
#define EECONFIG_USER_DATA_SIZE     512

#define DYNAMIC_KEYMAP_LAYER_COUNT  8

// 매트릭스 (DTS kbd_matrix 노드의 개수와 반드시 일치: wish65 = 5 x 16)
#define MATRIX_ROWS                 5
#define MATRIX_COLS                 16

/*
 * [필수] 이 보드는 **col2row** 다 — Zephyr 의 구동측(col-gpios/INPUT_ABS_X)이 QMK 의 col 이다.
 * wish60(row2col)과 **반대**이고, 이 매크로가 없으면 **컴파일은 되고 키맵이 전치되어** 나온다.
 *
 * 왜 col2row 일 수밖에 없나: 컬럼을 74HC595 로 구동하는데 595 는 **출력 전용**(읽기 불가)이라
 * 반드시 구동측에 있어야 한다. 구동측이 col 이면 다이오드는 col2row 다. 구조적 귀결이다.
 * port/matrix.c 의 매핑 주석 참고.
 */
#define MATRIX_DRIVE_IS_QMK_COL

#define DEBOUNCE                    10

// QMK 처리 주기(ms). board DTS 의 kbd_matrix poll-period-ms 와는 별개 노브다(qmk.h 주석 참고).
// DTS 의 poll-period-ms(4ms)와 **다르다**. 의도적이다.
//
// 매트릭스가 4ms 마다만 갱신되므로 QMK 를 2ms 로 돌려도 **키 감지는 안 빨라진다**. 그럼에도
// 2ms 인 이유는 **디바운스 분해능** 때문이다 — sym_defer_pk 는 매 호출마다 elapsed 만큼 카운터를
// 줄이므로, 4ms 루프에서는 설정 10ms 가 실효 12ms 로 올림된다(10->6->2->0). 2ms 면 정확히 10ms.
//
// wish65 실측(키 누른 채, QMK 주기만 바꿈): 2ms=3.18mA / 4ms=2.81mA — **차이 0.37mA**.
// 그 정도면 디바운스 정확도 + wish60 과의 일관성을 사는 값으로 낼 만하다.
//
// [주의] wish60 의 §6.4 표(1ms=3.40 / 2ms=2.43 / 4ms=1.40)는 **매트릭스 주기와 QMK 주기를
// 함께** 바꾼 값이다. "주기 2배마다 ~1mA" 의 대부분은 **매트릭스 스캔 몫**이고 QMK 루프 단독은
// 위처럼 0.37mA 다. 그 표를 QMK 단독 효과로 읽지 말 것(실제로 그렇게 오독했다).
#define QMK_TASK_PERIOD_MS          2

// 언더글로우 — DTS led_strip 의 chain-length 와 반드시 일치(BUILD_ASSERT 가 본다).
#define RGB_MATRIX_LED_COUNT        18
// 기본값은 (COUNT+4)/5 개씩 나눠 렌더 → 한 프레임에 task 호출이 여러 번 필요하다.
// 18개는 한 번에 렌더해도 싸므로 호출 수를 줄여 프레임을 빨리 완성시킨다(끊김 방지).
#define RGB_MATRIX_LED_PROCESS_LIMIT RGB_MATRIX_LED_COUNT
// 밝기 상한 — **보드가 정하는 전력 예산**이다(그래서 QMK 가 이걸 키보드 config.h 에 뒀다).
// VIA 는 0~255 를 scale8(v, MAXIMUM_BRIGHTNESS) 로 스케일하므로 슬라이더는 항상
// "이 보드가 허용하는 범위의 0~100%" 를 뜻한다. 낮춰도 정상 동작이다.
//
// wish60 과 동일하게 하드웨어 한계까지 연다. 실측(wish60, 16개, breathing VAL 60): 65.08mA.
// [주의] 18개 풀 화이트 = 개당 ~60mA -> **약 1.1A**. 배터리(1000mAh)로는 한 시간도 못 간다.
// 상한은 열되 **기본값은 낮게** 둔다. 근거와 지속시간 표는 docs/PORTING-NOTES.md §6.10.
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 255
#define RGB_MATRIX_DEFAULT_VAL      60
// 켜진 상태로 부팅하지 않는다. ramune60(유선)은 default on 이지만 우리는 **무선**이라 다르다 —
// 18개 언더글로우는 배터리에서 수십 mA 라 사용자가 켜기로 선택해야 한다.
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
