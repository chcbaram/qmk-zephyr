<div align="center">

# qmk-zephyr

**QMK 키보드 로직 + Zephyr = VIA 되는 저전력 무선 키보드 펌웨어**

[![License](https://img.shields.io/badge/license-GPL--2.0-blue.svg)](LICENSE)
[![MCU](https://img.shields.io/badge/MCU-nRF52840%20%7C%20nRF52833-9cf.svg)](#)
[![Zephyr](https://img.shields.io/badge/Zephyr-4.1%20·%20NCS%20v3.1.0-7f52ff.svg)](https://github.com/nrfconnect/sdk-nrf)
[![VIA](https://img.shields.io/badge/VIA-supported-success.svg)](https://usevia.app)
[![idle](https://img.shields.io/badge/idle-38µA-brightgreen.svg)](#-실측-전력)

`wish60` 60% ANSI &nbsp;·&nbsp; `wish65` 65% ANSI &nbsp;— 둘 다 실기기 검증 완료

</div>

---

## 🎯 왜 하이브리드인가

|  | 강점 | 약점 |
|:--|:--|:--|
| **QMK** | 키보드 로직, **VIA 생태계** | 유선 전제 + 폴링 루프 → 저전력 약함 |
| **ZMK** | 이벤트 구동 → 저전력 강함 | 컴파일 기반 키맵, VIA 없음 |

> **로직은 QMK, 하드웨어·전력·무선은 Zephyr 네이티브.**

<br>

## ⚡ 실측 전력

| 상태 | 값 | 조건 |
|:--|--:|:--|
| **idle** | `38~60 µA` | BLE 연결, 키 없음 |
| **deep sleep** | `27.7 µA` | System OFF <sub>(wish60. ~25µA는 MCU 아닌 실장 MAX17048)</sub> |
| **타이핑 중** | `2.4~3.2 mA` | 키 계속 누른 최악치 |
| **RGB 켬** | `65 mA` | 네오픽셀 16개, breathing, 밝기 60/255 |

> 🔋 **1000mAh 기준 — RGB 끄면 `~110일`, 켜면 `~2일`.**
> 스캔·BLE가 아니라 **RGB만이 변수**다.

<br>

## 🏗 구조

```
QMK quantum/  (vendor-in · GPLv2 · 우리는 고치지 않는다)
   │ matrix            │ timer        │ eeprom         │ host_driver / raw_hid
   ▼                   ▼              ▼                ▼
port/  (우리가 쓴 어댑터)
   matrix.c            timer.c        eeprom.c         driver_usb.c / driver_ble.c
   │ input(kbd-matrix) │ k_uptime     │ emu-eeprom     │ usbd_next HID · BLE HOG
   ▼                   ▼              ▼                ▼
Zephyr 4.1 (NCS v3.1.0)
```

- **QMK 트리 무수정** — 어댑터가 Zephyr 쪽으로 맞춘다 → GPL 분기 최소화 + upstream 동기화
- **드라이버는 Zephyr 네이티브** — `gpio-kbd-matrix` · `zephyr,emu-eeprom` · `worldsemi,ws2812-spi` · `regulator-fixed`
- **ZMK에서 빌린 건 판단** — activity 상태머신, 프로파일 멀티호스트, 전력 설계 <sub>(코드는 재작성)</sub>
- **ZMK에서 이식한 코드는 하나** — 74HC595 GPIO 확장 <sub>(네이티브 `ti,sn74hc595`는 1칩 고정이라 직렬 불가)</sub>

<br>

## 🧩 키보드 하나 = 두 트리

```
boards/baram/wish65/                        ← 하드웨어 (Zephyr board)
  wish65.dts  wish65_defconfig                핀 · 매트릭스 · 파티션 · 전원 · 클럭
                    ⇕  이름으로 연결 (CMake가 BOARD에서 유도)
src/ap/modules/qmk/keyboards/baram/wish65/  ← 로직 (QMK · GPLv2 격리)
  config.h  keymap.c  json/WISH65-VIA.JSON    매트릭스 · VID/PID · 키맵 · VIA 레이아웃
```

- ✅ **검증됨** — wish65 추가 시 **공통 코드 무수정**. SoC 다름(nRF52833), 컬럼을 시프트레지스터로 확장,
  매트릭스 방향 반대, LED 개수 다름에도
- 💡 **"보드에 그 부품이 없다" = DTS에서 노드를 뺀다** — 그걸로 끝

<br>

## ✨ 기능

| | |
|:--|:--|
| **기본** | 유선/무선 타이핑 · 6KRO · 미디어/시스템 키 · Caps 인디케이터 · 부트로더 키 |
| **VIA** | 실시간 키맵 편집 + 전원 재인가 후 유지 |
| **무선** | BLE HID · **호스트 5대 프로파일**(동시 연결·즉시 전환) · USB/BLE 자동 전환 · 배터리 보고 · TX power |
| **저전력** | activity 상태머신 · deep sleep(System OFF, 키로 웨이크) · RGB 자동 소등 · 콘솔 빌드타임 분리 |
| **RGB** | QMK `RGB_MATRIX`(위치 기반 효과) · 17종 · 전원 레일 게이팅 |
| **VIA 메뉴** | 프로파일/본딩 · 전력 타임아웃 · TX power · 런타임 디바운스 · `HOLD_ON_OTHER_KEY_PRESS` |

<br>

## 🔨 빌드

> 전제: NCS v3.1.0 @ `/opt/nordic/ncs/v3.1.0`

```bash
cd firmware/nrf52-qmk-fw

./build.sh                 # wish60 릴리스
./build.sh wish65          # wish65 릴리스
./build.sh wish65 -d       # 개발 빌드 (UART 콘솔 + CLI)
./release.sh               # 배포 패키지 → output/<board>-<ver>.zip
```

- 산출물 → `build/<board>[-debug]/zephyr/zephyr.uf2`
- 플래시 → **더블탭 리셋 후 UF2 매스스토리지에 드래그드롭** <sub>(Adafruit nRF52 UF2 부트로더 전제, 빌드 안 함)</sub>

> [!WARNING]
> **개발 빌드로 배터리를 측정하지 말 것** — UART 콘솔이 `~1.2mA` (idle의 20배).
> **VS Code nRF Connect 확장은 "Use sysbuild"를 반드시 끌 것** — 켜면 빌드는 되고 **부팅하지 않는다**.

<br>

## 📖 문서

### **[`docs/PORTING-NOTES.md`](firmware/nrf52-qmk-fw/docs/PORTING-NOTES.md)**

결정의 근거 · 실측 전력 · 실기기 함정. **이 프로젝트의 알맹이.**

<details>
<summary><b>겪은 함정 맛보기</b> (펼치기)</summary>

<br>

- **콘솔 하나가 idle 전류를 14배로** — nRF52840 UARTE는 RX가 켜지면 HFCLK를 계속 잡는다.
  런타임 게이팅은 오히려 악화 → **빌드타임 제거가 답**
- **`CONFIG_PM_DEVICE_RUNTIME`이 키보드를 죽였다** — 매트릭스 드라이버가 자기를 suspend 했다. 전력도 10배 악화
- **QMK 폴링 vs ZMK 이벤트** — 이 하이브리드의 본질적 난제. *"루프가 자면 반영 안 됨"* 버그가 **6번** 다른 얼굴로 나왔다
- **ZMK의 30초를 용도도 모르고 베꼈다** — 그건 **OLED 끄기용**이었고 RGB와 무관했다
- **참고 구현은 근거가 아니다** — zmk-config만 보고 wish65에 Caps LED가 없다고 단정했으나 **회로도엔 있었다**
- **System OFF는 GPIO를 유지한다** — 레일이 켜진 채 잠들면 네오픽셀이 65mA로 영원히

측정으로 뒤집힌 가설, 지어낸 근거를 정정한 기록도 그대로 남겼다.

</details>

<br>

## 📄 라이선스

**GPL v2** — [LICENSE](LICENSE) <sub>(upstream QMK와 동일 전문)</sub>

- `src/ap/modules/qmk/quantum/` = [QMK Firmware](https://github.com/qmk/qmk_firmware) GPLv2 코드
- 결합 저작물인 이 펌웨어 **전체가 GPLv2**로 배포된다
- <sub>초기엔 루트가 MIT였으나 QMK를 vendor-in 한 이상 사실과 달라 배포를 시작하며 바로잡았다</sub>

**참고 · 차용**

| 프로젝트 | 라이선스 | 무엇을 |
|:--|:--|:--|
| [QMK Firmware](https://github.com/qmk/qmk_firmware) | GPL-2.0 | 키보드 로직 코어 |
| [ZMK](https://github.com/zmkfirmware/zmk) | MIT | 저전력·무선 설계 참고, 74HC595 드라이버 이식 |
| [Zephyr / NCS](https://github.com/nrfconnect/sdk-nrf) | Apache-2.0 | RTOS · 드라이버 |
