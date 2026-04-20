/*
 * pemicu.c
 *
 * Kode pemicu (boot) untuk arsitektur ARM64 (AArch64).
 * Dipanggil oleh bootloader (misalnya U-Boot).
 *
 * Tugas:
 *   1. Inisialisasi prosesor ARM64
 *   2. Siapkan tabel vektor eksepsi
 *   3. Siapkan MMU dan paging (4 tingkat: PGD→PUD→PMD→PTE)
 *   4. Deteksi memori dari informasi bootloader (Device Tree)
 *   5. Lompat ke titik masuk kernel
 */

#include "../../lampiran/arsitek.h"
#include "../../lampiran/mesin.h"

/* ================================================================
 * FALLBACK DEFINISI ARM64
 *
 * Jika kompilasi dilakukan tanpa flag -DARSITEK_ARM64=1,
 * definisi SCTLR dan GIC dari mesin.h tidak akan tersedia.
 * Berikan fallback agar kode tetap dapat dikompilasi.
 * ================================================================ */
#ifndef SCTLR_M
#define SCTLR_M     BIT(0)             /* MMU Enable */
#define SCTLR_A     BIT(1)             /* Alignment Check */
#define SCTLR_C     BIT(2)             /* Data Cache Enable */
#define SCTLR_I     BIT(12)            /* Instruction Cache Enable */
#define SCTLR_Z     BIT(11)            /* Branch Prediction */
#endif

#ifndef KERNEL_ALAMAT_DASAR
#define KERNEL_ALAMAT_DASAR  0x00080000  /* Kernel dimuat di 512 KB */
#endif

/* ================================================================
 * KONSTANTA PLATFORM ARM64
 * ================================================================ */

/* ---- Alamat dasar UART PL011 (QEMU virt) ---- */
#define UART0_ALAMAT_DASAR   0x09000000

/* ---- Offset register UART PL011 ---- */
#define UART_DR      0x00   /* Data Register */
#define UART_RSR     0x04   /* Receive Status Register */
#define UART_FR      0x18   /* Flag Register */
#define UART_ILPR    0x20   /* IrDA Low-Power Counter Register */
#define UART_IBRD    0x24   /* Integer Baud Rate Register */
#define UART_FBRD    0x28   /* Fractional Baud Rate Register */
#define UART_LCR_H   0x2C   /* Line Control Register */
#define UART_CR      0x30   /* Control Register */
#define UART_IFLS    0x34   /* Interrupt FIFO Level Select Register */
#define UART_IMSC    0x38   /* Interrupt Mask Set/Clear Register */
#define UART_RIS     0x3C   /* Raw Interrupt Status Register */
#define UART_MIS     0x40   /* Masked Interrupt Status Register */
#define UART_ICR     0x44   /* Interrupt Clear Register */
#define UART_DMACR   0x48   /* DMA Control Register */

/* ---- Bit register UART ---- */
#define UART_FR_TXFF     BIT(5)   /* Transmit FIFO penuh */
#define UART_FR_RXFE     BIT(4)   /* Receive FIFO kosong */
#define UART_FR_BUSY     BIT(3)   /* UART sedang sibuk */
#define UART_CR_UARTEN   BIT(0)   /* Aktifkan UART */
#define UART_CR_TXE      BIT(8)   /* Aktifkan transmit */
#define UART_CR_RXE      BIT(9)   /* Aktifkan receive */
#define UART_LCR_H_FEN   BIT(4)   /* Aktifkan FIFO */
#define UART_LCR_H_WLEN8 0x60     /* Panjang kata 8 bit */

/* ---- Alamat GICv3 (QEMU virt) ---- */
#define GICV3_GICD_BASE   0x08000000   /* GIC Distributor */
#define GICV3_GICR_BASE   0x080A0000   /* GIC Redistributor */

/* ---- Offset register GICv3 Distributor ---- */
#define GICD_CTLR         0x0000
#define GICD_TYPER        0x0004
#define GICD_ISENABLE     0x0100
#define GICD_ICENABLE     0x0180
#define GICD_ICPEND       0x0280
#define GICD_IPRIORITY    0x0400
#define GICD_ICFG         0x0C00

/* ---- Offset register GICv3 Redistributor ---- */
#define GICR_CTLR         0x0000
#define GICR_TYPER        0x0008
#define GICR_WAKER        0x0014

/* ---- Konstanta tabel halaman ARM64 ---- */

/* Jumlah entri per tabel (4KB / 8 byte per entri = 512) */
#define ENTRI_PER_TABEL   512

/* Jumlah tabel PTE yang dialokasikan (memetakan 32 MB pertama) */
#define PTE_JUMLAH_TABEL  16

/* Ukuran halaman */
#define HALAMAN_UKURAN    4096

/* Ukuran blok 2 MB (pada tingkat PMD) */
#define BLOK2M_UKURAN     (2ULL * 1024 * 1024)

/* Ukuran blok 1 GB (pada tingkat PUD) */
#define BLOK1G_UKURAN     (1ULL * 1024 * 1024 * 1024)

/* ---- Atribut entri tabel halaman ARM64 ---- */

/* Bit dasar deskriptor */
#define PTE_VALID         (1ULL << 0)     /* Entri valid */
#define PTE_TABEL         (1ULL << 1)     /* Deskriptor tabel (menuju level berikutnya) */
#define PTE_BLOK_HALAMAN  (0ULL << 1)     /* Deskriptor blok/halaman (daun) */

/* Atribut memori */
#define PTE_ATTRINDX(x)   ((uint64)(x) << 2)  /* Indeks atribut MAIR_EL1 */
#define PTE_NS            (1ULL << 5)     /* Non-Secure */
#define PTE_AF            (1ULL << 6)     /* Access Flag (WAJIB disetel) */
#define PTE_nG            (1ULL << 7)     /* non-Global */
#define PTE_AP(x)         ((uint64)(x) << 8)  /* Access Permissions */
#define PTE_SH(x)         ((uint64)(x) << 10) /* Shareability */

/* Mask alamat output (bit [47:12]) */
#define PTE_MASK_ALAMAT   0x0000FFFFFFFFF000ULL

/* Ekstrak alamat dari entri */
#define PTE_ALAMAT(entri) ((entri) & PTE_MASK_ALAMAT)

