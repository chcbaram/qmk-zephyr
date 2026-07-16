#include "ble.h"
#include "hw_def.h"
#include "log.h"   // logPrintf (콘솔 비활성 빌드에선 no-op)

#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <bluetooth/services/hids.h>

/*
 * BLE HID (HOG) — NCS BT_HIDS 사용. (ZMK 는 GATT 를 직접 짜지만 NCS 는 기성 서비스 제공)
 *
 * 리포트 맵: 키보드(ID1)+LED out, System(ID3), Consumer(ID4).
 * USB 쪽(usb_hid.c 의 exk 디스크립터)과 Report ID 규약을 맞춰 QMK 코드가 동일하게 동작한다.
 * HOG 에서는 각 리포트가 별도 characteristic 이라 payload 에 Report ID 를 넣지 않는다
 * (ID 는 Report Reference 디스크립터가 가짐) → usage 2바이트만 전송.
 */

#define BASE_USB_HID_SPEC_VERSION   0x0101

#define BLE_KBD_REPORT_LEN          8   // mods + reserved + keys[6]
#define BLE_EXTRA_REPORT_LEN        2   // usage16
#define BLE_LED_REPORT_LEN          1

#define BLE_REP_ID_KEYS             1
#define BLE_REP_ID_SYSTEM           3
#define BLE_REP_ID_CONSUMER         4

enum
{
  BLE_INP_KEYS_IDX = 0,
  BLE_INP_SYSTEM_IDX,
  BLE_INP_CONSUMER_IDX,
};

enum
{
  BLE_OUTP_LED_IDX = 0,
};

BT_HIDS_DEF(hids_obj,
            BLE_LED_REPORT_LEN,
            BLE_KBD_REPORT_LEN,
            BLE_EXTRA_REPORT_LEN,
            BLE_EXTRA_REPORT_LEN);

static struct bt_conn *cur_conn;
static uint8_t         led_state;
static bool            is_init = false;

