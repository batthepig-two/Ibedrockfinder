# Ibedrockfinder & Ibedrockseeder

Two interactive Minecraft Java Edition tools for any POSIX terminal.

- **Ibedrockfinder** — know your seed, find where a bedrock pattern is.
- **Ibedrockseeder** — know your coordinates, find what your seed is.

Built by **Batthepig**.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Ibedrockfinder](#ibedrockfinder)
   - [Get and Build](#get-and-build)
   - [Running](#running)
   - [Step-by-Step Usage](#step-by-step-usage)
   - [Pattern Input Reference](#pattern-input-reference)
   - [Understanding Results](#understanding-results)
3. [Ibedrockseeder](#ibedrockseeder)
   - [Get and Build](#get-and-build-1)
   - [Running](#running-1)
   - [Step-by-Step Usage](#step-by-step-usage-1)
   - [Understanding Results](#understanding-results-1)
   - [Search Speed Guide](#search-speed-guide)
4. [Platform Notes](#platform-notes)

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

## Ibedrockfinder

**You know your seed. You want to find where a bedrock pattern is in the world.**

### Get and Build

Download the single source file directly — no git needed:

```sh
curl -O https://raw.githubusercontent.com/batthepig-two/Ibedrockfinder/main/ibedrockfinder.c
```

Or with wget:

```sh
wget https://raw.githubusercontent.com/batthepig-two/Ibedrockfinder/main/ibedrockfinder.c
```

Build:

```sh
clang -O3 -std=c11 -o ibedrockfinder ibedrockfinder.c -lm
```

Or with gcc:

```sh
gcc -O3 -std=c11 -o ibedrockfinder ibedrockfinder.c -lm
```

### Running

```sh
./ibedrockfinder
```

The program is fully interactive. Press **Enter** at any prompt to accept the default shown in brackets.

---

### Step-by-Step Usage

#### Step 1 — Y level

```
Y level [-60]:
```

Enter the Y coordinate of the bedrock layer you are looking at (from F3 in-game).

Valid Y ranges per dimension:

| Dimension | Valid Y | Notes |
|---|---|---|
| Overworld floor | -64 to -60 | Y -64 is always bedrock |
| Nether floor | 0 to 4 | Y 0 is always bedrock |
| Nether ceiling | 123 to 127 | Y 127 is always bedrock |

The best Y levels to search are the middle ones (-62, -61, 2, 3, 124, 125) — they have a roughly 50/50 mix of bedrock and stone, making your pattern more distinctive.

---

#### Step 2 — Dimension

```
Dimension (overworld_floor/nether_floor/nether_ceiling) [overworld_floor]:
```

Type one of: `overworld_floor`, `nether_floor`, `nether_ceiling`

---

#### Step 3 — Pattern size

```
Pattern width  (X size) [2]:
Pattern length (Z size) [16]:
```

Width is the X direction, length is the Z direction. Maximum 32 in each direction. A larger, more detailed pattern gives fewer false positives.

---

#### Step 4 — Pattern rows

```
Enter 16 row(s) of width 2.  '1'=bedrock, '0'=stone, '?'=unknown.
```

Type each row from north to south (increasing Z):

| Input | Meaning |
|---|---|
| `1` `#` `B` `b` | Bedrock block |
| `0` `.` `S` `s` | Non-bedrock (stone, air, etc.) |
| `?` | Unknown — skip this cell |

Spaces are ignored. Short rows are padded with `?`. Press Enter alone for an all-unknown row.

---

#### Step 5 — World seed

```
World seed [0]:
```

Enter your Java Edition world seed. Find it with `/seed` in-game (requires cheats or op).

---

#### Step 6 — Search area

```
Center X [0]:
Center Z [0]:
Radius (blocks) [5000]:
```

Center your search on where you think you are and set the radius. A 5,000-block radius completes in seconds. The fewer results you get, the better — try a small radius first if you know roughly where you are.

---

#### Step 7 — Options

```
Match all 8 rotations? (1=yes, 0=no) [1]:
Max results to keep (closest first) [200]:
```

Rotations tests all 4 rotations and 2 mirror flips of your pattern. Leave it on unless you know the exact orientation.

---

### Pattern Input Reference

```
1  #  B  b  →  bedrock
0  .  S  s  →  non-bedrock
?            →  unknown / wildcard
```

**Tips:**

- Use Y=-62 or Y=-61 (overworld) for best pattern quality — more bedrock variation.
- Avoid Y=-64 and Y=-60: the extreme layers have almost no variation.
- A 3×6 or 2×10 area is usually specific enough to get under 5 results.
- Use `?` freely for cells you could not see clearly.

---

### Understanding Results

```
Kept N closest hits:

    X        Z        O   DIST
    -------- -------- --- ----------
    -441     -827     4   937.2
    1871     -870     3   2063.4
```

| Column | Meaning |
|---|---|
| **X** | Block X coordinate of the north-west corner of the matched pattern |
| **Z** | Block Z coordinate of the north-west corner of the matched pattern |
| **O** | Orientation index (0–7) — which of the 8 rotations/flips matched |
| **DIST** | Distance in blocks from your chosen search center |

The closest hit to where you think you are is almost always the correct one. Go to that (X, Z) in-game and compare.

---

## Ibedrockseeder

**You know your coordinates. You want to find your world seed.**

This tool is the reverse of Ibedrockfinder. Instead of scanning locations for a known seed, it scans seeds for a known location. You must know the **exact absolute block coordinates** of each block in your pattern — read them directly off the F3 screen while standing next to each block.

### Get and Build

```sh
curl -O https://raw.githubusercontent.com/batthepig-two/Ibedrockfinder/main/ibedrockseeder.c
```

Or with wget:

```sh
wget https://raw.githubusercontent.com/batthepig-two/Ibedrockfinder/main/ibedrockseeder.c
```

Build:

```sh
clang -O3 -std=c11 -o ibedrockseeder ibedrockseeder.c -lm
```

Or with gcc:

```sh
gcc -O3 -std=c11 -o ibedrockseeder ibedrockseeder.c -lm
```

### Running

```sh
./ibedrockseeder
```

---

### Step-by-Step Usage

#### Step 1 — Y level and dimension

Same as Ibedrockfinder. Use a middle layer (Y=-62 or Y=-61 in the overworld) for best results.

#### Step 2 — Pattern origin

```
Origin X [0]:
Origin Z [0]:
```

The absolute block coordinates of the **top-left (north-west) corner** of your pattern — the block with the smallest X and smallest Z. Stand on it in-game and read X, Z from F3.

#### Step 3 — Pattern size and rows

```
Pattern width  (X blocks) [4]:
Pattern height (Z blocks) [4]:
```

Same row input as Ibedrockfinder. Use `1`/`b` for bedrock, `0`/`s` for stone, `?` to skip.

**Important:** every cell you mark as bedrock or stone must be accurate. A single wrong cell will silently exclude your real seed and include wrong ones. If unsure about a block, use `?`.

#### Step 4 — Seed search range

```
Seed range start [-140737488355328]:
Seed range end   [140737488355327]:
```

The tool searches every integer seed in this range and reports any that match your pattern. The default is the full 32-bit signed integer space (about 4 billion seeds).

See [Search Speed Guide](#search-speed-guide) to understand what range makes sense for your situation.

---

### Understanding Results

```
Matching seed(s):

  -27494042902671370001
```

Each number is a world seed that would produce exactly the bedrock/stone cells you described at those coordinates. Verify with `/seed` in-game.

If you get **multiple results**, add more known cells to narrow it down — the more blocks you describe, the rarer the pattern and the fewer false positives.

If you get **no results**, check:
- Are the X, Y, Z coordinates exactly right? (re-read F3)
- Did you confuse bedrock and stone?
- Is the Y level in the valid bedrock range for your dimension?
- Is your seed outside the searched range?

---

### Search Speed Guide

Typical speed: **150 – 350 million seeds per second**.

| Situation | Recommended range | Approximate time |
|---|---|---|
| You typed a number you roughly remember | ±a few billion around your guess | Seconds |
| You know it was a small number | -10 million to +10 million | Under 1 second |
| Full 32-bit range | -2,147,483,648 to 2,147,483,647 | ~15 seconds |
| Full random-seed space (2^48, **default**) | -140,737,488,355,328 to 140,737,488,355,327 | Many hours |

A more distinctive pattern (more known cells, especially bedrock cells at Y=-62) means mismatches are caught after checking just 1–2 blocks per seed, making the search faster even for the same range.

---

## Platform Notes

### a-Shell (iPhone / iPad)

a-Shell includes clang. Download and build in the terminal:

```sh
curl -O https://raw.githubusercontent.com/batthepig-two/Ibedrockfinder/main/ibedrockfinder.c
clang -O3 -std=c11 -o ibedrockfinder ibedrockfinder.c -lm
./ibedrockfinder
```

Same steps for ibedrockseeder — just change the filename.

### macOS

If `clang` is not found, run `xcode-select --install` first.

### Windows (WSL)

1. Open PowerShell as Administrator and run `wsl --install`, then restart.
2. Open the Ubuntu app.
3. Run `sudo apt install clang`, then follow the curl download and build steps above.
