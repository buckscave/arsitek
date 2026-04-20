/*
 * pemicu.c
 *
 * Kode pemicu (boot) untuk arsitektur ARM32.
 * Dipanggil oleh bootloader (misalnya U-Boot).
 *
 * Tugas:
 *   1. Inisialisasi prosesor ARM32
 *   2. Siapkan tabel vektor eksepsi
 *   3. Siapkan MMU dan paging
 *   4. Deteksi memori dari informasi bootloader
 *   5. Lompat ke titik masuk kernel
 */

#include "../../lampiran/arsitek.h"
#include "../../lampiran/mesin.h"

/* ================================================================
 * FALLBACK DEFINISI ARM32
 *
 * Jika kompilasi dilakukan tanpa flag -DARSITEK_ARM32=1,
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

#ifndef GIC_DISTRIBUTOR_BASE
#define GIC_DISTRIBUTOR_BASE   0x2C001000
#define GIC_CPU_INTERFACE_BASE 0x2C002000
#define GICD_CTRL              0x0000
#define GICD_TYPER             0x0004
#define GICD_ISENABLE          0x0100
#define GICD_ICENABLE          0x0180
#define GICD_ISPEND            0x0200
#define GICD_ICPEND            0x0280
#define GICD_ISACTIVE          0x0300
#define GICD_ICACTIVE          0x0380
#define GICD_IPRIORITY         0x0400
#define GICD_ITARGETS          0x0800
#define GICD_ICFG              0x0C00
#define GICD_SGIR              0x0F00
#define GICC_CTRL              0x0000
#define GICC_PMR               0x0004
#define GICC_IAR               0x000C
#define GICC_EOIR              0x0010
#endif

#ifndef KERNEL_ALAMAT_DASAR
#define KERNEL_ALAMAT_DASAR  0x00010000  /* Kernel dimuat di 64 KB */
#endif

/* ================================================================
 * KONSTANTA PLATFORM ARM32
 * ================================================================ */

/* ---- Alamat dasar UART PL011 ---- */
#define UART0_ALAMAT_DASAR   0x101F1000  /* PL011 pada VersatilePB */

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
#define UART_LCR_H_STP2  BIT(3)   /* 2 stop bit */
#define UART_LCR_H_PEN   BIT(1)   /* Parity enable */

/* ---- Konstanta MMU ARM32 ---- */
#define SEKSI_UKURAN       0x00100000  /* 1 MB per seksi */
#define SEKSI_JUMLAH       4096        /* 4096 seksi = 4 GB ruang alamat */
#define SEKSI_MAKS_PETA    1024        /* Petakan 1 GB pertama (1024 seksi) */

/* Deskriptor seksi (first-level descriptor, format pendek ARMv7):
 *   [31:20]  = PA[31:20] alamat dasar fisik
 *   [19]     = NS (Non-Secure)
 *   [18]     = 0 (bukan superseksi)
 *   [17]     = nG (non-Global)
 *   [16]     = S (Shared)
 *   [15:14]  = TEX[1:0]
 *   [13:12]  = TEX[2], AP[2]
 *   [11:10]  = AP[1:0] (izin akses)
 *   [9]      = implementasi
 *   [8:5]    = Domain
 *   [4]      = 0
 *   [3]      = C (Cacheable)
 *   [2]      = B (Bufferable)
 *   [1:0]    = 10 (tipe = seksi)
 */

/* Deskriptor untuk memori normal (cacheable, bufferable) */
#define SEKSI_MEMORI_NORMAL(alamat) \
    (((alamat) & 0xFFF00000) | 0x00000C0E)
    /* AP=11 (akses penuh), C=1, B=1, tipe=seksi */

/* Deskriptor untuk memori perangkat (non-cacheable, non-bufferable) */
#define SEKSI_MEMORI_PERANGKAT(alamat) \
    (((alamat) & 0xFFF00000) | 0x00000C02)
    /* AP=11 (akses penuh), C=0, B=0, tipe=seksi */

/* Deskriptor kosong (tidak terpetakan) */
#define SEKSI_KOSONG  0x00000000

/* ---- Domain Access Control ---- */
#define DACR_DOMAIN0_KLIEN   0x00000001  /* Domain 0 = klien (cek AP) */
#define DACR_DOMAIN0_MANAJER 0x00000003  /* Domain 0 = manajer (abaikan AP) */
#define DACR_SEMUA_MANAJER   0x55555555  /* Semua domain = manajer */

