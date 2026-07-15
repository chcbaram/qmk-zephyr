cmake_minimum_required(VERSION 3.13)

# Phase 0: RGB 미사용(추후 Phase 6에서 ZMK led_strip 흡수).
set(RGBLIGHT_ENABLE false)

# QMK 디바운스 알고리즘 (quantum/debounce/<TYPE>.c)
set(DEBOUNCE_TYPE sym_defer_pk)
