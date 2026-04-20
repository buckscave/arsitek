/*
 * x86/interupsi.c
 *
 * Penanganan interupsi dan eksepsi untuk arsitektur x86 (IA-32).
 * Menyiapkan GDT, IDT, PIC, dan handler eksepsi CPU.
 *
 * GDT (Tabel Deskriptor Global):
 *   Entri 0: NULL (wajib)
 *   Entri 1: Segmen Kode Kernel (ring 0, 32-bit, batas 4 GB)
 *   Entri 2: Segmen Data Kernel (ring 0, 32-bit, batas 4 GB)
 *   Entri 3: Segmen Kode Pengguna (ring 3, 32-bit, batas 4 GB)
 *   Entri 4: Segmen Data Pengguna (ring 3, 32-bit, batas 4 GB)
 *
 * IDT (Tabel Deskriptor Interupsi):
 *   Entri 0-19:  Eksepsi CPU (pembagian nol, page fault, dll.)
 *   Entri 20-31: Dicadangkan oleh Intel
 *   Entri 32-47: IRQ perangkat keras (PIC master + slave)
 *   Entri 48-255: Interupsi perangkat lunak yang tersedia
 *
 * PIC (8259A):
 *   Master: IRQ 0-7  → INT 32-39
 *   Slave:  IRQ 8-15 → INT 40-47
 */

#include "../../lampiran/arsitek.h"
#include "../../lampiran/mesin.h"

/* ================================================================
 * VARIABEL GLOBAL
 * ================================================================ */

/* GDT — 6 entri (NULL + 4 segmen + TSS) */
static EntriGDT gdt[6] SEJAJAR(8);

/* IDT — 256 entri */
static EntriIDT idt[INTERUPSI_MAKSIMUM] SEJAJAR(8);

/* Register deskriptor GDT dan IDT */
static RegisterDeskriptor penuding_gdt;
static RegisterDeskriptor penuding_idt;

/* ================================================================
 * BANTUAN GDT
 * ================================================================ */

/*
 * isi_entri_gdt — Isi satu entri GDT dengan parameter yang diberikan.
 *
 * Parameter:
 *   entri       — pointer ke entri GDT
 *   dasar       — alamat dasar segmen
 *   batas       — batas segmen
 *   akses       — byte akses (type, DPL, present)
 *   bendera_batas — flag granularitas + bit tinggi batas
 */
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
 * BANTUAN IDT
 * ================================================================ */

/*
 * isi_entri_idt — Isi satu entri IDT untuk handler 32-bit.
 *
 * Parameter:
 *   entri       — pointer ke entri IDT
 *   handler     — alamat fungsi handler interupsi
 *   selektor    — selektor segmen kode (biasanya 0x08)
 *   tipe        — byte akses (0x8E = interrupt gate 32-bit)
 */
static void isi_entri_idt(EntriIDT *entri, ukuran_ptr handler,
                           uint16 selektor, uint8 tipe)
{
    entri->batas_bawah   = (uint16)(handler & 0xFFFF);
    entri->dasar_bawah   = (uint16)(handler & 0xFFFF);
    entri->dasar_tengah  = (uint8)((handler >> 16) & 0xFF);
    entri->akses         = tipe;
    entri->bendera       = 0x00;    /* Reserved, harus 0 untuk 32-bit */
    entri->dasar_atas    = (uint8)((handler >> 24) & 0xFF);
}

/* ================================================================
 * HANDLER EKSEPSI CPU
 * ================================================================ */

/* Makro untuk mendeklarasikan handler eksepsi stub */
#define DEFINISIKAN_HANDLER_EKSEPSI(nomor, pesan)            \
    void handler_eksepsi_##nomor(void)                        \
    {                                                         \
        notulen_catat(NOTULEN_FATAL, pesan);                  \
        mesin_hentikan();                                     \
    }

