// Copyright 2026 baram
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

/*
 * wish40 (4 x 12) — 40% 42키.
 *
 * 매트릭스 (r,c) 는 zmk-config 의 wish40 matrix-transform(default_transform) 기준이다.
 * **주신 wish40.json 의 좌표와는 다르다** — JSON 은 물리 배치(폭)만 참고했고 좌표는 ZMK 를 따랐다.
 * 각 행의 실제 배선(= transform 이 쓰는 셀), 나머지는 KC_NO:
 *   row0: 0~11        (12키)
 *   row1: 0~10        (11키,           11 없음)
 *   row2: 0, 2~11     (11키, 1 없음)
 *   row3: 0,2,3,5,7,9,10,11  (8키)
 *
 * 레이어 3개(사용자 제공 VIA 화면 기준). 나머지 레이어는 VIA 가 EEPROM 에서 채운다.
 *
 * LT 키(사용자 지정):
 *   LT(1, KC_TAB)  — 탭(탭) / 레이어1(홀드)
 *   LT(2, KC_SLSH) — /?(탭) / 레이어2(홀드)
 */
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    /*
     * [0] 기본
     *   Grv/Esc  Q W E R T Y U I O P  Bspc
     *   LT1/Tab  A S D F G H J K L     Enter
     *   LShift     Z X C V B N M       Up  LT2//
     *   Ctrl Gui Alt   Space Space     Left Down Right
     */
    [0] = {
      //  0              1        2        3        4        5        6        7        8        9        10       11
        { QK_GESC,       KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_BSPC },
        { LT(1, KC_TAB), KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_ENT,  KC_NO   },
        { KC_LSFT,       KC_NO,   KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_UP,   LT(2, KC_SLSH) },
        { KC_LCTL,       KC_NO,   KC_LGUI, KC_LALT, KC_NO,   KC_SPC,  KC_NO,   KC_SPC,  KC_NO,   KC_LEFT, KC_DOWN, KC_RGHT }
    },

    /*
     * [1] 숫자/기호 — LT(1) 홀드
     *   Boot  1 2 3 4 5 6 7 8 9 0  (투명)
     *   나머지 투명. row2 의 , 는 유지.
     */
    [1] = {
      //  0        1        2        3        4        5        6        7        8        9        10       11
        { QK_BOOT, KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_TRNS },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_NO   },
        { KC_TRNS, KC_NO,   KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_COMM, KC_TRNS, KC_TRNS },
        { KC_TRNS, KC_NO,   KC_TRNS, KC_TRNS, KC_NO,   KC_TRNS, KC_NO,   KC_TRNS, KC_NO,   KC_TRNS, KC_TRNS, KC_TRNS }
    },

    /*
     * [2] RGB — LT(2) 홀드
     *   대부분 투명. row2 에 RGB_TOG / RGB_MOD.
     */
    [2] = {
      //  0        1        2        3        4        5        6        7        8        9        10       11
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_NO   },
        { KC_TRNS, KC_NO,   KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, RGB_TOG, RGB_MOD, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS },
        { KC_TRNS, KC_NO,   KC_TRNS, KC_TRNS, KC_NO,   KC_TRNS, KC_NO,   KC_TRNS, KC_NO,   KC_TRNS, KC_TRNS, KC_TRNS }
    }
};
