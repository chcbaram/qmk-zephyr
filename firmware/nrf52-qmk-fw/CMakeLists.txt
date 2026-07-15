cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})



project(nrf52-qmk-fw)


# 빌드할 키보드 선택. 미지정 시 board 이름으로 기본값 유도.
# 예: west build -b NRF52840/nrf52840 -- -DKEYBOARD_PATH=keyboards/baram/wish60
if (NOT DEFINED KEYBOARD_PATH)
  set(KEYBOARD_PATH "keyboards/baram/wish60")
endif()

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
