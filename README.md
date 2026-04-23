# Ibedrockfinder

Interactive Minecraft bedrock pattern finder for any POSIX terminal.
Enter a pattern you observe in-game and it will scan your world seed to
find all locations where that pattern appears in the bedrock layer.

Built by **Batthepig**. Works on Linux, macOS, iPhone (a-Shell), and any
other POSIX environment with a C11 compiler.

---

## Table of Contents

1. [Features](#features)
2. [Prerequisites](#prerequisites)
3. [Getting the Code](#getting-the-code)
4. [Building](#building)
5. [Running](#running)
6. [Usage Guide](#usage-guide)
7. [Pattern Input Reference](#pattern-input-reference)
8. [Understanding Results](#understanding-results)
9. [Platform Notes](#platform-notes)
10. [Credits](#credits)

---

## Features

- **Java Edition 1.18+** — exact coordinate-accurate results using Mojang's
  real xoroshiro128++ positional RNG and the bedrock-salt MD5 pipeline.
  Hits will match what you see in-game.
- **Bedrock Edition** — approximate. Same per-layer probabilities, but
  coordinates are not guaranteed to match a real Bedrock world (Mojang has
  not published the C++ source). Use Java mode for reliable results.
- Pattern entry with wildcard cells (`?`).
- All 8 rotations and mirror flips tested automatically.
- Top-N closest results returned, sorted by distance.
- Progress bar with ETA during scan.

---

## Prerequisites

You need:

- **git** — to clone the repository
- **A C11 compiler** — clang or gcc
- **make** *(optional, but recommended)*

### By platform

| Platform | Install |
|---|---|
| **Ubuntu / Debian** | `sudo apt install git clang make` |
| **Fedora / RHEL** | `sudo dnf install git clang make` |
| **Arch Linux** | `sudo pacman -S git clang make` |
| **macOS** | `xcode-select --install` (installs git, clang, make) |
| **iPhone (a-Shell)** | clang and make are built-in, git via `pkg install git` |
| **Windows** | Use [WSL](https://learn.microsoft.com/en-us/windows/wsl/) (Ubuntu), then follow the Ubuntu steps |

---

## Getting the Code

```sh
git clone -b cubiomes-integration https://github.com/batthepig-two/Ibedrockfinder.git
cd Ibedrockfinder
```

---

## Building

### With make (recommended)

```sh
make
```

### Manual — clang

```sh
clang -O3 -std=c11 -o ibedrockfinder ibedrockfinder.c cubiomes/util.c cubiomes/noise.c -lm
```

### Manual — gcc

```sh
gcc -O3 -std=c11 -o ibedrockfinder ibedrockfinder.c cubiomes/util.c cubiomes/noise.c -lm
```

### Clean build

```sh
make clean && make
```

---

## Running

```sh
./ibedrockfinder
```

The program is fully interactive — it will prompt you for every input.
Press **Enter** at any prompt to accept the default value shown in brackets.

---

## Usage Guide

### Step 1 — Y level

```
Y level [-60]:
```

Enter the Y coordinate of the bedrock layer you are looking at (shown in F3
on Java Edition). Valid ranges:

| Dimension | Valid Y range | Notes |
|---|---|---|
| Overworld floor | -64 to -60 | Y -64 is always bedrock |
| Nether floor | 0 to 4 | Y 0 is always bedrock |
| Nether ceiling | 123 to 127 | Y 127 is always bedrock |

---

### Step 2 — Dimension

```
Dimension (overworld_floor/nether_floor/nether_ceiling) [overworld_floor]:
```

Type one of:
- `overworld_floor`
- `nether_floor`
- `nether_ceiling`

---

### Step 3 — Pattern size

```
Pattern width  (X size) [2]:
Pattern length (Z size) [16]:
```

Enter the width (X) and length (Z) of the rectangular region you observed.
Maximum is 32 in each direction. A larger pattern gives fewer false positives.

---

### Step 4 — Pattern rows

```
Enter 16 row(s) of width 2.  '1'=bedrock, '0'=stone, '?'=unknown.
```

Enter each row from north to south (increasing Z). Characters:

| Character | Meaning |
|---|---|
| `1` or `#` or `B` | Bedrock block |
| `0` or `.` or `S` | Stone / air (not bedrock) |
| `?` | Unknown / don't care |

Spaces are ignored. Short rows are padded with `?`. Press Enter alone for
an all-unknown row.

**Example** — a 2-wide column where you know the left cell is bedrock and
the right is stone for the first row, both unknown for the second:

```
row  0: 10
row  1:
```

---

### Step 5 — Edition and seed

```
Edition (java/bedrock) [java]:
World seed [0]:
```

- Type `java` or `j` for Java Edition (coordinate-accurate).
- Type `bedrock` or `b` for Bedrock Edition (approximate).
- Enter your world seed as a number (can be negative).

To find your world seed in Java Edition: `/seed` command in-game.

---

### Step 6 — Search area

```
Center X [0]:
Center Z [0]:
Radius (blocks) [5000]:
```

Enter the center of your search and how far out to scan. Larger radius =
longer scan time. A 5000-block radius on modern hardware typically takes
a few seconds.

---

### Step 7 — Options

```
Match all 8 rotations? (1=yes, 0=no) [1]:
Max results to keep (closest first) [200]:
```

- **Rotations** — if enabled, all 4 rotations and both mirror flips of your
  pattern are tested. Recommended unless your pattern is asymmetric and you
  know its exact orientation.
- **Max results** — how many of the closest hits to keep. Results beyond
  this count are discarded during the scan.

---

## Pattern Input Reference

Quick reference for entering patterns:

```
1  #  B  b  →  bedrock
0  .  S  s  →  stone (not bedrock)
?         →  unknown (skip this cell)
```

Tips for good patterns:
- Use a layer like Y=-62 (overworld) where blocks alternate — more contrast
  makes the pattern more unique.
- Avoid Y=-64 (always bedrock) and Y=-59 (always stone) — they have no
  variation to match.
- A 2x8 or 3x6 pattern is usually enough to get unique results.
- Use `?` freely for cells you are unsure about — false unknowns only reduce
  specificity, they do not cause wrong results.

---

## Understanding Results

After the scan completes you will see:

```
Kept N closest hits:

    X        Z        O   DIST
    -------- -------- --- ----------
    -512     208      0   560.3
    1024     -96      2   1028.5
```

| Column | Meaning |
|---|---|
| **X** | Block X coordinate of the pattern's north-west corner |
| **Z** | Block Z coordinate of the pattern's north-west corner |
| **O** | Orientation index (0–7, one of 8 rotations/flips) |
| **DIST** | Distance from your search center |

Go to the reported (X, Z) in-game at the Y level you searched and verify
the pattern matches. If no hit is close to your known location, try a larger
radius or a more specific pattern.

---

## Platform Notes

### a-Shell (iPhone / iPad)

a-Shell includes clang and make. Install git with:

```sh
pkg install git
```

Then clone and build as normal. The binary runs entirely on-device — no
server or network connection is needed during scanning.

### macOS

If `clang` is not found, run:

```sh
xcode-select --install
```

This installs the Command Line Tools which include clang, make, and git.

### Linux

Any distribution with a C11 compiler works. Install via your package
manager if needed (see Prerequisites table above).

### Windows (WSL)

1. Install WSL: open PowerShell as Administrator and run
   `wsl --install`, then restart.
2. Open the Ubuntu app from the Start menu.
3. Follow the Ubuntu steps in Prerequisites.

---

## Credits

- Bedrock finder algorithm and tool by **Batthepig**
- xoroshiro128++ / RNG primitives from **[cubiomes](https://github.com/Cubitect/cubiomes)** by Cubitect (MIT License)
- Java Edition accuracy based on Mojang's XoroshiroPositionalRandomFactory and `Mth.getSeed(x, y, z)`
