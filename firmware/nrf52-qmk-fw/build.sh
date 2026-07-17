#!/usr/bin/env bash
#
# 빌드 헬퍼 — NCS v3.1.0, out-of-tree board, --no-sysbuild
#
#   ./build.sh                 wish60 릴리스
#   ./build.sh wish65          wish65 릴리스
#   ./build.sh wish65 -d       wish65 개발(콘솔)
#   ./build.sh wish60 -d -p    pristine 재빌드
#
# 산출물: build/<board>[-debug]/zephyr/zephyr.uf2
#
# [보드별 빌드 폴더] 한 폴더를 공유하면 보드를 바꿀 때마다 pristine 이 필요하고, 그때
# 다른 보드의 산출물이 지워진다(실제로 겪었다). 보드/빌드타입마다 폴더를 나누면 그럴 일이 없고
# 증분 빌드도 산다.
#
# [--no-sysbuild] 빼먹으면 부팅하지 않는다 — NCS Partition Manager 가 앱을 0x0 에 링크해
# 미리 플래시된 UF2 부트로더의 code_partition(0x1000)과 어긋난다. docs/PORTING-NOTES.md §2.2
set -e

APP="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

TC=/opt/nordic/ncs/toolchains/5c0d382932
export PATH="$TC/bin:$TC/opt/bin:$PATH"
export ZEPHYR_BASE=/opt/nordic/ncs/v3.1.0/zephyr
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
export ZEPHYR_SDK_INSTALL_DIR=/opt/nordic/ncs/toolchains/0c0f19d91c/opt/zephyr-sdk

BOARD=wish60
DEBUG=""
PRISTINE=auto

for arg in "$@"; do
  case "$arg" in
    -d|--debug)    DEBUG=y ;;
    -p|--pristine) PRISTINE=always ;;
    -h|--help)
      sed -n '2,17p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
      echo "사용 가능한 보드: $(ls -1 "$APP/boards/baram")"
      exit 0 ;;
    -*) echo "알 수 없는 옵션: $arg"; exit 1 ;;
    *)  BOARD="$arg" ;;
  esac
done

if [ ! -d "$APP/boards/baram/$BOARD" ]; then
  echo "보드가 없다: $BOARD"
  echo "사용 가능: $(ls -1 "$APP/boards/baram" | tr '\n' ' ')"
  exit 1
fi

# SoC 는 board.yml 에서 읽는다 — 여기 표를 두면 보드를 추가할 때마다 고쳐야 한다.
SOC=$(awk '/socs:/{f=1;next} f&&/name:/{print $3; exit}' "$APP/boards/baram/$BOARD/board.yml")
if [ -z "$SOC" ]; then
  echo "board.yml 에서 SoC 를 못 읽었다: boards/baram/$BOARD/board.yml"
  exit 1
fi

BUILD="$APP/build/$BOARD${DEBUG:+-debug}"
EXTRA=""
[ -n "$DEBUG" ] && EXTRA="-DDEBUG_CONSOLE=y"

echo "board: $BOARD/$SOC   빌드: $([ -n "$DEBUG" ] && echo "개발(콘솔)" || echo "릴리스")   -> ${BUILD#$APP/}"

cd "$APP"
# BOARD_ROOT / DTS_ROOT 는 CMakeLists.txt 가 스스로 넣는다 — 여기서 안 넘긴다.
"$TC/bin/west" build -b "$BOARD/$SOC" -d "$BUILD" -p "$PRISTINE" --no-sysbuild . ${EXTRA:+-- $EXTRA}

echo
echo "산출물: ${BUILD#$APP/}/zephyr/zephyr.uf2"
