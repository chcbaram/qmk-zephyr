#include "ble.h"
#include "hw_def.h"
#include "log.h"   // logPrintf (콘솔 비활성 빌드에선 no-op)

#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/hci_vs.h>
#include <zephyr/sys/byteorder.h>   // sys_cpu_to_le16
#include <bluetooth/services/hids.h>
#include <zephyr/bluetooth/services/bas.h>
#include "battery.h"
#include "qmk/qmk.h"
#include "cli.h"

#if CLI_USE(HW_BLE)
static void cliBle(cli_args_t *args);
#endif

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

static uint8_t         led_state;
static bool            is_init = false;

/*
 * 프로파일 (ZMK app/src/ble.c 패턴).
 *
 * peer 가 BT_ADDR_LE_ANY = "비어있음"(새 호스트를 받을 수 있음).
 * ZMK 와 동일하게 **광고는 항상 열린 광고**를 쓴다. directed advertising 은 프라이버시를
 * 쓰는 호스트(주소가 계속 바뀜)에서 깨지기 때문에 ZMK 도 주석 처리해 뒀다.
 * 대신 "어느 호스트가 붙어 있느냐"가 아니라 **어느 conn 으로 리포트를 보내느냐**로 전환한다
 * → 5대가 동시에 붙어 있어도 활성 프로파일만 키를 받는다(전환 시 재연결 대기 없음).
 */
typedef struct
{
  bt_addr_le_t peer;
} ble_profile_t;

static ble_profile_t profiles[BLE_PROFILE_COUNT];
static uint8_t       active_profile = 0;

/*
 * 재연결 루프 감지 — 로그 스팸 방지.
 *
 * 호스트에만 본딩이 남으면 그 호스트가 "연결 -> 암호화 실패 -> 0x13 로 끊김"을 무한 반복한다
 * (호스트 본딩은 우리가 못 지운다). 그대로 두면 로그가 끝없이 찍혀 콘솔을 못 쓴다.
 * 같은 주소가 짧은 간격으로 반복 해제되면 **로그를 끄고 카운트만 센다** — 상태는 `ble info` 로 본다.
 * 로그만 억제한다. 연결 동작 자체는 건드리지 않는다(정상 재연결을 막으면 안 되므로).
 */
#define BLE_LOOP_WINDOW_MS    3000   // 이 안에 다시 끊기면 "반복"으로 본다
#define BLE_LOOP_MUTE_AFTER   3      // 연속 3회부터 로그 억제

static bt_addr_le_t loop_addr;
static uint32_t     loop_cnt;
static uint32_t     loop_last_ms;

static bool ble_loop_muted(void)
{
  return loop_cnt >= BLE_LOOP_MUTE_AFTER;
}

// 암호화까지 성공했으면 루프가 아니다.
static void ble_loop_reset(void)
{
  loop_cnt = 0;
  bt_addr_le_copy(&loop_addr, BT_ADDR_LE_ANY);
}

// disconnected() 에서 호출.
static void ble_loop_note(const bt_addr_le_t *addr)
{
  uint32_t now = k_uptime_get_32();

  if (bt_addr_le_cmp(addr, &loop_addr) == 0 && (now - loop_last_ms) < BLE_LOOP_WINDOW_MS)
  {
    loop_cnt++;
  }
  else
  {
    bt_addr_le_copy(&loop_addr, addr);
    loop_cnt = 1;
  }
  loop_last_ms = now;
}

static void ble_advertising_update(void);

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
  BT_DATA(BT_DATA_NAME_COMPLETE, KBD_NAME, sizeof(KBD_NAME) - 1),
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

/*
 * 배터리 서비스(BAS) 갱신 — 60초 주기(ZMK 의 BATTERY_REPORT_INTERVAL 과 동일).
 *
 * 주기를 더 짧게 잡지 않는 이유: 샘플링은 SAADC(또는 I2C)를 깨워 HFCLK 를 잡는다.
 * idle 80.9µA 를 유지하려면 깨우는 횟수 자체가 비용이다. 배터리 잔량은 분 단위로 변하므로
 * 60초면 충분하다. 연결이 없으면 아예 돌리지 않는다(광고 중엔 읽어줄 상대도 없다).
 */
