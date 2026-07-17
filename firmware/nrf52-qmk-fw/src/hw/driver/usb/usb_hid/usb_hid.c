
#include "usb_hid.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/bos.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>

#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_hid.h>

LOG_MODULE_REGISTER(usb_hid);


#define HID_KBD_NODE   DT_NODELABEL(usb_hid_kbd)
#define HID_VIA_NODE   DT_NODELABEL(usb_hid_via)
#define HID_EXK_NODE   DT_NODELABEL(usb_hid_exk)

static const struct device *hid_kbd_dev = DEVICE_DT_GET(HID_KBD_NODE);
static const struct device *hid_via_dev = DEVICE_DT_GET(HID_VIA_NODE);
static const struct device *hid_exk_dev = DEVICE_DT_GET(HID_EXK_NODE);


static const uint8_t hid_report_kbd_desc[] =
{
  HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
  HID_USAGE(HID_USAGE_GEN_DESKTOP_KEYBOARD),
  HID_COLLECTION(HID_COLLECTION_APPLICATION),
  HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP_KEYPAD), /* HID_USAGE_MINIMUM(Keyboard LeftControl) */
  HID_USAGE_MIN8(0xE0),                         /* HID_USAGE_MAXIMUM(Keyboard Right GUI) */
  HID_USAGE_MAX8(0xE7),
  HID_LOGICAL_MIN8(0),
  HID_LOGICAL_MAX8(1),
  HID_REPORT_SIZE(1),
  HID_REPORT_COUNT(8),                          /* HID_INPUT(Data,Var,Abs) */
  HID_INPUT(0x02),
  HID_REPORT_SIZE(8),
  HID_REPORT_COUNT(1),                          /* HID_INPUT(Cnst,Var,Abs) */
  HID_INPUT(0x03),
  HID_REPORT_SIZE(1),
  HID_REPORT_COUNT(5),
  HID_USAGE_PAGE(HID_USAGE_GEN_LEDS),           /* HID_USAGE_MINIMUM(Num Lock) */
  HID_USAGE_MIN8(1),                            /* HID_USAGE_MAXIMUM(Kana) */
  HID_USAGE_MAX8(5),                            /* HID_OUTPUT(Data,Var,Abs) */
  HID_OUTPUT(0x02),
  HID_REPORT_SIZE(3),
  HID_REPORT_COUNT(1),                          /* HID_OUTPUT(Cnst,Var,Abs) */
  HID_OUTPUT(0x03),
  HID_REPORT_SIZE(8),
  HID_REPORT_COUNT(6),
  HID_LOGICAL_MIN8(0),
  HID_LOGICAL_MAX8(101),
  HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP_KEYPAD), /* HID_USAGE_MIN8(Reserved) */
  HID_USAGE_MIN8(0),                            /* HID_USAGE_MAX8(Keyboard Application) */
  HID_USAGE_MAX8(101),                          /* HID_INPUT (Data,Ary,Abs) */
  HID_INPUT(0x00),
  HID_END_COLLECTION,
};

static uint8_t hid_report_via_desc[] = 
{
  //
  0x06, 0x60, 0xFF, // Usage Page (Vendor Defined)
  0x09, 0x61,       // Usage (Vendor Defined)
  0xA1, 0x01,       // Collection (Application)
  // Data to host
  0x09, 0x62,       //   Usage (Vendor Defined)
  0x15, 0x00,       //   Logical Minimum (0)
  0x26, 0xFF, 0x00, //   Logical Maximum (255)
  0x95, 32,         //   Report Count
  0x75, 0x08,       //   Report Size (8)
  0x81, 0x02,       //   Input (Data, Variable, Absolute)
  // Data from host
  0x09, 0x63,       //   Usage (Vendor Defined)
  0x15, 0x00,       //   Logical Minimum (0)
  0x26, 0xFF, 0x00, //   Logical Maximum (255)
  0x95, 32,         //   Report Count
  0x75, 0x08,       //   Report Size (8)
  0x91, 0x02,       //   Output (Data, Variable, Absolute)
  0xC0              // End Collection
};

