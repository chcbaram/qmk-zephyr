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

**함정 — 저전력 루프가 EEPROM 쓰기를 삼킨다.**
settle-flush 판정은 `eeprom_task()` 가 하는데 그건 메인 루프가 부른다. 루프가 idle 로 오래 자면
**flush 도 같이 멈춘다** → VIA 저장/키맵 편집 직후 전원이 꺼지면 유실된다.
실제로 겪었다: RGB 를 끄고 전원을 껐다 켜니 다시 켜져 있었다(최대 idle 타임아웃 30초의 창).
→ `eeprom_is_dirty()` 를 노출하고, `qmkGetIdleWaitMs()` 가 dirty 인 동안엔 20ms 이상 자지 않는다.
   `qmkWake()`(VIA 명령 후 1회 깨움)만으론 부족하다 — 깨어난 시점엔 아직 100ms 가 안 지났다.

> TODO: `eeprom_task()` 폴링을 **sleep 진입 훅**으로 옮기는 방안. 지금 `k_work` 로 다른 스레드에
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

| 채널 | value | 컨트롤 | 의미 |
|---|---|---|---|
| 9 (`sys_port.c`) | 1 | toggle | 부트로더(UF2) 진입 |
| 9 | 2~4 | toggle ×3 | EEPROM 초기화 (**3개 다 켜야** 실행) |
| 15 (`power_cfg.c`) | 1 | dropdown | idle 타임아웃(초) |
| 15 | 2 | dropdown | sleep 타임아웃(분) |
| 16 (`ble_cfg.c`) | 1 | dropdown | 활성 프로파일(0~4) |
| 16 | 2~6 | toggle ×5 | 프로파일별 **본딩 유무**. 끄면 그 본딩 삭제 |
| 16 | 7 | button | 전 프로파일 본딩 삭제 |
| 16 | 8 | dropdown | TX power (**dBm 이 아니라 인덱스**) |

**TX power 는 인덱스로 주고받는다.** VIA dropdown 값은 **1바이트 부호없음**이라 `-40dBm` 같은
음수를 그대로 못 싣는다. `BLE_TX_POWER_TBL`(ble_cfg.h) 인덱스를 주고받고 펌웨어가 dBm 으로 바꾼다.
JSON options 는 **C 테이블에서 생성**했다(손으로 맞추면 어긋난다).
컨트롤러가 미지원 값은 **가까운 값으로 클램프**하므로, 로그의 `적용 %ddBm` 이 요청과 다를 수 있다.
핸들 종류마다 따로 걸어야 한다 — **광고(ADV)와 연결(CONN)은 별개**라 `connected()` 에서 재적용한다.

BLE 토글은 **끄는 방향만** 의미가 있다(= 삭제). 켜는 방향은 무시한다 — 본딩은 호스트가
페어링해야 생기지 VIA 로 만들 수 없다. 그래서 "선택"은 드롭다운, "삭제"는 토글로 나눴다.

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
#### 키보드 `config.h` 는 **모든 TU 맨 앞에** 강제 include (`-include`)

`target_compile_options(app PRIVATE -include <kbd>/config.h)` — 실제 QMK 빌드 시스템과 같은 방식이다.

**문제**: quantum 헤더들이 여러 경로로 `eeconfig.h` 를 `config.h` 보다 먼저 끌어온다
(`via.h` 의 `#include "eeconfig.h"` 가 `#include "action.h"` → … → `config.h` 보다 먼저).
그 시점엔 `EECONFIG_USER_DATA_SIZE` 가 없어 `eeconfig.h` 의 `#ifndef → 0` 이 걸린다.

**실제 피해(정확히)** — 터진 버그가 아니라 **지뢰**였다:
- `EECONFIG_SIZE` 는 매크로라 **사용 시점**에 전개된다. 그땐 config.h 가 들어와 512 이므로
  **주소값 자체는 맞게 나온다**. `EECONFIG_USER_DATABLOCK` 은 USER_DATA_SIZE 와 무관.
- 실제로 깨진 건 `#if (EECONFIG_USER_DATA_SIZE) > 0` 가드 → **데이터블록 API 선언 누락**
  (implicit declaration). 시그니처가 우연히 맞아 동작은 했다.
- **잠재 위험**: config.h 를 전혀 안 거치는 TU 가 생기면 그 TU 만 0 으로 계산한다.

**baram-qmk 도 같은 상태다**(via.h 구조가 동일). 그쪽은 `hw_def.h` 로 config.h 를 모으지만
그건 **hw 계층용**이고 QMK 코어 TU 는 `_util.h`/`gpio.h`/`host.h` 의 개별 include 에 의존한다.
즉 baram 이 더 나은 게 아니라 안 고친 것 — upstream QMK 방식(`-include`)을 택했다.

> `add_compile_options()` 는 **안 먹는다** — Zephyr 의 `app` 라이브러리에 디렉터리 스코프 옵션이
> 붙지 않는다. 반드시 `target_compile_options(app ...)` 를 쓸 것. (`add_compile_definitions` 는 먹는다)

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

### 4.1.1 매트릭스 방향은 보드마다 다르다 — 틀리면 키맵이 **조용히 전치**된다

Zephyr `gpio-kbd-matrix` 의 명명은 QMK 와 무관하다: `col-gpios` = **구동 출력**(INPUT_ABS_X),
`row-gpios` = **입력 라인**(INPUT_ABS_Y). 그 구동/입력이 QMK 의 row 인지 col 인지는
**다이오드 방향이 정한다**:

