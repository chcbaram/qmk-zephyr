# QMK 코어 vendor-in 빌드 조각. 최상위 CMakeLists 에서 include() 한 뒤
# ${QMK_SRC_FILES} 를 target_sources 에, ${QMK_INC_DIR} 를 include 에 추가한다.
#
# baram-qmk 의 CMakeLists 를 nRF52/Zephyr 용으로 단순화:
#  - gen_layout.py / web_hid / 런타임 디바운스 / kkuk / kill_switch 등 baram 확장 제거
#  - port/ 는 Zephyr 어댑터(직접 작성)로 대체

set(QMK_ROOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/ap/modules/qmk")
set(QMK_KEYBOARD_PATH "${QMK_ROOT_PATH}/${KEYBOARD_PATH}")

if (EXISTS "${QMK_KEYBOARD_PATH}/config.cmake")
  include("${QMK_KEYBOARD_PATH}/config.cmake")
endif()

# 디바운스 알고리즘 파일 선택 (config.cmake 의 DEBOUNCE_TYPE)
if (NOT DEFINED DEBOUNCE_TYPE)
  set(DEBOUNCE_TYPE sym_defer_pk)
endif()
#
# DEBOUNCE_RUNTIME 이면 원본을 직접 컴파일하지 않고 port/debounce_wrapper.c 가 감싼다
# (원본이 debounce_time_get() 을 선언 없이 부른다 — 자세한 이유는 그 파일 주석 참고).
# 둘 다 컴파일하면 debounce() 가 중복 정의되므로 **원본은 목록에서 뺀다**.
# 래퍼를 port/debounce/ 하위에 둔 이유: 아래 glob 이 port/*.c 라 재귀가 아니다 — 같은 파일이
# glob 과 DEBOUNCE_FILES 양쪽에 잡히는 것을 피한다.
if (DEBOUNCE_RUNTIME)
  set(DEBOUNCE_FILES ${QMK_ROOT_PATH}/port/debounce/debounce_wrapper.c)
  add_compile_definitions(DEBOUNCE_IMPL_FILE="${QMK_ROOT_PATH}/quantum/debounce/${DEBOUNCE_TYPE}.c")
else()
  set(DEBOUNCE_FILES ${QMK_ROOT_PATH}/quantum/debounce/${DEBOUNCE_TYPE}.c)
endif()

# RGB 언더글로우 — 키보드 config.cmake 가 켠다.
#
# RGBLIGHT 가 아니라 RGB_MATRIX 다(위치 기반 효과. ramune60 과 같은 선택 — config.cmake 주석 참고).
# rgb_matrix_drivers.c 는 **키보드별**이다(보드마다 LED 배치가 다르다) — baram-qmk 규약.
# quantum/rgb_matrix/rgb_matrix_drivers.c(순정)는 컴파일하지 않는다: QMK 내장 ws2812 드라이버를
# 전제하는데 우리 백엔드는 Zephyr led_strip 이라 키보드 쪽 구현으로 대체한다.
if (RGB_MATRIX_ENABLE)
  list(APPEND QMK_ADD_FILES "${QMK_ROOT_PATH}/quantum/rgb_matrix/rgb_matrix.c")
  list(APPEND QMK_ADD_FILES "${QMK_ROOT_PATH}/quantum/process_keycode/process_rgb.c")
  list(APPEND QMK_ADD_FILES "${QMK_KEYBOARD_PATH}/driver/rgb_matrix_drivers.c")
  # rgb_matrix 가 <lib/lib8tion/lib8tion.h> 를 쓴다(QMK 의 8비트 고정소수점 수학 라이브러리).
  # upstream QMK 에서 src/lib/lib8tion 으로 vendor-in — src/lib 는 이미 include 경로다.
  list(APPEND QMK_ADD_FILES "${CMAKE_CURRENT_LIST_DIR}/../../../lib/lib8tion/lib8tion.c")
  add_compile_definitions(RGB_MATRIX_ENABLE)
endif()

# 컴파일할 파일만 명시적으로 나열 (quantum 트리 전체를 긁지 않는다)
file(GLOB QMK_SRC_FILES CONFIGURE_DEPENDS
  ${QMK_ROOT_PATH}/*.c
  ${QMK_ROOT_PATH}/port/*.c
  ${QMK_ROOT_PATH}/port/platforms/*.c
  ${QMK_ROOT_PATH}/port/protocol/*.c
  ${QMK_ROOT_PATH}/port/via/*.c

  # 보드별 port (VIA 채널 라우터 등). 보드마다 노출 채널이 달라 여기 둔다 — baram-qmk 와 동일.
  ${QMK_KEYBOARD_PATH}/port/*.c

  ${QMK_ROOT_PATH}/quantum/quantum.c
  ${QMK_ROOT_PATH}/quantum/via.c
  ${QMK_ROOT_PATH}/quantum/keyboard.c
  ${QMK_ROOT_PATH}/quantum/action.c
  ${QMK_ROOT_PATH}/quantum/action_tapping.c
  ${QMK_ROOT_PATH}/quantum/action_util.c
  ${QMK_ROOT_PATH}/quantum/action_layer.c
  ${QMK_ROOT_PATH}/quantum/keycode_config.c
  ${QMK_ROOT_PATH}/quantum/led.c
  ${QMK_ROOT_PATH}/quantum/keymap_common.c
  ${QMK_ROOT_PATH}/quantum/dynamic_keymap.c
  ${QMK_ROOT_PATH}/quantum/eeconfig.c
  ${QMK_ROOT_PATH}/quantum/keymap_introspection.c
  ${QMK_ROOT_PATH}/quantum/color.c
  ${QMK_ROOT_PATH}/quantum/mousekey.c

  ${QMK_ROOT_PATH}/quantum/sequencer/*.c
  ${QMK_ROOT_PATH}/quantum/logging/*.c
  ${QMK_ROOT_PATH}/quantum/send_string/*.c
  ${QMK_ROOT_PATH}/quantum/process_keycode/process_key_override.c
  ${QMK_ROOT_PATH}/quantum/process_keycode/process_grave_esc.c

  ${DEBOUNCE_FILES}
  ${QMK_ADD_FILES}
)

set(QMK_INC_DIR
  ${QMK_ROOT_PATH}/port
  ${QMK_ROOT_PATH}/port/platforms
  ${QMK_ROOT_PATH}/port/protocol
  ${QMK_ROOT_PATH}/port/via
  ${QMK_ROOT_PATH}/quantum
  ${QMK_ROOT_PATH}/quantum/logging
  ${QMK_ROOT_PATH}/quantum/keymap_extras
  ${QMK_ROOT_PATH}/quantum/sequencer
  ${QMK_ROOT_PATH}/quantum/send_string
  ${QMK_ROOT_PATH}/quantum/process_keycode
  ${QMK_ROOT_PATH}/quantum/rgblight
  ${QMK_ROOT_PATH}/quantum/rgb_matrix
  ${QMK_ROOT_PATH}/quantum/rgb_matrix/animations
  ${QMK_ROOT_PATH}/quantum/rgb_matrix/animations/runners

  ${QMK_KEYBOARD_PATH}
  ${QMK_KEYBOARD_PATH}/port
)

# 런타임 디바운스 시간 (VIA). vendored sym_defer_pk.c 의 DEBOUNCE_RUNTIME 훅이
# debounce_time_get()(port/via/debounce_cfg.c)을 부른다 — QMK 쪽 수정은 없다.
# [주의] config.h 의 컴파일타임 DEBOUNCE 는 여전히 0 보다 커야 한다(`#if DEBOUNCE > 0` 가
# 구현 전체를 가둔다 — 0 이면 파일이 패스스루가 되어 런타임 값이 무의미해진다).
if (DEBOUNCE_RUNTIME)
  add_compile_definitions(DEBOUNCE_RUNTIME)
endif()

# HOLD_ON_OTHER_KEY_PRESS 런타임 on/off (VIA).
# 키보드 config.h 에 HOLD_ON_OTHER_KEY_PRESS_PER_KEY 가 있어야 action_tapping.c 가
# get_hold_on_other_key_press() 를 부른다 — 없으면 토글이 조용히 무효가 된다.
if (HOLD_OKP_RUNTIME)
  add_compile_definitions(HOLD_OKP_RUNTIME)
endif()

add_compile_definitions(VIA_ENABLE)
add_compile_definitions(RAW_ENABLE)
add_compile_definitions(DYNAMIC_KEYMAP_ENABLE)
add_compile_definitions(KEY_OVERRIDE_ENABLE)
add_compile_definitions(EXTRAKEY_ENABLE)
add_compile_definitions(MOUSEKEY_ENABLE)
add_compile_definitions(MOUSE_ENABLE)

add_compile_definitions(QMK_KEYBOARD_H="quantum.h")
add_compile_definitions(QMK_KEYMAP_CONFIG_H="${QMK_KEYBOARD_PATH}/config.h")

# 키보드 config.h 를 **모든 번역 단위 맨 앞에** 강제로 넣는다(실제 QMK 빌드 시스템과 동일).
#
# 없으면: quantum 헤더들이 여러 경로로 eeconfig.h 를 먼저 끌어오는데(예: via.h -> eeconfig.h
# 가 via.h -> action.h -> config.h 보다 먼저), 그 시점엔 EECONFIG_USER_DATA_SIZE 가 없어서
# eeconfig.h 의 `#ifndef -> 0` 이 걸린다. 그러면 데이터블록 API 선언이 통째로 빠지고
# (implicit declaration) 나중에 config.h 가 512 로 재정의해 **번역 단위마다 EEPROM 레이아웃이
# 어긋난다**. config.h 를 맨 앞에 두면 이 부류가 전부 사라진다.
target_compile_options(app PRIVATE -include "${QMK_KEYBOARD_PATH}/config.h")
add_compile_definitions(KEYMAP_C="${QMK_KEYBOARD_PATH}/keymap.c")

if (RGBLIGHT_ENABLE)
  add_compile_definitions(RGBLIGHT_ENABLE)
endif()
