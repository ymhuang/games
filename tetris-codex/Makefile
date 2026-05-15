APP := tetris
BUILD := build
SRC := src/main.c
OBJ := $(BUILD)/main.o
ELF := $(BUILD)/$(APP).elf
EFI := $(BUILD)/$(APP).efi

CC ?= gcc
LD ?= ld
OBJCOPY ?= objcopy
READELF ?= readelf

CFLAGS := -std=c99 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector \
	-fno-stack-check -fpic -fshort-wchar -mno-red-zone -maccumulate-outgoing-args \
	-mno-sse -mno-mmx -mno-80387 -mno-avx -fno-asynchronous-unwind-tables \
	-fno-unwind-tables -fno-ident -I src

LDFLAGS := -nostdlib -znocombreloc -T uefi.lds -shared -Bsymbolic

.PHONY: all uefi-build check clean

all: uefi-build

uefi-build: $(EFI)

check: uefi-build
	$(READELF) -l $(ELF)
	@if $(READELF) -l $(ELF) | grep -q 'RWE'; then \
		echo "error: ELF contains an RWX load segment"; \
		exit 1; \
	fi

$(BUILD):
	mkdir -p $(BUILD)

$(OBJ): $(SRC) src/uefi.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(ELF): $(OBJ) uefi.lds
	$(LD) $(LDFLAGS) $< -o $@

$(EFI): $(ELF)
	$(OBJCOPY) -j .text -j .sdata -j .data -j .reloc --strip-all \
		--target=efi-app-x86_64 $< $@

clean:
	rm -rf $(BUILD)
