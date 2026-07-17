#include "quantum.h"
#include "led.h"

/*
 * 인디케이터 LED — wish60 은 보드에 LED 1개(P0.31, DTS: led). 그걸 Caps Lock 에 쓴다.
 *
 * QMK 가 배선을 다 해준다: keyboard_task() -> led_task() -> host_keyboard_leds() 로 호스트가
 * 보낸 LED 리포트를 읽고, 바뀌면 led_update_kb() 를 부른다.
 * host_keyboard_leds() 는 활성 host_driver 를 타므로 **USB/BLE 어느 쪽이든 그대로 동작**한다
 * (driver_usb.c / driver_ble.c 의 keyboard_leds).
 *
 * QMK 순정 led_update_ports() 는 LED_CAPS_LOCK_PIN + QMK gpio API 를 전제한다. 우리 LED 는
 * Zephyr gpio_dt_spec(hw/driver/led.c)이라 led_update_kb() 를 오버라이드해 우리 API 로 켠다.
 *
 * 보드마다 인디케이터 구성이 다르므로 키보드 트리에 둔다(via_port.c / keycode_port.c 와 같은 이유).
 *
 * [전력] 켜져 있으면 ≈1.2mA 다(실측). Caps Lock 은 평소 꺼져 있어 무선에서도 문제되지 않지만,
 * "항상 켜두는 인디케이터"를 추가할 때는 idle 57µA 대비 20배임을 기억할 것.
 */
bool led_update_kb(led_t led_state)
{
  bool res = led_update_user(led_state);

  if (res)
  {
    if (led_state.caps_lock)
    {
      ledOn(_DEF_LED1);
    }
    else
    {
      ledOff(_DEF_LED1);
    }
  }
  return res;
}