/* Masukkan alamat ke entri (hanya bit [47:12]) */
#define PTE_TETAP_ALAMAT(alamat) ((uint64)(alamat) & PTE_MASK_ALAMAT)

/* ---- Nilai Shareability ---- */
#define PTE_SH_TIDAK      0ULL   /* Non-shareable */
#define PTE_SH_LOKAR      2ULL   /* Outer Shareable */
#define PTE_SH_BAGI       3ULL   /* Inner Shareable */

/* ---- Nilai Access Permissions ---- */
#define PTE_AP_RW_EL1     0ULL   /* Baca/Tulis pada EL1 saja */
#define PTE_AP_RW_ALL     1ULL   /* Baca/Tulis pada EL1 dan EL0 */
#define PTE_AP_R_EL1      2ULL   /* Baca saja pada EL1 */
#define PTE_AP_R_ALL      3ULL   /* Baca saja pada EL1 dan EL0 */

/* ---- Indeks atribut MAIR_EL1 ---- */
#define MAIR_NORMAL       0ULL   /* Indeks 0: Memori Normal (Cacheable) */
#define MAIR_PERANGKAT    1ULL   /* Indeks 1: Memori Perangkat (Device) */

/* ---- Deskriptor lengkap ---- */

/* Deskriptor tabel (menuju level berikutnya) */
#define DESK_TABEL(alamat) \
    (PTE_VALID | PTE_TABEL | PTE_TETAP_ALAMAT(alamat))

/* Halaman 4 KB — memori normal (cacheable) */
#define DESK_HALAMAN_NORMAL(alamat) \
    (PTE_VALID | PTE_BLOK_HALAMAN | \
     PTE_ATTRINDX(MAIR_NORMAL) | PTE_AF | \
     PTE_AP(PTE_AP_RW_EL1) | PTE_SH(PTE_SH_BAGI) | \
     PTE_TETAP_ALAMAT(alamat))

/* Halaman 4 KB — memori perangkat (non-cacheable) */
#define DESK_HALAMAN_PERANGKAT(alamat) \
    (PTE_VALID | PTE_BLOK_HALAMAN | \
     PTE_ATTRINDX(MAIR_PERANGKAT) | PTE_AF | \
     PTE_AP(PTE_AP_RW_EL1) | PTE_SH(PTE_SH_LOKAR) | \
     PTE_TETAP_ALAMAT(alamat))

/* Blok 2 MB — memori normal (pada tingkat PMD) */
#define DESK_BLOK2M_NORMAL(alamat) \
    (PTE_VALID | PTE_BLOK_HALAMAN | \
     PTE_ATTRINDX(MAIR_NORMAL) | PTE_AF | \
     PTE_AP(PTE_AP_RW_EL1) | PTE_SH(PTE_SH_BAGI) | \
     PTE_TETAP_ALAMAT(alamat))

/* Blok 2 MB — memori perangkat (pada tingkat PMD) */
#define DESK_BLOK2M_PERANGKAT(alamat) \
    (PTE_VALID | PTE_BLOK_HALAMAN | \
     PTE_ATTRINDX(MAIR_PERANGKAT) | PTE_AF | \
     PTE_AP(PTE_AP_RW_EL1) | PTE_SH(PTE_SH_LOKAR) | \
     PTE_TETAP_ALAMAT(alamat))

/* ---- Konstanta TCR_EL1 ---- */
#define TCR_T0SZ          (16ULL)          /* 48-bit alamat virtual */
#define TCR_IRGN0_WB      (1ULL << 8)     /* Inner Write-Back */
#define TCR_ORGN0_WB      (1ULL << 10)    /* Outer Write-Back */
#define TCR_SH0_BAGI      (3ULL << 12)    /* Inner Shareable */
#define TCR_TG0_4KB       (0ULL << 14)    /* Granula 4 KB */
#define TCR_IPS_40BIT     (2ULL << 32)    /* PA 40-bit (1 TB) */

/* Nilai TCR_EL1 lengkap */
#define TCR_EL1_NILAI \
    (TCR_T0SZ | TCR_IRGN0_WB | TCR_ORGN0_WB | \
     TCR_SH0_BAGI | TCR_TG0_4KB | TCR_IPS_40BIT)

/* ---- Konstanta MAIR_EL1 ---- */
/*
 * Indeks 0: Memori Normal — Write-Back, Read/Write Allocate
 *   Attr = 0xFF (Inner: WB RA WA, Outer: WB RA WA)
 * Indeks 1: Memori Perangkat — nGnRnE
 *   Attr = 0x00 (Device-nGnRnE)
 */
#define MAIR_EL1_NILAI  0x00000000000000FFULL

/* ---- Konstanta FDT (Flattened Device Tree) ---- */
#define FDT_MAGIC         0xD00DFEED
#define FDT_BEGIN_NODE    0x00000001
#define FDT_END_NODE      0x00000002
#define FDT_PROP          0x00000003
#define FDT_NOP           0x00000004
#define FDT_END           0x00000009

/* ---- Ukuran tumpukan boot ---- */
#define TUMPUKAN_UKURAN   0x00004000  /* 16 KB */

/* ---- Batas wilayah perangkat I/O (QEMU virt) ---- */
#define WILAYAH_IO_MULAI    0x08000000ULL
#define WILAYAH_IO_SELESAI  0x0FFFFFFFULL

/* ================================================================
 * STRUKTUR DATA FDT
 * ================================================================ */

/* Header FDT (selalu big-endian) */
typedef struct {
    uint32 magic;               /* 0xD00DFEED */
    uint32 totalsize;           /* Ukuran total DTB */
    uint32 off_dt_struct;       /* Offset blok struktur */
    uint32 off_dt_strings;      /* Offset blok string */
    uint32 off_mem_rsvmap;      /* Offset peta cadangan memori */
    uint32 version;             /* Versi FDT */
    uint32 last_comp_version;   /* Versi kompatibel terakhir */
    uint32 boot_cpuid_phys;    /* CPU fisik boot */
    uint32 size_dt_strings;     /* Ukuran blok string */
    uint32 size_dt_struct;      /* Ukuran blok struktur */
} HeaderFdt;

/* ================================================================
 * VARIABEL GLOBAL
 * ================================================================ */

/* ---- Tabel halaman 4 tingkat ---- */

