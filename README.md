# Ibedrockfinder

Interactive Minecraft Java Edition bedrock pattern finder for any POSIX terminal.
Enter a pattern of bedrock blocks you observe in-game and it will scan your world
seed to find all locations where that exact pattern appears.

Built by **Batthepig**.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Getting the File](#getting-the-file)
3. [Building](#building)
4. [Running](#running)
5. [Step-by-Step Usage Guide](#step-by-step-usage-guide)
6. [Pattern Input Reference](#pattern-input-reference)
7. [Understanding Results](#understanding-results)
8. [Platform Notes](#platform-notes)

---

## Prerequisites

You only need a C11 compiler — no git required.

| Platform | Install |
|---|---|
| Ubuntu / Debian | `sudo apt install clang` |
| Fedora / RHEL | `sudo dnf install clang` |
| Arch Linux | `sudo pacman -S clang` |
| macOS | `xcode-select --install` |
| iPhone — a-Shell | clang is built-in, nothing to install |
| Windows | Install [WSL](https://learn.microsoft.com/en-us/windows/wsl/), then follow Ubuntu steps |

---

## Getting the File

No git needed. Download the single source file directly:

### With curl

```sh
curl -O https://raw.githubusercontent.com/batthepig-two/Ibedrockfinder/main/ibedrockfinder.c
```

### With wget

```sh
wget https://raw.githubusercontent.com/batthepig-two/Ibedrockfinder/main/ibedrockfinder.c
```

---

## Building

```sh
clang -O3 -std=c11 -o ibedrockfinder ibedrockfinder.c -lm
```

Or with gcc:

```sh
gcc -O3 -std=c11 -o ibedrockfinder ibedrockfinder.c -lm
```

---

## Running

```sh
./ibedrockfinder
```

The program is fully interactive. It will prompt you for each input one at a time.
Press **Enter** at any prompt to accept the default shown in brackets.

---

## Step-by-Step Usage Guide

### Step 1 — Y level

```
Y level [-60]:
```

Enter the Y coordinate of the bedrock layer you are looking at.
You can see your current Y coordinate in the F3 debug screen in-game.

Valid Y ranges per dimension:

| Dimension | Valid Y | Notes |
|---|---|---|
| Overworld floor | -64 to -60 | Y -64 is always bedrock |
| Nether floor | 0 to 4 | Y 0 is always bedrock |
| Nether ceiling | 123 to 127 | Y 127 is always bedrock |

The best Y levels to search are the middle ones (-62, -61, 2, 3, 124, 125) because
they have a mix of bedrock and non-bedrock blocks, making patterns more distinctive.

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

Enter the width (X direction) and length (Z direction) of the rectangular area you
observed. Maximum 32 in each direction. A larger, more detailed pattern produces
fewer false positives and narrows down results faster.

---

### Step 4 — Pattern rows

```
Enter 16 row(s) of width 2.  '1'=bedrock, '0'=stone, '?'=unknown.
```

Type each row from north to south (increasing Z). Accepted characters:

| Input | Meaning |
|---|---|
| `1` `#` `B` `b` | Bedrock block |
| `0` `.` `S` `s` | Non-bedrock (stone, air, etc.) |
| `?` | Unknown — skip this cell |

Spaces are ignored. Short rows are padded with `?` automatically.
Press Enter alone to make an entire row unknown.

**Example** — 2 wide, first row has bedrock on the left and non-bedrock on
the right, second row is unknown:

```
row  0: 10
row  1:
```

---

### Step 5 — World seed

```
World seed [0]:
```

Enter your Java Edition world seed as a number (can be negative).

To find your seed: run `/seed` in-game (requires cheats or op permission).

---

### Step 6 — Search area

```
Center X [0]:
Center Z [0]:
Radius (blocks) [5000]:
```

Set the center point of the search and how far out to scan. A 5000-block radius
usually completes in a few seconds. Increase the radius if you expect the pattern
to be further from the center.

---

### Step 7 — Options

```
Match all 8 rotations? (1=yes, 0=no) [1]:
Max results to keep (closest first) [200]:
```

- **Rotations** — tests all 4 rotations and 2 mirror flips of your pattern.
  Leave this on unless your pattern is clearly asymmetric and you know its
  exact in-game orientation.
- **Max results** — how many of the closest matches to keep. Hits beyond
  this count are dropped as the scan runs.

---

## Pattern Input Reference

```
1  #  B  b  →  bedrock
0  .  S  s  →  non-bedrock
?            →  unknown / wildcard
```

**Tips for a good pattern:**

- Use a Y level in the middle of the bedrock range (e.g. Y=-62 in the overworld)
  where blocks are roughly 50/50 — more contrast means more unique patterns.
- Avoid the top and bottom layers — Y=-64 and Y=-59 have no variation.
- A 3×6 or 2×10 area is usually specific enough to get under 5 results.
- Use `?` freely for cells you did not observe. Unknown cells are skipped and
  do not cause wrong results — they only make the pattern less specific.

---

## Understanding Results

After the scan finishes:

```
Kept N closest hits:

    X        Z        O   DIST
    -------- -------- --- ----------
    -512     208      0   560.3
    1024     -96      2   1028.5
```

| Column | Meaning |
|---|---|
| **X** | Block X coordinate of the north-west corner of the matched pattern |
| **Z** | Block Z coordinate of the north-west corner of the matched pattern |
| **O** | Orientation index (0–7), representing which of the 8 rotations/flips matched |
| **DIST** | Distance in blocks from your chosen search center |

Go to the reported (X, Z) in your world at the Y level you searched and compare
the pattern. The closest hit to your known location is usually the correct one.

---

## Platform Notes

### a-Shell (iPhone / iPad)

a-Shell includes clang. Download the file and build:

```sh
curl -O https://raw.githubusercontent.com/batthepig-two/Ibedrockfinder/main/ibedrockfinder.c
clang -O3 -std=c11 -o ibedrockfinder ibedrockfinder.c -lm
./ibedrockfinder
```

### macOS

If `clang` is not found, run `xcode-select --install` first.

### Windows (WSL)

1. Open PowerShell as Administrator and run `wsl --install`, then restart.
2. Open the Ubuntu app.
3. Run `sudo apt install clang`, then follow the curl download and build steps above.