| 보드 | 다이오드 | 구동(ABS_X) | 입력(ABS_Y) | config.h |
|---|---|---|---|---|
| wish60 | row2col | QMK **row** | QMK col | (기본) |
| wish65 | col2row | QMK **col** | QMK row | `MATRIX_DRIVE_IS_QMK_COL` |

**정반대다.** 예전엔 `port/matrix.c` 가 wish60 기준으로 박아뒀는데, 그대로 wish65 를 올리면
**컴파일은 되고 키맵이 전치되어** 나온다. 새 보드는 DTS 의 diode-direction 과 이 매크로가
맞는지 반드시 확인할 것.

**595 보드는 반드시 col2row 다** — 구조적 귀결이지 선택이 아니다. 595 는 출력 전용(읽기 불가)이라
반드시 구동측에 있어야 하고, 컬럼을 595 로 확장했으면 구동측 = col 이므로 다이오드는 col2row 다.

### 4.1.2 "보드에 없는 부품"은 DTS 노드를 빼면 끝나야 한다

새 보드가 공통 코드를 못 건드리게 하는 규약. `hw_def.h` 가 `DT_NODE_EXISTS` 로 기능을 켜고
(§7-A), **소비자 헤더도 같이 no-op 을 제공**해야 완성된다 — `log.h`(콘솔 없는 빌드)와
`led.h`(디버그 LED 없는 보드)가 그 패턴이다:

```c
#ifdef _USE_HW_LED
bool ledInit(void); void ledOn(uint8_t ch); ...
#else
#define ledInit()   (true)
#define ledOn(ch)   ((void)0)     // 호출부마다 #ifdef 를 두지 않기 위해
#endif
```

hw_def.h 만 고치고 헤더를 안 따라가면 **그 부품 없는 보드에서 컴파일이 깨진다**(wish65 를
넣을 때 실제로 겪었다 — `HW_LED_MAX_CH` 가 정의되지 않았다).

**실증**: wish65 는 디버그 LED 가 없다. DTS 에서 `leds` 노드를 빼는 것만으로 `ledInit`/
`ledToSleep` 심볼이 바이너리에서 사라졌고(nm 확인), ap.c/activity.c 의 호출부는 한 줄도
안 고쳤다.

**"보드마다 다를 수 있다"는 파일을 나눌 이유가 못 된다 — 먼저 무엇이 다른지 보라.**
Caps LED 를 두고 실제로 겪은 판단이다:

| 무엇이 다른가 | 어디서 해결하나 |
|---|---|
| 핀 번호 | DTS (`caps_led` 노드) |
| **극성**(반전 구동) | DTS (`GPIO_ACTIVE_LOW` -> `gpio_pin_set_dt` 가 `gpio_driver_data.invert` 로 뒤집는다) |
| 부품 유무 | DTS (노드를 뺀다 -> `DT_NODE_EXISTS` 로 컴파일아웃) |
| **로직 구조**(RGB 로 표시, NumLock 추가, 레이어 표시) | 키보드 트리 (`KBD_CUSTOM_INDICATOR` 로 opt-out) |

앞의 셋은 전부 DTS 몫이라 **공통 기본 구현 하나**(`port/led_port.c`)로 충분하다. 넷째만
키보드 트리가 필요하고, 그건 명시적 opt-out 으로 연다. 처음엔 "보드마다 다를 수 있으니"
키보드 트리에 뒀는데, 실제로 달랐던 것은 핀과 유무뿐이라 **같은 파일 두 개**가 됐다.

**약한 심볼로 기본값을 주면 안 된다** — QMK 가 이미 `led_update_kb` 를 weak 로 정의하므로
우리가 하나 더 두면 링커가 임의로 골라 **조용히 틀린다**. 그래서 `#ifndef` opt-out 을 쓴다.

**공통이 `keyboard_post_init_kb()` 를 가져가면 안 된다** — 그건 키보드 몫의 훅이라, 뺏으면
키보드가 인디케이터와 무관한 초기화를 할 자리를 잃는다. 핀 설정은 어차피 하드웨어
초기화이므로 `SYS_INIT(APPLICATION)` 으로 뺐다.

### 4.1.3 보드 사실은 **회로도로 확인**하라 — 참고 코드는 근거가 아니다

wish65 를 zmk-config 만 보고 만들었다가 회로도에서 두 개가 틀렸다:

| | zmk-config 로 추정 | 회로도(실제) |
|---|---|---|
| Caps LED | **없다**(ZMK 엔 `blue_led` 뿐) | **P1.09** (Q1 2N7002 -> LED1) |
| 디버그 LED | P1.09 | **없다** (LED3 는 충전기 CHRG 표시, MCU 무관) |
| UART | P0.06/P0.08 (추측) | **TX P0.05 / RX P0.04** |
| 32.768kHz | 있다고 **가정** | **X2 확인** (XL1/XL2, 12pF) |

ZMK 보드 정의는 **그 프로젝트가 쓰는 것만** 담는다 — 안 쓰는 부품은 없는 것처럼 보인다.
32.768kHz 도 ZMK 는 K32SRC 를 지정하지 않아 근거가 되지 못했다(크리스탈이 없으면 LFCLK 가
기동하지 않아 **BLE 가 아예 안 붙는다** — 가정으로 넘길 문제가 아니다).

