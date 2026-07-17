# qmk-zephyr

**QMK 키보드 로직을 Zephyr(nRF Connect SDK) 위에 올린 무선 키보드 펌웨어.**
VIA로 설정하고, BLE로 연결하고, 배터리로 오래 간다.

nRF52840 / nRF52833 기반. 현재 두 보드가 실기기에서 동작한다 — **wish60**(60% ANSI)과
**wish65**(65% ANSI).

---

## 왜 만들었나

무선 커스텀 키보드를 만들려면 지금은 둘 중 하나를 골라야 한다:

- **QMK** — 키보드 로직이 성숙하고 **VIA 생태계**가 있다. 그런데 유선 USB 전제 + 폴링 루프라
  **저전력이 약하다**.
- **ZMK** — Zephyr 기반 이벤트 구동이라 **저전력에 강하다**. 그런데 키맵 설정이 컴파일 기반이고
  VIA 같은 실시간 GUI 편집이 없다.

**둘 다 갖고 싶었다.** 그래서 하이브리드로 갔다 — 키보드 로직은 QMK를 그대로 쓰고, 하드웨어·전력·
무선은 Zephyr 네이티브로 다시 짰다.

결과는 이렇다:

| | 값 | 조건 |
|---|---|---|
| idle | **38~60µA** | BLE 연결, 키 없음 |
| deep sleep | **27.7µA** | System OFF (wish60. 이 중 ~25µA는 MCU가 아니라 실장된 MAX17048) |
| 타이핑 중 | 2.4~3.2mA | 키를 계속 누른 최악치 |
| RGB 켬 | 65mA | 네오픽셀 16개, breathing, 밝기 60/255 |

**1000mAh 배터리로 RGB를 끄면 ~110일, 켜면 ~2일이다.** 스캔이나 BLE가 아니라 **RGB만이 변수**다.

---

## 구조 — 세 계층

QMK 코어는 하드웨어를 **네 개의 심볼 그룹**으로만 요구한다. 각각을 Zephyr에 매핑했다.

```
QMK quantum/  (vendor-in, 우리는 고치지 않는다 · GPLv2)
   │ matrix          │ timer        │ eeprom          │ host_driver / raw_hid
port/  (우리가 쓴 어댑터)
   matrix.c          timer.c        eeprom.c          driver_usb.c / driver_ble.c
   │ input(kbd-matrix) │ k_uptime    │ emu-eeprom      │ usbd_next HID · BLE HOG
Zephyr 4.1 (NCS v3.1.0)
```

**핵심 원칙: QMK 트리를 수정하지 않는다.** 어댑터(`port/`)가 Zephyr 쪽으로 맞춰간다.
GPL 분기를 최소화하고 upstream 동기화를 쉽게 하기 위해서다.

**ZMK에서는 코드가 아니라 판단을 빌렸다.** activity 상태머신, 프로파일 방식 멀티 호스트, 전력
설계는 ZMK의 접근을 참고했지만 우리 스타일로 다시 썼다. 드라이버는 대부분 **Zephyr 네이티브**를
그대로 쓴다(`gpio-kbd-matrix`, `zephyr,emu-eeprom`, `worldsemi,ws2812-spi`, `regulator-fixed`).
ZMK에서 실제로 이식한 건 **74HC595 GPIO 확장 드라이버 하나**뿐이다 — 네이티브
`ti,sn74hc595`가 1칩 고정이라 직렬 연결을 못 해서.

---

## 키보드 하나 = 두 트리

하드웨어와 로직을 분리한다. **이름 규약이 둘을 잇는 유일한 장치**다.

```
boards/baram/wish65/                      ← 하드웨어 (Zephyr board)
  wish65.dts                                핀, 매트릭스, 파티션, 네오픽셀, 전원 레일
  wish65_defconfig                          클럭, DCDC, 장치 식별정보
                                       ↕ 이름으로 연결 (CMake 가 BOARD 에서 유도)
src/ap/modules/qmk/keyboards/baram/wish65/ ← 로직 (QMK · GPLv2 격리)
  config.h                                  매트릭스 크기, 이름, VID/PID, RGB 설정
  keymap.c  json/WISH65-VIA.JSON            키맵, VIA 레이아웃
```

**이 구조가 실제로 검증됐다.** 두 번째 보드(wish65)를 넣을 때 **공통 코드를 건드리지 않았다** —
SoC가 다르고(nRF52833), 컬럼을 시프트레지스터로 확장하고, 매트릭스 방향이 반대고, LED 개수도
다른데도. "보드에 그 부품이 없다"는 **DTS에서 노드를 빼는 것만으로** 끝난다.

---

## 만든 것

**키보드 기본** — 유선/무선 타이핑, 6KRO, 미디어·시스템 키, VIA 실시간 키맵 편집(전원 재인가 후 유지),
Caps Lock 인디케이터, 부트로더 진입 키

