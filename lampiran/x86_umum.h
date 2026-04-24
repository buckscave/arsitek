/*
 * x86_umum.h
 *
 * Fungsi-fungsi umum bersama untuk arsitektur x86 dan x64.
 * Menyediakan inisialisasi PIC 8259A, PIT 8254, serta
 * bantuan entri GDT dan IDT yang digunakan oleh kedua
 * varian arsitektur Intel/AMD.
 *
 * Modul ini menghilangkan duplikasi kode antara
 * konstruksi/x86/interupsi.c dan konstruksi/x64/interupsi.c.
 */

#ifndef X86_UMUM_H
#define X86_UMUM_H

#include "../lampiran/arsitek.h"

#if defined(ARSITEK_X86) || defined(ARSITEK_X64)

/* ================================================================
 * BANTUAN GDT
 * ================================================================ */

/*
 * x86_umum_isi_entri_gdt - Isi satu entri GDT.
 *
 * Parameter:
 *   entri         - pointer ke entri GDT
 *   dasar         - alamat dasar segmen
 *   batas         - batas segmen
 *   akses         - byte akses (type, DPL, present)
 *   bendera_batas - flag granularitas + bit tinggi batas
 */
void x86_umum_isi_entri_gdt(EntriGDT *entri,
    uint32 dasar, uint32 batas,
    uint8 akses, uint8 bendera_batas);

/* ================================================================
 * BANTUAN IDT
 * ================================================================ */

/*
 * x86_umum_isi_entri_idt32 - Isi entri IDT 32-bit (x86).
 */
void x86_umum_isi_entri_idt32(EntriIDT *entri,
    ukuran_ptr handler, uint16 selektor,
    uint8 tipe);

/*
 * x86_umum_isi_entri_idt64 - Isi entri IDT 64-bit (x64).
 */
void x86_umum_isi_entri_idt64(EntriIDT *entri,
    uint64 handler, uint16 selektor,
    uint8 tipe, uint8 ist);

/* ================================================================
 * INISIALISASI PIC 8259A
 * ================================================================ */

/*
 * x86_umum_siapkan_pic - Pemetaan ulang PIC 8259A.
 *
 * Master: IRQ 0-7  -> INT 32-39
 * Slave:  IRQ 8-15 -> INT 40-47
 */
void x86_umum_siapkan_pic(void);

/* ================================================================
 * INISIALISASI PIT 8254
 * ================================================================ */

/*
 * x86_umum_siapkan_pit - Siapkan timer sistem.
 *
 * Parameter:
 *   frekuensi_hz - frekuensi interupsi timer.
 *                  Nilai umum: 100 (10 ms per tick)
 */
void x86_umum_siapkan_pit(uint32 frekuensi_hz);

/* ================================================================
 * KIRIM EOI
 * ================================================================ */

/*
 * x86_umum_kirim_eoi - Kirim End-of-Interrupt ke PIC.
 * Jika IRQ >= 8, kirim EOI ke kedua PIC.
 */
void x86_umum_kirim_eoi(uint8 irq);

#endif /* ARSITEK_X86 || ARSITEK_X64 */

#endif /* X86_UMUM_H */
