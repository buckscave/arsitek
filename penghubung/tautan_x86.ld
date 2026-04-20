/* ================================================================
 * tautan_x86.ld — Berkas tautan (linker script) untuk x86
 *
 * Menentukan tata letak memori kernel Arsitek pada arsitektur x86.
 * Kernel dimuat pada alamat 1 MB (0x100000) oleh bootloader.
 * ================================================================ */

ENTRY(arsitek_mulai)

SECTIONS
{
    /* Alamat dasar kernel — 1 MB */
    . = 0x100000;

    /* Bagian kode — hanya baca, dapat dieksekusi */
    .text : ALIGN(4K)
    {
        *(.text)
        *(.text.*)
    }

    /* Bagian data hanya baca — string konstanta, tabel */
    .rodata : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* Bagian data — variabel global yang sudah diinisialisasi */
    .data : ALIGN(4K)
    {
        *(.data)
        *(.data.*)
    }

    /* Bagian BSS — variabel global yang belum diinisialisasi */
    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    /* Buang bagian yang tidak diperlukan */
    /DISCARD/ :
    {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
    }
}
