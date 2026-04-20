/* ================================================================
 * tautan_arm64.ld — Berkas tautan (linker script) untuk ARM64
 *
 * Menentukan tata letak memori kernel Arsitek pada arsitektur ARM64.
 * Kernel dimuat pada alamat 0x80000 (512 KB) sesuai konvensi.
 * ================================================================ */

ENTRY(arsitek_mulai)

SECTIONS
{
    /* Alamat dasar kernel ARM64 — 512 KB */
    . = 0x80000;

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
