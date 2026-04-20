/*
 * arm64/interupsi.c
 *
 * Penanganan interupsi untuk arsitektur ARM64 (AArch64).
 * Menyiapkan tabel vektor eksepsi dan GICv3.
 *
 * Tabel vektor ARM64 (16 entri, masing-masing 128-byte aligned):
 *   Setiap tingkat pengecualian (EL) memiliki 4 tipe vektor:
 *     - Synchronous (SPsel = SP0)
 *     - IRQ (SPsel = SP0)
 *     - FIQ (SPsel = SP0)
 *     - SError (SPsel = SP0)
 *     - Synchronous (SPsel = SPx)
 *     - IRQ (SPsel = SPx)
 *     - FIQ (SPsel = SPx)
 *     - SError (SPsel = SPx)
 *     - Synchronous (AArch64, lower EL)
 *     - IRQ (AArch64, lower EL)
 *     - FIQ (AArch64, lower EL)
 *     - SError (AArch64, lower EL)
 *     - Synchronous (AArch32, lower EL)
 *     - IRQ (AArch32, lower EL)
 *     - FIQ (AArch32, lower EL)
 *     - SError (AArch32, lower EL)
 *
 * GICv3 menggunakan register sistem (ICC_*_EL1) untuk
 * pengelolaan interupsi pada CPU interface.
 */

#include "../../lampiran/arsitek.h"
#include "../../lampiran/mesin.h"

/* ================================================================
 * KONSTANTA GICv3
 * ================================================================ */

/* GIC Distributor register (MMIO) */
#define GICD_BASE               0x08000000UL
#define GICD_CTRL_REG           ((volatile uint32 *)((ukuran_ptr)(GICD_BASE) + (ukuran_ptr)0x0000))
#define GICD_TYPER_REG          ((volatile uint32 *)((ukuran_ptr)(GICD_BASE) + (ukuran_ptr)0x0004))
#define GICD_ISENABLE_REG(n)    ((volatile uint32 *)((ukuran_ptr)(GICD_BASE) + (ukuran_ptr)(0x0100 + ((n) / 32) * 4)))
#define GICD_ICENABLE_REG(n)    ((volatile uint32 *)((ukuran_ptr)(GICD_BASE) + (ukuran_ptr)(0x0180 + ((n) / 32) * 4)))
#define GICD_ICPEND_REG(n)      ((volatile uint32 *)((ukuran_ptr)(GICD_BASE) + (ukuran_ptr)(0x0280 + ((n) / 32) * 4)))
#define GICD_IPRIORITY_REG(n)   ((volatile uint32 *)((ukuran_ptr)(GICD_BASE) + (ukuran_ptr)(0x0400 + ((n) / 4) * 4)))
#define GICD_ICFG_REG(n)        ((volatile uint32 *)((ukuran_ptr)(GICD_BASE) + (ukuran_ptr)(0x0C00 + ((n) / 16) * 4)))

/* GIC Redistributor register */
#define GICR_BASE               0x080A0000UL
#define GICR_WAKER_REG          ((volatile uint32 *)((ukuran_ptr)(GICR_BASE) + (ukuran_ptr)0x0014))

/* Jumlah interupsi maksimum */
#define GIC_INT_MAKS            128

/* ================================================================
 * HANDLER EKSEPSI ARM64
 * ================================================================ */

void handler_sinkron_el1(void)
{
    uint64 esr;
    uint64 far;
    __asm__ volatile ("mrs %0, esr_el1" : "=r"(esr));
    __asm__ volatile ("mrs %0, far_el1" : "=r"(far));
    notulen_catat(NOTULEN_FATAL, "ARM64: Eksepsi sinkron EL1");
    mesin_hentikan();
}

void handler_irq_el1(void)
{
    /* Baca IRQ dari GIC ICC_IAR0_EL1 */
    uint64 iar;
    __asm__ volatile ("mrs %0, ICC_IAR0_EL1" : "=r"(iar));
    /* Kirim EOI */
    __asm__ volatile ("msr ICC_EOIR0_EL1, %0" : : "r"(iar));
}

void handler_fiq_el1(void)
{
    notulen_catat(NOTULEN_KESALAHAN, "ARM64: FIQ EL1");
}

void handler_serror_el1(void)
{
    notulen_catat(NOTULEN_FATAL, "ARM64: SError EL1");
    mesin_hentikan();
}

void handler_sinkron_el0(void)
{
    notulen_catat(NOTULEN_PERINGATAN, "ARM64: Eksepsi sinkron dari EL0");
}

void handler_irq_el0(void)
{
    handler_irq_el1();   /* Sama dengan IRQ EL1 */
}

void handler_fiq_el0(void)
{
    handler_fiq_el1();
}

void handler_serror_el0(void)
{
    handler_serror_el1();
}

/* ================================================================
 * INISIALISASI GDT (TIDAK ADA PADA ARM64)
 * ================================================================ */