### 4.2 시프트레지스터(74HC595)로 GPIO 확장 — wish65
wish65 는 GPIO 부족을 **595 2개 직렬(16핀)** 로 해결하고, **구동측(col)을 595 에, 읽기측(row)을 MCU GPIO** 에 둔다
(`diode-direction = "col2row"`). 이 구성이 Zephyr `gpio-kbd-matrix` 의 명명(col=출력, row=입력)과 **그대로 일치**한다.

- **네이티브 `ti,sn74hc595` 는 부족하다**: 바인딩이 `ngpios: const: 8`(1칩 고정), 드라이버도 `uint8_t output`
  → **직렬 연결 미지원**. wish65 의 16핀 불가.
- **ZMK `gpio_595.c`(221줄)는 이식 가능**: `zephyr/drivers/gpio.h` + `spi.h` 등 **안정 API 만** 사용
  (kscan 과 달리 제거된 서브시스템 의존 없음), `nwrite = ngpios/8` 로 직렬 지원.
- → **kscan 은 네이티브, 595 는 ZMK 것 이식** — 각각 나은 쪽을 취한다.

**구현됨** (Phase 7-B): `src/hw/driver/gpio_595.c` + `dts/bindings/gpio/baram,gpio-595.yaml`.
ZMK 원본에서 바꾼 것 — compatible(`baram,gpio-595`), API 등록(`DEVICE_API(gpio, ...)`; Zephyr 4.1 은
드라이버 API 를 iterable section 에 넣어 검증한다), 그리고 **원본 버그 하나**: drv_data 첫 멤버가
`struct gpio_driver_config` 였는데 GPIO 드라이버는 `struct gpio_driver_data` 여야 한다. 둘 다 첫
멤버가 `gpio_port_pins_t` 라 크기가 같아 우연히 동작하지만, `gpio_pin_set_dt()` 가 GPIO_ACTIVE_LOW
를 처리할 때 읽는 `invert` 필드가 후자에만 있다. ACTIVE_LOW 를 쓰는 순간 조용히 틀린다.

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
2. TX power 는 `bleSetTxPower(int8_t dbm)` 래퍼로 감싼다(구현 완료).
   > **정정**: 여기 원래 "런타임 TX power 는 `sdc_hci_cmd_vs_write_tx_power_level()` 로 **SDC 전용**"
   > 이라고 적었는데 **틀렸다**. 실제 API 는 **Zephyr 표준 VS HCI**
   > `BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL`(0xfc0e)이고, NCS SoftDevice Controller 도 이 명령을
   > 그대로 구현한다(`nrf/.../hci_internal.c`, `CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL` 로 게이팅).
   > Zephyr 자체 컨트롤러도 같은 명령을 지원하므로 **NCS 락인이 아니다**.
   > (Zephyr 샘플 `samples/bluetooth/hci_pwr_ctrl` 이 같은 패턴)
   래퍼는 여전히 유지한다 — VIA 핸들러가 HCI 를 직접 만지지 않게 하는 건 그 자체로 좋은 경계다.

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

호스트가 LED 리포트를 주기적으로 쓰면 여기에 ~11µA 가 더 붙는다 — §6.8.

### 6.8 호스트의 주기적 LED write 가 slave latency 를 무효화한다 (~11µA)

**증상**: 같은 배포 빌드인데 idle 이 57.41µA 로도, 62.76µA 로도 나온다. 코드 회귀가 아니다.

**정체**: 어떤 호스트는 LED output report 를 **값이 안 바뀌어도 정확히 3.000초 주기로** 쓴다
(실측: USB SET_REPORT 가 3.005/3.002/3.004s 간격, BLE 도 동일. USB 를 빼도 BLE 단독으로 계속됨).
우리 코드엔 3초짜리가 없다 — 주기 워크는 BAS(60초) 뿐이고 conn param 재시도는 5초×3회로 끝난다.

**왜 비싼가**: `led_outp_rep_handler` 가 `led_state != data` 에서 곧장 return 해도 **이미 늦었다**.
라디오는 그 전에 데이터 PDU 를 RX 하고 ACK 를 TX 했다. 게다가 **데이터를 받으면 slave latency 가
무효화된다** — peripheral 은 다음 연결 이벤트들을 건너뛰지 못한다. PPK2 로 51.2ms 창을 잡으면
**11.25ms(=connection interval) 간격의 연속 3발**이 보인다. 341ms 간격이 아니다.
10초 뷰에서 "크고 넓은 스파이크" 하나로 뭉개져 보이던 게 실은 이 3연발이다.

**수지** (interval 11.25ms / latency 30 기준):

| 항목 | 계산 | 기여 |
|---|---|---|
| 평상시 연결 이벤트 | ~6µC ÷ 0.341s | ~18µA |
| **호스트 LED write** | **34.26µC ÷ 3s** | **~11µA** |
| MCU 슬립 바닥 | — | ~30µA |
| | | **≈60µA** (실측 62.76µA) |

**대응: 없다. 우리가 고칠 게 아니다.** 3초마다 쓸지는 호스트가 정하고, LED output report 를
없앨 수도 없다(Caps LED 가 그걸로 온다). output report 특성에서 Write Request 를 빼고 Write
Without Response 만 노출하면 응답 TX 가 빠지지만, HIDS 스펙이 둘 다 요구하고 `bt_hids` 가
관리하는 영역이라 호환성 위험 대비 이득이 미미하다.

**측정할 때 기억할 것**: idle 전류를 비교하려면 **같은 호스트·같은 세션에서 번갈아** 재라.
호스트가 바뀌면 5~11µA 는 그냥 흔들린다. 그리고 스파이크 없는 평탄 구간만 선택하면
MCU 슬립 바닥이 나오는데, 그게 같으면 차이는 전부 라디오/호스트 몫이다.

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