/* Tingkat 1: PGD (Page Global Directory) — 512 entri, 4 KB */
SEJAJAR(4096) static uint64 tabel_pgd[ENTRI_PER_TABEL]
    BAGIAN(".tabel_halaman");

/* Tingkat 2: PUD (Page Upper Directory) — 512 entri, 4 KB */
SEJAJAR(4096) static uint64 tabel_pud[ENTRI_PER_TABEL]
    BAGIAN(".tabel_halaman");

/* Tingkat 3: PMD (Page Middle Directory) — 512 entri, 4 KB */
SEJAJAR(4096) static uint64 tabel_pmd[ENTRI_PER_TABEL]
    BAGIAN(".tabel_halaman");

/* Tingkat 4: PTE (Page Table Entry) — 16 tabel, masing-masing 4 KB
 * Memetakan 32 MB pertama dengan halaman 4 KB (16 × 2 MB) */
SEJAJAR(4096) static uint64 kolam_pte[PTE_JUMLAH_TABEL * ENTRI_PER_TABEL]
    BAGIAN(".tabel_halaman");

/* Tumpukan (stack) boot */
SEJAJAR(16) static uint8 tumpukan_boot[TUMPUKAN_UKURAN]
    BAGIAN(".tumpukan");

/* Informasi RAM terdeteksi */
static ukuran_t ram_total_kb;
static uint64   ram_alamat_mulai;
static uint64   ram_ukuran_byte;

/* Penanda apakah MMU sudah aktif */
static logika mmu_aktif;

/* ================================================================
 * FUNGSI INLINE — AKSES REGISTER ARM64
 * ================================================================ */

/* Baca Exception Level saat ini */
static uint32 baca_el(void)
{
    uint64 el;
    __asm__ volatile ("mrs %0, CurrentEL" : "=r"(el));
    return (uint32)((el >> 2) & 3);
}

/* Baca SCTLR_EL1 */
static uint64 baca_sctlr_el1(void)
{
    uint64 nilai;
    __asm__ volatile ("mrs %0, sctlr_el1" : "=r"(nilai));
    return nilai;
}

/* Tulis SCTLR_EL1 */
static void tulis_sctlr_el1(uint64 nilai)
{
    __asm__ volatile ("msr sctlr_el1, %0" : : "r"(nilai) : "memory");
}

/* Baca TTBR0_EL1 */
static uint64 baca_ttbr0_el1(void)
{
    uint64 nilai;
    __asm__ volatile ("mrs %0, ttbr0_el1" : "=r"(nilai));
    return nilai;
}

/* Tulis TTBR0_EL1 */
static void tulis_ttbr0_el1(uint64 nilai)
{
    __asm__ volatile ("msr ttbr0_el1, %0" : : "r"(nilai) : "memory");
}

/* Tulis TCR_EL1 */
static void tulis_tcr_el1(uint64 nilai)
{
    __asm__ volatile ("msr tcr_el1, %0" : : "r"(nilai) : "memory");
}

/* Baca TCR_EL1 */
static uint64 baca_tcr_el1(void)
{
    uint64 nilai;
    __asm__ volatile ("mrs %0, tcr_el1" : "=r"(nilai));
    return nilai;
}

/* Tulis MAIR_EL1 */
static void tulis_mair_el1(uint64 nilai)
{
    __asm__ volatile ("msr mair_el1, %0" : : "r"(nilai) : "memory");
}

/* Tulis VBAR_EL1 — alamat dasar tabel vektor */
static void tulis_vbar_el1(uint64 nilai)
{
    __asm__ volatile ("msr vbar_el1, %0" : : "r"(nilai) : "memory");
}

/* Baca VBAR_EL1 */
static uint64 baca_vbar_el1(void)
{
    uint64 nilai;
    __asm__ volatile ("mrs %0, vbar_el1" : "=r"(nilai));
    return nilai;
}

/* Baca MIDR_EL1 — identifikasi CPU */
static uint64 baca_midr_el1(void)
{
    uint64 nilai;
    __asm__ volatile ("mrs %0, midr_el1" : "=r"(nilai));
    return nilai;
}

/* Invalidasi seluruh TLB */
static void invalidasi_tlb_semua(void)
{
    __asm__ volatile ("tlbi vmalle1is" : : : "memory");
}

/* Rintangan data (data memory barrier) */
static void rintangan_dmb(void)
{
    __asm__ volatile ("dmb ish" : : : "memory");
}

/* Rintangan sinkronisasi data (data synchronization barrier) */
static void rintangan_dsb(void)
{
    __asm__ volatile ("dsb ish" : : : "memory");
}

/* Rintangan instruksi (instruction synchronization barrier) */
static void rintangan_isb(void)
{
    __asm__ volatile ("isb" : : : "memory");
}

/* Setel register SP (Stack Pointer) */
static void setel_sp(uint64 nilai)
{
    __asm__ volatile ("mov sp, %0" : : "r"(nilai) : "memory");
}

/* Setel SP_EL1 untuk digunakan setelah ERET dari EL2 */
static void setel_sp_el1(uint64 nilai)
{
    __asm__ volatile ("msr sp_el1, %0" : : "r"(nilai) : "memory");
}

/* ================================================================
 * FUNGSI UART PL011
 * ================================================================ */

/* Akses register UART via pointer volatile */
#define REG_UART(reg) (*(volatile uint32 *)((ukuran_ptr)(UART0_ALAMAT_DASAR) + (ukuran_ptr)(reg)))

/* Kirim satu karakter melalui UART */
static void uart_kirim(char c)
{
    /* Tunggu sampai Transmit FIFO tidak penuh */
    while (REG_UART(UART_FR) & UART_FR_TXFF) {
        /* Putar tunggu */
    }
    /* Tulis karakter ke Data Register */
    REG_UART(UART_DR) = (uint32)c;
}

/* Kirim string null-terminated melalui UART */
static void uart_kirim_string(const char *str)
{
    while (*str != '\0') {
        if (*str == '\n') {
            uart_kirim('\r');
        }
        uart_kirim(*str);
        str++;
    }
}

/* Kirim nilai heksadesimal 32-bit melalui UART */
static void uart_kirim_hex(uint32 nilai)
{
    int i;
    uint8 nibel;
    char huruf;

    uart_kirim_string("0x");

    for (i = 28; i >= 0; i -= 4) {
        nibel = (uint8)((nilai >> i) & 0xF);
        if (nibel < 10) {
            huruf = (char)('0' + nibel);
        } else {
            huruf = (char)('A' + nibel - 10);
        }
        uart_kirim(huruf);
    }
}