**무선** — BLE HID, **호스트 5대 프로파일 전환**(ZMK 방식 — 여러 호스트가 동시에 연결된 채로 있고
전환이 즉시), USB/BLE 자동 전환, 배터리 잔량 보고, TX power 조절

**저전력** — activity 상태머신(ACTIVE/IDLE/SLEEP), deep sleep(System OFF, 키 입력으로 웨이크),
RGB/인디케이터 자동 소등, 콘솔 빌드타임 분리

**RGB** — QMK `RGB_MATRIX`(RGBLIGHT 아님 — 위치 기반 효과), 17가지 효과, 전원 레일 게이팅

**VIA 커스텀 메뉴** — 프로파일 전환/본딩 관리, 전력 타임아웃, TX power, 런타임 디바운스 시간,
`HOLD_ON_OTHER_KEY_PRESS` 토글

---

## 빌드

NCS v3.1.0이 `/opt/nordic/ncs/v3.1.0`에 있다고 전제한다.

```bash
cd firmware/nrf52-qmk-fw

./build.sh                 # wish60 릴리스
./build.sh wish65          # wish65 릴리스
./build.sh wish65 -d       # 개발 빌드 (UART 콘솔 + CLI)
./build.sh --help

./release.sh               # 배포 패키지 → output/<board>-<ver>.zip
```

산출물은 `build/<board>[-debug]/zephyr/zephyr.uf2`.
**플래시: 더블탭 리셋 → UF2 매스스토리지에 드래그드롭.** (Adafruit nRF52 UF2 부트로더가
보드에 미리 들어있다고 전제한다 — 부트로더는 빌드하지 않는다)

> **개발 빌드는 배터리 측정에 쓰면 안 된다.** UART 콘솔이 ~1.2mA를 먹는다 — idle의 20배다.

VS Code nRF Connect 확장으로 빌드한다면 **"Use sysbuild"를 반드시 꺼야 한다.**
켜져 있으면 빌드는 성공하는데 **부팅하지 않는다**(자세한 건 문서 §7.1).

---

## 문서

**[`firmware/nrf52-qmk-fw/docs/PORTING-NOTES.md`](firmware/nrf52-qmk-fw/docs/PORTING-NOTES.md)** —
이 프로젝트의 진짜 알맹이다. 결정의 근거, 실측 전력, 그리고 **실기기에서 겪은 함정들**이
왜 그랬는지와 함께 적혀 있다.

몇 가지만 맛보기로:

- **콘솔 하나가 idle 전류를 14배로 만들었다** — nRF52840 UARTE는 RX가 켜져 있으면 HFCLK를 계속
  잡는다. 런타임 게이팅은 오히려 악화됐고, 빌드타임 제거가 답이었다.
- **`CONFIG_PM_DEVICE_RUNTIME`이 키보드를 죽였다** — 매트릭스 드라이버가 자기를 suspend 했다.
  전력도 10배 나빠졌다.
- **QMK 폴링 vs ZMK 이벤트** — 이 하이브리드의 본질적 난제다. "루프가 자고 있으면 반영이 안 된다"는
  버그가 여섯 번쯤 다른 얼굴로 나왔다.
- **ZMK의 30초 타임아웃을 용도도 모르고 베껴왔다** — 그건 OLED 끄기용이었고 RGB와 무관했다.
- **참고 구현은 근거가 아니다** — zmk-config만 보고 wish65에 Caps LED가 없다고 단정했는데, 회로도엔
  있었다. ZMK 보드 정의는 그 프로젝트가 쓰는 것만 담는다.

측정으로 뒤집힌 가설, 지어낸 근거를 정정한 기록도 그대로 남겼다. 다음에 같은 함정을 밟지 않으려고
쓴 문서다.

---

## 라이선스

**GPL v2** — [LICENSE](LICENSE) (upstream QMK와 동일한 전문).

`src/ap/modules/qmk/quantum/`은 [QMK Firmware](https://github.com/qmk/qmk_firmware)에서
가져온 GPLv2 코드다. 결합 저작물인 이 펌웨어 전체가 **GPLv2**로 배포된다.

> 초기엔 루트 LICENSE가 MIT였는데, QMK를 vendor-in 한 이상 사실과 달랐다. 배포를 시작하며 바로잡았다.

**참고/차용한 프로젝트:**
- [QMK Firmware](https://github.com/qmk/qmk_firmware) (GPL-2.0) — 키보드 로직 코어
- [ZMK](https://github.com/zmkfirmware/zmk) (MIT) — 저전력·무선 설계 참고, 74HC595 드라이버 이식
- [Zephyr / nRF Connect SDK](https://github.com/nrfconnect/sdk-nrf) (Apache-2.0) — RTOS·드라이버
