#ifndef HW_DEF_H_
#define HW_DEF_H_


#include "bsp.h"
#include <zephyr/devicetree.h>


// 버전의 단일 출처는 **앱 Kconfig 의 KBD_FW_VERSION** 이다(레포 루트의 Kconfig).
// 여기 문자열을 박으면 안 된다 — BLE DIS 의 Firmware Revision 은 Kconfig 로만 설정되고,
// Kconfig 는 C 매크로를 못 읽는다. 그래서 방향이 이쪽뿐이다(C 가 Kconfig 를 읽는다).
#define _DEF_FIRMWATRE_VERSION      CONFIG_KBD_FW_VERSION
#define _DEF_BOARD_NAME             "QMK-ZEPHYR"



#define _HW_DEF_RTOS_THREAD_PRI_CLI           5
#define _HW_DEF_RTOS_THREAD_PRI_UART          5

#define _HW_DEF_RTOS_THREAD_MEM_CLI           (6*1024)
#define _HW_DEF_RTOS_THREAD_MEM_UART          (2*1024)

// USB HID 전송 스레드 — hid_device_submit_report() 가 **호스트를 기다리며 블록**하기 때문에
// 필요하다(자세한 이유는 driver/usb/usb_hid/usb_hid.c 의 usb_tx_thread 주석).
// 하는 일이 msgq 대기 + submit 뿐이라 스택은 작아도 된다.
#define _HW_DEF_RTOS_THREAD_PRI_USB_TX        5
#define _HW_DEF_RTOS_THREAD_MEM_USB_TX        (1024)



#define _USE_HW_QSPI
// 키 매트릭스 스캔은 Zephyr 네이티브 gpio-kbd-matrix(DTS: kbd_matrix, 저전력 인터럽트 스캔)가
// 담당하고 port/matrix.c 가 input 이벤트로 소비한다. 구 폴링 드라이버(driver/keys.c)는 퇴역.
// #define _USE_HW_KEYS
#define      HW_KEYS_PRESS_MAX     6      // QMK keyboard report 키 개수(6KRO boot)

/*
 * [보드별 기능은 DTS 가 정한다]
 *
 * 이 파일은 **모든 보드가 공유**한다. 여기서 기능을 무조건 켜면, 그 부품이 없는 보드는
 * DEVICE_DT_GET(DT_NODELABEL(...)) 이 없는 노드로 확장돼 **빌드가 깨진다**. 쓰지도 않는
 * 더미 노드를 DTS 에 넣어 회피하게 되는데 그건 거짓말이다.
 *
 * 그래서 "보드에 그 노드가 있으면 켠다". battery.c 가 백엔드(VDDH/MAX17048)를 DTS 로 고르는
 * 것과 같은 방식이다. 새 보드는 **이 파일을 건드리지 않고** DTS 만 쓰면 된다.
 */

// 디버그용 LED 만(하트비트 등). 키보드 인디케이터(Caps)는 별개 부품이라 여기 없다 —
// DTS 의 caps_led 노드 + keyboards/<kbd>/port/led_port.c 참고.
#if DT_NODE_EXISTS(DT_NODELABEL(led))
#define _USE_HW_LED
#define      HW_LED_MAX_CH          1
#endif

// 배터리 잔량. 백엔드(VDDH ADC / MAX17048)는 DTS 가 정한다 — driver/battery.c
#define _USE_HW_BATTERY

// 외부 전원 레일(네오픽셀 전용). DTS: ext_power (regulator-fixed)
#if DT_NODE_EXISTS(DT_NODELABEL(ext_power))
#define _USE_HW_EXT_POWER
#endif

// 네오픽셀 언더글로우. DTS: led_strip (worldsemi,ws2812-spi)
//
// LED 개수는 **DTS 의 chain-length 에서 읽는다** — 예전엔 여기 16 이 박혀 있어서 DTS/config.h
// 와 3중 중복이었고 강제하는 장치가 없었다(개수 다른 보드를 만들면 조용히 16개만 켜진다).
// 이제 진실은 DTS 한 곳이고, QMK 쪽 RGB_MATRIX_LED_COUNT 와의 일치는 BUILD_ASSERT 가 본다
// (keyboards/<kbd>/driver/rgb_matrix_drivers.c).
#if DT_NODE_EXISTS(DT_NODELABEL(led_strip))
#define _USE_HW_WS2812
#define      HW_WS2812_MAX_CH       DT_PROP(DT_NODELABEL(led_strip), chain_length)
#endif

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
#define _USE_CLI_HW_WS2812          1


#endif
