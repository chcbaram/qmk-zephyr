#ifndef QMK_H_
#define QMK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "quantum.h"

bool qmkInit(void);
void qmkUpdate(void);

// 저전력: 눌린 키가 없고 디바운스가 정착했으면 true → 메인 루프가 잠들어도 된다.
bool qmkIsIdle(void);
// 키 입력이 있을 때까지 블록(그 동안 CPU sleep). 입력 이벤트가 깨운다.
void qmkWaitActivity(void);

#ifdef __cplusplus
}
#endif

#endif