static uint8_t hid_report_exk_desc[] =
{
  //
  0x05, 0x01,               // Usage Page (Generic Desktop)
  0x09, 0x80,               // Usage (System Control)
  0xA1, 0x01,               // Collection (Application)
  0x85, 3,                  //   Report ID
  0x19, 0x01,               //   Usage Minimum (Pointer)
  0x2A, 0xB7, 0x00,         //   Usage Maximum (System Display LCD Autoscale)
  0x15, 0x01,               //   Logical Minimum
  0x26, 0xB7, 0x00,         //   Logical Maximum
  0x95, 0x01,               //   Report Count (1)
  0x75, 0x10,               //   Report Size (16)
  0x81, 0x00,               //   Input (Data, Array, Absolute)
  0xC0,                     // End Collection

  0x05, 0x0C,               // Usage Page (Consumer)
  0x09, 0x01,               // Usage (Consumer Control)
  0xA1, 0x01,               // Collection (Application)
  0x85, 4,                  //   Report ID
  0x19, 0x01,               //   Usage Minimum (Consumer Control)
  0x2A, 0xA0, 0x02,         //   Usage Maximum (AC Desktop Show All Applications)
  0x15, 0x01,               //   Logical Minimum
  0x26, 0xA0, 0x02,         //   Logical Maximum
  0x95, 0x01,               //   Report Count (1)
  0x75, 0x10,               //   Report Size (16)
  0x81, 0x00,               //   Input (Data, Array, Absolute)
  0xC0                      // End Collection
};


enum kb_leds_idx
{
  KB_LED_NUMLOCK = 0,
	KB_LED_CAPSLOCK,
	KB_LED_SCROLLLOCK,
	KB_LED_COUNT,
};

enum kb_report_idx {
	KB_MOD_KEY = 0,
	KB_RESERVED,
	KB_KEY_CODE1,
	KB_KEY_CODE2,
	KB_KEY_CODE3,
	KB_KEY_CODE4,
	KB_KEY_CODE5,
	KB_KEY_CODE6,
	KB_REPORT_COUNT,
};

struct kb_event {
	uint16_t code;
	int32_t value;
};

K_MSGQ_DEFINE(kb_msgq, sizeof(struct kb_event), 2, 1);

UDC_STATIC_BUF_DEFINE(report, KB_REPORT_COUNT);
static uint32_t kb_duration;
static bool     kb_ready;
static bool     via_ready;
static uint8_t  kb_led_state;

// VIA raw HID 수신 콜백(호스트→디바이스 OUT 리포트). port/via_hid.c 가 등록.
static void (*via_receive_cb)(uint8_t *data, uint8_t length);

// hid_device_submit_report() 는 report 버퍼가 정렬돼야 하고, input_report_done
// 콜백이 없으면 동기(전송 완료까지 블록)로 처리된다. QMK 리포트를 정렬 버퍼로
// 복사해 전송한다. (동기 전송이라 단일 정적 버퍼로 충분)
// [소유권] 이 버퍼들은 **usb_tx_thread 만** 만진다. Zephyr 가 전송 중 이 메모리를 참조로
// 읽으므로(hid_buf_alloc_ext) 다른 스레드가 덮어쓰면 DMA 중인 데이터가 깨진다.
// __aligned(4): hid_dev_submit_report() 가 IS_ALIGNED(report, sizeof(void*)) 를 assert 한다.
static uint8_t __aligned(4) kbd_tx_buf[KB_REPORT_COUNT];
static uint8_t __aligned(4) exk_tx_buf[8];
static uint8_t __aligned(4) via_tx_buf[32];

/*
 * USB HID 전송 스레드.
 *
 * [왜 스레드가 필요한가] Zephyr 의 hid_device_submit_report() 는 input_report_done 콜백을
 * 등록하지 않으면 **호스트가 리포트를 가져갈 때까지 K_FOREVER 로 블록**한다:
 *
 *     // zephyr/subsys/usb/device_next/class/usbd_hid.c
 *     if (ops->input_report_done == NULL) {
 *         k_sem_take(&ddata->in_sem, K_FOREVER);
 *     }
 *
 * 이걸 QMK 메인 루프에서 부르면 **키보드 전체가 호스트를 기다리며 선다** — 매트릭스 처리,
 * 디바운스 카운트다운, 다음 키 감지가 전부 밀린다. 평상시엔 폴링 1ms 라 짧지만 호스트가
 * 늦으면 그만큼 통째로 멈춘다("가끔 멈칫" 의 정체. BLE 는 bt_gatt_notify 가 논블로킹이라
 * 같은 증상이 없었고, 그 비대칭이 단서였다).
 *
 * [왜 input_report_done 콜백 대신 스레드인가] 콜백을 달면 비동기가 되지만 **버퍼 수명을
 * 우리가 책임져야 한다** — Zephyr 는 위 버퍼를 참조로 큐잉하므로 전송이 끝나기 전에
 * 덮어쓰면 깨진다. msgq 는 값을 복사하므로 그 문제가 아예 없다. Zephyr 가 기본을 동기로
 * 둔 이유도 이 버퍼 수명이다.
 *
 * 호스트가 멈춰도 **이 스레드만 막히고 키보드는 계속 돈다.** 큐가 차면 버린다 — 호스트가
 * 안 듣는데 쌓아둘 이유가 없고, 쌓다가 메인 루프를 막으면 원래 문제로 되돌아간다.
 */