### 6.7 인디케이터 LED (Caps Lock)

QMK 가 배선을 다 해준다 — `keyboard_task()` → `led_task()` → `host_keyboard_leds()` 로 호스트
LED 리포트를 읽고, 바뀌면 `led_update_kb()`(weak)를 부른다. `host_keyboard_leds()` 는 활성
host_driver 를 타므로 **USB/BLE 어느 쪽이든 그대로 동작**한다(driver_usb/driver_ble 의 keyboard_leds).

- **핀 (실보드 기준)**: 디버그 LED = **P0.31**(`led`, hw/driver/led.c), Caps = **P0.05**(`caps_led`).
  **ZMK wish60.dts 의 `blue_led`(P0.31)를 Caps 로 베끼면 안 된다** — 그건 디버그용이고 Caps 는 별개 부품이다.
- **드라이버를 나눠 둔다**: `led.c` 는 개발용(하트비트)이고 Caps 는 키보드 기능이라 수명이 다르다.
  Caps 는 `led_port.c` 가 `GPIO_DT_SPEC_GET(caps_led)` 로 직접 소유한다. DTS 도 노드를 분리
  (`indicator_leds`). `compatible` 은 `gpio-leds` — `gpio-keys` 를 컨테이너로 쓰면 입력 드라이버가
  붙어 `zephyr-code` 를 요구한다. `CONFIG_LED` 를 안 켜므로 LED 서브시스템은 안 붙고 DT 매크로만 쓴다.
- **미설정 핀은 µA 를 샌다**: Caps 핀을 추가하고 `GPIO_OUTPUT_INACTIVE` 로 구동했더니 **대기 전류가
  10µA 이상 줄었다**(실측). 플로팅 입력에 LED 회로가 물리면 핀이 중간 전압에 떠서 입력 버퍼에
  관통 전류가 흐른다. **보드의 미사용/미설정 핀은 명시적으로 구동할 것.**
- 구현: `keyboards/<kbd>/port/led_port.c` 에서 `led_update_kb()` 오버라이드.
  **ramune60(upstream QMK, baram 보드)과 같은 구조** — 차이는 GPIO 접근뿐이다
  (ramune60: QMK `gpio_write_pin(GP7)` / 우리: Zephyr `gpio_dt_spec` 기반 `ledOn/ledOff`).
  보드마다 인디케이터 구성이 다르므로 키보드 트리에 둔다.
- **슬립 시 소등은 이미 된다**: USB 서스펜드 → `suspend_power_down_quantum()` → `led_suspend()`
  → `led_set(0)` → `led_update_kb({0})`. deep sleep → `activityEnterSleep()` → `ledToSleep()`.
- **전력**: 켜져 있으면 ≈1.2mA(실측) — idle 57µA 의 **20배**. Caps Lock 은 평소 꺼져 있어
  괜찮지만 "항상 켜두는 인디케이터"를 추가할 땐 이 숫자를 기억할 것.

> **LED 리포트는 비동기다**: `led_task()` 는 `host_keyboard_leds()` 를 **폴링**하는데 호스트의
> LED 리포트는 언제든 온다. 루프가 idle 로 자고 있으면 반영이 안 된다(LED 가 안 켜짐).
> → USB: `usbHidSetKbdLedFunc(qmkWake)` / BLE: `led_outp_rep_handler` 에서 `qmkWake()`.
> §2.6 폴링 모델의 또 다른 청구서.

> **버그 하나 발견/수정**: `ledToSleep()` 이 `nrf_gpio_cfg_default(led_tbl[i].pin)` 을 불렀는데,
> `HW_TYPE_DT` 항목은 `.pin` 이 0 이다(핀 정보는 `h_dt` 안). 즉 LED 가 아니라 **P0.00 = XL1
> (32.768kHz 크리스탈)** 을 리셋하고 있었다. System OFF 직전에만 불려 실피해는 없었지만,
> 다른 데서 부르면 LFXO 가 깨진다 → DT 항목은 `gpio_pin_configure_dt(..., GPIO_DISCONNECTED)` 로.

**부트로더 키**: 추가 작업 불필요. VIA 기본 키코드 `QK_BOOT` → `process_record_quantum` 의
`QK_BOOTLOADER` → `reset_keyboard()` → `bootloader_jump()`(GPREGRET 0x57 + reboot, §2.3).
VIA 의 SYSTEM > BOOT 토글(`sys_port.c`)도 같은 일을 한다.

### 6.6 네오픽셀 / ext-power (Phase 8)

**ZMK 의 `zmk,ext-power-generic` 을 흡수하지 않았다.** 그건 본질적으로 "GPIO 로 켜고 끄는 고정
레귤레이터"라 **Zephyr 네이티브 `regulator-fixed`** 로 충분하다(`regulator_enable/disable`).
kbd-matrix(§2.4) / MAX17048(§6.3) / ws2812 와 같은 판단 — **흡수 코드 0**.

| | 우리 선택 | ZMK |
|---|---|---|
| 전원 레일 | `regulator-fixed` (네이티브) | `zmk,ext-power-generic` (자체) |
| 네오픽셀 | `worldsemi,ws2812-spi` (네이티브) | 같음 |

