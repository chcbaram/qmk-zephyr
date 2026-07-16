#pragma once


#define KBD_NAME                    "WISH60"

// VIA/USB 식별자 (usb.c 디스크립터가 이 값을 사용, VIA JSON/info.json 과 일치)
// PID 는 다른 baram 보드와 겹치지 않게: 0x5201~0x5207,0x5210 사용중 → wish60 = 0x5208
#define USB_VID                     0x0483
#define USB_PID                     0x5208


// eeprom — emu-eeprom(DTS eeprom0 size) 및 port/platforms/eeprom.c PORT_EEPROM_SIZE 와 일치
#define TOTAL_EEPROM_BYTE_COUNT     4096

// 우리 설정(전력 타임아웃 등)을 담을 사용자 영역. baram-qmk/VENOM 과 동일하게 512B 고정.
// 항목별 오프셋은 port/port.h 의 맵 참고. [주의] 이 크기를 바꾸면 키맵 주소가 밀린다 —
// 그래서 넉넉히 한 번 잡고 그 안에서만 늘린다.
#define EECONFIG_USER_DATA_SIZE     512

#define DYNAMIC_KEYMAP_LAYER_COUNT  8

// 매트릭스 (DTS kscan/keys 노드의 row/col 개수와 반드시 일치: wish60 = 5 x 15)
#define MATRIX_ROWS                 5
#define MATRIX_COLS                 15

#define DEBOUNCE                    10

#define GRAVE_ESC_ENABLE
