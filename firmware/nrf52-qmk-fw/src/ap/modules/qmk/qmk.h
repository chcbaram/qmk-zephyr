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
// timeout_ms == 0 이면 무한 대기. activity 상태머신이 데드라인을 넘겨준다.
void qmkWaitActivity(uint32_t timeout_ms);

// 마지막 키 입력 이후 경과 시간(ms). activity 상태머신이 idle/sleep 판정에 쓴다.
uint32_t qmkGetInactiveMs(void);

/*
 * 활성 구간(키 눌림)에서 qmkUpdate() 를 부를 주기(ms). 키보드 config.h 에서 재정의 가능.
 *
 * 매트릭스 재스캔 주기(board DTS 의 kbd_matrix poll-period-ms)와는 **별개 노브**다:
 *   poll-period-ms    = 하드웨어 재스캔 주기 (키 변화 감지 지연)
 *   QMK_TASK_PERIOD_MS = QMK 처리 주기        (디바운스/태핑 granularity)
 * 같은 값일 필요가 없다. 둘 다 CPU 를 깨우므로 전력엔 둘 다 영향을 준다.
 *
 * 실측(배터리, 키 누른 채, 둘을 같이 바꿨을 때):
 *   1ms = 3.40mA / 2ms = 2.43mA / 4ms = 1.40mA  (§6.4)
 * QMK 디바운스/태핑은 **시간 기반**이라 주기를 늘려도 로직은 정상이다(granularity 만 거칠어짐).
 */
#ifndef QMK_TASK_PERIOD_MS
#define QMK_TASK_PERIOD_MS   2
#endif

#ifdef __cplusplus
}
#endif

#endif
