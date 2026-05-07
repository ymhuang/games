# UEFI Tetris Project Reproduction

This document describes how to recreate the current `uefi-tetris` project and
produce the same kind of UEFI output binary.

## Goal

Build a freestanding x86_64 UEFI Tetris game that runs from UEFI Shell or a
QEMU/OVMF FAT disk as:

```text
tetris.efi
```

The current project is a two-player Tetris implementation:

- Player 1 uses arrow keys and Space.
- Player 2 uses WASD and F.
- Both players have independent boards, pieces, scores, line counts, fall speed,
  and game-over state.
- Clearing one line only affects the local board.
- Clearing two, three, or four lines sends exactly that many garbage rows to the
  other player from the bottom of their board.

## Directory Layout

Create this project layout:

```text
uefi-tetris/
  Makefile
  README.md
  PROJECT_REPRODUCTION.md
  uefi.lds
  src/
    main.c
    uefi.h
```

Generated build output goes under:

```text
uefi-tetris/build/
  main.o
  tetris.elf
  tetris.efi
```

## Toolchain Requirements

Use a GNU x86_64 toolchain that can emit EFI PE/COFF through `objcopy`:

```sh
gcc --version
ld --version
objcopy --version
make --version
```

No EDK II or gnu-efi headers are required. The project carries a minimal local
UEFI header in `src/uefi.h`.

## Build File

Use this Makefile behavior:

- Compile `src/main.c` as freestanding C99.
- Use `-fshort-wchar` so C wide strings match UEFI `CHAR16`.
- Use `-mno-red-zone` and `-maccumulate-outgoing-args` for UEFI x86_64 ABI
  compatibility.
- Link with `ld -shared -Bsymbolic` through `uefi.lds`.
- Convert ELF to `efi-app-x86_64` with `objcopy`.
- Keep only `.text`, `.sdata`, `.data`, and `.reloc`.
- Strip symbols from the final `.efi`.

The key output command is:

```sh
objcopy -j .text -j .sdata -j .data -j .reloc --strip-all \
  --target=efi-app-x86_64 build/tetris.elf build/tetris.efi
```

## Linker Script Requirements

The linker script must place the first loadable section at RVA `0x1000`:

```ld
. = 0;
ImageBase = .;
. = 0x1000;
```

This matters for OVMF. An earlier image with sections starting at RVA `0` was
rejected by UEFI Shell with:

```text
Command Error Status: Unsupported
```

The final PE/COFF should have:

- Subsystem: EFI application
- BaseOfCode: `0x1000`
- A `.reloc` directory
- No COFF symbols

## Minimal UEFI Header

`src/uefi.h` defines only the UEFI types and protocols used by the game:

- Integer aliases such as `UINT8`, `UINT16`, `UINT32`, `UINT64`, `UINTN`
- `CHAR16`, `EFI_STATUS`, `EFI_HANDLE`, `EFI_EVENT`
- `EFI_SYSTEM_TABLE`
- `EFI_BOOT_SERVICES`
- `EFI_SIMPLE_TEXT_INPUT_PROTOCOL`
- `EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL`
- `EFI_GRAPHICS_OUTPUT_PROTOCOL`
- GOP Blt pixel and operation types

Important details:

- UEFI calls use `EFIAPI __attribute__((ms_abi))`.
- EFI error statuses must set the high bit:

```c
#define EFIERR(a) (0x8000000000000000ULL | (a))
#define EFI_UNSUPPORTED EFIERR(3)
```

## PE Relocation Block

The source includes a small dummy `.reloc` section:

```c
static const UINT8 dummy_reloc[12] __attribute__((section(".reloc"), used)) = {
    0, 0, 0, 0,
    12, 0, 0, 0,
    0, 0, 0, 0
};
```

This makes `objcopy` emit a Base Relocation Directory. Some firmware rejects
EFI images without relocations even when the program itself does not require
runtime fixups.

## Graphics

Use GOP `Blt` fills instead of raw framebuffer writes.

Reason:

- Some OVMF/GOP modes are `PixelBltOnly`.
- Direct framebuffer access can fail or be unavailable.
- `Blt(EfiBltVideoFill)` works across more UEFI environments.

The game draws each rectangle with one GOP Blt fill call.

## Game State Model

The two-player version uses a `Player` struct:

```c
typedef struct {
    UINT32 board[BOARD_H][BOARD_W];
    Piece current;
    Piece next_piece;
    UINTN score;
    UINTN lines;
    UINTN frame;
    UINTN drop_frames;
    int game_over;
} Player;
```

There are two players:

```c
static Player players[2];
```

All gameplay functions operate on a `Player *`:

- Collision
- Piece lock
- Line clearing
- Movement
- Rotation
- Hard drop
- Speed update
- Drawing

## Controls

Player 1:

- Left arrow: move left
- Right arrow: move right
- Up arrow: rotate
- Down arrow: soft drop
- Space: hard drop

Player 2:

- A: move left
- D: move right
- W: rotate
- S: soft drop
- F: hard drop

Global:

- P: pause/resume
- R: restart both players
- Q or Esc: quit

## Garbage Attack Rule

When a player locks a piece, clear full rows and count how many rows were
cleared by that lock.

Rule:

- 1 cleared line: local clear only
- 2 cleared lines: append 2 garbage rows to opponent
- 3 cleared lines: append 3 garbage rows to opponent
- 4 cleared lines: append 4 garbage rows to opponent

Garbage behavior:

- Rows are appended from the bottom.
- Existing rows move upward.
- Each garbage row is filled except for one random empty hole.
- If the top row is occupied before a push, the receiving player loses.
- If the push makes the receiving player's active piece collide, the receiving
  player loses.

## Build Commands

From the project root:

```sh
make clean
make uefi-build
```

Expected output:

```text
build/tetris.efi
```

The linker may print this warning:

```text
ld: warning: build/tetris.elf has a LOAD segment with RWX permissions
```

That warning exists in the current build and does not prevent the OVMF-tested
EFI image from running.

## Output Verification

Check the output type:

```sh
file build/tetris.efi
```

Expected shape:

```text
PE32+ executable for EFI (application), x86-64
```

Inspect PE metadata:

```sh
objdump -x build/tetris.efi
```

Expected important fields:

```text
HAS_RELOC
Subsystem        0000000a (EFI application)
BaseOfCode       0000000000001000
Base Relocation Directory [.reloc]
SYMBOL TABLE:
no symbols
```

## Running In UEFI Shell

Copy `build/tetris.efi` to a FAT EFI disk or a QEMU FAT directory.

In UEFI Shell:

```text
fs0:
tetris.efi
```

If `fs0:` is not correct, use `map -r` in UEFI Shell and choose the filesystem
that contains `tetris.efi`.

## QEMU/OVMF Notes

Run with graphics enabled. Do not use `-nographic` or `-display none`.

Example shape:

```sh
qemu-system-x86_64 \
  -bios /path/to/OVMF_CODE.fd \
  -vga std \
  -drive file=fat:rw:/path/to/fat-dir,format=raw
```

Put `tetris.efi` inside `/path/to/fat-dir`.

## Troubleshooting

If UEFI Shell prints:

```text
Command Error Status: Unsupported
```

and the app prints no message first, the firmware probably rejected the PE/COFF
image before entering `EfiMain`. Check:

- The first section starts at RVA `0x1000`.
- `.reloc` exists.
- `objcopy` used `--target=efi-app-x86_64`.
- The final image is not stale.

If the app prints that GOP is missing or Blt is unavailable, the UEFI
environment does not expose the graphics protocol the game needs.