/* Kirim nilai heksadesimal 64-bit melalui UART */
static void uart_kirim_hex64(uint64 nilai)
{
    int i;
    uint8 nibel;
    char huruf;

    uart_kirim_string("0x");

    for (i = 60; i >= 0; i -= 4) {
        nibel = (uint8)((nilai >> i) & 0xF);
        if (nibel < 10) {
            huruf = (char)('0' + nibel);
        } else {
            huruf = (char)('A' + nibel - 10);
        }
        uart_kirim(huruf);
    }
}

/* Baca satu karakter dari UART */
static char uart_terima(void)
{
    while (REG_UART(UART_FR) & UART_FR_RXFE) {
        /* Tunggu */
    }
    return (char)(REG_UART(UART_DR) & 0xFF);
}

/* Inisialisasi UART PL011 */
static void uart_siapkan(void)
{
    uint32 tmp;

    /* Langkah 1: Nonaktifkan UART terlebih dahulu */
    REG_UART(UART_CR) = 0;

    /* Langkah 2: Tunggu sampai transmit selesai */
    while (REG_UART(UART_FR) & UART_FR_BUSY) {
        /* Tunggu */
    }

    /* Langkah 3: Bersihkan semua interupsi tertunda */
    REG_UART(UART_ICR) = 0x07FF;

    /* Langkah 4: Setel baud rate — 115200 bps
     * Asumsi clock UART = 24 MHz (QEMU virt)
     * IBRD = 24000000 / (16 * 115200) = 13.02 → 13
     * FBRD = 0.02 * 64 + 0.5 = 1.78 → 1
     */
    REG_UART(UART_IBRD) = 13;
    REG_UART(UART_FBRD) = 1;

    /* Langkah 5: Setel format — 8 data bit, tanpa parity, 1 stop bit */
    tmp = UART_LCR_H_WLEN8 | UART_LCR_H_FEN;
    REG_UART(UART_LCR_H) = tmp;

    /* Langkah 6: Nonaktifkan semua interupsi UART */
    REG_UART(UART_IMSC) = 0;

    /* Langkah 7: Aktifkan UART — transmit dan receive */
    REG_UART(UART_CR) = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
}

/* ================================================================
 * HANDLER EKSEPSI ARM64
 * ================================================================ */

/* Handler umum — cetak pesan dan berhenti */
__attribute__((used)) void handler_eksepsi_berhenti(void)
{
    uart_kirim_string("[PEMICU] EKSEPSI FATAL! Sistem berhenti.\n");
    hentikan_cpu();
}

/* Handler IRQ — cetak pesan dan berhenti untuk sekarang */
__attribute__((used)) void handler_irq_berhenti(void)
{
    uart_kirim_string("[PEMICU] IRQ diterima (belum tertangani).\n");
    hentikan_cpu();
}

/* Forward declaration — tabel vektor didefinisikan di bawah */
extern void vektor_eksepsi(void);

/* ================================================================
 * TABEL VEKTOR EKSEPSI ARM64
 *
 * Pada AArch64, tabel vektor memiliki 16 entri,
 * masing-masing sejajarkan 128 byte (0x80).
 * Total ukuran = 16 × 128 = 2048 byte (2 KB).
 *
 * Entri dibagi menjadi 4 kelompok:
 *   1. Eksepsi dari EL1 dengan SP0
 *   2. Eksepsi dari EL1 dengan SPx
 *   3. Eksepsi dari EL0 mode AArch64
 *   4. Eksepsi dari EL0 mode AArch32
 *
 * Setiap kelompok memiliki 4 vektor:
 *   - Sinkron (Synchronous)
 *   - IRQ
 *   - FIQ
 *   - SError
 *
 * Untuk pemicu boot, semua handler hanya berhenti (hang).
 * Kernel akan memasang handler yang sesuai nanti.
 * ================================================================ */

__attribute__((used)) SEJAJAR(2048) BAGIAN(".vektor")
void vektor_eksepsi(void)
{
    __asm__ volatile (
        /* ===== EL1 dengan SP0 ===== */
        ".align 7\n"                   /* Sinkron EL1_SP0 */
        "b handler_eksepsi_berhenti\n"
        ".align 7\n"                   /* IRQ EL1_SP0 */
        "b handler_irq_berhenti\n"
        ".align 7\n"                   /* FIQ EL1_SP0 */
        "b handler_eksepsi_berhenti\n"
        ".align 7\n"                   /* SError EL1_SP0 */
        "b handler_eksepsi_berhenti\n"

        /* ===== EL1 dengan SPx ===== */
        ".align 7\n"                   /* Sinkron EL1_SPx */
        "b handler_eksepsi_berhenti\n"
        ".align 7\n"                   /* IRQ EL1_SPx */
        "b handler_irq_berhenti\n"
        ".align 7\n"                   /* FIQ EL1_SPx */
        "b handler_eksepsi_berhenti\n"
        ".align 7\n"                   /* SError EL1_SPx */
        "b handler_eksepsi_berhenti\n"

        /* ===== EL0 AArch64 ===== */
        ".align 7\n"                   /* Sinkron EL0_A64 */
        "b handler_eksepsi_berhenti\n"
        ".align 7\n"                   /* IRQ EL0_A64 */
        "b handler_irq_berhenti\n"
        ".align 7\n"                   /* FIQ EL0_A64 */
        "b handler_eksepsi_berhenti\n"
        ".align 7\n"                   /* SError EL0_A64 */
        "b handler_eksepsi_berhenti\n"

        /* ===== EL0 AArch32 ===== */
        ".align 7\n"                   /* Sinkron EL0_A32 */
        "b handler_eksepsi_berhenti\n"
        ".align 7\n"                   /* IRQ EL0_A32 */
        "b handler_irq_berhenti\n"
        ".align 7\n"                   /* FIQ EL0_A32 */
        "b handler_eksepsi_berhenti\n"
        ".align 7\n"                   /* SError EL0_A32 */
        "b handler_eksepsi_berhenti\n"
    );
}

