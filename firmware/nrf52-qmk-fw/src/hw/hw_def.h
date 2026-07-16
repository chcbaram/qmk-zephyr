#ifndef HW_DEF_H_
#define HW_DEF_H_


#include "bsp.h"


#define _DEF_FIRMWATRE_VERSION      "V250929R1"
#define _DEF_BOARD_NAME             "QMK-ZEPHYR"



#define _HW_DEF_RTOS_THREAD_PRI_CLI           5
#define _HW_DEF_RTOS_THREAD_PRI_UART          5

#define _HW_DEF_RTOS_THREAD_MEM_CLI           (6*1024)
#define _HW_DEF_RTOS_THREAD_MEM_UART          (2*1024)



#define _USE_HW_QSPI
// 키 매트릭스 스캔은 Zephyr 네이티브 gpio-kbd-matrix(DTS: kbd_matrix, 저전력 인터럽트 스캔)가
// 담당하고 port/matrix.c 가 input 이벤트로 소비한다. 구 폴링 드라이버(driver/keys.c)는 퇴역.
// #define _USE_HW_KEYS
#define      HW_KEYS_PRESS_MAX     6      // QMK keyboard report 키 개수(6KRO boot)

#define _USE_HW_LED
#define      HW_LED_MAX_CH          1

// 배터리 잔량. 백엔드(VDDH ADC / MAX17048)는 DTS 가 정한다 — driver/battery.c
#define _USE_HW_BATTERY

// [저전력] 디버그 콘솔(UART/CLI/로그).
//
// _USE_HW_DEBUG_CONSOLE 은 여기서 정의하지 않는다 — CMake 의 -DDEBUG_CONSOLE=y 가
// 이 매크로와 debug.conf(CONFIG_SERIAL 등)를 함께 켠다. 여기서 손으로 켜면 C 코드만
// 살아나고 Kconfig 는 안 따라와서 반쪽이 된다(반대도 마찬가지).
//
// 콘솔은 배터리 실사용 전류를 지배한다(실측, USB 미연결·BLE 연결 idle):
//   콘솔 있음 1.13mA  vs  콘솔 없음 80.9µA  — 14배.
//   nRF52840 UARTE 는 RX 활성 상태에서 HFCLK 를 계속 잡는다.
// 런타임 게이팅(PM suspend)도 시도했으나 오히려 악화됐다(1.21→1.98mA). 빌드타임 제거가 답.

#ifdef _USE_HW_DEBUG_CONSOLE

#define _USE_HW_UART
#define      HW_UART_MAX_CH         1
#define      HW_UART_CH_SWD         _DEF_UART1
#define      HW_UART_CH_CLI         HW_UART_CH_SWD

#define _USE_HW_CLI
#define      HW_CLI_CMD_LIST_MAX    32
#define      HW_CLI_CMD_NAME_MAX    16
#define      HW_CLI_LINE_HIS_MAX    8
#define      HW_CLI_LINE_BUF_MAX    64

#define _USE_HW_CLI_GUI
#define      HW_CLI_GUI_WIDTH       80
#define      HW_CLI_GUI_HEIGHT      24

#define _USE_HW_LOG
#define      HW_LOG_CH              HW_UART_CH_SWD
#define      HW_LOG_BOOT_BUF_MAX    4096
#define      HW_LOG_LIST_BUF_MAX    4096

#endif




//-- CLI
//
#define _USE_CLI_HW_UART            1
#define _USE_CLI_HW_BUTTON          1
#define _USE_CLI_HW_LOG             1
#define _USE_CLI_HW_KEYS            1
#define _USE_CLI_HW_MODULE          1
#define _USE_CLI_HW_BATTERY         1
#define _USE_CLI_HW_ACTIVITY        1
#define _USE_CLI_HW_BLE             1


#endif
