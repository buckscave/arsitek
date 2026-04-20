/*
 * x64/interupsi.c
 *
 * Penanganan interupsi untuk arsitektur x64 (AMD64).
 * Menyiapkan GDT 64-bit, IDT 64-bit (entri 16 byte),
 * PIC 8259A, dan handler eksepsi.
 *
 * Perbedaan utama dari x86:
 *   - IDT entri 16 byte (bukan 8 byte)
 *   - Alamat handler 64-bit (menggunakan offset lanjutan)
 *   - IST (Interrupt Stack Table) untuk handler tertentu
 *   - Nomor vektor dan kode kesalahan di stack berbeda
 */

#include "../../lampiran/arsitek.h"
#include "../../lampiran/mesin.h"

/* ================================================================
 * VARIABEL GLOBAL
 * ================================================================ */

/* GDT — termasuk segmen 64-bit */
static EntriGDT gdt[7] SEJAJAR(8);

/* IDT — 256 entri, masing-masing 16 byte pada x64 */
static EntriIDT idt[INTERUPSI_MAKSIMUM] SEJAJAR(16);

/* Register deskriptor */
static RegisterDeskriptor penuding_gdt;
static RegisterDeskriptor penuding_idt;

/* ================================================================
 * BANTUAN GDT
 * ================================================================ */

static void isi_entri_gdt(EntriGDT *entri, uint32 dasar, uint32 batas,
                           uint8 akses, uint8 bendera_batas)
{
    entri->batas_bawah   = (uint16)(batas & 0xFFFF);
    entri->dasar_bawah   = (uint16)(dasar & 0xFFFF);
    entri->dasar_tengah  = (uint8)((dasar >> 16) & 0xFF);
    entri->akses         = akses;
    entri->bendera_batas = (uint8)((batas >> 16) & 0x0F);
    entri->bendera_batas |= bendera_batas;
    entri->dasar_atas    = (uint8)((dasar >> 24) & 0xFF);
}

/* ================================================================
 * BANTUAN IDT x64 (entri 16 byte)
 * ================================================================ */

/*
 * isi_entri_idt64 — Isi satu entri IDT 64-bit.
 * Pada x64, setiap entri IDT berukuran 16 byte dengan
 * alamat handler terbagi menjadi 4 bagian.
 */
static void isi_entri_idt64(EntriIDT *entri, uint64 handler,
                             uint16 selektor, uint8 tipe, uint8 ist)
{
    entri->batas_bawah   = (uint16)(handler & 0xFFFF);
    entri->dasar_bawah   = (uint16)(handler & 0xFFFF);
    entri->dasar_tengah  = (uint8)((handler >> 16) & 0xFF);
    entri->akses         = tipe;
    entri->bendera       = (uint8)((ist & 0x07) << 0);
    entri->dasar_atas    = (uint8)((handler >> 24) & 0xFF);
    entri->dasar_lanjutan = (uint32)((handler >> 32) & 0xFFFFFFFF);
    entri->dicadangkan   = 0;
}

/* ================================================================
 * HANDLER EKSEPSI x64
 * ================================================================ */

/* Handler stub — masing-masing hanya mencatat dan menghentikan */
#define DEFINISIKAN_HANDLER_EKSEPSI(nomor, pesan)            \
    void handler_eksepsi_##nomor(void)                        \
    {                                                         \
        notulen_catat(NOTULEN_FATAL, pesan);                  \
        mesin_hentikan();                                     \
    }