/* ================================================================
 * TURUN DARI EL2 KE EL1
 *
 * U-Boot biasanya memulai kernel pada EL2.
 * Kernel Arsitek berjalan pada EL1.
 * Fungsi ini menurunkan level eksepsi dari EL2 ke EL1
 * menggunakan instruksi ERET.
 * ================================================================ */

static void turun_ke_el1(void)
{
    uint64 el;
    uint64 reg_val;

    el = (uint64)baca_el();

    if (el == 1) {
        uart_kirim_string("[PEMICU] Sudah di EL1, tidak perlu turun.\n");
        return;
    }

    if (el != 2) {
        uart_kirim_string("[PEMICU] PERINGATAN: EL tidak didukung: ");
        uart_kirim_hex((uint32)el);
        uart_kirim('\n');
        return;
    }

    uart_kirim_string("[PEMICU] Berada di EL2, turun ke EL1...\n");

    /* Langkah 1: Nonaktifkan trap EL2 pada CPTR_EL2
     * (izinkan akses FP/SIMD dari EL1) */
    __asm__ volatile ("msr cptr_el2, xzr");

    /* Langkah 2: Nonaktifkan trap EL2 pada HCR_EL2 */
    __asm__ volatile ("msr hcr_el2, xzr");

    /* Langkah 3: Aktifkan akses timer fisik dari EL1 */
    reg_val = 3;
    __asm__ volatile ("msr cnthctl_el2, %0" : : "r"(reg_val));

    /* Langkah 4: Setel SCTLR_EL1 ke keadaan awal
     * (MMU dan cache nonaktif) */
    reg_val = 0;
    tulis_sctlr_el1(reg_val);

    /* Langkah 5: Set SP_EL1 agar tumpukan tersedia setelah ERET */
    setel_sp_el1((uint64)tumpukan_boot + TUMPUKAN_UKURAN);

    /* Langkah 6: Set SPSR_EL2 — kembali ke EL1h (mode EL1 dengan SP_EL1)
     * DAIF = 1111 (semua interupsi dimatikan)
     * M[4:0] = 00101 (EL1h) */
    reg_val = 0x00000000000003C5ULL;
    __asm__ volatile ("msr spsr_el2, %0" : : "r"(reg_val));

    /* Langkah 7: Set ELR_EL2 ke alamat setelah ERET,
     * lalu jalankan ERET untuk kembali ke EL1 */
    __asm__ volatile (
        "adr x0, 1f\n\t"         /* Alamat label 1 (setelah eret) */
        "msr elr_el2, x0\n\t"
        "eret\n"                  /* Kembali ke EL1 pada label 1 */
        "1:\n\t"                  /* Eksekusi berlanjut di EL1 */
        :
        :
        : "x0"
    );

    /* Sekarang berjalan di EL1 */
    uart_kirim_string("[PEMICU] Turun dari EL2 ke EL1 berhasil.\n");
}

/* ================================================================
 * INISIALISASI GICv3
 * ================================================================ */

/* Akses register GIC Distributor */
#define REG_GICD(offset) \
    (*(volatile uint32 *)((ukuran_ptr)(GICV3_GICD_BASE) + (ukuran_ptr)(offset)))

/* Akses register GIC Redistributor */
#define REG_GICR(offset) \
    (*(volatile uint32 *)((ukuran_ptr)(GICV3_GICR_BASE) + (ukuran_ptr)(offset)))

/* Nonaktifkan semua interupsi pada GIC Distributor */
static void gic_nonaktifkan_distributor(void)
{
    uint32 i;
    uint32 maks_irq;

    /* Nonaktifkan Distributor */
    REG_GICD(GICD_CTLR) = 0;

    /* Baca jumlah IRQ yang didukung */
    maks_irq = (REG_GICD(GICD_TYPER) & 0x1F) + 1;
    maks_irq = maks_irq * 32;
    if (maks_irq > 1020) {
        maks_irq = 1020;
    }

    /* Nonaktifkan setiap IRQ */
    for (i = 0; i < maks_irq; i += 32) {
        REG_GICD(GICD_ICENABLE + (i / 32) * 4) = 0xFFFFFFFF;
    }

    /* Bersihkan IRQ tertunda */
    for (i = 0; i < maks_irq; i += 32) {
        REG_GICD(GICD_ICPEND + (i / 32) * 4) = 0xFFFFFFFF;
    }

    /* Setel prioritas default */
    for (i = 0; i < maks_irq; i += 4) {
        REG_GICD(GICD_IPRIORITY + (i / 4) * 4) = 0xA0A0A0A0;
    }

    /* Setel konfigurasi default (level-triggered) */
    for (i = 0; i < maks_irq; i += 16) {
        REG_GICD(GICD_ICFG + (i / 16) * 4) = 0;
    }

    /* Aktifkan Distributor (Grup 1 Non-secure) */
    REG_GICD(GICD_CTLR) = 0x2;
}

/* Siapkan GIC CPU Interface via system register */
static void gic_siapkan_cpu(void)
{
    uint64 reg_val;

    /* Aktifkan System Register Interface (ICC_SRE_EL1) */
    __asm__ volatile ("mrs %0, icc_sre_el1" : "=r"(reg_val));
    reg_val |= 0x7;  /* Aktifkan SRE, Disable bypass */
    __asm__ volatile ("msr icc_sre_el1, %0" : : "r"(reg_val));

    /* Setel mask prioritas — izinkan semua prioritas (ICC_PMR_EL1) */
    reg_val = 0xFF;
    __asm__ volatile ("msr icc_pmr_el1, %0" : : "r"(reg_val));

    /* Setel Binary Point Register (ICC_BPR1_EL1) */
    reg_val = 0;
    __asm__ volatile ("msr icc_bpr1_el1, %0" : : "r"(reg_val));

    /* Aktifkan Grup 1 interupsi (ICC_IGRPEN1_EL1) */
    reg_val = 1;
    __asm__ volatile ("msr icc_igrpen1_el1, %0" : : "r"(reg_val));
}

/* ================================================================
 * SIAPKAN TABEL VEKTOR EKSEPSI
 * ================================================================ */

static void siapkan_vektor(void)
{
    /* Atur VBAR_EL1 ke alamat fungsi tabel vektor */
    tulis_vbar_el1((uint64)vektor_eksepsi);
    rintangan_isb();

    uart_kirim_string("[PEMICU] Tabel vektor eksepsi siap pada ");
    uart_kirim_hex64((uint64)vektor_eksepsi);
    uart_kirim('\n');
}

