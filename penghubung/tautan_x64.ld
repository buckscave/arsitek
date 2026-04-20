/* ================================================================
 * tautan_x64.ld — Berkas tautan (linker script) untuk x64
 *
 * Menentukan tata letak memori kernel Arsitek pada arsitektur x64.
 * Kernel dimuat pada alamat 2 MB (0x200000) di atas halaman awal.
 * ================================================================ */

ENTRY(arsitek_mulai)

SECTIONS
{
    /* Alamat dasar kernel — 2 MB (menghindari area terpeta rendah) */
    . = 0x200000;

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