- **레일은 부팅 시 꺼진 상태**(`regulator-boot-on` 안 줌). RGB 를 켤 때만 올린다.
  네오픽셀은 **검은색을 표시해도 컨트롤러가 개당 ~0.7mA** — 16개면 ≈11mA 로 idle 80.9µA 의 **140배**다.
- `startup-delay-us = 1000` — 레일이 올라오기 전에 SPI 를 쏘면 첫 프레임이 깨진다.
- `ws2812Refresh()` 는 레일이 내려가 있으면 **전송을 건너뛴다**(SPI 만 깨워 전력을 먹으므로).
- wish60/wish65 둘 다 이 레일은 **네오픽셀만** 끊는다(595 는 상시 전원) — 확인됨(§4.2).
- `HW_WS2812_MAX_CH` 와 DTS `chain-length` 는 **일치해야 한다**.

**RGBLIGHT 가 아니라 RGB_MATRIX 를 쓴다.**

| | RGBLIGHT | **RGB_MATRIX (채택)** |
|---|---|---|
| LED 모델 | 선형 체인(인덱스) | **물리 좌표(x,y) + 플래그** |
| 효과 | 인덱스 기반 | **위치 기반**(splash/gradient/reactive) |
| VIA | 채널 2 | **채널 3** (둘 다 자동) |

언더글로우여도 RGB_MATRIX 가 표현이 낫고, **baram 의 upstream QMK 보드(ramune60)도 언더글로우에
rgb_matrix 를 쓴다**(`keyboard.json` 의 `rgb_matrix.layout`, flags=2). 일관성 면에서도 맞다.

- `g_led_config` 는 **C 로 직접 쓴다** — ramune60 은 `keyboard.json` → python 빌드 시스템이
  생성하지만 우리는 그 빌드 시스템을 안 쓴다. 좌표는 60% 외곽 균등 배치(224x64 좌표계, 확인됨).
- QMK 코어가 `rgb_matrix_init()`(keyboard.c:433) / `rgb_matrix_task()`(682)를 **자동 호출**한다 → 배선 공짜.
- `lib8tion`(QMK 8비트 고정소수점 수학)이 필요하다 → upstream QMK 에서 `src/lib/lib8tion` 으로 vendor-in.
  `quantum/rgb_matrix/animations/runners` 도 include 경로에 넣어야 한다(`rgb_matrix_runners.inc`).
- 순정 `rgb_matrix_drivers.c` 는 **컴파일하지 않는다** — QMK 내장 ws2812 드라이버를 전제한다.
  키보드별 `driver/rgb_matrix_drivers.c` 가 우리 `ws2812.c`(→Zephyr led_strip)로 연결한다.

**레일은 RGB on/off 를 자동으로 따라간다**(`flush()` 에서). "끈다"가 색만 검게 하는 것이면
컨트롤러 11mA 가 그대로 남는다 — **레일까지 내려야** 0 이 된다. 끌 때는 **검은색을 먼저 쏘고**
레일을 내린다(순서를 바꾸면 LED 가 마지막 색을 붙든 채 꺼져 다시 켤 때 번쩍인다).

**함정 3 — VIA 내장 `qmk_rgb_matrix` 메뉴는 효과 이름이 어긋난다.**
QMK 의 효과 enum 은 **켠 것만** 들어가 인덱스가 압축된다(`rgb_matrix_effects.inc` 가
`ENABLE_RGB_MATRIX_*` 로 가드됨). VIA 내장 메뉴는 **47개 전체**의 고정 목록이라 부분만 켜면
이름과 실제 동작이 어긋난다.
→ ramune60 처럼 **자체 Lighting 메뉴**에 목록을 직접 쓴다. 인덱스는 `.inc` 순서:
  `0=All Off, 1=Solid Color, 2~ = 켠 애니메이션`(config.h 의 정의 순서가 아니라 `.inc` 순서).
  **JSON 드롭다운 개수 = 2 + ENABLE 개수** 여야 한다.

**함정 4 — VIA 로 RGB 를 켜도 불이 안 들어온다.**
루프가 idle 이면 `qmkWaitActivity()` 에서 블록 중이다. VIA 가 `rgb_matrix_enable_noeeprom()`
을 불러도 **루프가 안 깨면 `rgb_matrix_task()` 가 안 돌아** flush 가 없고 레일도 안 올라온다.
→ `qmkWake()` 를 `via_hid_receive()` 에서 호출한다. **키 입력 말고도 루프를 돌려야 하는 경로가
  있으면 반드시 깨워야 한다** — §2.6 의 폴링 모델이 만든 또 하나의 함정.
  (`qmkWake()` 는 `last_activity_ms` 를 건드리지 않는다 — 사용자 입력이 아니므로 idle/sleep
   타이머를 리셋하면 안 된다)

**밝기 상한은 보드별 노브다.** `RGB_MATRIX_MAXIMUM_BRIGHTNESS` 가 키보드 `config.h` 에 있는
이유가 그것 — 보드마다 전력 예산이 다르다. VIA 는 0~255 를 `scale8(v, MAXIMUM_BRIGHTNESS)` 로
스케일하므로 슬라이더는 항상 **"이 보드가 허용하는 범위의 0~100%"** 를 뜻한다(낮춰도 정상 동작).
wish60 은 255(ramune60 과 동일) + `RGB_MATRIX_DEFAULT_VAL 60` 으로 기본값만 낮게.
16개 풀 화이트는 개당 ~60mA → **약 1A**, 배터리로는 40분 남짓이다.

**함정 0 — `CONFIG_PM_DEVICE_RUNTIME` 은 절대 켜지 말 것 (키보드가 죽는다).**