DEFINISIKAN_HANDLER_EKSEPSI(0,  "Eksepsi x64: Pembagian dengan nol")
DEFINISIKAN_HANDLER_EKSEPSI(1,  "Eksepsi x64: Debug")
DEFINISIKAN_HANDLER_EKSEPSI(2,  "Eksepsi x64: NMI")
DEFINISIKAN_HANDLER_EKSEPSI(3,  "Eksepsi x64: Breakpoint")
DEFINISIKAN_HANDLER_EKSEPSI(4,  "Eksepsi x64: Overflow")
DEFINISIKAN_HANDLER_EKSEPSI(5,  "Eksepsi x64: BOUND range exceeded")
DEFINISIKAN_HANDLER_EKSEPSI(6,  "Eksepsi x64: Opcode tidak valid")
DEFINISIKAN_HANDLER_EKSEPSI(7,  "Eksepsi x64: Device not available")
DEFINISIKAN_HANDLER_EKSEPSI(8,  "Eksepsi x64: Double Fault")
DEFINISIKAN_HANDLER_EKSEPSI(9,  "Eksepsi x64: Coprocessor segment overrun")
DEFINISIKAN_HANDLER_EKSEPSI(10, "Eksepsi x64: TSS tidak valid")
DEFINISIKAN_HANDLER_EKSEPSI(11, "Eksepsi x64: Segmen tidak ada")
DEFINISIKAN_HANDLER_EKSEPSI(12, "Eksepsi x64: Stack segment fault")
DEFINISIKAN_HANDLER_EKSEPSI(13, "Eksepsi x64: General Protection Fault")
DEFINISIKAN_HANDLER_EKSEPSI(14, "Eksepsi x64: Page Fault")
DEFINISIKAN_HANDLER_EKSEPSI(16, "Eksepsi x64: x87 FPU error")
DEFINISIKAN_HANDLER_EKSEPSI(17, "Eksepsi x64: Alignment check")
DEFINISIKAN_HANDLER_EKSEPSI(18, "Eksepsi x64: Machine check")
DEFINISIKAN_HANDLER_EKSEPSI(19, "Eksepsi x64: SIMD floating-point")
DEFINISIKAN_HANDLER_EKSEPSI(32, "IRQ 0: Timer")
DEFINISIKAN_HANDLER_EKSEPSI(33, "IRQ 1: Papan Ketik")
DEFINISIKAN_HANDLER_EKSEPSI(34, "IRQ 2: Cascade")
DEFINISIKAN_HANDLER_EKSEPSI(35, "IRQ 3: COM2")
DEFINISIKAN_HANDLER_EKSEPSI(36, "IRQ 4: COM1")
DEFINISIKAN_HANDLER_EKSEPSI(37, "IRQ 5: LPT2")
DEFINISIKAN_HANDLER_EKSEPSI(38, "IRQ 6: Floppy")
DEFINISIKAN_HANDLER_EKSEPSI(39, "IRQ 7: LPT1")
DEFINISIKAN_HANDLER_EKSEPSI(40, "IRQ 8: RTC")
DEFINISIKAN_HANDLER_EKSEPSI(41, "IRQ 9: ACPI")
DEFINISIKAN_HANDLER_EKSEPSI(42, "IRQ 10: Tersedia")
DEFINISIKAN_HANDLER_EKSEPSI(43, "IRQ 11: Tersedia")
DEFINISIKAN_HANDLER_EKSEPSI(44, "IRQ 12: PS/2 Tetikus")
DEFINISIKAN_HANDLER_EKSEPSI(45, "IRQ 13: FPU")
DEFINISIKAN_HANDLER_EKSEPSI(46, "IRQ 14: IDE Utama")
DEFINISIKAN_HANDLER_EKSEPSI(47, "IRQ 15: IDE Sekunder")

/* ================================================================
 * INISIALISASI GDT x64
 * ================================================================ */

void mesin_siapkan_gdt(void)
{
    /* Entri 0: NULL */
    isi_entri_gdt(&gdt[0], 0, 0, 0, 0);

    /* Entri 1: Kode Kernel 32-bit (untuk kompatibilitas) */
    isi_entri_gdt(&gdt[1], 0, 0xFFFFFFFF, 0x9A, 0xCF);

    /* Entri 2: Data Kernel 32-bit */
    isi_entri_gdt(&gdt[2], 0, 0xFFFFFFFF, 0x92, 0xCF);

    /* Entri 3: Kode Kernel 64-bit — L (long mode) bit set */
    isi_entri_gdt(&gdt[3], 0, 0, 0x9A, 0x20);

    /* Entri 4: Data Kernel 64-bit */
    isi_entri_gdt(&gdt[4], 0, 0, 0x92, 0x00);

    /* Entri 5: Kode Pengguna 64-bit */
    isi_entri_gdt(&gdt[5], 0, 0, 0xFA, 0x20);

    /* Entri 6: Data Pengguna 64-bit */
    isi_entri_gdt(&gdt[6], 0, 0, 0xF2, 0x00);

    /* Atur dan muat GDT */
    penuding_gdt.batas = (uint16)(sizeof(gdt) - 1);
    penuding_gdt.dasar = (ukuran_ptr)gdt;

    __asm__ volatile ("lgdt %0" : : "m"(penuding_gdt));

    /* Muat ulang segmen data */
    __asm__ volatile (
        "movw $0x20, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"
        :
        :
        : "ax"
    );
}