/* ---- Konstanta ATAG ---- */
#define ATAG_NONE      0x00000000
#define ATAG_CORE      0x54410001
#define ATAG_MEM       0x54410002
#define ATAG_VIDEOTEXT 0x54410003
#define ATAG_RAMDISK   0x54410004
#define ATAG_INITRD2   0x54410006
#define ATAG_SERIAL    0x54410007
#define ATAG_CMDLINE   0x54410009

/* ---- Ukuran tumpukan boot ---- */
#define TUMPUKAN_UKURAN  0x00004000  /* 16 KB tumpukan boot */

/* ---- Batas wilayah perangkat I/O (pada VersatilePB) ---- */
#define WILAYAH_IO_MULAI   0x10000000  /* Awal wilayah I/O */
#define WILAYAH_IO_SELESAI 0x1FFFFFFF  /* Akhir wilayah I/O */
#define WILAYAH_IO_GIC     0x2C000000  /* Awal wilayah GIC */

/* ================================================================
 * STRUKTUR DATA ATAG
 * ================================================================ */

/* Header ATAG umum */
typedef struct {
    uint32 ukuran;   /* Ukuran dalam word (termasuk header) */
    uint32 tipe;     /* Tipe ATAG */
} HeaderAtag;

/* Isi ATAG_CORE */
typedef struct {
    uint32 bendera;    /* Bendera (1 = memuat ramdisk) */
    uint32 halaman;    /* Ukuran halaman MSB */
    uint32 akar;       /* Nomor perangkat root MSB */
} IsiAtagCore;

/* Isi ATAG_MEM */
typedef struct {
    uint32 ukuran;     /* Ukuran memori dalam byte */
    uint32 mulai;      /* Alamat awal fisik */
} IsiAtagMem;

/* Isi ATAG_CMDLINE */
typedef struct {
    char baris[1];     /* Baris perintah (variable length) */
} IsiAtagCmdline;

/* Struktur ATAG lengkap */
typedef struct StrukturAtag {
    HeaderAtag header;
    union {
        IsiAtagCore    core;
        IsiAtagMem     mem;
        IsiAtagCmdline cmdline;
        uint32         data[8];   /* Data generik */
    } isi;
} StrukturAtag;

/* ================================================================
 * VARIABEL GLOBAL
 * ================================================================ */

/* Tabel halaman tingkat pertama (page table)
 * Harus sejajarkan 16 KB (syarat TTBR0 pada ARM32).
 * 4096 entri x 4 byte = 16 KB.
 */
SEJAJAR(16384) static uint32 tabel_halaman[SEKSI_JUMLAH]
    BAGIAN(".tabel_halaman");

/* Tumpukan (stack) boot */
SEJAJAR(8) static uint8 tumpukan_boot[TUMPUKAN_UKURAN]
    BAGIAN(".tumpukan");

/* Informasi RAM terdeteksi */
static ukuran_t ram_total_kb;
static ukuran_t ram_alamat_mulai;
static ukuran_t ram_ukuran_byte;

/* Penanda apakah MMU sudah aktif */
static logika mmu_aktif;

/* ================================================================
 * FUNGSI INLINE — AKSES REGISTER CP15 ARM32
 * ================================================================ */

/* Baca register CPSR (Current Program Status Register) */
static uint32 baca_cpsr(void)
{
    uint32 nilai;
    __asm__ volatile ("mrs %0, cpsr" : "=r"(nilai));
    return nilai;
}

/* Tulis register CPSR */
static void tulis_cpsr(uint32 nilai)
{
    __asm__ volatile ("msr cpsr_c, %0" : : "r"(nilai) : "memory");
}

/* Baca register SCTLR (System Control Register, CP15) */
static uint32 baca_sctlr(void)
{
    uint32 nilai;
    __asm__ volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r"(nilai));
    return nilai;
}

/* Tulis register SCTLR */
static void tulis_sctlr(uint32 nilai)
{
    __asm__ volatile ("mcr p15, 0, %0, c1, c0, 0" : : "r"(nilai) : "memory");
}

/* Baca register TTBR0 (Translation Table Base Register 0) */
static uint32 baca_ttbr0(void)
{
    uint32 nilai;
    __asm__ volatile ("mrc p15, 0, %0, c2, c0, 0" : "=r"(nilai));
    return nilai;
}

/* Tulis register TTBR0 */
static void tulis_ttbr0(uint32 nilai)
{
    __asm__ volatile ("mcr p15, 0, %0, c2, c0, 0" : : "r"(nilai) : "memory");
}

/* Baca register TTBCR (Translation Table Base Control Register) */
static uint32 baca_ttbcr(void)
{
    uint32 nilai;
    __asm__ volatile ("mrc p15, 0, %0, c2, c0, 2" : "=r"(nilai));
    return nilai;
}