#define BLE_BAS_INTERVAL_MS   (60 * 1000)

static void bas_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(bas_work, bas_work_handler);

static void bas_work_handler(struct k_work *work)
{
  uint8_t pct;

  if (!bleIsConnected())
  {
    return;   // 재연결 시 connected() 가 다시 스케줄한다
  }

  if (batteryUpdate() && batteryGetPercent(&pct))
  {
    bt_bas_set_battery_level(pct);
  }

  k_work_schedule(&bas_work, K_MSEC(BLE_BAS_INTERVAL_MS));
}

// 호스트 → 디바이스 LED 리포트(CapsLock 등)
static void led_outp_rep_handler(struct bt_hids_rep *rep, struct bt_conn *conn, bool write)
{
  if (!write || rep->size < 1)
  {
    return;
  }

  if (led_state != rep->data[0])
  {
    led_state = rep->data[0];
    qmkWake();   // led_task() 가 폴링한다 — 루프가 자고 있으면 반영이 안 된다
  }
}

static void boot_kb_outp_rep_handler(struct bt_hids_rep *rep, struct bt_conn *conn, bool write)
{
  led_outp_rep_handler(rep, conn, write);
}

/*
 * settings(NVS) 저장. 키 이름은 ZMK 와 같은 규약을 쓴다.
 *   ble/profiles/<n> : 해당 프로파일의 peer 주소
 *   ble/active       : 활성 프로파일 인덱스
 */
static void ble_profile_save(uint8_t index)
{
  char key[32];

  snprintf(key, sizeof(key), "ble/profiles/%d", index);
  settings_save_one(key, &profiles[index].peer, sizeof(bt_addr_le_t));
}

static void ble_active_save(void)
{
  settings_save_one("ble/active", &active_profile, sizeof(active_profile));
}

// settings_load() 가 부팅 시 저장된 값을 여기로 되돌려준다.
static int ble_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
  const char *next;

  if (settings_name_steq(name, "active", &next) && !next)
  {
    uint8_t idx;

    if (read_cb(cb_arg, &idx, sizeof(idx)) > 0 && idx < BLE_PROFILE_COUNT)
    {
      active_profile = idx;
    }
    return 0;
  }

  if (settings_name_steq(name, "profiles", &next) && next)
  {
    int idx = atoi(next);

    if (idx >= 0 && idx < BLE_PROFILE_COUNT)
    {
      if (read_cb(cb_arg, &profiles[idx].peer, sizeof(bt_addr_le_t)) <= 0)
      {
        bt_addr_le_copy(&profiles[idx].peer, BT_ADDR_LE_ANY);
      }
    }
    return 0;
  }

  return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(ble_port, "ble", NULL, ble_settings_set, NULL, NULL);

static void ble_profile_set_addr(uint8_t index, const bt_addr_le_t *addr)
{
  char str[BT_ADDR_LE_STR_LEN];

  bt_addr_le_copy(&profiles[index].peer, addr);
  ble_profile_save(index);

  bt_addr_le_to_str(addr, str, sizeof(str));
  logPrintf("[  ] ble profile %d -> %s\n", index, str);
}

bool bleProfileIsOpen(uint8_t index)
{
  if (index >= BLE_PROFILE_COUNT)
  {
    return false;
  }
  return bt_addr_le_cmp(&profiles[index].peer, BT_ADDR_LE_ANY) == 0;
}

// 해당 프로파일의 peer 로 맺어진 연결을 찾는다. 없으면 NULL.
// 반환된 conn 은 bt_conn_lookup_addr_le() 가 ref 를 올리므로 **호출자가 unref 해야 한다**.
static struct bt_conn *ble_profile_conn(uint8_t index)
{
  if (index >= BLE_PROFILE_COUNT || bleProfileIsOpen(index))
  {
    return NULL;
  }
  return bt_conn_lookup_addr_le(BT_ID_DEFAULT, &profiles[index].peer);
}

bool bleProfileIsConnected(uint8_t index)
{
  struct bt_conn *conn = ble_profile_conn(index);

  if (conn == NULL)
  {
    return false;
  }
  bt_conn_unref(conn);
  return true;
}

uint8_t bleProfileGetActive(void)
{
  return active_profile;
}

