# ================================================================
# Makefile — Kernel Arsitek
#
# Membangun kernel untuk empat arsitektur:
#   make arch=x86   — IA-32 (32-bit)
#   make arch=x64   — AMD64/Intel 64 (64-bit)
#   make arch=arm32 — ARMv7
#   make arch=arm64 — AArch64
#
# Default: x86
# ================================================================

# Arsitektur target
ARCH ?= x86

# Nama kernel
KERNEL = arsitek

# Direktori
SRC      = .
LAMPIRAN = $(SRC)/lampiran
KONSTRUKSI = $(SRC)/konstruksi
PENINJAU = $(SRC)/peninjau
NOTULEN  = $(SRC)/notulen
PENYEDIA = $(SRC)/penyedia
PENGEMBANG = $(SRC)/pengembang
PIGURA  = $(SRC)/pigura
PENGHUBUNG = $(SRC)/penghubung
PEMICU   = $(SRC)/pemicu
BUILD    = build/$(ARCH)

# ================================================================
# KONFIGURASI PER ARSITEKTUR
# ================================================================

ifeq ($(ARCH),x86)
    CROSS_COMPILE =
    CC      = $(CROSS_COMPILE)gcc
    LD      = $(CROSS_COMPILE)ld
    NASM    = nasm
    OBJCOPY = $(CROSS_COMPILE)objcopy

    CFLAGS  = -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin \
	      -fno-stack-protector -fno-pic -fno-pie -Wall -Wextra \
	      -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
	      -std=c89 -O2 -I$(LAMPIRAN) -DARSITEK_X86=1
    LDFLAGS = -m elf_i386 -T $(PENGHUBUNG)/tautan_x86.ld -nostdlib
    ASMFLAGS = -f elf32

    PEMICU_ASM       = $(PEMICU)/x86/pemicu.asm
    PEMICU_BIN       = $(BUILD)/pemicu_x86.bin
    PEMICU_FMT       = bin
    PEMICU_C         =
    PEMICU_LANJUTAN_ASM =

    KONSTRUKSI_C = $(KONSTRUKSI)/mesin.c $(KONSTRUKSI)/x86/memori.c $(KONSTRUKSI)/x86/interupsi.c

else ifeq ($(ARCH),x64)
    CROSS_COMPILE =
    CC      = $(CROSS_COMPILE)gcc
    LD      = $(CROSS_COMPILE)ld
    NASM    = nasm
    OBJCOPY = $(CROSS_COMPILE)objcopy

    CFLAGS  = -m64 -ffreestanding -nostdlib -nostdinc -fno-builtin \
	      -fno-stack-protector -fno-pic -fno-pie -mno-red-zone \
	      -Wall -Wextra -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
	      -std=c89 -O2 -I$(LAMPIRAN) -DARSITEK_X64=1
    LDFLAGS = -m elf_x86_64 -T $(PENGHUBUNG)/tautan_x64.ld -nostdlib
    ASMFLAGS = -f elf64

    PEMICU_ASM       = $(PEMICU)/x64/pemicu_awal.asm
    PEMICU_BIN       = $(BUILD)/pemicu_x64.bin
    PEMICU_FMT       = bin
    PEMICU_C         =
    PEMICU_LANJUTAN_ASM = $(PEMICU)/x64/pemicu_lanjutan.asm

    KONSTRUKSI_C = $(KONSTRUKSI)/mesin.c $(KONSTRUKSI)/x64/memori.c $(KONSTRUKSI)/x64/interupsi.c

else ifeq ($(ARCH),arm32)
    CROSS_COMPILE ?= arm-none-eabi-
    CC      = $(CROSS_COMPILE)gcc
    LD      = $(CROSS_COMPILE)ld
    OBJCOPY = $(CROSS_COMPILE)objcopy

    CFLAGS  = -mcpu=cortex-a8 -ffreestanding -nostdlib -nostdinc -fno-builtin \
	      -fno-stack-protector -Wall -Wextra \
	      -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
	      -std=c89 -O2 -I$(LAMPIRAN) -DARSITEK_ARM32=1
    LDFLAGS = -T $(PENGHUBUNG)/tautan_arm32.ld -nostdlib

    PEMICU_ASM       =
    PEMICU_BIN       =
    PEMICU_FMT       =
    PEMICU_C         = $(PEMICU)/arm32/pemicu.c
    PEMICU_LANJUTAN_ASM =

    KONSTRUKSI_C = $(KONSTRUKSI)/mesin.c $(KONSTRUKSI)/arm32/memori.c $(KONSTRUKSI)/arm32/interupsi.c $(LAMPIRAN)/arm32_divisi.c

else ifeq ($(ARCH),arm64)
    CROSS_COMPILE ?= aarch64-none-elf-
    CC      = $(CROSS_COMPILE)gcc
    LD      = $(CROSS_COMPILE)ld
    OBJCOPY = $(CROSS_COMPILE)objcopy

    CFLAGS  = -mcpu=cortex-a57 -ffreestanding -nostdlib -nostdinc -fno-builtin \
	      -fno-stack-protector -mgeneral-regs-only -Wall -Wextra \
	      -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
	      -std=c89 -O2 -I$(LAMPIRAN) -DARSITEK_ARM64=1
    LDFLAGS = -T $(PENGHUBUNG)/tautan_arm64.ld -nostdlib

    PEMICU_ASM       =
    PEMICU_BIN       =
    PEMICU_FMT       =
    PEMICU_C         = $(PEMICU)/arm64/pemicu.c
    PEMICU_LANJUTAN_ASM =

    KONSTRUKSI_C = $(KONSTRUKSI)/mesin.c $(KONSTRUKSI)/arm64/memori.c $(KONSTRUKSI)/arm64/interupsi.c

