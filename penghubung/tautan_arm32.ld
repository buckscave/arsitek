/* ================================================================
 * tautan_arm32.ld — Berkas tautan (linker script) untuk ARM32
 *
 * Menentukan tata letak memori kernel Arsitek pada arsitektur ARM32.
 * Kernel dimuat pada alamat 0x10000 (64 KB) oleh U-Boot.
 * ================================================================ */

ENTRY(arsitek_mulai)

SECTIONS
{
    /* Alamat dasar kernel ARM32 — 64 KB */
    . = 0x10000;

    .text : ALIGN(4K)
    {
        *(.text)
        *(.text.*)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data : ALIGN(4K)
    {
        *(.data)
        *(.data.*)
    }

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
    }
}
