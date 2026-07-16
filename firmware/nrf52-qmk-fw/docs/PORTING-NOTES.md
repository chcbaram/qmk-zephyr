# QMK-on-Zephyr 포팅 설계 노트

nRF52840(NCS v3.1.0 / Zephyr 4.1) 위에서 **QMK 키보드 로직 + VIA + 무선(BLE) + 저전력**을 구현하기
위한 설계 결정과 그 근거를 정리한다. "왜 이렇게 했는지"를 남기는 것이 목적이다.

- 참고 코드베이스: `baram-qmk-8k`(QMK 포크, STM32U5), `zmk`(Zephyr 키보드 펌웨어), `zmk-config`(wish60/65)
- 진행 계획: `~/.claude/plans/qmk-starry-fairy.md`

---

## 1. 큰 그림

QMK는 **유선 USB 전제 + 폴링 루프** 구조라 저전력이 약하다. ZMK는 **이벤트 구동**이라 저전력에 강하지만
키보드 로직/VIA 생태계는 QMK가 앞선다. 그래서 **하이브리드**를 택했다.

- **키보드 로직 = QMK** (`quantum/` 순정 무수정 vendor-in) — action/layer/tapping/VIA/dynamic_keymap
- **하드웨어·저전력 = Zephyr 네이티브** (kbd-matrix, USB, 향후 BLE/PM)
- **접착 계층 = `port/`** (우리가 작성하는 어댑터)

```
QMK quantum/ (순정, GPLv2, 무수정)
   │ matrix        │ timer      │ eeprom          │ host_driver / raw_hid
port/ (Zephyr 어댑터)
   matrix.c        timer.c      eeprom.c          host.c + driver_usb.c, via_hid.c
   │ input(kbd-matrix) │ k_uptime │ emu-eeprom     │ usbd_next HID
Zephyr 4.1 (NCS)
```

**QMK 코어를 수정하지 않는다**는 원칙을 지킨다 → GPL 분기 최소화 + upstream 동기화 용이.

---

## 2. 핵심 결정과 근거

### 2.1 QMK 네이티브 `host_driver_t` 사용 (참고본의 직접호출 방식 폐기)

`baram-qmk-8k`는 `host.c`를 수정해 `usbHidSendReport()`를 직접 박아 넣어 QMK의 드라이버 추상화를
우회했다. 이 프로젝트는 **원본 방식으로 되돌렸다**:

- `host.c`는 순정 유지 — 전송은 오직 `(*driver->send_keyboard)(report)` 로만.
- `host_driver_t` 인스턴스 2개: `port/driver_usb.c`(현재), `port/driver_ble.c`(Phase 5).
- **USB/BLE 전환 = `host_set_driver()`** — QMK 네이티브 `outputselect` 메커니즘 그대로.

→ 별도 endpoint 라우터가 필요 없고, 상위 QMK 로직은 transport를 모른다.

### 2.2 빌드는 반드시 `--no-sysbuild`

NCS의 **Partition Manager(sysbuild 기본)** 가 DTS의 `zephyr,code-partition` 을 무시하고 앱을 **0x0** 에
링크한다. 그런데 `CONFIG_BUILD_OUTPUT_UF2` 는 UF2를 **0x1000**(부트로더 슬롯)에 배치한다 →
**링크 주소와 배치 주소가 어긋나 즉시 하드폴트**(부팅 안 됨).

- `--no-sysbuild` 로 빌드하면 board DTS의 code-partition 이 그대로 링크 오프셋이 된다(검증: `_vector_table = 0x1000`).
- VS Code nRF Connect 확장도 Build Configuration 에서 **Sysbuild "No sysbuild"** 로 설정할 것.
- (대안으로 `pm_static.yml` 로 PM 에게 0x1000 을 지시하는 방법도 동작 확인했으나, 보드마다 파티션
  정보를 중복 관리해야 해서 폐기. board DTS 를 단일 진실 소스로 둔다.)

### 2.3 부트로더는 미리 플래시된 UF2 부트로더 전제 (wish60 기준)

MCUboot 듀얼슬롯을 쓰지 않는다. Adafruit nRF52 UF2 부트로더가 이미 보드에 있다고 보고 **앱만** 빌드한다.

| 파티션 | 주소 | 크기 | 용도 |
|---|---|---|---|
| sd (MBR) | 0x00000000 | 4KB | 예약 |
| code | 0x00001000 | ~844KB | 앱 (`zephyr,code-partition`) |
| eeprom | 0x000d4000 | 32KB | emu-eeprom 백엔드 |
| storage | 0x000dc000 | 96KB | 향후 NVS/settings(BLE bond) |
| boot (UF2) | 0x000f4000 | 48KB | **미리 들어있는 부트로더** |

`bootloader_jump()` = GPREGRET 에 매직 `0x57` 기록 후 `sys_reboot()` (Adafruit 부트로더 DFU 진입 규약).

### 2.4 키 매트릭스: Zephyr 네이티브 `gpio-kbd-matrix` (ZMK kscan 미사용)

**ZMK 의 `kscan_gpio_matrix` 를 쓰지 않기로 했다.** 이유:

- ZMK 드라이버는 `zephyr/drivers/kscan.h` / `kscan_driver_api` 에 의존하는데, **upstream Zephyr 4.x 는
  kscan 서브시스템을 제거**했다. NCS v3.1.0(Zephyr 4.1)에도 `kscan.h` 가 없다.
- 최신 ZMK 는 Zephyr 4.1 로 올라갔지만(`zmkfirmware/zephyr` 포크 `v4.1.0+zmk-fixes`), **자기 포크에
  kscan 을 살려두는 패치를 유지**하는 방식이다. 우리가 쓰려면 제거된 서브시스템을 부활시켜야 하고,
  NCS 업그레이드마다 깨질 위험을 떠안는다.
- **Zephyr 4.1 에 네이티브 대체제가 있다**: `gpio-kbd-matrix`(input 서브시스템). 코드 확인 결과
  **ZMK kscan 과 동일한 저전력 기법**이다 — idle 시 전 컬럼 구동(`COLUMN_DRIVE_ALL`) + row 인터럽트
  (`GPIO_INT_EDGE_TO_ACTIVE`), 스캔 스레드는 세마포어에서 블록(CPU sleep), 눌림 인터럽트로 wakeup,
  `poll_timeout` 후 다시 idle. `PM_DEVICE` 지원.