/* ================================================================
 * SIAPKAN TABEL HALAMAN 4 TINGKAT (PGD→PUD→PMD→PTE)
 *
 * Pemetaan identitas 1 GB pertama:
 *   - 32 MB pertama: menggunakan halaman 4 KB (PTE)
 *     → menunjukkan struktur 4 tingkat penuh
 *     → 16 tabel PTE × 512 entri × 4 KB = 32 MB
 *   - 992 MB sisanya: menggunakan blok 2 MB (PMD)
 *     → lebih efisien untuk memori besar
 *     → 496 entri blok × 2 MB = 992 MB
 *
 * Struktur pemetaan:
 *   PGD[0]  → tabel_pud
 *   PUD[0]  → tabel_pmd
 *   PMD[0..15]  → kolam_pte[0..15] (tabel PTE, 4 KB per halaman)
 *   PMD[16..511] → blok 2 MB langsung
 * ================================================================ */

/* Tentukan apakah alamat berada di wilayah perangkat I/O */
static logika adalah_wilayah_perangkat(uint64 alamat)
{
    /* Wilayah I/O QEMU virt: 0x08000000 - 0x0FFFFFFF */
    if (alamat >= WILAYAH_IO_MULAI && alamat <= WILAYAH_IO_SELESAI) {
        return BENAR;
    }
    return SALAH;
}

/* Dapatkan pointer ke tabel PTE ke-i dari kolam */
static uint64 *dapat_tabel_pte(int indeks)
{
    return &kolam_pte[indeks * ENTRI_PER_TABEL];
}

/* Siapkan tabel halaman dengan pemetaan identitas */
static void siapkan_tabel_halaman(void)
{
    int i;
    int j;
    uint64 alamat;
    uint64 deskriptor;

    /* Langkah 1: Inisialisasi seluruh tabel ke nol */
    isi_memori(tabel_pgd, 0, sizeof(tabel_pgd));
    isi_memori(tabel_pud, 0, sizeof(tabel_pud));
    isi_memori(tabel_pmd, 0, sizeof(tabel_pmd));
    isi_memori(kolam_pte, 0, sizeof(kolam_pte));

    /* Langkah 2: Hubungkan PGD → PUD
     * PGD[0] berisi deskriptor tabel yang menunjuk ke tabel_pud */
    tabel_pgd[0] = DESK_TABEL((uint64)tabel_pud);

    /* Langkah 3: Hubungkan PUD → PMD
     * PUD[0] berisi deskriptor tabel yang menunjuk ke tabel_pmd */
    tabel_pud[0] = DESK_TABEL((uint64)tabel_pmd);

    /* Langkah 4: Isi tabel PMD
     * PMD[0..15]: deskriptor tabel yang menunjuk ke tabel PTE
     * PMD[16..511]: deskriptor blok 2 MB langsung */

    for (i = 0; i < ENTRI_PER_TABEL; i++) {
        alamat = (uint64)i * BLOK2M_UKURAN;

        if (i < PTE_JUMLAH_TABEL) {
            /* Deskriptor tabel: menunjuk ke tabel PTE ke-i */
            tabel_pmd[i] = DESK_TABEL((uint64)dapat_tabel_pte(i));
        } else {
            /* Deskriptor blok 2 MB */
            if (adalah_wilayah_perangkat(alamat)) {
                tabel_pmd[i] = DESK_BLOK2M_PERANGKAT(alamat);
            } else {
                tabel_pmd[i] = DESK_BLOK2M_NORMAL(alamat);
            }
        }
    }

    /* Langkah 5: Isi tabel PTE (untuk 32 MB pertama)
     * Setiap tabel PTE memetakan 512 halaman × 4 KB = 2 MB */
    for (i = 0; i < PTE_JUMLAH_TABEL; i++) {
        for (j = 0; j < ENTRI_PER_TABEL; j++) {
            /* Hitung alamat fisik untuk entri PTE ini */
            alamat = (uint64)i * BLOK2M_UKURAN + (uint64)j * HALAMAN_UKURAN;

            if (adalah_wilayah_perangkat(alamat)) {
                deskriptor = DESK_HALAMAN_PERANGKAT(alamat);
            } else {
                deskriptor = DESK_HALAMAN_NORMAL(alamat);
            }

            kolam_pte[i * ENTRI_PER_TABEL + j] = deskriptor;
        }
    }

    uart_kirim_string("[PEMICU] Tabel halaman 4 tingkat siap:\n");
    uart_kirim_string("  PGD pada ");
    uart_kirim_hex64((uint64)tabel_pgd);
    uart_kirim('\n');
    uart_kirim_string("  PUD pada ");
    uart_kirim_hex64((uint64)tabel_pud);
    uart_kirim('\n');
    uart_kirim_string("  PMD pada ");
    uart_kirim_hex64((uint64)tabel_pmd);
    uart_kirim('\n');
    uart_kirim_string("  PTE (16 tabel) pada ");
    uart_kirim_hex64((uint64)kolam_pte);
    uart_kirim('\n');
    uart_kirim_string("  Pemetaan: 32 MB halaman 4KB + 992 MB blok 2MB\n");
}

/* Aktifkan MMU pada EL1 */
static void aktifkan_mmu(void)
{
    uint64 sctlr;

    /* Langkah 1: Pastikan MMU belum aktif */
    sctlr = baca_sctlr_el1();
    if (sctlr & SCTLR_M) {
        uart_kirim_string("[PEMICU] PERINGATAN: MMU sudah aktif!\n");
    }

    /* Langkah 2: Invalidasi seluruh cache dan TLB */
    invalidasi_tlb_semua();
    rintangan_dsb();

    /* Langkah 3: Setel MAIR_EL1 — atribut memori
     * Indeks 0: Normal Cacheable (0xFF)
     * Indeks 1: Device-nGnRnE (0x00) */
    tulis_mair_el1(MAIR_EL1_NILAI);

    /* Langkah 4: Setel TCR_EL1 — parameter tabel halaman */
    tulis_tcr_el1(TCR_EL1_NILAI);

    /* Langkah 5: Setel TTBR0_EL1 — alamat tabel PGD */
    tulis_ttbr0_el1((uint64)tabel_pgd);

    /* Langkah 6: Rintangan memori untuk memastikan semua
     * penulisan register selesai sebelum mengaktifkan MMU */
    rintangan_dsb();
    rintangan_isb();

    /* Langkah 7: Aktifkan MMU, cache data, cache instruksi */
    sctlr = baca_sctlr_el1();
    sctlr |= SCTLR_M;     /* Aktifkan MMU */
    sctlr |= SCTLR_C;     /* Aktifkan cache data */
    sctlr |= SCTLR_I;     /* Aktifkan cache instruksi */
    sctlr |= SCTLR_A;     /* Aktifkan pengecekan alignment */

    tulis_sctlr_el1(sctlr);

    /* Langkah 8: Rintangan instruksi setelah mengaktifkan MMU */
    rintangan_isb();

    mmu_aktif = BENAR;

    uart_kirim_string("[PEMICU] MMU aktif. SCTLR_EL1 = ");
    uart_kirim_hex64(baca_sctlr_el1());
    uart_kirim('\n');
}