SPI 하나 잡으려고 켰다가 **키가 아예 안 먹었다.** `input_kbd_matrix_common_init()` 이
`pm_device_runtime_enable(dev)` 를 부르기 때문에 **키 매트릭스가 런타임 PM 대상이 되고**,
아무도 `pm_device_runtime_get()` 을 하지 않으니 suspend 된 채 스캔 스레드가 안 돈다.
전력도 같이 나빠졌다: **idle 69µA → 652µA**(워크큐 + 딸려오는 노드들).
`zephyr,pm-device-runtime-auto` 를 spi3 에만 줘도 소용없다 — 켜지는 건 **전역 스위치**다.

> **Phase 6 의 UART 런타임 PM(1.21→1.98mA) 과 정확히 같은 함정을 반복했다.**
> 문서에 "런타임 게이팅은 실패했다"고 적어두고도 또 밟았다.
> **교훈: 런타임 PM 전역 스위치를 켜지 말고, 아는 지점만 `pm_device_action_run()` 으로 직접 건드린다.**
> (`CONFIG_PM_DEVICE` 만 켜는 건 안전하다 — 실측 57µA. `pm_device_runtime_enable()` 은
>  RUNTIME 이 꺼져 있으면 0 을 반환하는 스텁이라 드라이버 init 도 안 깨진다.)

**함정 1 — SPIM3 가 RGB 를 꺼도 ~1mA 를 계속 먹는다 (실측).**
`spi_nrfx_spim.c` 는 **첫 전송 때** `configure()` 안에서 `nrfx_spim_init()` 하고 그 뒤
**계속 ENABLE 로 남는다**. `CONFIG_PM_DEVICE` 가 없으면 uninit 될 기회가 없다.
게다가 spi3 = **SPIM3 = nRF52840 anomaly 195**("SPIM3 continues to draw current after disable")
대상이고, nrfx 의 workaround 는 `nrfx_spim_uninit()` 때 적용되므로 **uninit 을 해야 효과가 난다**.
→ **`CONFIG_PM_DEVICE=y` 만** 켜고, RGB on/off 시점에 `pm_device_action_run(spi3, RESUME/SUSPEND)`
   를 직접 부른다(`rgb_matrix_drivers.c`). 런타임 PM 은 함정 0 참고 — 쓰면 안 된다.

> RGB 를 **한 번도 안 쓰면** SPI 는 초기화조차 안 된다(전송이 없으므로) → 그때 idle 은 69µA.
> 즉 이 문제는 "RGB 를 켰다 끈 뒤"에만 나타난다.

**함정 2 — RGB 애니메이션이 첫 프레임에서 멈춘다.**
`ap.c` 는 idle 이면 `qmkWaitActivity()` 에서 무한 블록한다(§2.6). RGB 는 주기적으로 프레임을
그려야 하므로 그대로 두면 패턴의 첫 프레임만 나오고 정지한다. **§2.6 의 "QMK 폴링 vs ZMK 이벤트"
가 RGB 에서 다시 나타나는 지점.**
→ `qmkGetIdleWaitMs()` 가 RGB 가 켜져 있으면 **task 주기(`QMK_TASK_PERIOD_MS`)**로 대기를 자른다.

> **`RGB_MATRIX_LED_FLUSH_LIMIT`(16ms)를 쓰면 안 된다** — 처음에 그렇게 했다가 브리딩이 뚝뚝
> 끊겼다. 그건 **프레임 주기**지 task 주기가 아니다. `rgb_matrix_task()` 는 상태머신이고
> RENDERING 이 `RGB_MATRIX_LED_PROCESS_LIMIT`(기본 (COUNT+4)/5 = 4)개씩 청크로 돈다 →
> 한 프레임에 **7번쯤** 불려야 한다. 16ms 로 깨우면 프레임당 ~112ms = **약 9fps**.
> 프레임 주기 제한은 `rgb_task_sync()` 가 알아서 건다.
> 추가로 `RGB_MATRIX_LED_PROCESS_LIMIT = LED_COUNT` 로 두면 호출 수가 7 → 4 로 준다(16개는 싸다).
> 전력: RGB 자체가 10mA 단위라 이 웨이크업 비용은 묻히고, RGB 를 끄면 원래대로 돌아간다.

**USB 서스펜드**: 호스트가 자면 `USBD_MSG_SUSPEND` → `suspend_power_down_quantum()` →
`rgb_matrix_set_suspend_state(true)` → 소등 + flush → 레일 down. usbd_next 메시지가 그동안
로그만 찍고 버려지고 있었다 — `usbSetSuspendFunc()` 로 연결했다(hw 가 QMK 를 직접 부르지 않게 콜백).

**하드웨어 먼저 검증**: QMK 를 얹기 전에 `ws2812 color/off` CLI 로 레일·색순서·개수를 확인한다. 여기서 틀리면 DTS(`color-mapping`, `chain-length`, MOSI 핀) 문제지 QMK 문제가 아니다.

### 6.10 RGB 전력 실측 (Phase 8-D) — 배터리 1000mAh 기준

wish60, 네오픽셀 16개, BLE 연결, USB 미연결. PPK2 10초 평균.