→ **같은 저전력 동작을 흡수 코드 0으로** 얻고 업스트림이 관리한다.

**명명 주의(중요)**: Zephyr 는 `row-gpios` = **입력(인터럽트)**, `col-gpios` = **구동 출력** 이다.
wish60 은 row2col 다이오드라 row 를 구동하고 col 을 읽으므로 **역매핑**했다:

| Zephyr | wish60 / QMK | 개수 |
|---|---|---|
| `row-gpios` (입력) | wish60 col → QMK **col** 인덱스 (`INPUT_ABS_Y`) | 15 |
| `col-gpios` (출력) | wish60 row → QMK **row** 인덱스 (`INPUT_ABS_X`) | 5 |

입력이 15개라 `CONFIG_INPUT_KBD_MATRIX_16_BIT_ROW=y` 필요(기본 8비트는 8개 한도).

### 2.5 디바운스: 드라이버 0 + QMK 담당

`debounce-down-ms = <0>`, `debounce-up-ms = <0>` 로 드라이버 디바운스를 끄고 **QMK 가 디바운스**한다.

- **QMK 시맨틱 유지** — 기존 폴링 드라이버 시절과 동일.
- **VIA 런타임 디바운스 전환 여지 보존** — 드라이버 파라미터는 `static const`(ROM)라 **런타임 변경
  불가**(런타임 setter 는 `input_kbd_matrix_actual_key_mask_set()` 뿐). QMK 디바운스면 VENOM 의
  `DEBOUNCE_RUNTIME` 을 나중에 쓸 수 있다.
- **저전력과 무관** — 드라이버의 인터럽트 idle 로직은 디바운스와 별개.
- **이중 디바운스 금지** (지연만 늘어남) → 드라이버 0 으로 확정.

기본 알고리즘은 **타이핑용 `sym_defer_pk`**(안정 후 보고, 채터링에 강함). `sym_eager_pk` 는 즉시 보고 +
락아웃으로 지연이 짧아 **게이밍용**(VENOM 기본).

### 2.6 QMK 폴링 ↔ ZMK 이벤트 구동: 하이브리드의 본질적 난제

| | ZMK | QMK |
|---|---|---|
| 메인 루프 | **없음.** `main()` 이 초기화만 하고 return → 전부 인터럽트/이벤트 구동 | `keyboard_task()` 를 **계속 폴링**해야 상태가 진행 |
| 타이머 | `k_work_delayable` 예약 → CPU 는 잠 | `timer_read()` 를 매 루프 폴링 |

**저전력을 막는 진짜 원인은 디바운스 위치가 아니라 QMK 의 폴링 루프 자체다.** 드라이버가 인터럽트로
잠들어도 메인 루프가 1ms 마다 깨면 CPU 는 못 잔다.

**해법(QMK 코어 무수정)**: QMK 를 개조하지 않고 **언제 호출할지를 우리가 제어**한다.

```
활성 구간 : qmkUpdate() 를 ≈1ms 주기로 (QMK 디바운스/탭핑 타이머 정상 진행)
완전 idle : (눌린 키 0개) && (마지막 입력 후 GRACE 경과) → 세마포어 블록 → CPU sleep
            input 콜백이 세마포어 give → 즉시 wakeup
```
- 디바운스가 루프를 필요로 하는 시점 = 키 누른 직후 수 ms → **어차피 깨어 있는 구간**이라 양립한다.
- `GRACE`(20ms) 는 QMK 내부 상태를 안 건드리고 디바운스가 정착할 시간을 준다.
- 구현: `port/matrix.c` 의 `qmkIsIdle()` / `qmkWaitActivity()`, `ap.c` 루프.

**의도적 한계**: 키가 안 눌린 상태에서 도는 QMK 타이머(tap dance, one-shot 만료 등)는 잠든 동안
진행되지 않고 다음 키 입력에서 재개된다. 현재 키맵(MO 레이어)엔 해당 없음.

**현실적 기대치**: 배터리를 좌우하는 건 idle(안 만지는 시간, 99.9%)이고 그 구간은 ZMK 와 동일하게
잠든다. 활성 중 폴링만 ZMK 대비 손해인데 그 시간은 극히 일부다.

### 2.7 EEPROM: emu-eeprom + RAM 미러 + settle-flush

nRF52840 엔 내부 EEPROM 이 없다. `zephyr,emu-eeprom`(플래시 에뮬, DTS `eeprom0`)을 백엔드로 쓴다.

- **RAM 미러**: 모든 읽기는 미러에서(런타임 플래시 접근 0). QMK `dynamic_keymap` 조회가 잦다.
- **settle-flush**: 쓰기는 미러 갱신 + dirty 범위 표시만. 마지막 쓰기 후 100ms 조용하면 **한 번의 블록
  쓰기**로 flush → VIA 편집 버스트(수백~수천 바이트)가 **플래시 쓰기 1회**로 통합된다.
- 원본 baram 은 **바이트 단위 쓰기 큐**였는데, 이는 바이트마다 개별 플래시 write 를 유발해
  **program/erase 횟수를 최대화**(전력 최악) + 큐 RAM 12~16KB 를 먹는다 → settle-flush 로 대체.

**전력 3-레버**: settle-flush(program↓) / `partition-erase`(erase 지연·통합↓) / `rambuf`(read↓).
program/erase 가 EEPROM 최대 에너지원이므로 앞의 둘이 핵심.

> TODO(Phase 6): `eeprom_task()` 폴링을 **sleep 진입 훅**으로 옮길 것. 지금 `k_work` 로 다른 스레드에
> 빼면 flush 와 `eeprom_mark` 간 `eeprom_buf`/dirty 범위 **경쟁 조건**이 생기므로 락 또는 동일 컨텍스트 필수.

### 2.10 BLE 프로파일 5개 (`port/ble.c`) — ZMK 방식

**핵심: "누가 붙어 있나"가 아니라 "어느 conn 으로 보내나"로 전환한다.**
ZMK 와 동일하게 5대가 **동시에 연결된 채로** 있고, 리포트는 활성 프로파일의 conn 으로만 나간다
(`ble_profile_conn()` → `bt_hids_inp_rep_send(conn, ...)`). 전환 시 재연결 대기가 없다.

