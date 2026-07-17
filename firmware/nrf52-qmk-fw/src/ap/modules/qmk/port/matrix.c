#include "matrix.h"
#include "debounce.h"
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>

/*
 * QMK 매트릭스 어댑터 — Zephyr 네이티브 gpio-kbd-matrix(input) 이벤트 소비형.
 *
 * 드라이버가 저전력 스캔(idle 시 인터럽트 대기 → CPU sleep, 키 눌림에 wakeup)을 담당하고,
 * 키 변화를 input 이벤트로 알려준다. 여기서는 그 이벤트로 raw 매트릭스를 갱신하고,
 * matrix_scan() 은 QMK 디바운스만 적용한다. (드라이버 디바운스는 DTS 에서 0)
 *
 * 인덱스 매핑 주의 (DTS 주석 참고):
 *   Zephyr 의 "col"(구동 출력, INPUT_ABS_X) = wish60 row  → QMK row
 *   Zephyr 의 "row"(입력 라인,  INPUT_ABS_Y) = wish60 col  → QMK col
 */

/*
 * 저전력 idle 판정용.
 *  - 키가 하나도 안 눌렸고, 마지막 입력 이벤트 후 GRACE 가 지나면 "완전 idle" 로 보고
 *    메인 루프(ap.c)가 세마포어에서 블록 → CPU sleep. 드라이버도 인터럽트 대기로 들어간다.
 *  - GRACE 는 QMK 디바운스(sym_defer_pk, DEBOUNCE=5ms)가 정착할 시간을 준다.
 *
 * 한계(의도적): 키가 안 눌린 상태에서 도는 QMK 타이머(tap dance, one-shot 만료 등)는
 * 잠든 동안 진행되지 않고 다음 키 입력에서 재개된다. 현재 키맵(MO 레이어)엔 해당 없음.
 */
#define MATRIX_IDLE_GRACE_MS   20

static K_SEM_DEFINE(kbd_activity_sem, 0, 1);
static volatile uint32_t     last_activity_ms;

static volatile matrix_row_t raw_matrix[MATRIX_ROWS];   // input 콜백이 갱신
static matrix_row_t          matrix[MATRIX_ROWS];       // 디바운스 결과

// input 콜백은 kbd-matrix 드라이버 스캔 스레드 컨텍스트에서 불린다(동기 모드).
// raw_matrix 는 이 콜백만 쓰고 matrix_scan() 은 읽기만 하므로 lost update 는 없다.
static void kbd_matrix_input_cb(struct input_event *evt, void *user_data)
{
  ARG_UNUSED(user_data);

  static uint8_t qmk_row;   // INPUT_ABS_X (드라이버 col = wish60/QMK row)
  static uint8_t qmk_col;   // INPUT_ABS_Y (드라이버 row = wish60/QMK col)

  switch (evt->code)
  {
    case INPUT_ABS_X:
      qmk_row = (uint8_t)evt->value;
      break;

    case INPUT_ABS_Y:
      qmk_col = (uint8_t)evt->value;
      break;

    case INPUT_BTN_TOUCH:
      if (qmk_row < MATRIX_ROWS && qmk_col < MATRIX_COLS)
      {
        if (evt->value)
        {
          raw_matrix[qmk_row] |= ((matrix_row_t)1 << qmk_col);
        }
        else
        {
          raw_matrix[qmk_row] &= ~((matrix_row_t)1 << qmk_col);
        }
      }
      // 메인 루프 깨우기 (잠들어 있었다면). 세마포어가 이미 차 있어도 무해.
      last_activity_ms = k_uptime_get_32();
      k_sem_give(&kbd_activity_sem);
      break;

    default:
      break;
  }
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(kbd_matrix)), kbd_matrix_input_cb, NULL);

