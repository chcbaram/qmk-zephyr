#pragma once

#include <stdbool.h>

/*
 * 저전력 정책. 현재는 디버그 인프라(콘솔 UART) 게이팅만 담당한다.
 * (activity 상태머신 / deep sleep 은 이후 확장)
 */

// USB 전원 유무에 따라 콘솔 UART 를 resume/suspend 한다. 주기적으로 호출(qmkUpdate).
void powerConsoleUpdate(void);

bool powerIsConsoleOn(void);