| 상태 | 실측 | 1000mAh 지속 | 비고 |
|---|---|---|---|
| **RGB 켬** — breathing, `VAL 60` | **65.08mA** (max 205.45mA) | **~15시간** | 브리딩은 밝기를 0~설정값으로 훑어 평균이 정적 효과의 절반쯤 |
| RGB 검정 + **레일 ON** | 7.12mA | ~5.9일 | **버그였다**(§6.9 함정 3). 네오픽셀 대기 전류 16 × ~0.4mA |
| RGB 끔 / idle 소등 (레일 OFF) | ~60µA | ~2년 | 자가방전이 먼저다 |
| deep sleep (System OFF) | 27.7µA | — | 이 중 ~25µA 는 MAX17048 |

**RGB 가 idle 의 약 1000배다.** 레일 게이팅(§6.6)과 idle 자동 소등(§6.9)이 선택이 아니라
필수인 이유다. 무선에선 호스트가 USB SUSPEND 를 안 보내므로 activity IDLE 이 유일한 방어선이다.

**예상 사용 시간** — 하루 8시간 사용 + 16시간 방치, `(8h × 사용전류 + 16h × 60µA) / 24h`:

| 사용 방식 | 평균 전류 | 예상 지속 |
|---|---|---|
| **RGB 끔** | ~0.37mA | **~110일** |
| RGB 켬 (`VAL 60`) | ~21.7mA | **~2일** |
| RGB 켬 (`VAL 255`) | ~85mA (외삽) | **~12시간** |

방치 16시간이 기여하는 건 1mAh 도 안 된다 — **사실상 RGB 만이 변수다.** 끄면 배터리를 신경 쓸
필요가 없고(자가방전이 먼저다), 켜면 하루~이틀짜리 기기가 된다. ZMK 무선 키보드들이 언더글로우를
기본 OFF 로 두는 이유이고, 우리도 `RGB_MATRIX_DEFAULT_ON false` 다.

이 표의 신뢰도를 낮추는 것들:
- **"타이핑 중 평균 전류"는 미측정이다.** 위에서 1mA 로 가정했다. 실측 2.43mA 는 **키를 계속
  누르고 있을 때** 값이고(§6.4), 실제 타이핑은 키 사이에 루프가 idle 로 빠지므로 60µA~2.43mA
  사이 어딘가다.
- **1000mAh 를 다 못 쓴다.** 보호회로 컷오프와 전압 강하로 실사용은 보통 80~90% → 0.85 를 곱할 것.

**밝기 상한은 255 로 열어둔다**(사용자 결정, 배터리 1000mAh). `VAL 255` 정적 효과는 **미측정**이며,
선형 외삽하면 ~550mA / ~1.8시간이지만 **신뢰하지 말 것** — 네오픽셀은 컨트롤러 대기 전류가
오프셋으로 깔리고 LED 도 완전 선형이 아니다. 피크가 800mA 를 넘기면 작은 LiPo 는 전압 강하로
보호회로가 끊기거나 브라운아웃이 날 수 있다. 그때 낮추라고 `RGB_MATRIX_MAXIMUM_BRIGHTNESS`
가 보드별 노브로 있다(§6 밝기 상한 논의).

### 6.9 RGB/인디케이터 소등 — 조건이 둘이므로 한 곳에서 합친다

**끄는 이유가 두 개다**: 호스트 PC 가 잠(USB SUSPEND) / 사용자가 자리를 뜸(activity IDLE).
무선에선 호스트가 SUSPEND 를 보내주지 않으므로 **IDLE 이 유일한 방어선**이다 — 없으면 RGB 가
켜진 채 mA 단위로 배터리를 태운다(idle 60µA 와 비교가 안 된다).

**각자 부르면 안 된다.** 둘이 서로를 모른 채 `suspend_power_down_quantum()` /
`suspend_wakeup_init_quantum()` 을 부르면, 한쪽 조건이 풀릴 때 다른 쪽 조건이 살아 있어도
켜버린다(PC 가 깨어났지만 여전히 idle → RGB 점등). `qmkSuspendUpdate()` 하나로 OR 해서
**전이할 때만** 부른다. QMK 의 `suspend_power_down_quantum()` 이 RGB 와 인디케이터
(`led_suspend()`)를 다 처리하므로 우리가 따로 할 일은 없다.

**함정 1 — `activityGetState()` 는 활성 구간에서 낡는다.** `activityUpdate()` 는 ap.c 의 idle
분기에서만 불린다. IDLE 상태에서 키를 누르면 루프가 활성 분기로 빠지고 `activityUpdate()` 가
영영 안 불려 state 는 IDLE 로 굳는다 → **RGB 가 안 돌아온다**. 계산 전용 `activityIsIdle()`
(inactive ms 로 직접 판정)을 쓸 것.

**함정 2 — IDLE 진입은 `activityUpdate()` 안에서 반영해야 한다.** 그 함수는
`qmkWaitActivity()` **직전에** 불린다. 다음 `qmkUpdate()` 로 미루면 그 "다음"이 sleep
데드라인(1시간) 뒤일 수 있다(RGB 가 꺼져 있으면 2ms 웨이크가 없으므로). §2.6 계열.

**함정 3 — `rgb_matrix_is_enabled()` 는 "지금 켜져 있나"가 아니다.** 사용자 설정일 뿐이라
서스펜드로 검게 표시 중이어도 true 다. 이 표현식을 그대로 쓰면 두 곳이 동시에 깨진다:

- `qmkGetIdleWaitMs()` — 소등 뒤에도 2ms 마다 깨어나 ~1mA 를 계속 먹는다.
  USB 서스펜드 전류 상한(2.5mA)에도 걸린다.