/* Handler eksepsi CPU — masing-masing menampilkan pesan dan menghentikan sistem */
DEFINISIKAN_HANDLER_EKSEPSI(0,  "Eksepsi: Pembagian dengan nol")
DEFINISIKAN_HANDLER_EKSEPSI(1,  "Eksepsi: Debug")
DEFINISIKAN_HANDLER_EKSEPSI(2,  "Eksepsi: NMI (Non-Maskable Interrupt)")
DEFINISIKAN_HANDLER_EKSEPSI(3,  "Eksepsi: Breakpoint")
DEFINISIKAN_HANDLER_EKSEPSI(4,  "Eksepsi: Overflow")
DEFINISIKAN_HANDLER_EKSEPSI(5,  "Eksepsi: Batas terlampaui (BOUND)")
DEFINISIKAN_HANDLER_EKSEPSI(6,  "Eksepsi: Opcode tidak valid")
DEFINISIKAN_HANDLER_EKSEPSI(7,  "Eksepsi: FPU tidak tersedia")
DEFINISIKAN_HANDLER_EKSEPSI(8,  "Eksepsi: Kesalahan ganda (Double Fault)")
DEFINISIKAN_HANDLER_EKSEPSI(9,  "Eksepsi: Segmen coprocessor terlampaui")
DEFINISIKAN_HANDLER_EKSEPSI(10, "Eksepsi: TSS tidak valid")
DEFINISIKAN_HANDLER_EKSEPSI(11, "Eksepsi: Segmen tidak ada")
DEFINISIKAN_HANDLER_EKSEPSI(12, "Eksepsi: Segmen stack tidak ada")
DEFINISIKAN_HANDLER_EKSEPSI(13, "Eksepsi: Pelanggaran umum (General Protection)")
DEFINISIKAN_HANDLER_EKSEPSI(14, "Eksepsi: Halaman tidak ditemukan (Page Fault)")
DEFINISIKAN_HANDLER_EKSEPSI(15, "Eksepsi: Dicadangkan")
DEFINISIKAN_HANDLER_EKSEPSI(16, "Eksepsi: Kesalahan FPU")
DEFINISIKAN_HANDLER_EKSEPSI(17, "Eksepsi: Pemeriksaan perataan (Alignment)")
DEFINISIKAN_HANDLER_EKSEPSI(18, "Eksepsi: Pemeriksaan mesin (Machine Check)")
DEFINISIKAN_HANDLER_EKSEPSI(19, "Eksepsi: SIMD floating-point")

/* Handler IRQ — stub yang mengirim EOI */
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
 * INISIALISASI GDT
 * ================================================================ */

/*
 * mesin_siapkan_gdt — Siapkan Tabel Deskriptor Global (GDT).
 *
 * GDT mendefinisikan segmen memori yang tersedia di sistem.
 * Pada mode terlindung, setiap akses memori melalui selektor
 * segmen yang merujuk ke entri GDT.
 */
void mesin_siapkan_gdt(void)
{
    /* Entri 0: NULL — wajib, semua bit nol */
    isi_entri_gdt(&gdt[0], 0, 0, 0, 0);

    /* Entri 1: Kode Kernel — ring 0, batas 4 GB, 32-bit */
    isi_entri_gdt(&gdt[1], 0, 0xFFFFFFFF,
                  0x9A,       /* Present, DPL=0, Code, Readable */
                  0xCF);      /* 4K gran, 32-bit, batas tinggi = 0xF */

    /* Entri 2: Data Kernel — ring 0, batas 4 GB, 32-bit */
    isi_entri_gdt(&gdt[2], 0, 0xFFFFFFFF,
                  0x92,       /* Present, DPL=0, Data, Writable */
                  0xCF);

    /* Entri 3: Kode Pengguna — ring 3, batas 4 GB, 32-bit */
    isi_entri_gdt(&gdt[3], 0, 0xFFFFFFFF,
                  0xFA,       /* Present, DPL=3, Code, Readable */
                  0xCF);

    /* Entri 4: Data Pengguna — ring 3, batas 4 GB, 32-bit */
    isi_entri_gdt(&gdt[4], 0, 0xFFFFFFFF,
                  0xF2,       /* Present, DPL=3, Data, Writable */
                  0xCF);

    /* Entri 5: TSS — Task State Segment (sementara kosong) */
    isi_entri_gdt(&gdt[5], 0, 0,
                  0x89,       /* Present, DPL=0, 32-bit TSS */
                  0x00);

    /* Atur penuding GDT */
    penuding_gdt.batas = (uint16)(sizeof(gdt) - 1);
    penuding_gdt.dasar = (ukuran_ptr)gdt;

    /* Muat GDT ke register GDTR */
    __asm__ volatile (
        "lgdt %0"
        :
        : "m"(penuding_gdt)
    );

    /* Muat ulang segmen data */
    __asm__ volatile (
        "movw $0x10, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"
        :
        :
        : "ax"
    );

    /* Lompat jauh untuk memuat ulang segmen kode */
    __asm__ volatile (
        "ljmp $0x08, $muat_ulang_kode\n\t"
        "muat_ulang_kode:\n\t"
    );
}

