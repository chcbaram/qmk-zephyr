#include "quantum.h"
#include "ble.h"

/*
 * 보드별 커스텀 키코드 — BLE 프로파일 전환.
 *
 * VIA 정의 JSON 의 "customKeycodes" 배열이 QK_KB_0(0x7E00) 부터 순서대로 매핑된다.
 * **배열 순서와 아래 enum 순서가 반드시 일치**해야 한다(VIA 는 인덱스로만 지정한다).
 *
 * process_record_kb() 는 quantum.c 의 weak 함수라 여기서 오버라이드한다(QMK 무수정 원칙).
 * 보드마다 노출 키코드가 다르므로 키보드 트리에 둔다(via_port.c 와 같은 이유).
 */
enum
{
  KC_BT_SEL_0 = QK_KB_0,   // customKeycodes[0]
  KC_BT_SEL_1,
  KC_BT_SEL_2,
  KC_BT_SEL_3,
  KC_BT_SEL_4,
  KC_BT_NEXT,
  KC_BT_PREV,
  KC_BT_CLR,               // 현재 프로파일의 본딩 삭제
};

bool process_record_kb(uint16_t keycode, keyrecord_t *record)
{
  // 누를 때만 처리(뗄 때 중복 실행 방지)
  if (!record->event.pressed)
  {
    return process_record_user(keycode, record);
  }

  switch (keycode)
  {
    case KC_BT_SEL_0 ... KC_BT_SEL_4:
      bleProfileSelect(keycode - KC_BT_SEL_0);
      return false;

    case KC_BT_NEXT:
      bleProfileNext();
      return false;

    case KC_BT_PREV:
      bleProfilePrev();
      return false;

    case KC_BT_CLR:
      bleProfileClear(BLE_PROFILE_COUNT);   // COUNT = 활성 프로파일
      return false;

    default:
      break;
  }

  return process_record_user(keycode, record);
}