/* Tulis register TTBCR — atur agar hanya TTBR0 digunakan */
static void tulis_ttbcr(uint32 nilai)
{
    __asm__ volatile ("mcr p15, 0, %0, c2, c0, 2" : : "r"(nilai) : "memory");
}

/* Baca register DACR (Domain Access Control Register) */
static uint32 baca_dacr(void)
{
    uint32 nilai;
    __asm__ volatile ("mrc p15, 0, %0, c3, c0, 0" : "=r"(nilai));
    return nilai;
}

/* Tulis register DACR */
static void tulis_dacr(uint32 nilai)
{
    __asm__ volatile ("mcr p15, 0, %0, c3, c0, 0" : : "r"(nilai) : "memory");
}

/* Baca register VBAR (Vector Base Address Register, ARMv7) */
static uint32 baca_vbar(void)
{
    uint32 nilai;
    __asm__ volatile ("mrc p15, 0, %0, c12, c0, 0" : "=r"(nilai));
    return nilai;
}

/* Tulis register VBAR — atur alamat dasar tabel vektor */
static void tulis_vbar(uint32 nilai)
{
    __asm__ volatile ("mcr p15, 0, %0, c12, c0, 0" : : "r"(nilai) : "memory");
}

/* Baca register MIDR (Main ID Register) — identifikasi CPU */
static uint32 baca_midr(void)
{
    uint32 nilai;
    __asm__ volatile ("mrc p15, 0, %0, c0, c0, 0" : "=r"(nilai));
    return nilai;
}

/* Baca register CTR (Cache Type Register) */
static uint32 baca_ctr(void)
{
    uint32 nilai;
    __asm__ volatile ("mrc p15, 0, %0, c0, c0, 1" : "=r"(nilai));
    return nilai;
}

/* Invalidasi seluruh TLB (Unified TLB) */
static void invalidasi_tlb_semua(void)
{
    __asm__ volatile ("mcr p15, 0, %0, c8, c7, 0" : : "r"(0) : "memory");
}

/* Invalidasi TLB untuk alamat tertentu */
static void invalidasi_tlb_alamat(uint32 alamat)
{
    __asm__ volatile ("mcr p15, 0, %0, c8, c7, 1" : : "r"(alamat) : "memory");
}

/* Invalidasi seluruh cache instruksi */
static void invalidasi_cache_i(void)
{
    __asm__ volatile ("mcr p15, 0, %0, c7, c5, 0" : : "r"(0) : "memory");
}

/* Invalidasi seluruh cache data */
static void invalidasi_cache_d(void)
{
    __asm__ volatile ("mcr p15, 0, %0, c7, c6, 0" : : "r"(0) : "memory");
}

/* Bilas (flush) seluruh cache data ke memori utama */
static void bilas_cache_d(void)
{
    __asm__ volatile ("mcr p15, 0, %0, c7, c10, 0" : : "r"(0) : "memory");
}

/* Bilas prefetch buffer (instruction pipeline) */
static void bilas_prefetch(void)
{
    __asm__ volatile ("mcr p15, 0, %0, c7, c5, 4" : : "r"(0) : "memory");
}

/* Rintangan memori (data memory barrier) */
static void rintangan_memori_dmb(void)
{
    __asm__ volatile ("mcr p15, 0, %0, c7, c10, 5" : : "r"(0) : "memory");
}

/* Rintangan sinkronisasi (data synchronization barrier) */
static void rintangan_sinkronisasi_dsb(void)
{
    __asm__ volatile ("mcr p15, 0, %0, c7, c10, 4" : : "r"(0) : "memory");
}

/* Rintangan instruksi (instruction synchronization barrier) */
static void rintangan_instruksi_isb(void)
{
    __asm__ volatile ("mcr p15, 0, %0, c7, c5, 4" : : "r"(0) : "memory");
}