/*
 * 광고는 활성 프로파일 기준으로만 켠다(ZMK update_advertising 과 동일한 판단):
 *   - 활성 프로파일이 비어있다            -> 광고(새 호스트 페어링을 받는다)
 *   - 활성 프로파일이 있는데 연결 안 됨   -> 광고(그 호스트가 돌아오길 기다린다)
 *   - 활성 프로파일이 연결됨              -> 광고 중지 (라디오/전력 절약)
 * 비활성 프로파일의 호스트는 자기가 알아서 재연결한다(본딩돼 있으므로).
 */
static bool adv_running = false;

static void ble_advertising_update(void)
{
  bool want_adv;
  int  err;

  if (!is_init)
  {
    return;
  }

  want_adv = bleProfileIsOpen(active_profile) || !bleProfileIsConnected(active_profile);

  if (want_adv == adv_running)
  {
    return;
  }

  if (want_adv)
  {
    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err && err != -EALREADY)
    {
      logPrintf("[E_] ble adv start (%d)\n", err);
      return;
    }
    if (!ble_loop_muted())
    {
      logPrintf("[  ] ble advertising (profile %d, %s)\n",
                active_profile, bleProfileIsOpen(active_profile) ? "open" : "reconnect");
    }
  }
  else
  {
    err = bt_le_adv_stop();
    if (err && err != -EALREADY)
    {
      logPrintf("[E_] ble adv stop (%d)\n", err);
      return;
    }
    logPrintf("[  ] ble adv stop (profile %d connected)\n", active_profile);
  }

  adv_running = want_adv;
}

/*
 * TX power — Zephyr 표준 VS HCI 로 설정한다(§ble.h 주석 참고).
 * 핸들 종류마다 따로 걸어야 한다: 광고(ADV)와 **연결(CONN)은 별개**다.
 * 그래서 새 연결이 생길 때마다 connected() 에서 다시 적용한다.
 */
static int8_t tx_power_dbm = 0;

static int ble_tx_power_apply(uint8_t handle_type, uint16_t handle, int8_t dbm)
{
  struct bt_hci_cp_vs_write_tx_power_level *cp;
  struct bt_hci_rp_vs_write_tx_power_level *rp;
  struct net_buf                           *buf;
  struct net_buf                           *rsp = NULL;
  int                                       err;

  buf = bt_hci_cmd_alloc(K_FOREVER);
  if (buf == NULL)
  {
    return -ENOBUFS;
  }

  cp                 = net_buf_add(buf, sizeof(*cp));
  cp->handle         = sys_cpu_to_le16(handle);
  cp->handle_type    = handle_type;
  cp->tx_power_level = dbm;

  err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL, buf, &rsp);
  if (err)
  {
    return err;
  }

  // 컨트롤러가 실제로 고른 값. 요청값이 미지원이면 가까운 값으로 클램프된다.
  rp = (void *)rsp->data;
  logPrintf("[  ] ble tx power: 요청 %ddBm -> 적용 %ddBm (type %d)\n",
            dbm, rp->selected_tx_power, handle_type);
  net_buf_unref(rsp);

  return 0;
}

// 새 연결에 현재 TX power 를 건다(연결 핸들은 광고와 별개라 매번 필요).
static void ble_tx_power_apply_conn(struct bt_conn *conn)
{
  uint16_t handle;

  if (tx_power_dbm == 0)
  {
    return;   // 0dBm = 컨트롤러 기본값 → 굳이 명령을 보내지 않는다
  }
  if (bt_hci_get_conn_handle(conn, &handle) == 0)
  {
    ble_tx_power_apply(BT_HCI_VS_LL_HANDLE_TYPE_CONN, handle, tx_power_dbm);
  }
}

bool bleSetTxPower(int8_t dbm)
{
  tx_power_dbm = dbm;

  // 광고에 적용. 연결에는 각 conn 마다 따로 걸어야 한다.
  if (ble_tx_power_apply(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0, dbm) != 0)
  {
    return false;
  }

  for (int i = 0; i < BLE_PROFILE_COUNT; i++)
  {
    struct bt_conn *conn = ble_profile_conn(i);

    if (conn != NULL)
    {
      ble_tx_power_apply_conn(conn);
      bt_conn_unref(conn);
    }
  }
  return true;
}