static const struct bt_data ad[] = {
  BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE,
                (CONFIG_BT_DEVICE_APPEARANCE >> 0) & 0xff,
                (CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),
  BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
  BT_DATA_BYTES(BT_DATA_UUID16_ALL,
                BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
                BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static const struct bt_data sd[] = {
  BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

// clang-format off
static const uint8_t report_map[] = {
  /* Keyboard (Report ID 1) */
  0x05, 0x01,       /* Usage Page (Generic Desktop) */
  0x09, 0x06,       /* Usage (Keyboard) */
  0xA1, 0x01,       /* Collection (Application) */
  0x85, BLE_REP_ID_KEYS,
  0x05, 0x07,       /*   Usage Page (Key Codes) */
  0x19, 0xE0, 0x29, 0xE7,
  0x15, 0x00, 0x25, 0x01,
  0x75, 0x01, 0x95, 0x08,
  0x81, 0x02,       /*   Input (Data,Var,Abs) : modifiers */
  0x95, 0x01, 0x75, 0x08,
  0x81, 0x01,       /*   Input (Const)        : reserved */
  0x95, 0x06, 0x75, 0x08,
  0x15, 0x00, 0x25, 0x65,
  0x05, 0x07, 0x19, 0x00, 0x29, 0x65,
  0x81, 0x00,       /*   Input (Data,Ary)     : keys[6] */
  /* LED output (Report ID 1) */
  0x95, 0x05, 0x75, 0x01,
  0x05, 0x08, 0x19, 0x01, 0x29, 0x05,
  0x91, 0x02,       /*   Output (Data,Var,Abs): LEDs */
  0x95, 0x01, 0x75, 0x03,
  0x91, 0x01,       /*   Output (Const)       : padding */
  0xC0,

  /* System control (Report ID 3) */
  0x05, 0x01,       /* Usage Page (Generic Desktop) */
  0x09, 0x80,       /* Usage (System Control) */
  0xA1, 0x01,
  0x85, BLE_REP_ID_SYSTEM,
  0x19, 0x01, 0x2A, 0xB7, 0x00,
  0x15, 0x01, 0x26, 0xB7, 0x00,
  0x95, 0x01, 0x75, 0x10,
  0x81, 0x00,
  0xC0,

  /* Consumer control (Report ID 4) */
  0x05, 0x0C,       /* Usage Page (Consumer) */
  0x09, 0x01,       /* Usage (Consumer Control) */
  0xA1, 0x01,
  0x85, BLE_REP_ID_CONSUMER,
  0x19, 0x01, 0x2A, 0xA0, 0x02,
  0x15, 0x01, 0x26, 0xA0, 0x02,
  0x95, 0x01, 0x75, 0x10,
  0x81, 0x00,
  0xC0,
};
// clang-format on

// 호스트 → 디바이스 LED 리포트(CapsLock 등)
static void led_outp_rep_handler(struct bt_hids_rep *rep, struct bt_conn *conn, bool write)
{
  if (!write || rep->size < 1)
  {
    return;
  }
  led_state = rep->data[0];
}

static void boot_kb_outp_rep_handler(struct bt_hids_rep *rep, struct bt_conn *conn, bool write)
{
  led_outp_rep_handler(rep, conn, write);
}

static void advertising_start(void)
{
  int err;

  err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
  if (err == -EALREADY)
  {
    return;
  }
  if (err)
  {
    logPrintf("[E_] ble adv start (%d)\n", err);
    return;
  }
  logPrintf("[  ] ble advertising\n");
}

static void connected(struct bt_conn *conn, uint8_t err)
{
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
  if (err)
  {
    logPrintf("[E_] ble connect %s (%u)\n", addr, err);
    return;
  }

  logPrintf("[OK] ble connected %s\n", addr);
  cur_conn = bt_conn_ref(conn);

  if (bt_hids_connected(&hids_obj, conn))
  {
    logPrintf("[E_] bt_hids_connected\n");
  }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
  logPrintf("[  ] ble disconnected (0x%02x)\n", reason);

  if (bt_hids_disconnected(&hids_obj, conn))
  {
    logPrintf("[E_] bt_hids_disconnected\n");
  }

  if (cur_conn)
  {
    bt_conn_unref(cur_conn);
    cur_conn = NULL;
  }
  advertising_start();
}

// 실제로 협상된 연결 파라미터. 호스트가 우리 요청(PPCP)을 거부/변경할 수 있으므로 확인용.
// interval 단위 1.25ms, timeout 단위 10ms. latency 가 0 이면 매 연결 이벤트마다 라디오가 깨어난다.
static void le_param_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency, uint16_t timeout)
{
  logPrintf("[  ] ble conn param: interval %d.%02dms, latency %d, timeout %dms\n",
            (interval * 125) / 100, (interval * 125) % 100, latency, timeout * 10);
  logPrintf("     radio wakeup ~%dms\n", ((interval * 125) / 100) * (latency + 1));
}

static bool le_param_req(struct bt_conn *conn, struct bt_le_conn_param *param)
{
  logPrintf("[  ] ble param req: int %d~%d, lat %d, to %d\n",
            param->interval_min, param->interval_max, param->latency, param->timeout);
  return true;
}

static void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
  if (err)
  {
    logPrintf("[E_] ble security %s level %u err %d\n", addr, level, err);
    return;
  }
  logPrintf("[OK] ble security level %u\n", level);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
  .connected         = connected,
  .disconnected      = disconnected,
  .security_changed  = security_changed,
  .le_param_req      = le_param_req,
  .le_param_updated  = le_param_updated,
};

static void hid_init(void)
{
  int                           err;
  struct bt_hids_init_param     init_param = {0};
  struct bt_hids_inp_rep       *inp;
  struct bt_hids_outp_feat_rep *outp;

  init_param.rep_map.data = report_map;
  init_param.rep_map.size = sizeof(report_map);

  init_param.info.bcd_hid        = BASE_USB_HID_SPEC_VERSION;
  init_param.info.b_country_code = 0x00;
  init_param.info.flags          = (BT_HIDS_REMOTE_WAKE | BT_HIDS_NORMALLY_CONNECTABLE);

  inp        = &init_param.inp_rep_group_init.reports[BLE_INP_KEYS_IDX];
  inp->size  = BLE_KBD_REPORT_LEN;
  inp->id    = BLE_REP_ID_KEYS;
  init_param.inp_rep_group_init.cnt++;

  inp        = &init_param.inp_rep_group_init.reports[BLE_INP_SYSTEM_IDX];
  inp->size  = BLE_EXTRA_REPORT_LEN;
  inp->id    = BLE_REP_ID_SYSTEM;
  init_param.inp_rep_group_init.cnt++;

  inp        = &init_param.inp_rep_group_init.reports[BLE_INP_CONSUMER_IDX];
  inp->size  = BLE_EXTRA_REPORT_LEN;
  inp->id    = BLE_REP_ID_CONSUMER;
  init_param.inp_rep_group_init.cnt++;

  outp          = &init_param.outp_rep_group_init.reports[BLE_OUTP_LED_IDX];
  outp->size    = BLE_LED_REPORT_LEN;
  outp->id      = BLE_REP_ID_KEYS;
  outp->handler = led_outp_rep_handler;
  init_param.outp_rep_group_init.cnt++;

  init_param.is_kb                     = true;
  init_param.boot_kb_outp_rep_handler  = boot_kb_outp_rep_handler;

  err = bt_hids_init(&hids_obj, &init_param);
  if (err)
  {
    logPrintf("[E_] bt_hids_init (%d)\n", err);
  }
}

bool bleInit(void)
{
  int err;

  hid_init();

  err = bt_enable(NULL);
  if (err)
  {
    logPrintf("[E_] bt_enable (%d)\n", err);
    return false;
  }

  // 본딩 정보 로드 (BT_SETTINGS, storage_partition/NVS)
  if (IS_ENABLED(CONFIG_SETTINGS))
  {
    settings_load();
  }

  advertising_start();

  is_init = true;
  logPrintf("[OK] bleInit()\n");
  return true;
}

bool bleIsConnected(void)
{
  return (is_init && cur_conn != NULL);
}

bool bleSendKeyboard(report_keyboard_t *report)
{
  if (!bleIsConnected())
  {
    return false;
  }
  // report_keyboard_t = mods + reserved + keys[6] (Report ID 없음: HOG 는 characteristic 로 구분)
  return bt_hids_inp_rep_send(&hids_obj, cur_conn, BLE_INP_KEYS_IDX,
                              (uint8_t *)report, BLE_KBD_REPORT_LEN, NULL) == 0;
}

bool bleSendExtra(report_extra_t *report)
{
  uint8_t idx;

  if (!bleIsConnected())
  {
    return false;
  }

  idx = (report->report_id == REPORT_ID_SYSTEM) ? BLE_INP_SYSTEM_IDX : BLE_INP_CONSUMER_IDX;

  // payload 는 usage 16bit 만 (Report ID 제외)
  return bt_hids_inp_rep_send(&hids_obj, cur_conn, idx,
                              (uint8_t *)&report->usage, BLE_EXTRA_REPORT_LEN, NULL) == 0;
}

uint8_t bleGetKbdLeds(void)
{
  return led_state;
}