/* Setel register SP (Stack Pointer) */
static void setel_sp(uint32 nilai)
{
    __asm__ volatile ("mov sp, %0" : : "r"(nilai) : "memory");
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
        /* Putar tunggu (spin-wait) */
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

/* Baca satu karakter dari UART (tunggu sampai tersedia) */
static char uart_terima(void)
{
    /* Tunggu sampai Receive FIFO tidak kosong */
    while (REG_UART(UART_FR) & UART_FR_RXFE) {
        /* Putar tunggu */
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

    /* Langkah 3: Bersihkan semua interupsi UART yang tertunda */
    REG_UART(UART_ICR) = 0x07FF;

    /* Langkah 4: Setel baud rate — 115200 bps
     * Asumsi clock UART = 24 MHz (VersatilePB)
     * IBRD = UARTCLK / (16 * BaudRate) = 24000000 / (16 * 115200) = 13.02
     *   Bagian bulat = 13
     * FBRD = pecahan * 64 + 0.5 = 0.02 * 64 + 0.5 = 1.78 → 1
     */
    REG_UART(UART_IBRD) = 13;
    REG_UART(UART_FBRD) = 1;

    /* Langkah 5: Setel format baris — 8 data bit, tanpa parity,
     * 1 stop bit, FIFO aktif */
    tmp = UART_LCR_H_WLEN8 | UART_LCR_H_FEN;
    REG_UART(UART_LCR_H) = tmp;

    /* Langkah 6: Setel ambang batas FIFO interupsi */
    REG_UART(UART_IFLS) = 0;  /* Level default */

    /* Langkah 7: Nonaktifkan semua interupsi UART */
    REG_UART(UART_IMSC) = 0;

    /* Langkah 8: Aktifkan UART — transmit dan receive */
    REG_UART(UART_CR) = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
}

/* ================================================================
 * HANDLER EKSEPSI ARM32
 * ================================================================ */

/* Handler untuk eksepsi Reset */
static void handler_reset(void)
{
    uart_kirim_string("[PEMICU] EKSEPSI: Reset! Sistem berhenti.\n");
    hentikan_cpu();
}

/* Handler untuk instruksi tidak terdefinisi */
static void handler_tidak_terdefinisi(void)
{
    uart_kirim_string("[PEMICU] EKSEPSI: Instruksi tidak terdefinisi!\n");
    hentikan_cpu();
}

/* Handler untuk panggilan SVC (Supervisor Call) */
static void handler_svc(void)
{
    uart_kirim_string("[PEMICU] EKSEPSI: SVC dipanggil.\n");
    /* Tidak berhenti — SVC adalah panggilan sistem normal */
}

/* Handler untuk Prefetch Abort (gagal mengambil instruksi) */
static void handler_prefetch_abort(void)
{
    uart_kirim_string("[PEMICU] EKSEPSI: Prefetch Abort!\n");
    hentikan_cpu();
}

/* Handler untuk Data Abort (gagal mengakses data) */
static void handler_data_abort(void)
{
    uart_kirim_string("[PEMICU] EKSEPSI: Data Abort!\n");
    hentikan_cpu();
}

/* Handler untuk vektor tidak digunakan (reserved) */
static void handler_tidak_digunakan(void)
{
    uart_kirim_string("[PEMICU] EKSEPSI: Vektor reserved terpicu!\n");
    hentikan_cpu();
}

/* Handler untuk IRQ (Interrupt Request) */
static void handler_irq(void)
{
    uart_kirim_string("[PEMICU] EKSEPSI: IRQ diterima.\n");
    /* Akan ditangani oleh kernel nanti */
}

/* Handler untuk FIQ (Fast Interrupt Request) */
static void handler_fiq(void)
{
    uart_kirim_string("[PEMICU] EKSEPSI: FIQ diterima.\n");
    /* Akan ditangani oleh kernel nanti */
}

/* ================================================================
 * TABEL VEKTOR EKSEPSI ARM32
 *
 * Pada ARM32, tabel vektor berisi instruksi LDR PC, [PC, #offset]
 * yang memuat alamat handler dari "literal pool" yang terletak
 * tepat setelah tabel vektor.
 *
 * Setiap instruksi LDR PC, [PC, #0x18] memuat alamat handler dari
 * posisi yang sesuai di literal pool karena:
 *   PC saat eksekusi = alamat_instruksi + 8 (ARM pipeline)
 *   Alamat tujuan   = alamat_instruksi + 8 + 0x18
 *                    = alamat_instruksi + 0x20
 * Jadi vektor ke-i (pada offset i*4) memuat dari offset (i*4 + 0x20)
 * = literal pool ke-i.
 * ================================================================ */

/* Instruksi LDR PC, [PC, #0x18] = 0xE59FF018 */
#define VEKTOR_LDR_PC  0xE59FF018

/* Tabel vektor: 8 instruksi + 8 alamat handler (literal pool) */
SEJAJAR(4) static uint32 tabel_vektor[16] BAGIAN(".vektor") = {
    /* 8 instruksi LDR PC — masing-masing memuat handler dari literal pool */
    VEKTOR_LDR_PC,  /* 0x00: Reset */
    VEKTOR_LDR_PC,  /* 0x04: Undefined Instruction */
    VEKTOR_LDR_PC,  /* 0x08: SVC (Software Interrupt) */
    VEKTOR_LDR_PC,  /* 0x0C: Prefetch Abort */
    VEKTOR_LDR_PC,  /* 0x10: Data Abort */
    VEKTOR_LDR_PC,  /* 0x14: Reserved (tidak digunakan) */
    VEKTOR_LDR_PC,  /* 0x18: IRQ */
    VEKTOR_LDR_PC,  /* 0x1C: FIQ */
    /* Literal pool: alamat handler — diisi oleh siapkan_vektor() */
    0, 0, 0, 0, 0, 0, 0, 0
};

/* ================================================================
 * INISIALISASI GIC (Generic Interrupt Controller)
 * ================================================================ */

/* Akses register GIC Distributor */
#define REG_GICD(offset) \
    (*(volatile uint32 *)((ukuran_ptr)(GIC_DISTRIBUTOR_BASE) + (ukuran_ptr)(offset)))

/* Akses register GIC CPU Interface */
#define REG_GICC(offset) \
    (*(volatile uint32 *)((ukuran_ptr)(GIC_CPU_INTERFACE_BASE) + (ukuran_ptr)(offset)))

/* Nonaktifkan semua interupsi pada GIC */
static void gic_nonaktifkan_semua(void)
{
    uint32 i;
    uint32 maks_irq;

    /* Baca tipe GIC untuk mengetahui jumlah IRQ */
    maks_irq = (REG_GICD(GICD_TYPER) & 0x1F) + 1;
    maks_irq = maks_irq * 32;  /* Setiap kata mengontrol 32 IRQ */
    if (maks_irq > 1020) {
        maks_irq = 1020;
    }

    /* Nonaktifkan Distributor */
    REG_GICD(GICD_CTRL) = 0;

    /* Nonaktifkan setiap IRQ */
    for (i = 0; i < maks_irq; i += 32) {
        REG_GICD(GICD_ICENABLE + (i / 32) * 4) = 0xFFFFFFFF;
    }

    /* Bersihkan semua IRQ yang tertunda */
    for (i = 0; i < maks_irq; i += 32) {
        REG_GICD(GICD_ICPEND + (i / 32) * 4) = 0xFFFFFFFF;
    }

    /* Setel prioritas default (rendah) untuk semua IRQ */
    for (i = 0; i < maks_irq; i += 4) {
        REG_GICD(GICD_IPRIORITY + (i / 4) * 4) = 0xA0A0A0A0;
    }

    /* Arahkan semua IRQ ke CPU 0 */
    for (i = 0; i < maks_irq; i += 4) {
        REG_GICD(GICD_ITARGETS + (i / 4) * 4) = 0x01010101;
    }

    /* Setel semua IRQ ke mode level-triggered */
    for (i = 0; i < maks_irq; i += 16) {
        REG_GICD(GICD_ICFG + (i / 16) * 4) = 0;
    }

    /* Aktifkan kembali Distributor */
    REG_GICD(GICD_CTRL) = 1;
}

/* Siapkan CPU Interface pada GIC */
static void gic_siapkan_cpu(void)
{
    /* Nonaktifkan CPU Interface dulu */
    REG_GICC(GICC_CTRL) = 0;

    /* Setel mask prioritas — izinkan semua prioritas */
    REG_GICC(GICC_PMR) = 0xFF;

    /* Aktifkan CPU Interface */
    REG_GICC(GICC_CTRL) = 1;
}

/* ================================================================
 * SIAPKAN TABEL VEKTOR EKSEPSI
 * ================================================================ */

static void siapkan_vektor(void)
{
    /* Isi literal pool dengan alamat handler */
    tabel_vektor[8]  = (uint32)(ukuran_ptr)handler_reset;
    tabel_vektor[9]  = (uint32)(ukuran_ptr)handler_tidak_terdefinisi;
    tabel_vektor[10] = (uint32)(ukuran_ptr)handler_svc;
    tabel_vektor[11] = (uint32)(ukuran_ptr)handler_prefetch_abort;
    tabel_vektor[12] = (uint32)(ukuran_ptr)handler_data_abort;
    tabel_vektor[13] = (uint32)(ukuran_ptr)handler_tidak_digunakan;
    tabel_vektor[14] = (uint32)(ukuran_ptr)handler_irq;
    tabel_vektor[15] = (uint32)(ukuran_ptr)handler_fiq;

    /* Atur VBAR (Vector Base Address Register) ke alamat tabel vektor */
    tulis_vbar((uint32)(ukuran_ptr)tabel_vektor);

    /* Pastikan VBAR tersimpan — rintangan instruksi */
    rintangan_instruksi_isb();

    uart_kirim_string("[PEMICU] Tabel vektor eksepsi siap pada alamat ");
    uart_kirim_hex((uint32)(ukuran_ptr)tabel_vektor);
    uart_kirim('\n');
}

/* ================================================================
 * SIAPKAN MMU — PEMETAAN SEKSI (1 MB)
 * ================================================================ */

/* Tentukan apakah alamat adalah wilayah perangkat I/O */
static logika adalah_wilayah_perangkat(uint32 alamat_seksi)
{
    uint32 dasar;

    dasar = alamat_seksi & 0xFFF00000;

    /* Wilayah I/O VersatilePB: 0x10000000 - 0x1FFFFFFF */
    if (dasar >= WILAYAH_IO_MULAI && dasar <= WILAYAH_IO_SELESAI) {
        return BENAR;
    }

    /* Wilayah GIC: 0x2C000000 - 0x2CFFFFFF */
    if (dasar >= WILAYAH_IO_GIC && dasar < (WILAYAH_IO_GIC + 0x01000000)) {
        return BENAR;
    }

    return SALAH;
}

/* Siapkan tabel halaman dengan pemetaan seksi identitas */
static void siapkan_tabel_halaman(void)
{
    uint32 i;
    uint32 alamat_fisik;

    /* Inisialisasi seluruh tabel halaman ke deskriptor kosong */
    isi_memori(tabel_halaman, 0, sizeof(tabel_halaman));

    /* Petakan 1 GB pertama dengan pemetaan identitas
     * (alamat virtual = alamat fisik) */
    for (i = 0; i < SEKSI_MAKS_PETA; i++) {
        alamat_fisik = i * SEKSI_UKURAN;

        if (adalah_wilayah_perangkat(alamat_fisik)) {
            /* Memori perangkat: non-cacheable, non-bufferable */
            tabel_halaman[i] = SEKSI_MEMORI_PERANGKAT(alamat_fisik);
        } else {
            /* Memori normal: cacheable, bufferable */
            tabel_halaman[i] = SEKSI_MEMORI_NORMAL(alamat_fisik);
        }
    }

    uart_kirim_string("[PEMICU] Tabel halaman siap: ");
    uart_kirim_hex((uint32)(ukuran_ptr)tabel_halaman);
    uart_kirim_string(" (");
    uart_kirim_hex(SEKSI_MAKS_PETA);
    uart_kirim_string(" seksi = ");
    uart_kirim_hex(SEKSI_MAKS_PETA);
    uart_kirim_string(" MB)\n");
}

/* Aktifkan MMU */
static void aktifkan_mmu(void)
{
    uint32 sctlr;
    uint32 ttbcr_val;

    /* Langkah 1: Pastikan MMU belum aktif */
    sctlr = baca_sctlr();
    if (sctlr & SCTLR_M) {
        uart_kirim_string("[PEMICU] PERINGATAN: MMU sudah aktif!\n");
    }

    /* Langkah 2: Invalidasi seluruh cache dan TLB */
    invalidasi_cache_i();
    invalidasi_cache_d();
    invalidasi_tlb_semua();

    /* Langkah 3: Atur TTBCR agar hanya TTBR0 yang digunakan
     * TTBCR = 0 berarti semua alamat menggunakan TTBR0 */
    ttbcr_val = 0;
    tulis_ttbcr(ttbcr_val);

    /* Langkah 4: Atur TTBR0 ke alamat tabel halaman */
    tulis_ttbr0((uint32)(ukuran_ptr)tabel_halaman);

    /* Langkah 5: Atur DACR — domain 0 sebagai manajer
     * (semua akses diizinkan tanpa cek AP) */
    tulis_dacr(DACR_SEMUA_MANAJER);

    /* Langkah 6: Rintangan memori untuk memastikan semua
     * penulisan register sebelum mengaktifkan MMU */
    rintangan_sinkronisasi_dsb();
    rintangan_instruksi_isb();

    /* Langkah 7: Aktifkan MMU (bit M), cache data (bit C),
     * cache instruksi (bit I), dan branch prediction (bit Z) */
    sctlr = baca_sctlr();
    sctlr |= SCTLR_M;    /* Aktifkan MMU */
    sctlr |= SCTLR_C;    /* Aktifkan cache data */
    sctlr |= SCTLR_I;    /* Aktifkan cache instruksi */
    sctlr |= SCTLR_Z;    /* Aktifkan branch prediction */
    sctlr |= SCTLR_A;    /* Aktifkan pengecekan alignment */

    tulis_sctlr(sctlr);

    /* Langkah 8: Rintangan instruksi setelah mengaktifkan MMU */
    rintangan_instruksi_isb();

    mmu_aktif = BENAR;

    uart_kirim_string("[PEMICU] MMU aktif. SCTLR = ");
    uart_kirim_hex(baca_sctlr());
    uart_kirim('\n');
}

/* ================================================================
 * DETEKSI MEMORI DARI ATAG
 * ================================================================ */

/* Parse struktur ATAG yang diberikan bootloader */
static void deteksi_memori_atag(uint32 alamat_atag)
{
    StrukturAtag *atag;
    uint32 ukuran_total;
    uint32 tipe;

    /* Inisialisasi default — jika ATAG tidak ditemukan */
    ram_total_kb = 0;
    ram_alamat_mulai = 0;
    ram_ukuran_byte = 0;

    if (alamat_atag == 0) {
        uart_kirim_string("[PEMICU] Tidak ada ATAG. Asumsi RAM 256 MB di 0x0.\n");
        ram_alamat_mulai = 0;
        ram_ukuran_byte = 256 * 1024 * 1024;
        ram_total_kb = 256 * 1024;
        return;
    }

    atag = (StrukturAtag *)(ukuran_ptr)alamat_atag;

    /* Telusuri daftar ATAG */
    while (1) {
        ukuran_total = atag->header.ukuran;
        tipe = atag->header.tipe;

        /* ATAG_NONE menandai akhir daftar */
        if (tipe == ATAG_NONE) {
            break;
        }

        /* Cek validitas ukuran ATAG */
        if (ukuran_total < 2) {
            uart_kirim_string("[PEMICU] PERINGATAN: ATAG tidak valid pada ");
            uart_kirim_hex((uint32)(ukuran_ptr)atag);
            uart_kirim('\n');
            break;
        }

        /* Proses ATAG berdasarkan tipe */
        if (tipe == ATAG_CORE) {
            uart_kirim_string("[PEMICU] ATAG_CORE ditemukan\n");
        }
        else if (tipe == ATAG_MEM) {
            /* ATAG_MEM berisi informasi memori fisik */
            ukuran_t ukuran_mb;

            ram_ukuran_byte = atag->isi.mem.ukuran;
            ram_alamat_mulai = atag->isi.mem.mulai;
            ram_total_kb = ram_ukuran_byte / 1024;

            ukuran_mb = ram_ukuran_byte / (1024 * 1024);
            uart_kirim_string("[PEMICU] ATAG_MEM: RAM pada ");
            uart_kirim_hex(ram_alamat_mulai);
            uart_kirim_string(", ukuran ");
            uart_kirim_hex((uint32)ukuran_mb);
            uart_kirim_string(" MB\n");
        }
        else if (tipe == ATAG_CMDLINE) {
            uart_kirim_string("[PEMICU] ATAG_CMDLINE ditemukan\n");
        }

        /* Maju ke ATAG berikutnya
         * ukuran_total dalam satuan word (4 byte) */
        atag = (StrukturAtag *)((ukuran_ptr)atag + ukuran_total * 4);
    }

    /* Jika tidak ada ATAG_MEM ditemukan, gunakan default */
    if (ram_ukuran_byte == 0) {
        uart_kirim_string("[PEMICU] ATAG_MEM tidak ditemukan. Asumsi 256 MB di 0x0.\n");
        ram_alamat_mulai = 0;
        ram_ukuran_byte = 256 * 1024 * 1024;
        ram_total_kb = 256 * 1024;
    }
}

/* ================================================================
 * INISIALISASI PROSESOR ARM32
 * ================================================================ */

static void inisialisasi_prosesor(void)
{
    uint32 midr;
    uint32 implementer;
    uint32 varian;
    uint32 arsitektur;
    uint32 bagian;
    uint32 revisi;

    /* Langkah 1: Nonaktifkan semua interupsi */
    nonaktifkan_interupsi();

    /* Langkah 2: Baca identitas CPU */
    midr = baca_midr();
    implementer = (midr >> 24) & 0xFF;
    varian       = (midr >> 20) & 0xF;
    arsitektur   = (midr >> 16) & 0xF;
    bagian       = (midr >> 4)  & 0xFFF;
    revisi       = midr & 0xF;

    uart_kirim_string("[PEMICU] CPU: MIDR=");
    uart_kirim_hex(midr);
    uart_kirim_string(" Implementer=");
    uart_kirim_hex(implementer);
    uart_kirim_string(" Arkitektur=");
    uart_kirim_hex(arsitektur);
    uart_kirim_string(" Bagian=");
    uart_kirim_hex(bagian);
    uart_kirim_string(" Revisi=");
    uart_kirim_hex(revisi);
    uart_kirim('\n');

    /* Langkah 3: Nonaktifkan MMU, cache, dan branch prediction
     * jika masih aktif dari bootloader */
    if (baca_sctlr() & SCTLR_M) {
        uint32 sctlr;

        sctlr = baca_sctlr();
        sctlr &= ~SCTLR_M;  /* Nonaktifkan MMU */
        sctlr &= ~SCTLR_C;  /* Nonaktifkan cache data */
        sctlr &= ~SCTLR_I;  /* Nonaktifkan cache instruksi */
        sctlr &= ~SCTLR_Z;  /* Nonaktifkan branch prediction */
        tulis_sctlr(sctlr);
        rintangan_instruksi_isb();
    }

    /* Langkah 4: Invalidasi cache dan TLB */
    invalidasi_cache_i();
    invalidasi_cache_d();
    invalidasi_tlb_semua();

    /* Langkah 5: Atur mode prosesor ke Supervisor (SVC) */
    {
        uint32 cpsr;

        cpsr = baca_cpsr();
        cpsr = (cpsr & ~0x1F) | 0x13;  /* Mode = 10011 (SVC) */
        cpsr |= (1 << 6);  /* Nonaktifkan FIQ */
        cpsr |= (1 << 7);  /* Nonaktifkan IRQ */
        tulis_cpsr(cpsr);
    }

    /* Langkah 6: Siapkan tumpukan (stack) */
    setel_sp((uint32)(ukuran_ptr)tumpukan_boot + TUMPUKAN_UKURAN);

    uart_kirim_string("[PEMICU] Prosesor ARM32 terinisialisasi. SP=");
    uart_kirim_hex((uint32)(ukuran_ptr)tumpukan_boot + TUMPUKAN_UKURAN);
    uart_kirim('\n');
}

/* ================================================================
 * TITIK MASUK PEMICU ARM32
 *
 * Dipanggil oleh U-Boot dengan register:
 *   r0 = 0 (atau magic number)
 *   r1 = tipe mesin (machine type number, deprecated)
 *   r2 = alamat ATAG/DTB
 * ================================================================ */

void pemicu_utama(uint32 r0, uint32 r1, uint32 r2)
{
    ukuran_t ram_mb;

    /* Langkah 1: Inisialisasi UART terlebih dahulu
     * (U-Boot sudah menyediakan tumpukan, sehingga pemanggilan
     * fungsi ini aman sebelum tumpukan kita sendiri disiapkan) */
    uart_siapkan();

    /* Langkah 2: Inisialisasi prosesor */
    inisialisasi_prosesor();

    uart_kirim_string("\n========================================\n");
    uart_kirim_string("  PEMICU ARM32 — Kernel Arsitek\n");
    uart_kirim_string("========================================\n\n");

    /* Langkah 3: Siapkan tabel vektor eksepsi */
    siapkan_vektor();

    /* Langkah 4: Nonaktifkan GIC terlebih dahulu */
    gic_nonaktifkan_semua();
    gic_siapkan_cpu();
    uart_kirim_string("[PEMICU] GIC terinisialisasi\n");

    /* Langkah 5: Deteksi memori dari ATAG */
    uart_kirim_string("[PEMICU] Membaca ATAG pada ");
    uart_kirim_hex(r2);
    uart_kirim('\n');
    deteksi_memori_atag(r2);

    /* Langkah 6: Siapkan tabel halaman dan aktifkan MMU */
    siapkan_tabel_halaman();
    aktifkan_mmu();

    /* Langkah 7: Tampilkan ringkasan */
    ram_mb = ram_ukuran_byte / (1024 * 1024);
    uart_kirim_string("[PEMICU] RAM terdeteksi: ");
    uart_kirim_hex((uint32)ram_mb);
    uart_kirim_string(" MB pada alamat ");
    uart_kirim_hex((uint32)ram_alamat_mulai);
    uart_kirim('\n');

    uart_kirim_string("[PEMICU] MMU aktif, cache aktif\n");
    uart_kirim_string("[PEMICU] Lompat ke titik masuk kernel...\n\n");

    /* Langkah 8: Lompat ke titik masuk kernel utama */
    arsitek_mulai();

    /* Jika kembali dari kernel (tidak seharusnya), hentikan */
    uart_kirim_string("[PEMICU] FATAL: Kernel kembali! Sistem berhenti.\n");
    hentikan_cpu();
}
