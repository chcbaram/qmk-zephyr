// Copyright 2026 baram
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

/*
 * wish65 (5 x 16) — 표준 65% ANSI (67키).
 *
 * 매트릭스 (r,c) 는 zmk-config 의 wish65 matrix-transform(layout_6_25u_transform) 기준이다.
 * 그 transform 이 쓰지 않는 셀 = 실제 배선이 없는 자리 -> KC_NO:
 *   (0,13) (1,13) (2,1) (2,13) (3,1) (3,12)
 *   row4 는 (4,0) (4,1) (4,2) (4,6) (4,10) (4,11) (4,13) (4,14) (4,15) 만 쓴다.
 *
 * 우측 열: 0~2행은 col14/15, 3~4행은 col13~15 (방향키 + PgUp/PgDn).
 */
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = {
      //  0        1        2        3        4        5        6        7        8        9        10       11       12       13       14       15
        { QK_GESC, KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS, KC_EQL,  KC_NO,   KC_BSPC, KC_DEL  },
        { KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC, KC_NO,   KC_BSLS, KC_HOME },
        { KC_CAPS, KC_NO,   KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT, KC_NO,   KC_ENT,  KC_PGUP },
        { KC_LSFT, KC_NO,   KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH, KC_NO,   KC_RSFT, KC_UP,   KC_PGDN },
        { KC_LCTL, KC_LGUI, KC_LALT, KC_NO,   KC_NO,   KC_NO,   KC_SPC,  KC_NO,   KC_NO,   KC_NO,   KC_RALT, MO(1),   KC_NO,   KC_LEFT, KC_DOWN, KC_RGHT }
    },
    [1] = {
      //  0        1        2        3        4        5        6        7        8        9        10       11       12       13       14       15
        { QK_BOOT, KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,  KC_NO,   KC_DEL,  KC_INS  },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_PSCR, KC_SCRL, KC_PAUS, KC_NO,   KC_TRNS, KC_TRNS },
        { KC_TRNS, KC_NO,   KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_NO,   KC_TRNS, KC_TRNS },
        { KC_TRNS, KC_NO,   KC_TRNS, KC_MPRV, KC_MPLY, KC_MNXT, KC_MUTE, KC_VOLD, KC_VOLU, KC_TRNS, KC_END,  KC_TRNS, KC_NO,   KC_TRNS, KC_TRNS, KC_TRNS },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_NO,   KC_NO,   KC_NO,   KC_TRNS, KC_NO,   KC_NO,   KC_NO,   KC_TRNS, KC_TRNS, KC_NO,   KC_TRNS, KC_TRNS, KC_TRNS }
    }
};
