# Ibedrockfinder

Single-file C bedrock pattern finder for Minecraft, optimized for quick use in a-Shell on iPhone.

## Build

```sh
clang -O3 -std=c11 -o ibedrockfinder ibedrockfinder.c
```

## Run

```sh
./ibedrockfinder
```

## Notes

- Java Edition mode implements the real 1.18+ xoroshiro128++ positional RNG path.
- Bedrock Edition mode is clearly marked approximate (pattern-statistical, not coordinate-exact).
- The scanner supports wildcards (`?`) and optional 8-orientation matching.

## a-Shell Git push reminder

If pasting multiline commands in a-Shell collapses into one line, run one command at a time or separate commands with `;`.
