# Ibedrockfinder

  Single-file interactive Minecraft bedrock pattern finder, built for a-Shell on iPhone
  (works on any POSIX terminal too).

  ## Features

  - **Java Edition 1.18+** — Real Mojang xoroshiro128++ positional RNG via [cubiomes](https://github.com/Cubitect/cubiomes).
    Pattern locations match your actual Java world.
  - **Bedrock Edition** — Approximate (same layer probabilities, not coordinate-exact).
  - Pattern entry with wildcard cells (`?`), all 8 rotations/flips, top-N closest results.

  ## Build

  Requires a C11 compiler. Uses cubiomes (included as `cubiomes/`).

  ### With make (recommended)

  ```sh
  make
  ./ibedrockfinder
  ```

  ### Manual

  ```sh
  clang -O3 -std=c11 -o ibedrockfinder ibedrockfinder.c cubiomes/util.c cubiomes/noise.c -lm
  ./ibedrockfinder
  ```

  Or with GCC:

  ```sh
  gcc -O3 -std=c11 -o ibedrockfinder ibedrockfinder.c cubiomes/util.c cubiomes/noise.c -lm
  ./ibedrockfinder
  ```

  ## Credits

  - Bedrock algorithm and tool by **Batthepig**
  - RNG / xoroshiro128++ implementation from **[cubiomes](https://github.com/Cubitect/cubiomes)** by Cubitect (MIT)
  