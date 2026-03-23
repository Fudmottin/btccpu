#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
TARGET="cpu_miner"

usage() {
   cat <<EOF
Usage:
  $(basename "$0")
  $(basename "$0") debug
  $(basename "$0") tests
  $(basename "$0") release
  $(basename "$0") rebuild
  $(basename "$0") clean
  $(basename "$0") run <host> <port> <user> <password>

Modes:
  (no args)   Configure Debug, build everything, run tests
  debug       Configure Debug, build everything, run tests
  tests       Configure Debug, build tests, run tests
  release     Configure Release, build everything, run tests
  rebuild     Remove build directory, then do default debug build
  clean       Remove build directory
  run         Run $TARGET, building a Debug tree first if needed

Notes:
  - Tests are enabled for all configure modes here.
  - Release enables native optimization.
EOF
}

configure() {
   local build_type="$1"
   local native_opt="$2"

   echo "==> Configuring"
   echo "    build dir : $BUILD_DIR"
   echo "    type      : $build_type"
   echo "    native opt: $native_opt"

   cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
      -DCMAKE_BUILD_TYPE="$build_type" \
      -DCPU_MINER_ENABLE_TESTS=ON \
      -DCPU_MINER_ENABLE_NATIVE_OPTIMIZATION="$native_opt"
}

build_target() {
   local target="$1"

   echo "==> Building target: $target"
   cmake --build "$BUILD_DIR" --target "$target"
}

build_all() {
   echo "==> Building all targets"
   cmake --build "$BUILD_DIR"
}

run_tests() {
   echo "==> Running tests"
   ctest --test-dir "$BUILD_DIR" --output-on-failure
}

clean() {
   echo "==> Removing $BUILD_DIR"
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
      echo "Configuring Debug build first..."
      configure "Debug" "OFF"
      build_target "$TARGET"
   fi

   exec "$exe" "$host" "$port" "$user" "$password"
}

do_debug() {
   configure "Debug" "OFF"
   build_all
   run_tests
}

do_tests() {
   configure "Debug" "OFF"
   build_target "cpu_miner_tests"
   run_tests
}

do_release() {
   configure "Release" "ON"
   build_all
   run_tests
}

main() {
   local cmd="${1:-debug}"

   case "$cmd" in
      debug)
         do_debug
         ;;
      tests)
         do_tests
         ;;
      release)
         do_release
         ;;
      rebuild)
         clean
         do_debug
         ;;
      clean)
         clean
         ;;
      run)
         shift
         run_miner "$@"
         ;;
      -h|--help|help)
         usage
         ;;
      *)
         echo "Unknown command: $cmd" >&2
         usage
         exit 1
         ;;
   esac
}

main "$@"

