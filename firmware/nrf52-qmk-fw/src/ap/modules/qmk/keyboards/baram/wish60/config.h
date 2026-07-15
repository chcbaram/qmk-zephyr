#pragma once


#define KBD_NAME                    "WISH60"

// VIA/USB 식별자 (usb.c 디스크립터 및 VIA JSON 과 일치)
#define USB_VID                     0x0483
#define USB_PID                     0x5206


// eeprom
//   Phase 1: RAM 백업(port/platforms/eeprom.c 의 PORT_EEPROM_SIZE 와 일치).
//   Phase 3: emu-eeprom(플래시 에뮬)로 영속화.
#define EECONFIG_USER_DATA_SIZE     512
#define TOTAL_EEPROM_BYTE_COUNT     4096

#define DYNAMIC_KEYMAP_LAYER_COUNT  8

// 매트릭스 (DTS kscan/keys 노드의 row/col 개수와 반드시 일치: wish60 = 5 x 15)
#define MATRIX_ROWS                 5
#define MATRIX_COLS                 15

#define DEBOUNCE                    5

#define GRAVE_ESC_ENABLE