int8_t bleGetTxPower(void)
{
  return tx_power_dbm;
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

  if (!ble_loop_muted())
  {
    logPrintf("[OK] ble connected %s\n", addr);
  }

  if (bt_hids_connected(&hids_obj, conn))
  {
    logPrintf("[E_] bt_hids_connected\n");
  }

  ble_tx_power_apply_conn(conn);   // 연결 핸들은 광고와 별개다

  // 여러 호스트가 동시에 붙을 수 있다. conn 을 우리가 붙들지 않고(ref 안 함) 필요할 때
  // 활성 프로파일 주소로 조회한다 -> 프로파일 전환이 곧 전송 대상 전환이 된다.
  adv_running = false;   // 연결되면 컨트롤러가 광고를 멈춘다
  ble_advertising_update();

  k_work_schedule(&bas_work, K_NO_WAIT);   // 연결 직후 1회 + 이후 60초 주기
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
  ble_loop_note(bt_conn_get_dst(conn));
  if (!ble_loop_muted())
  {
    logPrintf("[  ] ble disconnected (0x%02x)\n", reason);
  }

  if (bt_hids_disconnected(&hids_obj, conn))
  {
    logPrintf("[E_] bt_hids_disconnected\n");
  }

  if (!bleIsConnected())
  {
    k_work_cancel_delayable(&bas_work);
  }
  ble_advertising_update();
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
  ble_loop_reset();   // 암호화까지 성공 = 루프가 아니다
}

/*
 * 페어링 — 프로파일이 peer 주소를 학습하는 지점.
 *
 * 활성 프로파일이 이미 다른 호스트에게 잡혀 있으면 **페어링을 거부**한다. 안 그러면
 * 새 호스트가 기존 프로파일을 덮어써서 사용자가 슬롯을 잃는다(ZMK 와 동일한 방어).
 * 슬롯을 비우려면 사용자가 명시적으로 clear 해야 한다.
 */
static bool ble_pairing_allowed(struct bt_conn *conn)
{
  return bleProfileIsOpen(active_profile) ||
         bt_addr_le_cmp(&profiles[active_profile].peer, bt_conn_get_dst(conn)) == 0;
}

static enum bt_security_err auth_pairing_accept(struct bt_conn *conn,
                                                const struct bt_conn_pairing_feat *const feat)
{
  if (!ble_pairing_allowed(conn))
  {
    logPrintf("[E_] ble pairing rejected: profile %d 사용중\n", active_profile);
    return BT_SECURITY_ERR_PAIR_NOT_ALLOWED;
  }
  return BT_SECURITY_ERR_SUCCESS;
}

static void auth_pairing_complete(struct bt_conn *conn, bool bonded)
{
  if (!ble_pairing_allowed(conn))
  {
    // 여기까지 왔으면 accept 를 통과했는데 그 사이 프로파일이 바뀐 것 → 본딩을 되돌린다.
    logPrintf("[E_] ble pairing done but profile %d taken -> unpair\n", active_profile);
    bt_unpair(BT_ID_DEFAULT, bt_conn_get_dst(conn));
    return;
  }

  ble_profile_set_addr(active_profile, bt_conn_get_dst(conn));
  ble_advertising_update();
}

static void auth_cancel(struct bt_conn *conn)
{
  logPrintf("[  ] ble pairing cancelled\n");
}

static struct bt_conn_auth_cb auth_cb = {
  .pairing_accept = auth_pairing_accept,
  .cancel         = auth_cancel,
};