- **directed advertising 은 안 쓴다.** ZMK 도 코드에 넣었다가 주석 처리했다 — 프라이버시를 쓰는
  호스트는 주소가 계속 바뀌어 깨진다. 그래서 **항상 열린 광고**를 쓴다.
- 광고 on/off 판단: 활성 프로파일이 비었거나 연결 안 됨 → 광고 / 연결됨 → 중지(전력).
  비활성 프로파일의 호스트는 본딩돼 있으므로 자기가 알아서 재연결한다.
- **peer 주소 학습 = 페어링 완료 시점**(`auth_pairing_complete` → 활성 프로파일에 배정).
  활성 프로파일이 이미 잡혀 있으면 `auth_pairing_accept` 에서 **거부**한다 — 없으면 새 호스트가
  기존 슬롯을 덮어써 사용자가 프로파일을 잃는다. (`CONFIG_BT_SMP_APP_PAIRING_ACCEPT=y` 필요)
- 영속화: settings(NVS) `ble/profiles/<n>`(peer), `ble/active`(인덱스) — ZMK 와 같은 키 규약.

**비용**: `BT_MAX_CONN=5` + `BT_HIDS_MAX_CLIENT_COUNT=5` 로 RAM 이 **85KB → 95KB**(+10KB).
전력은 연결된 호스트 수에 비례해 늘어난다(호스트마다 연결 이벤트가 따로 돈다) — 켜둔 호스트가
많을수록 idle 이 올라간다. ZMK 도 동일한 트레이드오프다.

**함정 1 — 재페어링이 `security err 4` 로 실패한다.**
호스트에서 페어링을 지우고 다시 붙이면 키보드엔 **옛 본딩이 남아** 있다. Zephyr 의
`update_keys_check()`(smp.c)는 기존 본딩을 덮어쓰는 걸 거부하고 `BT_SMP_ERR_AUTH_REQUIREMENTS`
→ `security_changed(level 1, err 4)` 를 낸다. **`CONFIG_BT_SMP_ALLOW_UNAUTH_OVERWRITE=y` 필수**
(ZMK 도 `app/Kconfig` 에서 `imply` 로 켠다). 이걸 빼먹어서 실기기에서 BLE 키 입력이 안 됐다.

**함정 2 — 고아 본딩.** `bleProfileClearAll()` 이 `profiles[]` 만 돌면, 어느 프로파일에도 없는
본딩(프로파일 도입 전에 맺힌 것 등)은 **영영 못 지운다**. 그 호스트는 재페어링 때마다 실패한다.
→ `bt_unpair(BT_ID_DEFAULT, NULL)` 로 스택의 본딩을 통째로 지운다.

**함정 3 — 본딩 클리어 후 옛 호스트가 무한 재연결한다(0x13 루프).**
본딩은 **양쪽에 있다.** 키보드만 지우면 호스트는 자기 본딩으로 재연결 → 옛 LTK 로 암호화 시도 →
키가 없어 실패 → 호스트가 `0x13`(REMOTE_USER_TERM)으로 끊음 → 무한 반복.
로그에 `security` 줄이 **안 찍히는 것**이 구분점이다(SMP 까지 못 간다).
단순 스팸이 아니라 **옛 호스트가 광고 슬롯을 가로채 새 페어링을 막는다.**
→ 펌웨어로 못 막는다. **호스트에서도 장치를 삭제**해야 한다(ZMK 도 동일하게 안내한다).
`bleProfileClearAll()` 이 로그로 안내를 남긴다.

**전환 키코드**: VIA `customKeycodes` → `QK_KB_0`(0x7E00)부터 순서대로 매핑.
`keyboards/<kbd>/port/keycode_port.c` 가 `process_record_kb()`(weak) 를 오버라이드한다.
**JSON 배열 순서와 C enum 순서가 반드시 일치**해야 한다(VIA 는 인덱스로만 지정).

### 2.9 VIA 커스텀 채널 (`port/via_port.c`) — 설정을 VIA UI 로

무선/전력 설정을 VIA 에 노출한다. **QMK 코어는 안 건드린다**:
- `quantum/via.c` 의 **weak** `via_custom_value_command_kb()` 를 오버라이드해서 받는다
  (검증: `nm` 결과 우리 심볼이 `T`, QMK 기본이 밀려남).
- 채널 ID 는 숫자로만 통하므로 `via.h` 의 `via_channel_id` enum 을 수정할 필요가 없다 →
  `port/via_port.h` 에서 `ID_BARAM_POWER_CHANNEL 15` 로 정의. (8~14 는 baram-qmk 가 이미 사용 중)

| 채널 | value | 의미 | 단위 |
|---|---|---|---|
| 15 | 1 | idle 타임아웃 | 초 |
| 15 | 2 | sleep 타임아웃 | 분 |

- VIA 쪽은 정의 JSON 의 **`menus`**(v3 포맷, `VIA_PROTOCOL_VERSION 0x000C` 필요)에
  `content: [<이름>, <channel>, <value>]` 로 연결한다.