/* ================================================================
 * INISIALISASI IDT x64
 * ================================================================ */

void mesin_siapkan_idt(void)
{
    int i;

    /* Bersihkan seluruh IDT */
    for (i = 0; i < INTERUPSI_MAKSIMUM; i++) {
        isi_entri_idt64(&idt[i], 0, 0, 0, 0);
    }

    /* Pasang handler eksepsi CPU */
    isi_entri_idt64(&idt[0],  (uint64)(ukuran_ptr)handler_eksepsi_0,  0x08, 0x8E, 0);
    isi_entri_idt64(&idt[1],  (uint64)(ukuran_ptr)handler_eksepsi_1,  0x08, 0x8E, 0);
    isi_entri_idt64(&idt[2],  (uint64)(ukuran_ptr)handler_eksepsi_2,  0x08, 0x8E, 0);
    isi_entri_idt64(&idt[3],  (uint64)(ukuran_ptr)handler_eksepsi_3,  0x08, 0x8F, 0);
    isi_entri_idt64(&idt[4],  (uint64)(ukuran_ptr)handler_eksepsi_4,  0x08, 0x8E, 0);
    isi_entri_idt64(&idt[5],  (uint64)(ukuran_ptr)handler_eksepsi_5,  0x08, 0x8E, 0);
    isi_entri_idt64(&idt[6],  (uint64)(ukuran_ptr)handler_eksepsi_6,  0x08, 0x8E, 0);
    isi_entri_idt64(&idt[7],  (uint64)(ukuran_ptr)handler_eksepsi_7,  0x08, 0x8E, 0);
    isi_entri_idt64(&idt[8],  (uint64)(ukuran_ptr)handler_eksepsi_8,  0x08, 0x8E, 1);
    isi_entri_idt64(&idt[10], (uint64)(ukuran_ptr)handler_eksepsi_10, 0x08, 0x8E, 1);
    isi_entri_idt64(&idt[11], (uint64)(ukuran_ptr)handler_eksepsi_11, 0x08, 0x8E, 0);
    isi_entri_idt64(&idt[12], (uint64)(ukuran_ptr)handler_eksepsi_12, 0x08, 0x8E, 1);
    isi_entri_idt64(&idt[13], (uint64)(ukuran_ptr)handler_eksepsi_13, 0x08, 0x8E, 0);
    isi_entri_idt64(&idt[14], (uint64)(ukuran_ptr)handler_eksepsi_14, 0x08, 0x8E, 0);
    isi_entri_idt64(&idt[16], (uint64)(ukuran_ptr)handler_eksepsi_16, 0x08, 0x8E, 0);
    isi_entri_idt64(&idt[17], (uint64)(ukuran_ptr)handler_eksepsi_17, 0x08, 0x8E, 0);
    isi_entri_idt64(&idt[18], (uint64)(ukuran_ptr)handler_eksepsi_18, 0x08, 0x8E, 1);
    isi_entri_idt64(&idt[19], (uint64)(ukuran_ptr)handler_eksepsi_19, 0x08, 0x8E, 0);

    /* Pasang handler IRQ */
    for (i = 32; i <= 47; i++) {
        switch (i) {
        case 32: isi_entri_idt64(&idt[i], (uint64)(ukuran_ptr)handler_eksepsi_32, 0x08, 0x8E, 0); break;
        case 33: isi_entri_idt64(&idt[i], (uint64)(ukuran_ptr)handler_eksepsi_33, 0x08, 0x8E, 0); break;
        case 34: isi_entri_idt64(&idt[i], (uint64)(ukuran_ptr)handler_eksepsi_34, 0x08, 0x8E, 0); break;
        case 35: isi_entri_idt64(&idt[i], (uint64)(ukuran_ptr)handler_eksepsi_35, 0x08, 0x8E, 0); break;
        case 36: isi_entri_idt64(&idt[i], (uint64)(ukuran_ptr)handler_eksepsi_36, 0x08, 0x8E, 0); break;
        case 37: isi_entri_idt64(&idt[i], (uint64)(ukuran_ptr)handler_eksepsi_37, 0x08, 0x8E, 0); break;
        case 38: isi_entri_idt64(&idt[i], (uint64)(ukuran_ptr)handler_eksepsi_38, 0x08, 0x8E, 0); break;
        case 39: isi_entri_idt64(&idt[i], (uint64)(ukuran_ptr)handler_eksepsi_39, 0x08, 0x8E, 0); break;
        case 40: isi_entri_idt64(&idt[i], (uint64)(ukuran_ptr)handler_eksepsi_40, 0x08, 0x8E, 0); break;
        case 41: isi_entri_idt64(&idt[i], (uint64)(ukuran_ptr)handler_eksepsi_41, 0x08, 0x8E, 0); break;
        case 42: isi_entri_idt64(&idt[i], (uint64)(ukuran_ptr)handler_eksepsi_42, 0x08, 0x8E, 0); break;
        case 43: isi_entri_idt64(&idt[i], (uint64)(ukuran_ptr)handler_eksepsi_43, 0x08, 0x8E, 0); break;
        case 44: isi_entri_idt64(&idt[i], (uint64)(ukuran_ptr)handler_eksepsi_44, 0x08, 0x8E, 0); break;
        case 45: isi_entri_idt64(&idt[i], (uint64)(ukuran_ptr)handler_eksepsi_45, 0x08, 0x8E, 0); break;
        case 46: isi_entri_idt64(&idt[i], (uint64)(ukuran_ptr)handler_eksepsi_46, 0x08, 0x8E, 0); break;
        case 47: isi_entri_idt64(&idt[i], (uint64)(ukuran_ptr)handler_eksepsi_47, 0x08, 0x8E, 0); break;
        }
    }

    /* Atur dan muat IDT */
    penuding_idt.batas = (uint16)(sizeof(idt) - 1);
    penuding_idt.dasar = (ukuran_ptr)idt;

    __asm__ volatile ("lidt %0" : : "m"(penuding_idt));
}