else
    $(error Arsitektur tidak dikenali: $(ARCH). Gunakan: x86, x64, arm32, arm64)
endif

# ================================================================
# BERKAS SUMBER
# ================================================================

# Semua berkas C kernel (kecuali pemicu ARM yang terpisah)
C_SRCS = $(SRC)/arsitek.c \
	 $(KONSTRUKSI_C) \
	 $(PEMICU_C) \
	 $(PENINJAU)/peninjau.c \
	 $(PENINJAU)/cpu.c \
	 $(PENINJAU)/ram.c \
	 $(PENINJAU)/usb.c \
	 $(PENINJAU)/pci.c \
	 $(PENINJAU)/layar.c \
	 $(PENINJAU)/papan_ketik.c \
	 $(PENINJAU)/tetikus.c \
	 $(PENINJAU)/penyimpanan.c \
	 $(PENINJAU)/jaringan.c \
	 $(PENINJAU)/dma.c \
	 $(NOTULEN)/notulen.c \
	 $(PENYEDIA)/penyedia.c \
	 $(PENYEDIA)/tipedata.c \
	 $(PENYEDIA)/alamat.c \
	 $(PENYEDIA)/jalur.c \
	 $(PENYEDIA)/sela.c \
	 $(PENGEMBANG)/pengembang.c \
	 $(PENGEMBANG)/koneksi.c \
	 $(PENGEMBANG)/kendali.c \
	 $(PENGEMBANG)/antarmuka.c \
	 $(PIGURA)/pigura.c

# Berkas objek C
C_OBJS = $(patsubst %.c,$(BUILD)/%.o,$(C_SRCS))

# Berkas objek assembly pemicu (boot sector — format bin, tidak ditautkan)
ifneq ($(PEMICU_ASM),)
    PEMICU_OBJ = $(BUILD)/pemicu_boot.o
else
    PEMICU_OBJ =
endif

# Berkas objek assembly pemicu lanjutan (format ELF, ditautkan dengan kernel)
ifneq ($(PEMICU_LANJUTAN_ASM),)
    PEMICU_LANJUTAN_OBJ = $(BUILD)/pemicu_lanjutan.o
else
    PEMICU_LANJUTAN_OBJ =
endif

# Semua berkas objek yang ditautkan ke kernel ELF
OBJS = $(PEMICU_LANJUTAN_OBJ) $(C_OBJS)

# Target akhir
KERNEL_BIN = $(BUILD)/$(KERNEL).bin
KERNEL_ELF = $(BUILD)/$(KERNEL).elf

# ================================================================
# ATURAN BUILD
# ================================================================

.PHONY: all clean info

# Target utama
all: $(BUILD) $(KERNEL_BIN) $(PEMICU_BIN)

# Buat direktori build
$(BUILD):
	mkdir -p $(BUILD)
	mkdir -p $(BUILD)/konstruksi/x86
	mkdir -p $(BUILD)/konstruksi/x64
	mkdir -p $(BUILD)/konstruksi/arm32
	mkdir -p $(BUILD)/konstruksi/arm64
	mkdir -p $(BUILD)/peninjau
	mkdir -p $(BUILD)/notulen
	mkdir -p $(BUILD)/penyedia
	mkdir -p $(BUILD)/pengembang
	mkdir -p $(BUILD)/pigura
	mkdir -p $(BUILD)/pemicu/arm32
	mkdir -p $(BUILD)/pemicu/arm64
	mkdir -p $(BUILD)/lampiran

# Kompilasi assembly pemicu boot (hanya untuk referensi, tidak ditautkan)
ifneq ($(PEMICU_ASM),)
$(PEMICU_OBJ): $(PEMICU_ASM) | $(BUILD)
	$(NASM) $(ASMFLAGS) -o $@ $<
endif

# Kompilasi assembly pemicu lanjutan (format ELF, akan ditautkan)
ifneq ($(PEMICU_LANJUTAN_ASM),)
$(PEMICU_LANJUTAN_OBJ): $(PEMICU_LANJUTAN_ASM) | $(BUILD)
	$(NASM) -f elf64 -o $@ $<
endif

# Kompilasi berkas C
$(BUILD)/%.o: %.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# Tautkan kernel menjadi ELF
$(KERNEL_ELF): $(OBJS) | $(BUILD)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# Konversi ELF ke biner mentah
$(KERNEL_BIN): $(KERNEL_ELF) | $(BUILD)
	$(OBJCOPY) -O binary $< $@

# Kompilasi pemicu boot menjadi biner boot (sektor boot terpisah)
ifneq ($(PEMICU_ASM),)
$(PEMICU_BIN): $(PEMICU_ASM) | $(BUILD)
	$(NASM) -f $(PEMICU_FMT) -o $@ $<
endif

# ================================================================
# BERSIHKAN
# ================================================================

clean:
	rm -rf build/

# ================================================================
# INFORMASI
# ================================================================

info:
	@echo "Kernel Arsitek — Sistem Build"
	@echo "Arsitektur : $(ARCH)"
	@echo "Compiler   : $(CC)"
	@echo "CFLAGS     : $(CFLAGS)"
	@echo "LDFLAGS    : $(LDFLAGS)"
	@echo "Target     : $(KERNEL_BIN)"