static struct bt_conn_auth_info_cb auth_info_cb = {
  .pairing_complete = auth_pairing_complete,
};

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

  for (int i = 0; i < BLE_PROFILE_COUNT; i++)
  {
    bt_addr_le_copy(&profiles[i].peer, BT_ADDR_LE_ANY);
  }

  bt_conn_auth_cb_register(&auth_cb);
  bt_conn_auth_info_cb_register(&auth_info_cb);

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

  /*
   * BLE 이름 = 키보드 config.h 의 KBD_NAME (USB 제품명과 같은 출처).
   *
   * prj.conf 는 **모든 보드가 공유**하므로 거기 CONFIG_BT_DEVICE_NAME 으로 키보드 이름을
   * 박으면 새 키보드가 남의 이름으로 광고된다(실제로 전부 "WISH60" 이었다). 광고 데이터만
   * 고치는 것으로는 부족하다 — GATT 의 Device Name 특성이 여전히 옛 이름을 반환해 거짓말이
   * 된다. bt_set_name() 이 둘 다 맞춘다.
   *
   * settings_load() **뒤**여야 한다. BT_SETTINGS 는 이름도 "bt/name" 으로 저장/복원하므로,
   * 앞에서 부르면 옛 이름이 우리 값을 덮는다. (값이 같으면 flash 쓰기도 없다)
   */
  err = bt_set_name(KBD_NAME);
  if (err)
  {
    logPrintf("[E_] bt_set_name %s (%d)\n", KBD_NAME, err);
  }

  is_init = true;
  ble_advertising_update();

#if CLI_USE(HW_BLE)
  cliAdd("ble", cliBle);
#endif

  logPrintf("[OK] bleInit() profile %d/%d %s\n", active_profile, BLE_PROFILE_COUNT,
            bleProfileIsOpen(active_profile) ? "(open)" : "(bonded)");
  return true;
}

bool bleIsConnected(void)
{
  return is_init && bleProfileIsConnected(active_profile);
}

// 리포트는 **활성 프로파일의 연결로만** 나간다. 다른 호스트가 붙어 있어도 받지 못한다.
static bool ble_send(uint8_t rep_idx, const uint8_t *data, uint8_t len)
{
  struct bt_conn *conn;
  int             err;

  if (!is_init)
  {
    return false;
  }

  conn = ble_profile_conn(active_profile);
  if (conn == NULL)
  {
    return false;
  }

  err = bt_hids_inp_rep_send(&hids_obj, conn, rep_idx, (uint8_t *)data, len, NULL);
  bt_conn_unref(conn);   // ble_profile_conn() 이 올린 ref

  return err == 0;
}

bool bleSendKeyboard(report_keyboard_t *report)
{
  // report_keyboard_t = mods + reserved + keys[6] (Report ID 없음: HOG 는 characteristic 로 구분)
  return ble_send(BLE_INP_KEYS_IDX, (const uint8_t *)report, BLE_KBD_REPORT_LEN);
}

bool bleSendExtra(report_extra_t *report)
{
  uint8_t idx = (report->report_id == REPORT_ID_SYSTEM) ? BLE_INP_SYSTEM_IDX : BLE_INP_CONSUMER_IDX;

  // payload 는 usage 16bit 만 (Report ID 제외)
  return ble_send(idx, (const uint8_t *)&report->usage, BLE_EXTRA_REPORT_LEN);
}

uint8_t bleGetKbdLeds(void)
{
  return led_state;
}


// ---- 프로파일 전환 --------------------------------------------------------------

bool bleProfileSelect(uint8_t index)
{
  if (index >= BLE_PROFILE_COUNT || index == active_profile)
  {
    return false;
  }

  active_profile = index;
  ble_active_save();

  logPrintf("[  ] ble profile %d %s%s\n", index,
            bleProfileIsOpen(index) ? "(open)" : "(bonded)",
            bleProfileIsConnected(index) ? " connected" : "");

  // 전송 대상이 바뀌므로 이전 호스트에 눌린 키가 남지 않도록 빈 리포트를 보낸다.
  // (host_set_driver 전환 때와 같은 stuck key 방어)
  ble_advertising_update();
  return true;
}

bool bleProfileNext(void)
{
  return bleProfileSelect((active_profile + 1) % BLE_PROFILE_COUNT);
}

bool bleProfilePrev(void)
{
  return bleProfileSelect((active_profile + BLE_PROFILE_COUNT - 1) % BLE_PROFILE_COUNT);
}

bool bleProfileClear(uint8_t index)
{
  if (index >= BLE_PROFILE_COUNT || bleProfileIsOpen(index))
  {
    return false;   // 범위 밖이거나 이미 비어있음
  }

  // 연결돼 있으면 먼저 끊는다. 안 그러면 본딩만 지워진 채 연결이 남아
  // 호스트는 붙어 있다고 믿고 우리는 암호화 키가 없는 어긋난 상태가 된다.
  struct bt_conn *conn = ble_profile_conn(index);
  if (conn != NULL)
  {
    bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    bt_conn_unref(conn);
  }

  bt_unpair(BT_ID_DEFAULT, &profiles[index].peer);
  ble_profile_set_addr(index, BT_ADDR_LE_ANY);
  ble_advertising_update();

  logPrintf("[  ] ble profile %d cleared\n", index);
  return true;
}