/* ================================================================
 * INISIALISASI PIC — SAMA DENGAN x86
 * ================================================================ */

void mesin_siapkan_pic(void)
{
    uint8 mask_master = baca_port(PIC1_DATA);
    uint8 mask_slave  = baca_port(PIC2_DATA);

    tulis_port(PIC1_PERINTAH, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    tunda_io();
    tulis_port(PIC2_PERINTAH, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    tunda_io();

    tulis_port(PIC1_DATA, 32);
    tunda_io();
    tulis_port(PIC2_DATA, 40);
    tunda_io();

    tulis_port(PIC1_DATA, 0x04);
    tunda_io();
    tulis_port(PIC2_DATA, 0x02);
    tunda_io();

    tulis_port(PIC1_DATA, PIC_ICW4_8086);
    tunda_io();
    tulis_port(PIC2_DATA, PIC_ICW4_8086);
    tunda_io();

    tulis_port(PIC1_DATA, mask_master);
    tulis_port(PIC2_DATA, mask_slave);
}

/* ================================================================
 * INISIALISASI PIT — SAMA DENGAN x86
 * ================================================================ */

void mesin_siapkan_pit(uint32 frekuensi_hz)
{
    uint16 pembagi;

    if (frekuensi_hz == 0) {
        frekuensi_hz = 100;
    }

    pembagi = (uint16)(PIT_FREKUENSI / frekuensi_hz);
    if (pembagi == 0) {
        pembagi = 1;
    }

    tulis_port(PIT_PERINTAH, 0x36);
    tunda_io();
    tulis_port(PIT_SALURAN0, (uint8)(pembagi & 0xFF));
    tunda_io();
    tulis_port(PIT_SALURAN0, (uint8)((pembagi >> 8) & 0xFF));
}
