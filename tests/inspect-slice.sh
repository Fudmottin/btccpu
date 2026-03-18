#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

files=(
  "src/stratum_client/stratum_client.hpp"
  "src/stratum_client/stratum_client.cpp"
  "src/stratum_client/messages.hpp"
  "src/stratum_client/messages.cpp"
  "src/mining_job/work_state.hpp"
  "src/mining_job/work_state.cpp"
)

for f in "${files[@]}"; do
  printf '\n===== %s =====\n' "$f"
  nl -ba "$f"
done