bool bleProfileClearActive(void)
{
  return bleProfileClear(active_profile);
}

void bleProfileClearAll(void)
{
  // 연결부터 끊는다(본딩만 지우면 호스트는 붙어 있다고 믿는 어긋난 상태가 된다).
  for (int i = 0; i < BLE_PROFILE_COUNT; i++)
  {
    struct bt_conn *conn = ble_profile_conn(i);

    if (conn != NULL)
    {
      bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
      bt_conn_unref(conn);
    }
    bt_addr_le_copy(&profiles[i].peer, BT_ADDR_LE_ANY);
    ble_profile_save(i);
  }

  // **스택의 본딩을 통째로** 지운다(NULL = 전체). profiles[] 만 돌면 "고아 본딩"이 남는다 —
  // 프로파일 도입 전에 맺힌 본딩이나 슬롯이 초기화된 뒤 남은 본딩은 어느 프로파일에도 없어서
  // 영영 못 지우고, 그 호스트는 재페어링 때마다 실패한다(실제로 겪음).
  bt_unpair(BT_ID_DEFAULT, NULL);

  bleProfileSelect(0);        // ZMK 와 동일하게 0 번으로 되돌린다
  ble_advertising_update();   // 0 번이 이미 활성이었으면 Select 가 no-op 이므로 여기서 한 번 더

  logPrintf("[  ] ble all bonds cleared\n");
  // 호스트 쪽 본딩은 우리가 못 지운다. 남아있으면 그 호스트가 옛 키로 재연결을 계속 시도하고
  // (연결 -> 암호화 실패 -> 0x13 로 끊김 -> 반복) 그 사이 광고 슬롯을 가로채 새 페어링을 막는다.
  logPrintf("     주의: 호스트(PC/폰)에서도 이 키보드를 삭제할 것.\n");
  logPrintf("           안 지우면 옛 키로 재연결을 반복해(0x13) 새 페어링이 막힌다.\n");
}


#if CLI_USE(HW_BLE)
void cliBle(cli_args_t *args)
{
  bool ret = false;

  if (args->argc == 1 && args->isStr(0, "info"))
  {
    char str[BT_ADDR_LE_STR_LEN];

    cliPrintf("active profile : %d\n", active_profile);
    for (int i = 0; i < BLE_PROFILE_COUNT; i++)
    {
      bt_addr_le_to_str(&profiles[i].peer, str, sizeof(str));
      cliPrintf("  [%d] %s %-30s %s\n",
                i,
                i == active_profile ? "*" : " ",
                bleProfileIsOpen(i) ? "(empty)" : str,
                bleProfileIsConnected(i) ? "connected" : "");
    }
    cliPrintf("advertising    : %s\n", adv_running ? "yes" : "no");

    if (loop_cnt > 0)
    {
      char str[BT_ADDR_LE_STR_LEN];

      bt_addr_le_to_str(&loop_addr, str, sizeof(str));
      cliPrintf("\n재연결 루프    : %s %d회%s\n", str, loop_cnt,
                ble_loop_muted() ? " (로그 억제중)" : "");
      if (ble_loop_muted())
      {
        cliPrintf("  호스트에만 본딩이 남아있다 -> 호스트(PC/폰)에서 이 키보드를 삭제할 것.\n");
      }
    }
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "sel"))
  {
    bleProfileSelect(args->getData(1));
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "next"))
  {
    bleProfileNext();
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "clear"))
  {
    if (args->isStr(1, "all"))
    {
      bleProfileClearAll();
    }
    else
    {
      bleProfileClear(args->getData(1));
    }
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("ble info\n");
    cliPrintf("ble sel   [0~%d]\n", BLE_PROFILE_COUNT - 1);
    cliPrintf("ble next\n");
    cliPrintf("ble clear [0~%d | all]\n", BLE_PROFILE_COUNT - 1);
  }
}
#endif
