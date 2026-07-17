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

/*
 * idle 일 때 얼마나 잘지(ms). 0 = 키 입력까지 무한 대기.
 *
 * 평소엔 activity 상태머신의 다음 데드라인이지만, **RGB 애니메이션이 켜져 있으면 프레임
 * 주기로 깨어나야 한다** — 안 그러면 무한 대기에 걸려 첫 프레임만 그려지고 멈춘다.
 * (§2.6 의 "QMK 폴링 vs ZMK 이벤트" 가 RGB 에서 다시 나타나는 지점)
 */
uint32_t qmkGetIdleWaitMs(void);

// RGB/인디케이터 소등 조건(USB 서스펜드 | activity IDLE)을 합쳐 반영한다. 전이할 때만 동작.
void qmkSuspendUpdate(void);

/*
 * 지금 소등 상태인가 (USB 서스펜드 | activity IDLE).
 *
 * RGB 드라이버가 "레일을 내려도 되나"를 판정할 때 **rgb_matrix_get_suspend_state() 를 쓰면
 * 안 된다** — rgb_matrix_set_suspend_state() 는 검은 프레임을 flush 한 **뒤에** 내부 플래그를
 * 세우므로, 정작 그 flush 안에서는 아직 false 다. 우리 플래그는 suspend_power_down_quantum()
 * 을 부르기 전에 세워지므로 flush 시점에 이미 참이다.
 */
bool qmkIsSuspended(void);

/*
 * deep sleep 진입 직전 **무조건** 소등한다(타임아웃 설정 무관).
 * System OFF 는 GPIO 출력을 유지하므로 레일이 켜진 채 잠들면 네오픽셀이 계속 켜져 있다.
 * port/activity.c 의 activityEnterSleep() 에서만 부른다.
 */
void qmkSuspendForSleep(void);

/*
 * 잠들어 있는 메인 루프를 깨운다.
 *
 * 루프는 idle 이면 qmkWaitActivity() 에서 블록한다. **키 입력 말고도 루프를 다시 돌려야 하는
 * 경로**가 있으면 여기로 깨워야 한다 — 예: VIA 가 RGB 를 켜면 rgb_matrix_task() 가 돌아야
 * 불이 들어온다. 안 깨우면 사용자가 키를 누를 때까지 아무 일도 안 일어난다(실제로 겪음).
 */
void qmkWake(void);

#ifdef __cplusplus
}
#endif

#endif