/* ================================================================
 * DETEKSI MEMORI DARI DEVICE TREE (DTB)
 * ================================================================ */

/* Konversi big-endian 32-bit ke host byte order */
static uint32 fdt_be32_ke_host(const uint8 *p)
{
    return ((uint32)p[0] << 24) | ((uint32)p[1] << 16) |
           ((uint32)p[2] << 8)  | (uint32)p[3];
}

/* Konversi big-endian 64-bit ke host byte order */
static uint64 fdt_be64_ke_host(const uint8 *p)
{
    return ((uint64)fdt_be32_ke_host(p) << 32) |
           (uint64)fdt_be32_ke_host(p + 4);
}

/* Periksa apakah string dimulai dengan awalan tertentu */
static logika string_mulai_dengan(const char *str, const char *awalan)
{
    while (*awalan != '\0') {
        if (*str != *awalan) {
            return SALAH;
        }
        str++;
        awalan++;
    }
    return BENAR;
}

/* Periksa apakah string sama */
static logika string_sama(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return SALAH;
        }
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0') ? BENAR : SALAH;
}

/* Hitung panjang string (aman, maks 256 karakter) */
static ukuran_t panjang_string_aman(const char *str, ukuran_t maks)
{
    ukuran_t pjg;
    pjg = 0;
    while (pjg < maks && str[pjg] != '\0') {
        pjg++;
    }
    return pjg;
}

/* Deteksi memori dari DTB */
static void deteksi_memori_dtb(uint64 alamat_dtb)
{
    HeaderFdt *header;
    const uint8 *struct_blok;
    const uint8 *string_blok;
    const uint8 *pos;
    uint32 token;
    uint32 panjang_prop;
    uint32 nama_offset;
    const char *nama_node;
    const char *nama_prop;
    int kedalaman;
    logika dalam_memory;
    uint32 address_cells;
    uint32 size_cells;
    ukuran_t panjang_nama;
    uint32 panjang_pad;

    /* Default jika DTB tidak valid */
    ram_total_kb = 0;
    ram_alamat_mulai = 0;
    ram_ukuran_byte = 0;
    address_cells = 2;   /* Default ARM64 */
    size_cells = 2;

    if (alamat_dtb == 0) {
        uart_kirim_string("[PEMICU] Tidak ada DTB. Asumsi RAM 1024 MB di 0x0.\n");
        ram_alamat_mulai = 0;
        ram_ukuran_byte = 1024ULL * 1024 * 1024;
        ram_total_kb = 1024 * 1024;
        return;
    }

    header = (HeaderFdt *)(ukuran_ptr)alamat_dtb;

    /* Validasi magic number FDT */
    if (fdt_be32_ke_host((const uint8 *)&header->magic) != FDT_MAGIC) {
        uart_kirim_string("[PEMICU] DTB tidak valid! Asumsi 1024 MB.\n");
        ram_alamat_mulai = 0;
        ram_ukuran_byte = 1024ULL * 1024 * 1024;
        ram_total_kb = 1024 * 1024;
        return;
    }

    /* Ambil alamat blok struktur dan string */
    struct_blok = (const uint8 *)alamat_dtb +
                  fdt_be32_ke_host((const uint8 *)&header->off_dt_struct);
    string_blok = (const uint8 *)alamat_dtb +
                  fdt_be32_ke_host((const uint8 *)&header->off_dt_strings);

    pos = struct_blok;
    kedalaman = 0;
    dalam_memory = SALAH;

    /* Telusuri blok struktur DTB */
    while (1) {
        token = fdt_be32_ke_host(pos);
        pos += 4;

        if (token == FDT_BEGIN_NODE) {
            /* Baca nama node */
            nama_node = (const char *)pos;

            /* Cek apakah ini node "memory" atau "memory@..." */
            if (kedalaman == 1) {
                dalam_memory = string_mulai_dengan(nama_node, "memory");
            } else {
                dalam_memory = SALAH;
            }

            /* Lewati nama node + padding ke kelipatan 4 byte */
            panjang_nama = panjang_string_aman(nama_node, 256);
            panjang_nama++;  /* Hitung null terminator */
            panjang_pad = (panjang_nama + 3) & ~((ukuran_t)3);
            pos += panjang_pad;

            kedalaman++;
        }
        else if (token == FDT_END_NODE) {
            kedalaman--;
            if (kedalaman <= 1) {
                dalam_memory = SALAH;
            }
            if (kedalaman == 0) {
                break;  /* Node root ditutup */
            }
        }
        else if (token == FDT_PROP) {
            /* Baca panjang dan offset nama properti */
            panjang_prop = fdt_be32_ke_host(pos);
            nama_offset = fdt_be32_ke_host(pos + 4);
            pos += 8;

            /* Dapatkan nama properti dari blok string */
            nama_prop = (const char *)string_blok + nama_offset;

            if (dalam_memory && string_sama(nama_prop, "reg")) {
                /* Properti "reg" dalam node memory:
                 * Berisi pasangan (alamat, ukuran).
                 * Jumlah sel ditentukan oleh #address-cells dan #size-cells. */
                if (address_cells == 2) {
                    ram_alamat_mulai = fdt_be64_ke_host(pos);
                } else {
                    ram_alamat_mulai = (uint64)fdt_be32_ke_host(pos);
                }

                if (size_cells == 2) {
                    ram_ukuran_byte = fdt_be64_ke_host(
                        pos + address_cells * 4);
                } else {
                    ram_ukuran_byte = (uint64)fdt_be32_ke_host(
                        pos + address_cells * 4);
                }

                ram_total_kb = (ukuran_t)(ram_ukuran_byte / 1024);
            }

            /* Juga baca #address-cells dan #size-cells dari root */
            if (kedalaman == 1) {
                if (string_sama(nama_prop, "#address-cells")) {
                    address_cells = fdt_be32_ke_host(pos);
                }
                if (string_sama(nama_prop, "#size-cells")) {
                    size_cells = fdt_be32_ke_host(pos);
                }
            }

            /* Lewati nilai properti + padding ke kelipatan 4 byte */
            panjang_pad = (panjang_prop + 3) & ~((uint32)3);
            pos += panjang_pad;
        }
        else if (token == FDT_NOP) {
            /* Lewati token NOP */
        }
        else if (token == FDT_END) {
            break;
        }
        else {
            /* Token tidak dikenal — berhenti */
            break;
        }
    }

    /* Verifikasi hasil */
    if (ram_ukuran_byte == 0) {
        uart_kirim_string("[PEMICU] Node memory tidak ditemukan. Asumsi 1024 MB.\n");
        ram_alamat_mulai = 0;
        ram_ukuran_byte = 1024ULL * 1024 * 1024;
        ram_total_kb = 1024 * 1024;
    } else {
        uint64 ukuran_mb;

        ukuran_mb = ram_ukuran_byte / (1024 * 1024);
        uart_kirim_string("[PEMICU] DTB: RAM pada ");
        uart_kirim_hex64(ram_alamat_mulai);
        uart_kirim_string(", ukuran ");
        uart_kirim_hex64(ukuran_mb);
        uart_kirim_string(" MB\n");
    }
}