/* ================================================================
 * INISIALISASI IDT
 * ================================================================ */

/*
 * mesin_siapkan_idt — Siapkan Tabel Deskriptor Interupsi (IDT).
 *
 * IDT mendefinisikan handler untuk setiap vektor interupsi.
 * Vektor 0-19: eksepsi CPU
 * Vektor 32-47: IRQ perangkat keras
 * Vektor lainnya: tersedia untuk interupsi perangkat lunak
 */
void mesin_siapkan_idt(void)
{
    int i;

    /* Bersihkan seluruh IDT */
    for (i = 0; i < INTERUPSI_MAKSIMUM; i++) {
        isi_entri_idt(&idt[i], 0, 0, 0);
    }

    /* Pasang handler eksepsi CPU (vektor 0-19) */
    isi_entri_idt(&idt[0],  (ukuran_ptr)handler_eksepsi_0,  0x08, IDT_PINTU_INTERUPSI);
    isi_entri_idt(&idt[1],  (ukuran_ptr)handler_eksepsi_1,  0x08, IDT_PINTU_INTERUPSI);
    isi_entri_idt(&idt[2],  (ukuran_ptr)handler_eksepsi_2,  0x08, IDT_PINTU_INTERUPSI);
    isi_entri_idt(&idt[3],  (ukuran_ptr)handler_eksepsi_3,  0x08, IDT_PINTU_PERANGKAP);
    isi_entri_idt(&idt[4],  (ukuran_ptr)handler_eksepsi_4,  0x08, IDT_PINTU_INTERUPSI);
    isi_entri_idt(&idt[5],  (ukuran_ptr)handler_eksepsi_5,  0x08, IDT_PINTU_INTERUPSI);
    isi_entri_idt(&idt[6],  (ukuran_ptr)handler_eksepsi_6,  0x08, IDT_PINTU_INTERUPSI);
    isi_entri_idt(&idt[7],  (ukuran_ptr)handler_eksepsi_7,  0x08, IDT_PINTU_INTERUPSI);
    isi_entri_idt(&idt[8],  (ukuran_ptr)handler_eksepsi_8,  0x08, IDT_PINTU_INTERUPSI);
    isi_entri_idt(&idt[9],  (ukuran_ptr)handler_eksepsi_9,  0x08, IDT_PINTU_INTERUPSI);
    isi_entri_idt(&idt[10], (ukuran_ptr)handler_eksepsi_10, 0x08, IDT_PINTU_INTERUPSI);
    isi_entri_idt(&idt[11], (ukuran_ptr)handler_eksepsi_11, 0x08, IDT_PINTU_INTERUPSI);
    isi_entri_idt(&idt[12], (ukuran_ptr)handler_eksepsi_12, 0x08, IDT_PINTU_INTERUPSI);
    isi_entri_idt(&idt[13], (ukuran_ptr)handler_eksepsi_13, 0x08, IDT_PINTU_INTERUPSI);
    isi_entri_idt(&idt[14], (ukuran_ptr)handler_eksepsi_14, 0x08, IDT_PINTU_INTERUPSI);
    isi_entri_idt(&idt[15], (ukuran_ptr)handler_eksepsi_15, 0x08, IDT_PINTU_INTERUPSI);
    isi_entri_idt(&idt[16], (ukuran_ptr)handler_eksepsi_16, 0x08, IDT_PINTU_INTERUPSI);
    isi_entri_idt(&idt[17], (ukuran_ptr)handler_eksepsi_17, 0x08, IDT_PINTU_INTERUPSI);
    isi_entri_idt(&idt[18], (ukuran_ptr)handler_eksepsi_18, 0x08, IDT_PINTU_INTERUPSI);
    isi_entri_idt(&idt[19], (ukuran_ptr)handler_eksepsi_19, 0x08, IDT_PINTU_INTERUPSI);

    /* Pasang handler IRQ (vektor 32-47) */
    for (i = 32; i <= 47; i++) {
        switch (i) {
        case 32: isi_entri_idt(&idt[i], (ukuran_ptr)handler_eksepsi_32, 0x08, IDT_PINTU_INTERUPSI); break;
        case 33: isi_entri_idt(&idt[i], (ukuran_ptr)handler_eksepsi_33, 0x08, IDT_PINTU_INTERUPSI); break;
        case 34: isi_entri_idt(&idt[i], (ukuran_ptr)handler_eksepsi_34, 0x08, IDT_PINTU_INTERUPSI); break;
        case 35: isi_entri_idt(&idt[i], (ukuran_ptr)handler_eksepsi_35, 0x08, IDT_PINTU_INTERUPSI); break;
        case 36: isi_entri_idt(&idt[i], (ukuran_ptr)handler_eksepsi_36, 0x08, IDT_PINTU_INTERUPSI); break;
        case 37: isi_entri_idt(&idt[i], (ukuran_ptr)handler_eksepsi_37, 0x08, IDT_PINTU_INTERUPSI); break;
        case 38: isi_entri_idt(&idt[i], (ukuran_ptr)handler_eksepsi_38, 0x08, IDT_PINTU_INTERUPSI); break;
        case 39: isi_entri_idt(&idt[i], (ukuran_ptr)handler_eksepsi_39, 0x08, IDT_PINTU_INTERUPSI); break;
        case 40: isi_entri_idt(&idt[i], (ukuran_ptr)handler_eksepsi_40, 0x08, IDT_PINTU_INTERUPSI); break;
        case 41: isi_entri_idt(&idt[i], (ukuran_ptr)handler_eksepsi_41, 0x08, IDT_PINTU_INTERUPSI); break;
        case 42: isi_entri_idt(&idt[i], (ukuran_ptr)handler_eksepsi_42, 0x08, IDT_PINTU_INTERUPSI); break;
        case 43: isi_entri_idt(&idt[i], (ukuran_ptr)handler_eksepsi_43, 0x08, IDT_PINTU_INTERUPSI); break;
        case 44: isi_entri_idt(&idt[i], (ukuran_ptr)handler_eksepsi_44, 0x08, IDT_PINTU_INTERUPSI); break;
        case 45: isi_entri_idt(&idt[i], (ukuran_ptr)handler_eksepsi_45, 0x08, IDT_PINTU_INTERUPSI); break;
        case 46: isi_entri_idt(&idt[i], (ukuran_ptr)handler_eksepsi_46, 0x08, IDT_PINTU_INTERUPSI); break;
        case 47: isi_entri_idt(&idt[i], (ukuran_ptr)handler_eksepsi_47, 0x08, IDT_PINTU_INTERUPSI); break;
        }
    }

    /* Atur penuding IDT */
    penuding_idt.batas = (uint16)(sizeof(idt) - 1);
    penuding_idt.dasar = (ukuran_ptr)idt;

    /* Muat IDT ke register IDTR */
    __asm__ volatile (
        "lidt %0"
        :
        : "m"(penuding_idt)
    );
}

