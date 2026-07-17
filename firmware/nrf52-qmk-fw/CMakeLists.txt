cmake_minimum_required(VERSION 3.20.0)

# 디버그 콘솔(UART/CLI/로그) — 기본 OFF(저전력). 개발 시: -DDEBUG_CONSOLE=y
#
# C 코드(_USE_HW_DEBUG_CONSOLE)와 Kconfig(debug.conf 의 CONFIG_SERIAL 등)를 반드시
# 한 스위치로 함께 움직인다. 둘이 갈라지면 조용히 전력을 먹는다 — 실제로 C 만 껐을 때
# Zephyr 콘솔 스택이 Kconfig 로 남아 1.13mA 를 계속 먹었다(콘솔 완전 제거 시 80.9µA).
# EXTRA_CONF_FILE 은 find_package(Zephyr) 전에 설정돼야 한다.
option(DEBUG_CONSOLE "Enable debug console (UART/CLI/log). Costs ~1mA — off for battery builds." OFF)
if (DEBUG_CONSOLE)
  list(APPEND EXTRA_CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/debug.conf)
endif()

# 우리 DTS 바인딩(dts/bindings/) 을 Zephyr 가 찾게 한다 — 595 시프트레지스터(baram,gpio-595)용.
# find_package(Zephyr) 전에 설정돼야 한다.
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})



project(nrf52-qmk-fw)

if (DEBUG_CONSOLE)
  target_compile_definitions(app PRIVATE _USE_HW_DEBUG_CONSOLE)
endif()


# 빌드할 키보드 선택 — **board 이름에서 유도한다**.
#
# 키보드 한 대 = 두 트리다(§4.1): 하드웨어는 boards/baram/<board>/, 로직은
# keyboards/baram/<board>/. 이름 규약이 그 둘을 잇는 유일한 장치다.
#
# [왜 고정 기본값을 두지 않나] 예전엔 wish60 으로 고정돼 있었다. 그러면 `west build -b wish65`
# 가 **에러 없이 성공하고 wish60 의 키맵/MATRIX/USB PID 로 빌드된다** — 조용히 틀리는 게
# 컴파일 에러보다 나쁘다. 없는 키보드는 여기서 즉시 FATAL_ERROR 로 잡는다.
if (NOT DEFINED KEYBOARD_PATH)
  set(KEYBOARD_PATH "keyboards/baram/${BOARD}")
endif()
if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/ap/modules/qmk/${KEYBOARD_PATH}/config.h")
  message(FATAL_ERROR
    "키보드 트리가 없다: src/ap/modules/qmk/${KEYBOARD_PATH}\n"
    "  board '${BOARD}' 에 대응하는 키보드가 없다. 다음 중 하나를 하라:\n"
    "   - src/ap/modules/qmk/keyboards/baram/${BOARD}/ 를 만든다 (config.h 필수)\n"
    "   - 또는 -DKEYBOARD_PATH=keyboards/baram/<kbd> 로 명시한다")
endif()
message(STATUS "keyboard: ${KEYBOARD_PATH}  (board: ${BOARD})")

# QMK 코어 vendor-in 빌드 조각 (QMK_SRC_FILES / QMK_INC_DIR / compile defs 설정)
include(src/ap/modules/qmk/CMakeLists.txt)


# 지정한 폴더에 있는 파일만 포함한다.
#
file(GLOB SRC_FILES CONFIGURE_DEPENDS
  src/*.c
  src/ap/*.c
)

# 하위폴더에 있는 파일까지 포함한다. (qmk 모듈은 위 include 에서 선택적으로 처리)
#
file(GLOB_RECURSE SRC_FILES_RECURSE CONFIGURE_DEPENDS
  src/ap/modules/module.c
  src/ap/modules/common/*.c
  src/bsp/*.c
  src/common/*.c
  src/hw/*.c
)

target_sources(app PRIVATE
  ${SRC_FILES}
  ${SRC_FILES_RECURSE}
  ${QMK_SRC_FILES}
)

target_include_directories(app PRIVATE
  src
  src/ap
  src/ap/modules
  src/ap/modules/common
  src/bsp
  src/common
  src/common/core
  src/common/hw/include
  src/hw
  src/hw/driver/usb
  src/lib
  ${QMK_INC_DIR}
)

zephyr_linker_sources(SECTIONS src/bsp/ldscript/ldscript.ld)