void mesin_siapkan_gdt(void)
{
    /* ARM64 tidak menggunakan GDT — tidak ada yang perlu dilakukan */
}

/* ================================================================
 * INISIALISASI IDT — TABEL VEKTOR EKSEPSI ARM64
 * ================================================================ */

/*
 * mesin_siapkan_idt — Siapkan tabel vektor eksepsi ARM64.
 * Pada ARM64, tabel vektor berisi 16 entri handler,
 * masing-masing harus 128-byte aligned.
 *
 * Karena C tidak mendukung alignment 128 byte secara portabel,
 * kita menggunakan __attribute__((aligned(128))).
 * Setiap handler adalah fungsi kecil yang menyimpan register,
 * memanggil handler C, lalu memulihkan register.
 */
void mesin_siapkan_idt(void)
{
    /* Tabel vektor didefinisikan dalam assembly karena memerlukan
     * penyimpanan/pemulihan register secara manual.
     * Di sini kita hanya mengatur VBAR_EL1 untuk menunjuk ke tabel. */

    /* Untuk tahap awal, kita gunakan tabel sederhana
     * yang ditempatkan di area memori khusus.
     * Implementasi lengkap memerlukan file assembly terpisah. */

    /* Atur VBAR_EL1 — alamat tabel vektor */
    /* Catatan: pada implementasi lengkap, alamat ini menunjuk
     * ke tabel vektor yang didefinisikan dalam assembly. */
    /* Tabel vektor didefinisikan di pemicu.c sebagai vektor_eksepsi */
    extern void vektor_eksepsi(void);
    uint64 alamat_vektor = (uint64)(ukuran_ptr)vektor_eksepsi;

    __asm__ volatile ("msr vbar_el1, %0" : : "r"(alamat_vektor));
    __asm__ volatile ("isb");
}

/* ================================================================
 * INISIALISASI GICv3
 * ================================================================ */

/*
 * mesin_siapkan_pic — Inisialisasi GICv3.
 * GICv3 menggunakan kombinasi MMIO (Distributor, Redistributor)
 * dan register sistem (ICC_*_EL1) untuk pengelolaan interupsi.
 */
void mesin_siapkan_pic(void)
{
    int i;

    /* 1. Nonaktifkan distribusi */
    *GICD_CTRL_REG = 0;

    /* 2. Set prioritas default */
    for (i = 0; i < GIC_INT_MAKS; i += 4) {
        *GICD_IPRIORITY_REG(i) = 0xA0A0A0A0;
    }

    /* 3. Nonaktifkan semua interupsi */
    for (i = 0; i < GIC_INT_MAKS; i += 32) {
        *GICD_ICENABLE_REG(i) = 0xFFFFFFFF;
        *GICD_ICPEND_REG(i) = 0xFFFFFFFF;
    }

    /* 4. Wake up Redistributor */
    *GICR_WAKER_REG &= ~0x02;      /* Clear ProcessorSleep */

    /* 5. Aktifkan distribusi (Group 0 + Group 1 NS) */
    *GICD_CTRL_REG = 0x03;

    /* 6. Aktifkan CPU interface via system registers */
    {
        uint64 icc_sre;
        __asm__ volatile ("mrs %0, ICC_SRE_EL1" : "=r"(icc_sre));
        icc_sre |= 0x07;    /* SRE, Enable */
        __asm__ volatile ("msr ICC_SRE_EL1, %0" : : "r"(icc_sre));

        /* Set priority mask — terima semua prioritas */
        __asm__ volatile ("msr ICC_PMR_EL1, %0" : : "r"((uint64)0xF0));

        /* Aktifkan group 0 interupsi */
        __asm__ volatile ("msr ICC_IGRPEN1_EL1, %0" : : "r"((uint64)0x01));
    }
}

/* ================================================================
 * TIMER ARM64
 * ================================================================ */

/*
 * mesin_siapkan_pit — Inisialisasi timer ARM64.
 * ARM64 memiliki timer generik (Generic Timer) yang diakses
 * melalui register sistem (CNTFRQ_EL0, CNTP_TVAL_EL0, dll.).
 */
void mesin_siapkan_pit(uint32 frekuensi_hz)
{
    uint64 tval;

    /* Baca frekuensi timer */
    __asm__ volatile ("mrs %0, cntfrq_el0" : "=r"(tval));

    /* Hitung nilai reload untuk frekuensi yang diminta */
    if (frekuensi_hz > 0) {
        tval = tval / (uint64)frekuensi_hz;
    } else {
        tval = tval / 100;      /* Default: 100 Hz */
    }

    /* Atur timer value */
    __asm__ volatile ("msr cntp_tval_el0, %0" : : "r"(tval));

    /* Aktifkan timer */
    {
        uint64 ctrl;
        __asm__ volatile ("mrs %0, cntp_ctl_el0" : "=r"(ctrl));
        ctrl |= 0x01;     /* ENABLE bit */
        __asm__ volatile ("msr cntp_ctl_el0, %0" : : "r"(ctrl));
    }
}
