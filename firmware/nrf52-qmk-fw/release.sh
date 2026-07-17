#!/usr/bin/env bash
#
# 배포 패키지 생성 — 보드별 폴더 + zip
#
#   ./release.sh              모든 보드
#   ./release.sh wish65       특정 보드만
#
# 산출:
#   output/<board>/<board>-<ver>.uf2      펌웨어 (릴리스 빌드)
#   output/<board>/<BOARD>-VIA.JSON       usevia.app 용 정의
#   output/<board>-<ver>.zip              보드별 압축
#
# [디버그 이미지는 넣지 않는다] 콘솔 빌드는 전류가 ~1.2mA 라 배터리 실사용에 못 쓴다.
# 배포본에 섞이면 사용자가 그걸 올리고 "전력이 이상하다" 고 할 수밖에 없다.
#
# 버전은 **빌드 산출물의 .config**(CONFIG_KBD_FW_VERSION)에서 읽는다 — 소스 파일이 주장하는
# 값이 아니라 **실제로 컴파일된 값**이라야 파일명이 거짓말을 안 한다.
# (단일 출처는 앱 Kconfig 의 KBD_FW_VERSION. BLE DIS 의 Firmware Revision 도 같은 값을 쓴다)
set -e

APP="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$APP"

BOARDS=("$@")
if [ ${#BOARDS[@]} -eq 0 ]; then
  BOARDS=($(ls -1 "$APP/boards/baram"))
fi

OUT="$APP/output"
echo "보드: ${BOARDS[*]}"
echo

for BOARD in "${BOARDS[@]}"; do
  if [ ! -d "$APP/boards/baram/$BOARD" ]; then
    echo "보드가 없다: $BOARD"; exit 1
  fi

  # VIA JSON 은 키보드 트리에 있다(보드 트리가 아니다 — §4.1 의 두 트리 구조).
  JSON=$(ls "$APP/src/ap/modules/qmk/keyboards/baram/$BOARD/json/"*-VIA.JSON 2>/dev/null | head -1)
  if [ -z "$JSON" ]; then
    echo "VIA JSON 이 없다: keyboards/baram/$BOARD/json/*-VIA.JSON"; exit 1
  fi

  echo "===== $BOARD ====="
  ./build.sh "$BOARD" -p >/dev/null 2>&1 || { echo "  빌드 실패 — ./build.sh $BOARD 로 확인할 것"; exit 1; }

  UF2="$APP/build/$BOARD/zephyr/zephyr.uf2"
  [ -f "$UF2" ] || { echo "  uf2 가 없다: ${UF2#$APP/}"; exit 1; }

  VER=$(grep -oE '^CONFIG_KBD_FW_VERSION="[^"]+"' "$APP/build/$BOARD/zephyr/.config" | grep -oE '"[^"]+"' | tr -d '"')
  [ -n "$VER" ] || { echo "  .config 에서 CONFIG_KBD_FW_VERSION 을 못 읽었다"; exit 1; }

  DIR="$OUT/$BOARD"
  rm -rf "$DIR" "$OUT/$BOARD-$VER.zip"
  mkdir -p "$DIR"
  cp "$UF2"  "$DIR/$BOARD-$VER.uf2"
  cp "$JSON" "$DIR/"

  # zip 안에 <board>/ 폴더가 들어가게 한다 — 풀었을 때 파일이 흩어지지 않는다.
  (cd "$OUT" && zip -qr "$BOARD-$VER.zip" "$BOARD")

  echo "  $(cd "$OUT" && ls -1 "$BOARD" | sed 's/^/    /')"
  echo "  -> output/$BOARD-$VER.zip  ($(du -h "$OUT/$BOARD-$VER.zip" | cut -f1))"
done

echo
echo "완료: output/"