- **값 폭은 컨트롤 타입이 정한다.** 틀리면 값이 조용히 어긋난다.
  (출처: [VIA custom_ui 스펙](https://caniusevia.com/docs/custom_ui) — 추측하지 말고 여기를 볼 것)

  | 타입 | 폭 | 비고 |
  |---|---|---|
  | `toggle` | 1B | 0/1 (`options` 로 값 지정 가능) |
  | `range` | **max≤255 → 1B, 아니면 2B 빅엔디안** | **최댓값이 폭을 바꾼다** |
  | `dropdown` | 1B | 인덱스 또는 지정값 |
  | `button` | 1B | |
  | `color` | 2B | hue, sat |
  | `keycode` | 2B **빅엔디안** | `port/kill_switch.c` |

  타임아웃은 `dropdown` → 1바이트라 단위를 **초/분**으로 잡아 0~255 에 맞췄다.
- **드롭다운 기본값 주의**: `activity.c` 의 기본값(30초/60분)이 `options` 목록에 없으면
  VIA 가 빈 칸으로 표시한다. 기본값을 바꾸면 JSON 목록도 같이 볼 것.
#### EEPROM 배치 — 크기를 바꾸지 말고 오프셋으로 늘린다 (중요)

QMK EEPROM 주소는 **앞 영역 크기의 누적**이다:
`EECONFIG_BASE_SIZE + EECONFIG_KB_DATA_SIZE + EECONFIG_USER_DATA_SIZE`(=`EECONFIG_SIZE`)
→ `VIA_EEPROM_MAGIC_ADDR` → `VIA_EEPROM_CUSTOM_CONFIG_SIZE` → `DYNAMIC_KEYMAP_EEPROM_START`.
즉 **앞쪽 크기를 건드리면 사용자 키맵이 통째로 밀린다.**

→ baram-qmk/VENOM 방식을 따른다: `EECONFIG_USER_DATA_SIZE` 를 **512B 로 한 번 잡아 고정**하고
   설정은 그 안에서 **직접 오프셋**으로 배치(`port/port.h` 맵). 새 설정은 빈 오프셋을 쓰면 되므로
   크기가 안 바뀌고 키맵이 살아남는다.
   (`VIA_EEPROM_CUSTOM_CONFIG_SIZE` 를 쓰면 항목 추가마다 크기가 바뀌어 매번 키맵이 밀린다 —
    실제로 그렇게 짰다가 키맵이 전부 틀어졌다)

**`QMK_BUILDDATE`(port/version.h)는 고정 문자열**이다. 실제 빌드 날짜가 아니라서 **펌웨어를
업데이트해도 키맵이 살아남는다**(baram-qmk 원본도 동일). 대신 **EEPROM 레이아웃을 바꿨다면
반드시 이 값을 올려야** 한다 — 안 올리면 EEPROM 이 "유효"로 판정돼 옛 데이터를 어긋난 주소에서
읽는다. 반대로 로직만 바뀐 업데이트에서 올리면 사용자 키맵이 날아간다.

**`eeconfig_init_user_datablock()` 은 사용자 영역을 0 으로 민다** → 각 항목은 "저장된 적 있음"을
스스로 판별해야 한다(`power_cfg.c` 는 magic 바이트, baram `debounce_cfg.c` 는 범위 검증).
- 같은 통로로 나중에 **TX power**(반드시 `bleSetTxPower()` 래퍼로 — §4.3)와 **런타임 디바운스**를 태운다.

#### 파일 구성 (baram-qmk 와 동일)

| 위치 | 역할 |
|---|---|
| `port/via/via_port.h` 규약, `port/via/*.c` | **공용 기능** — 기능마다 자기 EEPROM + VIA 값 핸들러를 소유 (`power_cfg.c`, `sys_port.c`) |
| `keyboards/<vendor>/<kbd>/port/via_port.c` | **보드별 라우터** — `via_custom_value_command_kb()` 오버라이드, 그 보드가 노출할 채널만 연결 |

보드마다 노출 채널이 다르므로 라우터는 키보드 트리에 둔다.
`SYSTEM` 채널(9)은 VENOM 과 동일: DFU 진입 + **토글 3개를 모두 켜야** 실행되는 EEPROM 초기화.

### 2.8 USB PID 는 보드마다 분리

`usb.c` 가 `#include QMK_KEYMAP_CONFIG_H` 로 **board config.h 의 VID/PID/이름을 참조**한다 →
보드마다 자동 분리. baram 기존 사용: `0x5201~0x5207`, `0x5210` → **wish60 = 0x5208**.
config.h / info.json / VIA JSON / usb.c 디스크립터 **4곳이 일치**해야 한다.

---

## 3. 하드웨어에서 겪은 함정 (재발 방지)

### 3.1 USB VBUS-at-boot — 부팅 시 열거 안 됨
nRF52 는 `usbd_can_detect_vbus()` 가 true 라 `usbd_enable()` 이 **`VBUS_READY` 이벤트**에서 일어난다.
그런데 **부팅 시 USB 가 이미 연결(VBUS High)돼 있으면 그 "엣지" 이벤트가 오지 않아** USB 가 영영 안 켜진다.
(뺐다 끼면 동작 → 이 증상으로 진단됨)

**해결**: `nrf_power_usbregstatus_vbusdet_get(NRF_POWER)` 로 부팅 시 VBUS 상태를 직접 읽어, 이미
연결돼 있으면 즉시 `usbd_enable()`. (`usb.c`)

### 3.2 VIA 가 "Loading" 에서 멈춤
`via_output_report`(USB 수신 콜백) 안에서 응답을 `hid_device_submit_report`(동기 전송)으로 바로 보내면
USB 스택 컨텍스트에서 **재진입/블록**되어 응답이 안 나간다 → VIA 무한 Loading.

**해결**: OUT 리포트를 **메시지큐**(`via_rx_msgq`)에 넣고 **전용 스레드**(`via_process_thread`)가
`raw_hid_receive` + 응답 전송을 **콜백 밖에서** 수행. (`usb_hid.c`)

### 3.3 CLI 시리얼 입력이 죽음
`apMain` 의 타이트 루프(우선순위 높은 메인 스레드)가 CLI/UART 스레드(우선순위 5)를 **굶겼다**.
**해결**: 루프에 `k_msleep(1)`. (`k_yield()` 는 같거나 높은 우선순위에만 양보하므로 **효과 없음** —
낮은 우선순위 스레드를 돌리려면 실제로 재워야 한다.)

### 3.4 `via_iface_ready` 가 `kb_ready` 를 덮어씀
VIA 인터페이스 ready 콜백이 키보드용 플래그를 건드리던 버그 → `via_ready` 로 분리.

### 3.5 kbd-matrix row 인터럽트가 절반만 걸려 있었다 (`sense-edge-mask`)

**증상(조용함)**: idle 상태에서 **뒤쪽 컬럼의 첫 키를 놓친다.** 로그 한 줄 외엔 아무 표시가 없다.
게다가 deep sleep 에서 **영영 못 깨어난다**.

**원인**: Zephyr `gpio-kbd-matrix` 는 row 인터럽트를 `GPIO_INT_EDGE_TO_ACTIVE`(**엣지**)로 건다.
nRF 에서 엣지는 기본이 **GPIOTE IN 채널**이다(`gpio_nrfx.c`) → 두 가지가 동시에 깨진다:
1. **nRF52840 의 GPIOTE 채널은 8개뿐인데 row-gpios 는 15개.** 9번째부터 `-ENOMEM` 이고
   드라이버는 **첫 실패에서 곧바로 `return`** 해 나머지 row 는 인터럽트가 아예 없다.
2. **GPIOTE 는 System OFF 에서 꺼진다.** System OFF 웨이크업은 오직 DETECT(`PIN_CNF.SENSE`)로만.

**해결**: board DTS 의 `&gpio0`/`&gpio1` 에 **`sense-edge-mask`** — 마스크에 든 핀은 엣지라도
SENSE(PORT 이벤트)로 처리된다 → **채널 0개 + DETECT**. 마스크는 `row-gpios` 를 그대로 덮어야 한다.

**왜 ZMK 엔 없나**: ZMK kscan 은 `GPIO_INT_LEVEL_ACTIVE`(**레벨**)를 써서 애초에 GPIOTE 채널 분기를
안 탄다(→ SENSE 자동). 이 속성은 **네이티브 드라이버(엣지)를 택한 대가**이고, §2.4 의 트레이드오프가
현실로 나타난 지점이다. 그래도 판단은 유지 — ZMK kscan 이식은 upstream 이 제거한 서브시스템을
되살려 NCS 업그레이드마다 관리하는 일이다. **DTS 2줄 vs 영구 부채.**

**재발 방지**:
- `port/matrix.c` 의 **`BUILD_ASSERT`** 가 `row-gpios` 전 핀이 마스크에 있는지 검사 → 빠지면 **빌드 에러**.
  (핀 하나를 일부러 빼서 실제로 막히는 것까지 확인함)
- DTS 는 **`BIT()` 나열**로 쓴다(`#include <zephyr/dt-bindings/dt-util.h>`) — 16진수 계산 불필요,
  `row-gpios` 목록과 1:1 로 대응해 눈으로 검증된다.

---

## 4. 다중 보드 / 확장 노트

### 4.1 구조
키보드 1대 = **두 트리**, 이름 규약으로 링크.

```
boards/baram/<kbd>/                          # 하드웨어(Zephyr board) — 순수 HW
  <kbd>.dts, <kbd>_defconfig, Kconfig.<kbd>, board.yml/cmake
src/ap/modules/qmk/keyboards/<vendor>/<kbd>/ # 로직(QMK) — GPLv2 격리
  config.h(MATRIX/레이어/VID·PID), config.cmake(디바운스/RGB),
  keymap.c(GPL-2.0), info.json, json/<KBD>-VIA.JSON, json/<kbd>.json(KLE)
```
- 빌드: `west build -b <kbd> --no-sysbuild . -- -DBOARD_ROOT=<app> -DKEYBOARD_PATH=keyboards/baram/<kbd>`
- **정합 제약**: DTS 매트릭스 GPIO 개수 = QMK `config.h` 의 `MATRIX_ROWS/COLS`.
- wish60 매트릭스 (r,c) 는 **ZMK wish60 `matrix-transform` 이 진실 소스**. 물리 레이아웃/스타일은 VENOM-60 참고.

### 4.2 시프트레지스터(74HC595)로 GPIO 확장 — wish65
wish65 는 GPIO 부족을 **595 2개 직렬(16핀)** 로 해결하고, **구동측(col)을 595 에, 읽기측(row)을 MCU GPIO** 에 둔다
(`diode-direction = "col2row"`). 이 구성이 Zephyr `gpio-kbd-matrix` 의 명명(col=출력, row=입력)과 **그대로 일치**한다.

- **네이티브 `ti,sn74hc595` 는 부족하다**: 바인딩이 `ngpios: const: 8`(1칩 고정), 드라이버도 `uint8_t output`
  → **직렬 연결 미지원**. wish65 의 16핀 불가.
- **ZMK `gpio_595.c`(221줄)는 이식 가능**: `zephyr/drivers/gpio.h` + `spi.h` 등 **안정 API 만** 사용
  (kscan 과 달리 제거된 서브시스템 의존 없음), `nwrite = ngpios/8` 로 직렬 지원.
- → **kscan 은 네이티브, 595 는 ZMK 것 이식** — 각각 나은 쪽을 취한다.

**웨이크업은 성립한다**: 595 는 **래치**라 idle 진입 시 "전 컬럼 active" 를 한 번 쓰면 유지되고(이후 SPI 0),
**웨이크 인터럽트는 row = 진짜 MCU GPIO** 라 CPU 를 깨운다. 단 실무 주의:

1. ~~595 전원 유지~~ — **기우였다(확인됨)**. wish60/wish65 **둘 다 `ext-power` 는 네오픽셀 전원만**
   끊는다. 595 는 상시 전원이라 deep sleep 에서 컬럼 구동이 유지된다. (ext-power 가 매트릭스 구동원을
   끊는 설계였다면 영영 못 깨는 함정이 됐을 것 — 새 보드를 만들 땐 이 점을 확인할 것)
2. **reset/latch 핀 글리치 방지** — 슬립 중 MCU 핀이 하이임피던스가 되어 래치가 풀리지 않도록 풀업/다운 고정.
3. **SPI 비용** — 컬럼 구동마다 SPI 트랜잭션. 16컬럼 × 1kHz = 16k SPI/초로 과함 → **스캔 주기를 낮출 것**.

### 4.3 NCS 의존도 / 순수 Zephyr 4 이식성

의도적으로 **Zephyr 네이티브 컴포넌트를 우선 선택**해서 NCS 의존을 최소화했다.

| 구성요소 | 출처 | 순수 Zephyr 4 |
|---|---|---|
| `gpio-kbd-matrix`, `zephyr,emu-eeprom`, `usbd_next`, settings/NVS, `BT_BAS`/`BT_DIS` | upstream Zephyr | ✅ 그대로 |
| `nrf_power` HAL(VBUS) | hal_nordic(nrfx) | ✅ 양쪽 존재 |
| **`BT_HIDS`(HID over GATT) + `BT_CONN_CTX`** | **NCS 전용** | ❌ **대체 필요** |
| BLE 컨트롤러 | NCS=SoftDevice Controller | ✅ `BT_LL_SW_SPLIT` 로 교체 |
| `--no-sysbuild` | NCS Partition Manager 회피 | ✅ 순수 Zephyr 엔 PM 자체가 없어 더 단순 |

→ **실질 NCS 락인은 BT_HIDS 하나.** 옮길 때는 (a) `BT_GATT_SERVICE_DEFINE` 으로 HOG 직접 구현
(ZMK `app/src/hog.c` 가 정확히 이 방식 — 참고본 존재) 또는 (b) NCS `hids.c` vendor-in.

**`port/ble.c` 가 BT_HIDS 를 만지는 유일한 파일**이라, 갈아끼워도 `port/driver_ble.c`(host_driver_t)와
QMK 코어는 무수정이다. host_driver_t 추상화가 NCS 의존을 한 파일에 격리한다.
(Phase 4 에서 ZMK kscan 대신 네이티브를 고른 것도 여기서 이득 — ZMK 걸 썼으면 제거된 kscan
서브시스템까지 함께 짊어져야 했다.)

**현재 결정: NCS 유지.** 순수 Zephyr 이행은 보류(추후 재검토). 근거 —
- nRF 에 머무는 한 NCS 의 **SoftDevice Controller** 가 BLE 전력·안정성에서 유리하다(무선 키보드의 핵심).
- 전환 비용이 `port/ble.c` 하나로 묶여 있어 **지금 선제 작업을 할 이유가 없다**(나중에 해도 비용 동일).
- 순수 Zephyr 로 갈 진짜 트리거는 "**nRF 외 다른 MCU 도 지원**" 이다. 그 계획이 생기면 재검토.

**이식성 유지를 위해 지킬 규율 (추가 작업 아님, 원칙):**
1. NCS API 를 `port/ble.c` 밖으로 내보내지 않는다. `ble.h` 는 `report_keyboard_t`/`report_extra_t` 등
   QMK 타입만 노출하고 `bt_hids` 타입은 새지 않게 한다(현재 준수 중).
2. **TX power 는 반드시 `bleSetTxPower(int8_t dbm)` 같은 래퍼로 감쌀 것.** 런타임 TX power 는
   `sdc_hci_cmd_vs_write_tx_power_level()`(**SoftDevice Controller 전용**)이라, VIA 핸들러가 HCI 를
   직접 호출하면 새로운 NCS 락인이 `port/ble.c` 밖으로 퍼진다. 순수 Zephyr 컨트롤러는 방식이 다르다.

### 4.4 Split(분할 키보드) — 나중에 추가 가능

현재 대상 보드(wish60/65)는 non-split 이라 범위 밖이지만, 나중에 붙일 수 있게 설계돼 있다.

- **`port/matrix.c` 가 raw 매트릭스를 만드는 유일한 지점**이다. 원격 반쪽의 키 위치를 같은
  `raw_matrix[]` 에 merge 하면 QMK 코어는 **무수정**으로 동작한다.
- 필요한 것: split GATT 서비스(peripheral 이 위치 비트맵 notify, central 이 subscribe — ZMK
  `split/bluetooth/service.c` 패턴), peripheral 펌웨어(스캔+notify 만, QMK 불필요),
  central 은 호스트에겐 peripheral·반대편엔 central 이므로 `BT_CENTRAL=y` + `BT_MAX_CONN>=2`,
  `MATRIX_ROWS` 를 양쪽 합계로.
- **QMK 의 `SPLIT_KEYBOARD`(split_common)는 쓰지 않는다** — QMK 자체 전송(I2C/serial) 전제라
  매트릭스 레벨 병합이 훨씬 단순하다.

### 4.5 스캔 주기 최적화 (transport 인지)
- **USB**: 폴링 1ms → 1kHz 스캔이 의미 있음.
- **BLE**: 연결 간격이 **7.5~15ms** → 1kHz 스캔해도 리포트는 그 간격에만 나간다. **4~8ms 로 낮춰도 체감
  지연 0**, 활성 중 wakeup 5~10배 감소. → Phase 5 에서 **활성 transport 별 적응형**으로.
- 드라이버 `poll-period-ms` 와 QMK 루프 주기는 비슷하게 (QMK 를 더 빨리 돌려도 raw 가 안 바뀜).
- 하한 주의: 너무 느리면(>8ms) 디바운스 해상도·키 지연이 나빠진다.

---

## 5. 진행 상태 (Phase)

| Phase | 내용 | 상태 |
|---|---|---|
| 0 | QMK 코어 vendor-in + port/ 어댑터 + 빌드 골격 + 파티션/UF2 | ✅ 실기기 |
| 1 | 유선 USB 타이핑 + CLI | ✅ 실기기 |
| 2 | Extra keys(consumer/system) | ✅ 코드 정합 |
| 3 | VIA 프로토콜 + emu-eeprom 영속화 | ✅ 실기기(usevia 로딩·영속화) |
| 4 | 네이티브 kbd-matrix 저전력 스캔 + 이벤트 구동 루프 | ✅ 실기기(2.63→1.22mA) |
| 5 | BLE HOG + USB/BLE 전환(`host_set_driver`) | ✅ 실기기(유선·무선 동작) |
| 6 | 콘솔 빌드타임 분리 → **idle 80.9µA** | ✅ 실기기 |
| 6 | 배터리 잔량(VDDH) + BAS | ✅ 실기기(전압 정상) |
| 6 | activity 상태머신 + deep sleep(System OFF) | ✅ 실기기(27.7µA, 웨이크업 확인) |
| 6 | VIA 커스텀 메뉴 — 타임아웃 2개 | 🔄 실기기 검증 중 |
| 7 | 다중 보드 구조화(595/wish65 포함) | ⬜ |
| 8 | ws2812 네오픽셀 + QMK RGBLIGHT + VIA RGB + **ext-power** | ⬜ |

**ext-power 를 Phase 8 로 둔 이유**: 두 보드 모두 ext-power 가 끊는 대상은 **네오픽셀뿐**이다(§4.2).
즉 ws2812 드라이버가 없는 동안엔 ext-power 가 할 일이 없다 → ws2812 와 한 몸으로 넣는다.
Phase 6 엔 "부팅 시 레일을 명시적으로 off 로 구동"하는 방어만 남는다.

**ws2812 를 Phase 6 뒤로 미룬 이유**(둘 다 실측/구조 근거):
- **전력**: wish60 스트립은 16개. WS2812 는 검은색을 표시해도 컨트롤러가 개당 ~0.7mA →
  **약 11mA, idle 80.9µA 의 140배**. ext-power 게이팅 없이는 성립 자체가 안 된다.
  (ZMK 도 wish60 에 `RGB_UNDERGLOW_ON_START=n` + `AUTO_OFF_IDLE`/`AUTO_OFF_USB` 를 건다)
- **구조**: 지금 `ap.c` 는 idle 이면 `qmkWaitActivity()` 에서 **무한 블록**한다. RGB 애니메이션은
  주기적 wakeup 이 필요하므로 **activity 상태머신에 얹혀야 한다** → 상태머신(Phase 6)이 선행.
- 포팅 부담은 작다: `worldsemi,ws2812-spi` 는 **Zephyr 네이티브**(§3.1 과 같은 패턴), ZMK
  `ext_power_generic.c`(215줄)도 GPIO 토글이라 우리 스타일 30줄이면 된다. VIA 는 RGBLIGHT 를 기본 지원.

---

## 6. 전력 — 실측 기록

USB 미연결(배터리) + BLE 연결 idle 기준. Nordic Power Profiler.

| 시점 | 평균 | 무엇이 바뀌었나 |
|---|---|---|
| Phase 3 (폴링 루프) | 2.63 mA | |
| Phase 4 (이벤트 구동 + LED off) | 1.22 mA | 루프가 `qmkWaitActivity()` 로 블록 |
| Phase 5 (BLE 연결) | 1.21 mA | **BLE 가 더한 전류 ≈ 0** (slave latency 효과) |
| Phase 6 (콘솔 빌드타임 제거) | **80.9 µA** | UARTE 제거 — **14배** |
| Phase 6 (deep sleep, System OFF) | **27.7 µA** | 그중 ~25µA 는 MCU 가 아니라 MAX17048 (§6.5) |

### 6.1 UARTE 가 idle 전류를 지배했다 (가장 큰 교훈)

nRF52840 UARTE 는 **RX 활성 상태에서 HFCLK 를 계속 잡아 ~1mA** 를 먹는다. 1.2mA 의 정체는
키보드 로직도 BLE 도 아닌 **디버그 콘솔**이었다. 이걸 찾는 데 세 번 틀렸고, 그 실패들이 교훈이다:

1. **런타임 게이팅(PM suspend) — 실패.** `pm_device_action_run(uart0, SUSPEND)` 로 바닥은
   1.1→0.65mA 로 떨어졌지만 ~1.6ms 주기 wakeup 이 새로 생겨 **평균은 1.21→1.98mA 로 악화**.
   suspend 된 UART 와 콘솔 서브시스템이 충돌한다.
2. **C 코드만 제거 — 실패.** `_USE_HW_DEBUG_CONSOLE` 로 앱의 UART/CLI/로그를 통째 빼도
   **1.21→1.13mA, 사실상 무변화**. `prj.conf` 의 `CONFIG_SERIAL/CONSOLE/UART_CONSOLE` 이
   그대로라 Zephyr 콘솔 스택이 여전히 빌드·초기화됐기 때문.
3. **C + Kconfig 동시 제거 — 성공.** `CONFIG_SERIAL=n` 까지 가서야 **80.9µA**.

→ **콘솔 제거는 C 와 Kconfig 양쪽이어야 하며, 반드시 한 스위치로 묶어야 한다.**
   그래서 `-DDEBUG_CONSOLE=y`(CMake) 가 `_USE_HW_DEBUG_CONSOLE`(C) 와 `debug.conf`(Kconfig)를
   **함께** 켠다. 어느 한쪽만 손으로 켜지 말 것.

### 6.2 남은 80.9µA 의 정체

파형은 바닥이 0 에 붙고 **~345ms 간격 11.9mA 스파이크**만 남는다. 이건 BLE 라디오 wakeup 이고,
계산값(interval 11.25ms × (latency 30 + 1) = 348ms)과 일치한다. 즉 **남은 전류는 거의 전부 BLE**이며
이미 slave latency 로 최적화된 값이다. 더 줄이려면 latency/interval 을 늘려야 하는데 응답성과 맞바꾼다.

### 6.3 배터리 — VDDH 지금 / MAX17048 나중 (백엔드는 DTS 가 정함)

`battery.h` 는 `batteryGetPercent()` 만 노출하고 백엔드는 `battery.c` 안에서 DTS 로 고른다.
호출자(BAS)는 어느 쪽인지 모르므로 나중에 바꿔도 `battery.c` 밖은 안 바뀐다.

| 백엔드 | 조건 | 비고 |
|---|---|---|
| VDDH (SAADC `VDDHDIV5`) | 기본 | 외부 부품 0. **배터리가 VDDH 핀 직결(고전압 모드)** 전제 |
| MAX17048 (I2C 연료게이지) | DTS 에서 `maxim,max17048` 노드 okay | wish60 회로에 i2c1 @0x36 실장됨 |

- **MAX17048 은 Zephyr 네이티브 드라이버가 있다**(`drivers/fuel_gauge/max17048`). ZMK 는 자체
  `zmk,maxim-max17048` 를 쓰지만 우리는 업스트림을 그대로 쓴다 — kbd-matrix 와 같은 패턴(§3.1).
  확장 시: DTS 노드 + `CONFIG_FUEL_GAUGE=y` + `CONFIG_MAX17048=y` + `CONFIG_I2C=y`. 드라이버 포팅 없음.
- **참고**: ZMK wish60 은 VDDH 가 아니라 이 연료게이지를 쓴다(`zmk,battery = &builtin_fuel_gauge`,
  vbatt 노드는 주석 처리). VDDH 를 실제로 쓰는 건 wish65 다.
- **검증 완료(실기기)**: 전압이 실제 배터리와 일치 → **고전압 모드 확인**. VDDH 방식 유효.
- (판정법) 고전압 모드인지는 코드로 알 수 없다. `battery info` (개발 빌드 CLI) 의 전압을
  실제 배터리 전압과 비교할 것. 항상 ~3.3V 로 고정이면 일반 모드(VDDH=VDD)라 무의미 → 연료게이지로 간다.

**SOC 곡선**: ZMK 의 `mv * 2 / 15 - 459` 는 3.45~4.2V 직선 근사라 잘 안 맞는다(초반 급락/중반 정체).
OCV 구간 선형보간 테이블로 교체했다. 다만 전압 기반 SOC 는 무엇을 해도 근사다 — 곡선이 평평한
구간에선 10mV 가 잔량 ~10%, 타이핑 부하로 처지고, 충전 중엔 충전 전압을 읽는다. 정확도가 필요하면
MAX17048 이 답이다(칩이 부하/이력 보정).

**샘플링 주기 60초**(ZMK 와 동일), **연결 시에만** 동작. 샘플링은 SAADC/I2C 를 깨워 HFCLK 를 잡으므로
깨우는 횟수 자체가 idle 80.9µA 에 대한 비용이다. 광고 중엔 읽어줄 상대도 없어 아예 돌리지 않는다.

### 6.3 클럭 — 기본값에 의존하지 말 것

이 보드에는 외부 32.768kHz 크리스탈이 있고 `K32SRC_XTAL` 이 이미 선택돼 있었지만, board defconfig 가
아니라 **Zephyr 기본값**에 얹혀 있었다. LFXO/DCDC 는 전류를 좌우하므로 `NRF52840_defconfig` 에 명시했다.
**크리스탈 없는 보드를 파생시킬 땐 `K32SRC_RC` 로 바꿔야 한다**(안 그러면 LFCLK 가 안 뜬다).

### 6.5 deep sleep (System OFF) — 27.7µA, 여기가 바닥이다

`sys_poweroff()` = nRF52 System OFF. **깨어남 = 리셋 부팅**(RAM 리텐션도 꺼진다) → 타임아웃 1시간.
웨이크업은 **DETECT(PIN_CNF.SENSE)로만** — `sense-edge-mask` 가 전제다(§3.5). 실기기 검증 완료.

**측정 함정 1 — 디버거가 붙어 있으면 1.5mA 가 나온다.**
nRF52 는 디버그 인터페이스(DIF)가 활성이면 진짜 System OFF 에 들어가지 않고 **에뮬레이트**한다
(실측 1.51mA). DIF 는 SWD 를 한 번 붙이면 **핀 리셋/전원 재인가 전까지 래치**되므로 케이블만 뽑아선
안 풀린다. → 디버거 분리 + **배터리 재인가** 후 측정해야 진짜 값(27.7µA)이 나온다.
파형으로도 구분된다: 진짜로 자고 있으면 **BLE 광고 스파이크가 없다**(펌웨어가 멈췄으므로).

**측정 함정 2 — 남은 27.7µA 는 MCU 가 아니다.**
파형: ~25µA 바닥 + **250ms 주기** 430µA 스파이크. 우리 펌웨어는 멈춰 있으므로 MCU 가 아니다.
정체는 보드의 **MAX17048 연료게이지**(i2c1 @0x36) — 액티브 모드에서 **250ms 마다 셀 전압 측정**,
소비전류 **23µA(typ)**. 주기와 전류가 둘 다 일치한다. 우리는 I2C 를 켜지도 않아 칩이 POR 기본값
(액티브)으로 혼자 돈다. ZMK 도 init 에서 `set_sleep_enabled(false)` 로 깨우기만 하고 재우지 않는다.
→ **nRF52840 자체는 ~1-2µA. System OFF 는 정상 동작 중이다.**

**여기가 바닥인 이유**: 리튬폴리머 자가방전(월 2~3%)이 700mAh 기준 **≈28µA 상당**이다.
즉 배터리가 스스로 새는 속도가 이미 보드 전체 소비와 같다. 27.7µA → 2.9년(자가방전 무시 시).
더 줄여도 실수명이 안 는다. **최적화 종료 지점.**

> 나중 메모: 지금은 VDDH 를 쓰므로 MAX17048 이 **아무 일도 안 하면서 23µA** 를 먹는다. 재우면
> (MODE.EnSleep=1 → CONFIG.SLEEP=1) BLE idle 도 80.9 → ~58µA 가 된다. 다만 연료게이지 백엔드로
> 확장하면 깨어 있어야 하고, 23µA 가 정확한 잔량의 대가다. 중간값으로 HIBRT 하이버네이트
> (~3-4µA, 45초 측정)가 있고 키보드엔 45초면 충분하다. I2C 를 켜는 시점에 같이 다룰 것.

### 6.4 타이핑 중 전류: 3.4mA — QMK 폴링 모델의 값

키를 누르고 있는 동안 3.4mA(worst case, 연속 누름). `qmkIsIdle()` 이 false 인 동안 루프가
`k_msleep(1)` 로 1kHz 로 CPU 를 깨우기 때문이다 — **QMK 폴링 모델의 구조적 비용**이고
ZMK(완전 이벤트 구동)보다 높다. 다만 idle 이 배터리 수명을 지배하므로 우선순위는 낮다.
줄이려면 tap-hold 타이머가 없는 구간에서 루프 주기를 늘리는 방향(§4 의 QMK-폴링 vs ZMK-이벤트).

---

## 7. 빌드 / 플래시

```bash
export ZEPHYR_BASE=/opt/nordic/ncs/v3.1.0/zephyr
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
export ZEPHYR_SDK_INSTALL_DIR=/opt/nordic/ncs/toolchains/0c0f19d91c/opt/zephyr-sdk
PATH=/opt/nordic/ncs/toolchains/5c0d382932/bin:$PATH

# 릴리스(기본) — 콘솔 없음, 80.9µA
west build -b NRF52840/nrf52840 -d build -p always --no-sysbuild . -- \
     -DBOARD_ROOT=$PWD

# 개발 — UART 콘솔/CLI/로그 포함 (전류 ~1.2mA. 전력 측정용 빌드엔 절대 쓰지 말 것)
west build -b NRF52840/nrf52840 -d build -p always --no-sysbuild . -- \
     -DBOARD_ROOT=$PWD -DDEBUG_CONSOLE=y
```
- 산출물: `build/zephyr/zephyr.uf2` (start `0x1000`, family `0xada52840`)
- 플래시: 더블탭 리셋 → UF2 매스스토리지에 드래그드롭
- **`--no-sysbuild` 를 빼먹으면 부팅하지 않는다** (§2.2)
- 릴리스 빌드는 **시리얼이 물리적으로 없다** — 부팅 확인은 타이핑/BLE 연결로 한다

| 빌드 | FLASH | RAM | idle 전류 |
|---|---|---|---|
| 릴리스(기본) | 214 KB | 60 KB | **80.9 µA** |
| `-DDEBUG_CONSOLE=y` | 260 KB | 84 KB | ~1.2 mA |