- `rgb_matrix_drivers.c` 의 flush — **ext-power 레일이 안 내려간다**. 네오픽셀은 검정을
  표시해도 컨트롤러가 개당 ~0.4mA 라 16개면 **실측 7.12mA** — idle 60µA 의 100배다.
  "LED 는 꺼졌는데 전류가 mA 단위"면 이거다.

**함정 4 — flush 안에서는 `rgb_matrix_get_suspend_state()` 도 못 쓴다.**

```c
void rgb_matrix_set_suspend_state(bool state) {
    if (state && !suspend_state) {
        rgb_task_render(0);
        rgb_task_flush(0);      // <- 우리 flush 가 여기서 불린다
    }
    suspend_state = state;      // <- 플래그는 그 **뒤에** 세워진다
}
```

검은 프레임을 쏘는 그 flush 안에서 `get_suspend_state()` 는 아직 false 다. 그리고 이게
검정을 쏘는 **유일한** 기회다(그 뒤 루프는 잔다 — 함정 3 을 고쳤으므로 2ms 웨이크도 없다).
그래서 `qmkIsSuspended()`(우리 arbiter 플래그)를 쓴다. `qmkSuspendUpdate()` 가
`suspend_power_down_quantum()` **전에** 플래그를 세우는 것이 그 계약이다 — 순서를 바꾸지 말 것.

**순서**: `qmkUpdate()` 에서 `qmkSuspendUpdate()` 는 `output_select_task()` **뒤**여야 한다.
`suspend_wakeup_init_quantum()` → `led_wakeup()` → `led_set(host_keyboard_leds())` 가 활성
host_driver 를 타기 때문이다.

**인디케이터도 같이 끈다**(IDLE 에서 Caps LED 소등). "상시 점등 LED 없음" 원칙과 같은 이유다 —
인디케이터 LED 는 mA 단위라 한 시간이면 idle 60µA 가 무의미해진다. 키를 건드리면
`led_wakeup()` 이 호스트 LED 상태를 그대로 복구하므로 정보가 사라지지는 않는다.

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

### 6.4 키 눌림 중 전류 — 주기 2배마다 ~1mA (실측 3점)

키를 누르고 있는 동안(worst case, 연속 누름). **QMK 폴링 모델의 구조적 비용**이고
ZMK(완전 이벤트 구동)보다 높다. 스캔 주기와 QMK 처리 주기를 **함께** 바꾸며 측정:

| 주기 | 전류 | USB 리포트율 |
|---|---|---|
| 1ms | 3.40 mA | 1000 Hz |
| **2ms (채택)** | **2.43 mA** | 500 Hz |
| 4ms | 1.40 mA | 250 Hz |

**주기 2배마다 ~1mA씩** 줄어든다(0.97 / 1.03). 2점(1·2ms)으로 세운 `base + k/T` 모델은
4ms 를 1.95mA 로 예측했으나 **실측 1.40mA — 모델이 틀렸다.** 특히 "1.46mA 불가피한 base"
라는 주장은 허구였다(4ms 가 이미 그 아래). **바닥이 어디인지 아직 모른다 — 외삽하지 말 것.**

**2ms 채택 근거**: BLE 는 연결 간격이 11.25ms 라 이 값과 **무관**(무손실). 손해는 USB 리포트율뿐.
4ms 가 전력은 더 좋지만 USB 250Hz 는 유선에서 체감될 수 있어 균형점으로 2ms.
배터리 환산(700mAh, 하루 3h 키눌림 가정): 1ms 59일 / 2ms 78일 / 4ms 118일.

**QMK 로직은 안전하다**(코드 확인): 디바운스 `sym_defer_pk` 는 호출 횟수가 아니라 **경과 ms**로
카운터를 깎고(`TIMER_DIFF_FAST`), `timer_read_fast()` → `k_uptime_get_32()` = ms 다.
태핑도 `timer_elapsed()` 기반. 즉 주기를 늘려도 granularity 만 거칠어진다(10ms 디바운스 → 10~12ms).

#### 두 개의 독립 노브 (같을 필요 없다)

| 노브 | 위치 | 의미 |
|---|---|---|
| `poll-period-ms` | **board DTS** (`kbd_matrix`) | 하드웨어 재스캔 주기 = 키 변화 감지 지연 |
| `QMK_TASK_PERIOD_MS` | **키보드 `config.h`** | QMK 처리 주기 = 디바운스/태핑 granularity |

각자 제집에 둔다. 둘 다 CPU 를 깨우므로 전력엔 둘 다 영향을 준다.
`config.h` 값이 `ap.c` 까지 닿는 건 §2.9 의 `-include` 덕분이다(값을 바꿔 바이너리가
달라지는 것으로 검증함 — 안 닿으면 기본값이 **조용히** 먹는다).

> 한 번 스캔/처리를 DTS 하나로 묶었다가 되돌렸다. 둘은 독립 변수라 묶으면 "스캔은 빠르게,
> 처리는 느리게" 같은 조합을 막는다. 중복이 아니므로 단일 소스로 만들 이유가 없다.

#### 남은 실험 (미시도)

`stable-poll-period-ms` — 드라이버가 "방금 바뀜(불안정)"과 "계속 눌린 채(안정)"를 구분한다.
우리가 측정한 "키 누른 채"가 정확히 stable 케이스이므로, `poll-period-ms=1` +
`stable-poll-period-ms=8` 이면 **USB 1000Hz 를 유지하면서** 이 전류를 잡을 수 있을지 모른다.
(`ap.c` 루프도 같이 물러나야 해서 추가 설계 필요)

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
