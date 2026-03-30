# btccpu

Educational Bitcoin CPU miner using the CKPool Stratum v1 protocol.

The goal is understanding through implementation. This is not intended to be competitive or efficient compared to real mining hardware.

The code is written in C++20 and developed/tested on:
- Debian (Raspberry Pi 5)
- macOS (Homebrew toolchain)

Local setup:
- Bitcoin Core + CKPool running on a Raspberry Pi
- BitAxe miner used separately for lottery mining
- This project connects to the same Stratum server for experimentation

## Build

macOS (Homebrew):

```
brew install boost cmake

```
Debian:

```
sudo apt install libboost-all-dev cmake

```

Then:

```
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure

```

## Architecture

The code is organized into three layers:

- **Protocol**
  - `StratumClient`
  - Stratum v1 messages, socket I/O, share submission

- **Mining**
  - Coinbase, merkle root, and header preparation
  - Hashing backends (CPU now, others later)
  - Explicit, test-covered byte-order handling

- **Coordination**
  - `MiningCoordinator`
  - Connects prepared work to backends
  - Produces submission-ready shares

The main thread handles orchestration only:
- thread lifecycle
- event handling
- console output

## Design goals

- Strict separation of protocol and hashing
- Deterministic, testable mining logic
- Explicit byte-order semantics
- Minimize recomputation in the hot path
- Enable future backends (GPU, ASIC) without invasive changes