/*
 * [빌드타임 안전장치] row-gpios 의 모든 핀이 해당 GPIO 컨트롤러의 sense-edge-mask 에
 * 들어있는지 검사한다. 하나라도 빠지면 **컴파일 에러**.
 *
 * 왜 필요한가: Zephyr gpio-kbd-matrix 는 row 인터럽트를 엣지로 걸고, nRF 에서 엣지는
 * 기본이 GPIOTE IN 채널이다(gpio_nrfx.c). 그러면 두 가지가 조용히 깨진다 —
 *   1) GPIOTE 채널은 8개뿐. 우리 row 는 15개 → 9번째부터 -ENOMEM 이고 드라이버는
 *      첫 실패에서 return 해버려 뒤쪽 row 는 인터럽트가 아예 없다.
 *   2) GPIOTE 는 System OFF 에서 꺼진다 → deep sleep 에서 못 깨어난다.
 * 둘 다 런타임에 조용히 나타나므로(로그 한 줄 + 절반만 동작) 여기서 잡는다.
 *
 * 핀을 바꾸면 board DTS 의 sense-edge-mask 도 같이 고쳐야 한다 — 안 고치면 여기서 막힌다.
 * (ZMK kscan 은 LEVEL 인터럽트를 써서 이 분기를 안 타므로 마스크가 필요 없다.
 *  네이티브 드라이버를 택한 대가이고, 그 대가를 빌드타임으로 옮긴 것이다.)
 */
#define ROW_SENSE_MASK_CHECK(node_id, prop, idx)                                            \
  BUILD_ASSERT((BIT(DT_GPIO_PIN_BY_IDX(node_id, prop, idx)) &                               \
                DT_PROP_OR(DT_GPIO_CTLR_BY_IDX(node_id, prop, idx), sense_edge_mask, 0)),   \
               "kbd_matrix row-gpios pin missing from its GPIO controller's "            \
               "sense-edge-mask -- update &gpio0/&gpio1 sense-edge-mask in the board DTS. " \
               "(without it: GPIOTE runs out of channels and System OFF cannot wake)");

DT_FOREACH_PROP_ELEM(DT_NODELABEL(kbd_matrix), row_gpios, ROW_SENSE_MASK_CHECK)

void matrix_init(void)
{
  memset((void *)raw_matrix, 0, sizeof(raw_matrix));
  memset(matrix, 0, sizeof(matrix));

  debounce_init(MATRIX_ROWS);
}

void matrix_print(void)
{
}

bool matrix_can_read(void)
{
  return true;
}

matrix_row_t matrix_get_row(uint8_t row)
{
  return matrix[row];
}

uint8_t matrix_scan(void)
{
  matrix_row_t curr_matrix[MATRIX_ROWS];
  static matrix_row_t prev_raw[MATRIX_ROWS];

  // 드라이버가 갱신해 둔 raw 스냅샷 (스캔은 드라이버가 이미 수행)
  for (uint8_t row = 0; row < MATRIX_ROWS; row++)
  {
    curr_matrix[row] = raw_matrix[row];
  }

  bool changed = memcmp(prev_raw, curr_matrix, sizeof(curr_matrix)) != 0;
  if (changed)
  {
    memcpy(prev_raw, curr_matrix, sizeof(curr_matrix));
  }

  changed = debounce(curr_matrix, matrix, MATRIX_ROWS, changed);

  return (uint8_t)changed;
}

// ---- 저전력: 메인 루프(ap.c)용 idle 판정/대기 ------------------------------

// 눌린 키가 없고 마지막 입력 후 GRACE 가 지났으면 true (잠들어도 되는 상태).
bool qmkIsIdle(void)
{
  for (uint8_t row = 0; row < MATRIX_ROWS; row++)
  {
    if (raw_matrix[row] != 0)
    {
      return false;   // 키가 눌려있음 → 스캔/타이머 계속 돌아야 함
    }
  }
  return (k_uptime_get_32() - last_activity_ms) >= MATRIX_IDLE_GRACE_MS;
}

// 키 입력이 있을 때까지 블록. 이 동안 CPU 는 잠들고, kbd-matrix 드라이버는
// 전 컬럼 구동 + row 인터럽트 대기 상태로 들어간다(눌림 시 콜백이 세마포어를 준다).
// timeout_ms 는 activity 상태머신의 다음 데드라인(idle/sleep 전이) — 0 이면 무한.
void qmkWaitActivity(uint32_t timeout_ms)
{
  k_sem_take(&kbd_activity_sem, timeout_ms == 0 ? K_FOREVER : K_MSEC(timeout_ms));
}

uint32_t qmkGetInactiveMs(void)
{
  return k_uptime_get_32() - last_activity_ms;
}

void qmkWake(void)
{
  // 키 입력과 같은 경로로 깨운다. last_activity_ms 는 건드리지 않는다 —
  // 이건 "사용자 입력"이 아니므로 idle/sleep 타이머를 리셋하면 안 된다.
  k_sem_give(&kbd_activity_sem);
}
