#ifndef HW_DEF_H_
#define HW_DEF_H_


#include "bsp.h"


#define _DEF_FIRMWATRE_VERSION      "V250929R1"
#define _DEF_BOARD_NAME             "NRF52-QMK-FW"



#define _HW_DEF_RTOS_THREAD_PRI_CLI           5
#define _HW_DEF_RTOS_THREAD_PRI_UART          5

#define _HW_DEF_RTOS_THREAD_MEM_CLI           (6*1024)
#define _HW_DEF_RTOS_THREAD_MEM_UART          (2*1024)



#define _USE_HW_QSPI


#define _USE_HW_LED
#define      HW_LED_MAX_CH          1

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



//-- CLI
//
#define _USE_CLI_HW_UART            1
#define _USE_CLI_HW_BUTTON          1
#define _USE_CLI_HW_LOG             1


#endif