enum
{
  USB_TX_KBD = 0,
  USB_TX_EXK,
  USB_TX_VIA,
};

struct usb_tx_item
{
  uint8_t dev;
  uint8_t len;
  uint8_t data[32];   // VIA(32B)가 최대. kbd/exk 는 이보다 작다
};

K_MSGQ_DEFINE(usb_tx_q, sizeof(struct usb_tx_item), 8, 4);

static void usbTxThread(void *p1, void *p2, void *p3)
{
  struct usb_tx_item item;

  while (1)
  {
    k_msgq_get(&usb_tx_q, &item, K_FOREVER);

    switch (item.dev)
    {
      case USB_TX_KBD:
        memcpy(kbd_tx_buf, item.data, item.len);
        hid_device_submit_report(hid_kbd_dev, item.len, kbd_tx_buf);   // 여기서 블록돼도 된다
        break;

      case USB_TX_EXK:
        memcpy(exk_tx_buf, item.data, item.len);
        hid_device_submit_report(hid_exk_dev, item.len, exk_tx_buf);
        break;

      case USB_TX_VIA:
        memcpy(via_tx_buf, item.data, item.len);
        hid_device_submit_report(hid_via_dev, item.len, via_tx_buf);
        break;
    }
  }
}

K_THREAD_DEFINE(usb_tx_thread,
                _HW_DEF_RTOS_THREAD_MEM_USB_TX,
                usbTxThread, NULL, NULL, NULL,
                _HW_DEF_RTOS_THREAD_PRI_USB_TX, 0, 0);

// 큐에 넣고 **즉시 반환**한다. 실제 전송(블로킹)은 usb_tx_thread 가 한다.
static bool usb_tx_put(uint8_t dev, const uint8_t *data, uint16_t length)
{
  struct usb_tx_item item;

  if (length > sizeof(item.data))
  {
    length = sizeof(item.data);
  }
  item.dev = dev;
  item.len = (uint8_t)length;
  memcpy(item.data, data, length);

  // K_NO_WAIT: 큐가 차면 버린다. 여기서 기다리면 메인 루프가 막혀 원래 문제로 돌아간다.
  return k_msgq_put(&usb_tx_q, &item, K_NO_WAIT) == 0;
}


// static void input_cb(struct input_event *evt, void *user_data)
// {
//   struct kb_event kb_evt;

//   ARG_UNUSED(user_data);

//   kb_evt.code  = evt->code;
//   kb_evt.value = evt->value;
//   if (k_msgq_put(&kb_msgq, &kb_evt, K_NO_WAIT) != 0)
//   {
//     LOG_ERR("Failed to put new input event");
//   }
// }

// INPUT_CALLBACK_DEFINE(NULL, input_cb, NULL);

static void kb_iface_ready(const struct device *dev, const bool ready)
{
  LOG_INF("HID device %s interface is %s", dev->name, ready ? "ready" : "not ready");
  kb_ready = ready;

  if (!ready)
  {
    /*
     * USB 가 내려갔다 — 큐에 남은 리포트를 **버린다**.
     *
     * 갈 곳이 없어진 리포트다. 남겨두면 재연결 시 그대로 나가 **유령 키 입력**이 된다
     * (예: [press A] 가 남은 채 재연결 -> 호스트가 A 를 눌린 것으로 본다).
     *
     * 실제로는 Zephyr 가 클래스 disabled 상태에서 submit 을 -EACCES 로 즉시 거절하므로
     * TX 스레드가 알아서 비운다. 하지만 그건 **우연한 안전**이지 우리가 표현한 의도가
     * 아니다 — 여기서 명시한다.
     *
     * 새 리포트는 위 kb_ready=false 로 애초에 안 쌓인다(usbHidSendReport 가 먼저 본다).
     */
    k_msgq_purge(&usb_tx_q);
  }
}

