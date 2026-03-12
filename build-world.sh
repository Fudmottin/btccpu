#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
TARGET="${TARGET:-cpu_miner}"
JOBS="${JOBS:-}"
NATIVE_OPT="${NATIVE_OPT:-OFF}"

usage() {
   cat <<EOF
Usage:
  $(basename "$0") build
  $(basename "$0") run <host> <port> <user> <password>
  $(basename "$0") rebuild
  $(basename "$0") clean

Environment overrides:
  BUILD_DIR    default: $BUILD_DIR
  BUILD_TYPE   default: $BUILD_TYPE
  TARGET       default: $TARGET
  JOBS         default: auto / tool default
  NATIVE_OPT   default: $NATIVE_OPT   (ON or OFF)

Examples:
  BUILD_TYPE=Release $(basename "$0") build
  BUILD_TYPE=Release NATIVE_OPT=ON $(basename "$0") build
  $(basename "$0") run 192.168.0.104 3333 youruser x
EOF
}

configure() {
   cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DCPU_MINER_ENABLE_NATIVE_OPTIMIZATION="$NATIVE_OPT"
}

build() {
   configure
   if [[ -n "$JOBS" ]]; then
      cmake --build "$BUILD_DIR" --target "$TARGET" --parallel "$JOBS"
   else
      cmake --build "$BUILD_DIR" --target "$TARGET"
   fi
}

clean() {
   rm -rf "$BUILD_DIR"
}

run_miner() {
   if [[ $# -ne 4 ]]; then
      usage
      exit 1
   fi

   local host="$1"
   local port="$2"
   local user="$3"
   local password="$4"
   local exe="$BUILD_DIR/$TARGET"

   if [[ ! -x "$exe" ]]; then
      echo "Executable not found: $exe"
      echo "Building first..."
      build
   fi

   exec "$exe" "$host" "$port" "$user" "$password"
}

main() {
   local cmd="${1:-build}"

   case "$cmd" in
      build)
         build
         ;;
      rebuild)
         clean
         build
         ;;
      clean)
         clean
         ;;
      run)
         shift
         run_miner "$@"
         ;;
      *)
         usage
         exit 1
         ;;
   esac
}

main "$@"