/* ================================================================
 * INISIALISASI PROSESOR ARM64
 * ================================================================ */

static void inisialisasi_prosesor(void)
{
    uint64 midr;
    uint64 sctlr;

    /* Langkah 1: Nonaktifkan semua interupsi */
    nonaktifkan_interupsi();

    /* Langkah 2: Baca identitas CPU */
    midr = baca_midr_el1();
    uart_kirim_string("[PEMICU] CPU: MIDR_EL1=");
    uart_kirim_hex64(midr);
    uart_kirim('\n');

    /* Langkah 3: Nonaktifkan MMU dan cache jika masih aktif */
    sctlr = baca_sctlr_el1();
    if (sctlr & SCTLR_M) {
        sctlr &= ~SCTLR_M;   /* Nonaktifkan MMU */
        sctlr &= ~SCTLR_C;   /* Nonaktifkan cache data */
        sctlr &= ~SCTLR_I;   /* Nonaktifkan cache instruksi */
        tulis_sctlr_el1(sctlr);
        rintangan_isb();
    }

    /* Langkah 4: Invalidasi TLB */
    invalidasi_tlb_semua();
    rintangan_dsb();

    /* Langkah 5: Setel tumpukan (stack) */
    setel_sp((uint64)tumpukan_boot + TUMPUKAN_UKURAN);

    uart_kirim_string("[PEMICU] Prosesor ARM64 terinisialisasi. SP=");
    uart_kirim_hex64((uint64)tumpukan_boot + TUMPUKAN_UKURAN);
    uart_kirim_string(", EL=");
    uart_kirim_hex((uint32)baca_el());
    uart_kirim('\n');
}

/* ================================================================
 * TITIK MASUK PEMICU ARM64
 *
 * Dipanggil oleh U-Boot dengan register:
 *   x0 = alamat Device Tree Blob (DTB)
 *   x1 = 0 (reserved)
 *   x2 = 0 (reserved)
 *   x3 = 0 (reserved)
 * ================================================================ */

void pemicu_utama(uint64 x0, uint64 x1, uint64 x2, uint64 x3)
{
    uint64 alamat_dtb;
    uint64 ram_mb;

    /* Simpan alamat DTB sebelum register berubah */
    alamat_dtb = x0;

    /* Langkah 1: Inisialisasi UART terlebih dahulu
     * (U-Boot sudah menyediakan tumpukan, sehingga pemanggilan
     * fungsi ini aman sebelum tumpukan kita sendiri disiapkan) */
    uart_siapkan();

    /* Langkah 2: Inisialisasi prosesor (nonaktifkan interupsi, MMU, cache) */
    inisialisasi_prosesor();

    uart_kirim_string("\n========================================\n");
    uart_kirim_string("  PEMICU ARM64 — Kernel Arsitek\n");
    uart_kirim_string("========================================\n\n");

    /* Langkah 3: Turun dari EL2 ke EL1 jika diperlukan */
    turun_ke_el1();

    /* Langkah 4: Siapkan tabel vektor eksepsi */
    siapkan_vektor();

    /* Langkah 5: Inisialisasi GICv3 */
    gic_nonaktifkan_distributor();
    gic_siapkan_cpu();
    uart_kirim_string("[PEMICU] GICv3 terinisialisasi\n");

    /* Langkah 6: Deteksi memori dari Device Tree */
    uart_kirim_string("[PEMICU] Membaca DTB pada ");
    uart_kirim_hex64(alamat_dtb);
    uart_kirim('\n');
    deteksi_memori_dtb(alamat_dtb);

    /* Langkah 7: Siapkan tabel halaman 4 tingkat dan aktifkan MMU */
    siapkan_tabel_halaman();
    aktifkan_mmu();

    /* Langkah 8: Tampilkan ringkasan */
    ram_mb = ram_ukuran_byte / (1024 * 1024);
    uart_kirim_string("[PEMICU] RAM terdeteksi: ");
    uart_kirim_hex64(ram_mb);
    uart_kirim_string(" MB pada alamat ");
    uart_kirim_hex64(ram_alamat_mulai);
    uart_kirim('\n');

    uart_kirim_string("[PEMICU] MMU aktif, cache aktif, paging 4 tingkat\n");
    uart_kirim_string("[PEMICU] Lompat ke titik masuk kernel...\n\n");

    /* Langkah 9: Lompat ke titik masuk kernel utama */
    arsitek_mulai();

    /* Jika kembali dari kernel (tidak seharusnya), hentikan */
    uart_kirim_string("[PEMICU] FATAL: Kernel kembali! Sistem berhenti.\n");
    hentikan_cpu();
}
