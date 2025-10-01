cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})



project(nrf52-qmk-fw)



# 지정한 폴더에 있는 파일만 포함한다.
#
file(GLOB SRC_FILES CONFIGURE_DEPENDS
  src/*.c
  src/ap/*.c  
)

# 하위폴더에 있는 파일까지 포함한다.
#
file(GLOB_RECURSE SRC_FILES_RECURSE CONFIGURE_DEPENDS
  src/ap/modules/*.c
  src/bsp/*.c
  src/common/*.c
  src/hw/*.c
)

target_sources(app PRIVATE 
  ${SRC_FILES}
  ${SRC_FILES_RECURSE}
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
)

zephyr_linker_sources(SECTIONS src/bsp/ldscript/ldscript.ld)