static int kb_get_report(const struct device *dev,
                         const uint8_t type, const uint8_t id, const uint16_t len,
                         uint8_t *const buf)
{
  LOG_WRN("Get Report not implemented, Type %u ID %u", type, id);
  return 0;
}

static void (*p_kbd_led_func)(void) = NULL;

void usbHidSetKbdLedFunc(void (*func)(void))
{
  p_kbd_led_func = func;
}

static int kb_set_report(const struct device *dev,
                         const uint8_t type, const uint8_t id, const uint16_t len,
                         const uint8_t *const buf)
{
  if (type != HID_REPORT_TYPE_OUTPUT)
  {
    LOG_WRN("Unsupported report type");
    return -ENOTSUP;
  }

  // boot keyboard output report = 1바이트 LED 비트맵 (NumLock/CapsLock/ScrollLock)
  if (len >= 1)
  {
    bool changed = (kb_led_state != buf[0]);

    kb_led_state = buf[0];

    // led_task() 가 폴링으로 집어가는데, 루프가 자고 있으면 반영이 안 된다 → 깨운다.
    if (changed && p_kbd_led_func != NULL)
    {
      p_kbd_led_func();
    }
  }
  return 0;
}

static void kb_set_idle(const struct device *dev, const uint8_t id, const uint32_t duration)
{
  LOG_INF("Set Idle %u to %u", id, duration);
  kb_duration = duration;
}

static uint32_t kb_get_idle(const struct device *dev, const uint8_t id)
{
  LOG_INF("Get Idle %u to %u", id, kb_duration);
  return kb_duration;
}

static void kb_set_protocol(const struct device *dev, const uint8_t proto)
{
  LOG_INF("Protocol changed to %s",
          proto == 0U ? "Boot Protocol" : "Report Protocol");
}

static void kb_output_report(const struct device *dev, const uint16_t len, const uint8_t *const buf)
{
  kb_set_report(dev, HID_REPORT_TYPE_OUTPUT, 0U, len, buf);
}

static void via_iface_ready(const struct device *dev, const bool ready)
{
  LOG_INF("VIA device %s interface is %s", dev->name, ready ? "ready" : "not ready");
  via_ready = ready;
}

static int via_get_report(const struct device *dev,
                         const uint8_t type, const uint8_t id, const uint16_t len,
                         uint8_t *const buf)
{
  LOG_WRN("VIA Get Report not implemented, Type %u ID %u", type, id);
  return 0;
}

// VIA OUT 리포트 처리 큐. USB 수신 콜백(via_output_report) 안에서 응답(raw_hid_send →
// hid_device_submit_report, 동기)을 바로 보내면 USB 스택 컨텍스트에서 재진입/블록되어
// VIA 가 "Loading" 에서 멈춘다. 그래서 OUT 리포트를 큐에 넣고 전용 스레드가 처리한다.
K_MSGQ_DEFINE(via_rx_msgq, 32, 8, 4);

// 호스트→디바이스 32바이트 OUT 리포트를 큐에 적재(콜백 컨텍스트에서 즉시 반환).
static void via_deliver(const uint8_t *const buf, const uint16_t len)
{
  uint8_t  msg[32];
  uint16_t n = (len < sizeof(msg)) ? len : sizeof(msg);

  memset(msg, 0, sizeof(msg));
  memcpy(msg, buf, n);
  k_msgq_put(&via_rx_msgq, msg, K_NO_WAIT);
}

// VIA 처리 스레드: 큐에서 OUT 리포트를 꺼내 raw_hid_receive 처리(응답 전송 포함)를
// USB 콜백 밖에서 수행한다.
static void via_process_thread(void *p1, void *p2, void *p3)
{
  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  uint8_t buf[32];

  for (;;)
  {
    k_msgq_get(&via_rx_msgq, buf, K_FOREVER);
    if (via_receive_cb != NULL)
    {
      via_receive_cb(buf, sizeof(buf));
    }
  }
}

K_THREAD_DEFINE(via_tid, 2048, via_process_thread, NULL, NULL, NULL, 6, 0, 0);