/* ================================================================
 * INISIALISASI PIC
 * ================================================================ */

/*
 * mesin_siapkan_pic — Pemetaan ulang PIC 8259A.
 *
 * Secara bawaan, PIC master menggunakan IRQ 8-15 dan
 * PIC slave menggunakan IRQ 0-7, yang bertabrakan dengan
 * vektor eksepsi CPU. Kita memetakan ulang:
 *   Master: IRQ 0-7  → INT 32-39
 *   Slave:  IRQ 8-15 → INT 40-47
 *
 * Proses inisialisasi PIC (ICW1-ICW4):
 *   ICW1: Mulai inisialisasi, ICW4 diperlukan
 *   ICW2: Alamat vektor dasar
 *   ICW3: Konfigurasi cascade
 *   ICW4: Mode prosesor (8086)
 */
void mesin_siapkan_pic(void)
{
    /* Simpan mask interupsi saat ini */
    uint8 mask_master = baca_port(PIC1_DATA);
    uint8 mask_slave  = baca_port(PIC2_DATA);

    /* ICW1: Mulai inisialisasi mode cascade */
    tulis_port(PIC1_PERINTAH, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    tunda_io();
    tulis_port(PIC2_PERINTAH, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    tunda_io();

    /* ICW2: Alamat vektor dasar */
    tulis_port(PIC1_DATA, 32);      /* Master: vektor 32-39 */
    tunda_io();
    tulis_port(PIC2_DATA, 40);      /* Slave: vektor 40-47 */
    tunda_io();

    /* ICW3: Konfigurasi cascade */
    tulis_port(PIC1_DATA, 0x04);    /* Master: slave pada IRQ2 */
    tunda_io();
    tulis_port(PIC2_DATA, 0x02);    /* Slave: identitas cascade */
    tunda_io();

    /* ICW4: Mode 8086 */
    tulis_port(PIC1_DATA, PIC_ICW4_8086);
    tunda_io();
    tulis_port(PIC2_DATA, PIC_ICW4_8086);
    tunda_io();

    /* Kembalikan mask interupsi yang disimpan */
    tulis_port(PIC1_DATA, mask_master);
    tulis_port(PIC2_DATA, mask_slave);
}

/* ================================================================
 * INISIALISASI PIT
 * ================================================================ */

/*
 * mesin_siapkan_pit — Siapkan timer sistem (PIT 8253/8254).
 *
 * PIT menghasilkan interupsi periodik pada frekuensi yang
 * ditentukan. Interupsi ini digunakan untuk penjadwalan
 * dan penghitungan waktu sistem.
 *
 * Parameter:
 *   frekuensi_hz — frekuensi interupsi dalam Hertz
 *                  Nilai umum: 100 (10 ms per tick)
 *
 * Rumus pembagi:
 *   pembagi = FREKUENSI_PIT / frekuensi_hz
 *   Jika pembagi < 1, gunakan 1
 *   Jika pembagi > 65535, gunakan 65535
 */
void mesin_siapkan_pit(uint32 frekuensi_hz)
{
    uint16 pembagi;

    if (frekuensi_hz == 0) {
        frekuensi_hz = 100;     /* Default: 100 Hz */
    }

    pembagi = (uint16)(PIT_FREKUENSI / frekuensi_hz);
    if (pembagi == 0) {
        pembagi = 1;
    }

    /* Kirim perintah ke PIT:
     * - Saluran 0 (timer)
     * - Mode lo/hi byte (byte rendah lalu byte tinggi)
     * - Mode gelombang kotak (rate generator) */
    tulis_port(PIT_PERINTAH, 0x36);

    /* Tulis byte rendah pembagi */
    tulis_port(PIT_SALURAN0, (uint8)(pembagi & 0xFF));

    /* Tulis byte tinggi pembagi */
    tulis_port(PIT_SALURAN0, (uint8)((pembagi >> 8) & 0xFF));
}
