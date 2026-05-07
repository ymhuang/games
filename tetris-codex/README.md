# UEFI Tetris

A small freestanding C Tetris game for the UEFI Shell. It draws the board through
Graphics Output Protocol framebuffer access and reads controls through the UEFI
simple text input protocol.

## Build

```sh
make uefi-build
```

The output is:

```text
build/tetris.efi
```

This repository intentionally carries minimal UEFI type definitions in
`src/uefi.h`, so it does not require EDK II or gnu-efi headers for this build.

## Run

Copy `build/tetris.efi` to a FAT EFI system partition or a UEFI Shell virtual
disk, then run:

```text
tetris.efi
```

Controls:

- Arrow left/right: move
- Arrow up: rotate
- Arrow down: soft drop
- Space: hard drop
- P: pause
- R: restart after game over
- Q or Esc: quit

## Notes

The Makefile uses the local GNU toolchain to build an x86_64 UEFI application:

- `gcc` compiles freestanding x86_64 code using the Microsoft x64 ABI for UEFI
  calls.
- `ld` links an ELF image with `EfiMain` as the entry point.
- `objcopy` converts it to `efi-app-x86_64`.

Manual testing should be done in QEMU with OVMF or on UEFI-capable hardware.