static int via_set_report(const struct device *dev,
                         const uint8_t type, const uint8_t id, const uint16_t len,
                         const uint8_t *const buf)
{
  if (type != HID_REPORT_TYPE_OUTPUT)
  {
    LOG_WRN("Unsupported report type");
    return -ENOTSUP;
  }
  via_deliver(buf, len);   // control endpoint SET_REPORT 경로
  return 0;
}

static void via_output_report(const struct device *dev, const uint16_t len, const uint8_t *const buf)
{
  via_deliver(buf, len);   // interrupt OUT endpoint 경로(VIA 기본)
}

struct hid_device_ops kbd_ops = 
{
  .iface_ready   = kb_iface_ready,
  .get_report    = kb_get_report,
  .set_report    = kb_set_report,
  .set_idle      = kb_set_idle,
  .get_idle      = kb_get_idle,
  .set_protocol  = kb_set_protocol,
  .output_report = kb_output_report,
};

struct hid_device_ops via_ops = 
{
  .iface_ready   = via_iface_ready,
  .get_report    = via_get_report,
  .set_report    = via_set_report,
  // .set_idle      = kb_set_idle,
  // .get_idle      = kb_get_idle,
  // .set_protocol  = kb_set_protocol,
  .output_report = via_output_report,
};

// QMK host driver(port/driver_usb.c) 가 호출하는 전송 API.
// 키보드 리포트(boot 8바이트: mods+reserved+keys[6])를 kbd HID IN 으로 전송.
bool usbHidSendReport(uint8_t *data, uint16_t length)
{
  if (!kb_ready)
  {
    return false;
  }
  if (length > sizeof(kbd_tx_buf))
  {
    length = sizeof(kbd_tx_buf);
  }
  return usb_tx_put(USB_TX_KBD, data, length);
}

// System/Consumer control 리포트(report_extra_t: report_id + usage16)를 exk HID IN 으로 전송.
bool usbHidSendReportEXK(uint8_t *data, uint16_t length)
{
  if (!kb_ready)
  {
    return false;
  }
  if (length > sizeof(exk_tx_buf))
  {
    length = sizeof(exk_tx_buf);
  }
  return usb_tx_put(USB_TX_EXK, data, length);
}

uint8_t usbHidGetKbdLeds(void)
{
  return kb_led_state;
}

bool usbHidIsReady(void)
{
  return kb_ready;
}

// VIA 수신 콜백 등록 (port/via_hid.c 의 via_hid_receive).
void usbHidSetViaReceiveFunc(void (*func)(uint8_t *data, uint8_t length))
{
  via_receive_cb = func;
}

// VIA 응답(디바이스→호스트 32바이트 IN 리포트) 전송. QMK raw_hid_send → 이 함수.
bool usbHidSendReportVia(uint8_t *data, uint16_t length)
{
  if (!via_ready)
  {
    return false;
  }
  if (length > sizeof(via_tx_buf))
  {
    length = sizeof(via_tx_buf);
  }
  return usb_tx_put(USB_TX_VIA, data, length);
}

bool usbHidInit(void)
{
	int ret;


  if (!device_is_ready(hid_kbd_dev))
  {
    LOG_ERR("USB KBD Device is not ready");
    return -EIO;
  }
  if (!device_is_ready(hid_via_dev))
  {
    LOG_ERR("USB VIA Device is not ready");
    return -EIO;
  }
  if (!device_is_ready(hid_exk_dev))
  {
    LOG_ERR("USB EXK Device is not ready");
    return -EIO;
  }

  ret = hid_device_register(hid_kbd_dev,
                            hid_report_kbd_desc, sizeof(hid_report_kbd_desc),
                            &kbd_ops);
  if (ret != 0)
  {
    LOG_ERR("Failed to register hid_kbd_dev, %d", ret);
    return ret;
  }

  ret = hid_device_register(hid_via_dev,
                            hid_report_via_desc, sizeof(hid_report_via_desc),
                            &via_ops);
  if (ret != 0)
  {
    LOG_ERR("Failed to register hid_kbd_dev, %d", ret);
    return ret;
  }

  ret = hid_device_register(hid_exk_dev,
                            hid_report_exk_desc, sizeof(hid_report_exk_desc),
                            &kbd_ops);
  if (ret != 0)
  {
    LOG_ERR("Failed to register hid_kbd_dev, %d", ret);
    return ret;
  }  
  return true;
}