# Makefile for building Tetris UEFI app using GNU-EFI
EFIINCL ?= /usr/include/efi
EFIARCHINCL ?= /usr/include/efi/x86_64

CC := gcc
OBJCOPY := objcopy
LD := ld

# Try to autodetect common locations of the gnu-efi linker script
LDSCRIPT ?= $(firstword $(wildcard /usr/lib/elf_x86_64_efi.lds /usr/lib/gnu-efi/elf_x86_64_efi.lds /usr/lib/x86_64-linux-gnu/gnu-efi/elf_x86_64_efi.lds /usr/lib64/gnu-efi/elf_x86_64_efi.lds /usr/local/lib/gnu-efi/elf_x86_64_efi.lds))

ifeq ($(LDSCRIPT),)
$(error "Could not find elf_x86_64_efi.lds. Install gnu-efi or set LDSCRIPT in the environment to the full path of the linker script.")
endif

CFLAGS := -I$(EFIINCL) -I$(EFIARCHINCL) -fPIC -fshort-wchar -mno-red-zone -fno-stack-protector -fno-builtin -Wall -Wextra -O2
LDFLAGS := -nostdlib -znocombreloc -T $(LDSCRIPT) -shared -Bsymbolic -L$(dir $(LDSCRIPT)) -lefi -lgnuefi

SRC := src/tetris.c
OBJ := tetris.o
SO := tetris.so
EFI := tetris.efi

all: $(EFI)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC) -o $(OBJ)

$(SO): $(OBJ)
	$(LD) $(LDFLAGS) -o $(SO) $(OBJ)

$(EFI): $(SO)
	$(OBJCOPY) -j .text -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc -O binary $(SO) $(EFI)

clean:
	rm -f $(OBJ) $(SO) $(EFI)

.PHONY: all clean
