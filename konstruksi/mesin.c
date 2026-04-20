/*
 * mesin.c
 *
 * Inisialisasi mesin utama kernel Arsitek.
 * Mengoordinasikan seluruh proses penyiapan perangkat keras
 * yang bergantung pada arsitektur CPU.
 *
 * Urutan inisialisasi:
 *   1. GDT (Tabel Deskriptor Global) — hanya x86/x64
 *   2. IDT (Tabel Deskriptor Interupsi) — hanya x86/x64
 *   3. PIC (Pengendali Interupsi yang Dapat Diprogram) — hanya x86/x64
 *   4. PIT (Penghitung Interval yang Dapat Diprogram) — timer
 *   5. Paging — manajemen memori virtual
 *   6. Deteksi RAM fisik
 *   7. Aktifkan interupsi
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"

/* Penanda global: apakah konstruksi sudah siap */
int konstruksi_siap = SALAH;

/* ================================================================
 * FUNGSI UTAMA — INISIALISASI MESIN
 * ================================================================ */

/*
 * mesin_siapkan — Inisialisasi seluruh subsistem perangkat keras.
 * Dipanggil sekali oleh arsitek_mulai() sebelum peninjau berjalan.
 *
 * Proses ini menyiapkan lingkungan eksekusi dasar:
 * tabel deskriptor, pengendali interupsi, timer, dan paging.
 * Setelah fungsi ini selesai, kernel siap menerima interupsi
 * dan mengelola memori virtual.
 */
void mesin_siapkan(void)
{
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    /* --- x86/x64: Inisialisasi bertahap --- */

    /* 1. Siapkan GDT — tabel segmen dasar */
    mesin_siapkan_gdt();

    /* 2. Siapkan IDT — tabel penanganan interupsi dan eksepsi */
    mesin_siapkan_idt();

    /* 3. Siapkan PIC — pemetaan ulang IRQ ke vektor yang tepat */
    mesin_siapkan_pic();

    /* 4. Siapkan PIT — timer sistem dengan frekuensi 100 Hz */
    mesin_siapkan_pit(100);

    /* 5. Siapkan paging — pemetaan identitas memori virtual */
    mesin_siapkan_paging();

    /* 6. Deteksi jumlah RAM fisik yang tersedia */
    mesin_deteksi_ram();

#elif defined(ARSITEK_ARM32) || defined(ARSITEK_ARM64)
    /* --- ARM: Inisialisasi melalui konstruksi per arsitektur --- */

    /* Pada ARM, bootloader (U-Boot) sudah menyiapkan sebagian.
     * Kita hanya perlu mengonfigurasi MMU dan vektor eksepsi. */

    /* Siapkan tabel vektor eksepsi */
    mesin_siapkan_idt();

    /* Siapkan paging/section mapping */
    mesin_siapkan_paging();

    /* Deteksi RAM dari informasi bootloader */
    mesin_deteksi_ram();
#endif

    /* Tandai konstruksi telah siap */
    konstruksi_siap = BENAR;
}

/* ================================================================
 * FUNGSI UTILITAS MESIN
 * ================================================================ */

/*
 * mesin_kirim_eoi — Kirim sinyal End-of-Interrupt ke PIC.
 * Harus dipanggil di akhir setiap handler IRQ agar PIC
 * dapat menerima interupsi berikutnya.
 *
 * Parameter:
 *   irq — nomor IRQ (0-15). Jika irq >= 8, kirim EOI
 *         ke kedua PIC (master dan slave).
 */
void mesin_kirim_eoi(uint8 irq)
{
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    /* Kirim EOI ke PIC master */
    tulis_port(PIC1_PERINTAH, PIC_EOI);

    /* Jika IRQ berasal dari PIC slave (IRQ 8-15),
     * kirim EOI juga ke PIC slave */
    if (irq >= 8) {
        tulis_port(PIC2_PERINTAH, PIC_EOI);
    }
#else
    /* Pada ARM, GIC menangani EOI secara berbeda */
    TIDAK_DIGUNAKAN(irq);
#endif
}

/*
 * mesin_hentikan — Hentikan sistem secara fatal.
 * Dipanggil ketika terjadi kesalahan yang tidak dapat dipulihkan.
 * Tidak pernah kembali (NORETURN).
 */
void mesin_hentikan(void)
{
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    /* Matikan interupsi dan hentikan CPU */
    nonaktifkan_interupsi();
    while (1) {
        hentikan_cpu();
    }
#elif defined(ARSITEK_ARM32) || defined(ARSITEK_ARM64)
    /* Pada ARM, masuk ke mode WFI (Wait For Interrupt) selamanya */
    while (1) {
        __asm__ volatile ("wfi");
    }
#endif
}

/*
 * mesin_baca_irq — Baca nomor IRQ saat ini dari PIC.
 * Digunakan oleh handler interupsi umum untuk mengetahui
 * sumber interupsi.
 *
 * Mengembalikan nomor IRQ (0-15), atau 0xFF jika tidak ada.
 */
uint8 mesin_baca_irq(void)
{
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    uint8 irq = 0xFF;

    /* Periksa PIC master (IRQ 0-7) */
    tulis_port(PIC1_PERINTAH, 0x0B);   /* Baca ISR */
    tunda_io();
    if (baca_port(PIC1_PERINTAH) & 0xFF) {
        /* Ada interupsi di master — baca IRR untuk identifikasi */
        tulis_port(PIC1_PERINTAH, 0x0A);   /* Baca IRR */
        tunda_io();
        irq = 0; /* Akan ditentukan lebih lanjut oleh IDT */
    }

    /* Periksa PIC slave (IRQ 8-15) */
    tulis_port(PIC2_PERINTAH, 0x0B);
    tunda_io();
    if (baca_port(PIC2_PERINTAH) & 0xFF) {
        irq = 8;
    }

    return irq;
#else
    return 0xFF;
#endif
}

/*
 * mesin_deteksi_ram — Deteksi jumlah RAM fisik yang tersedia.
 * Mengembalikan jumlah RAM dalam kilobyte.
 *
 * Metode x86: Membaca BIOS Data Area (BDA) pada alamat 0x0413
 * untuk mendapatkan ukuran memori konvensional (bawah 640 KB),
 * lalu mencoba metode INT 0x15 untuk memori ekstended.
 *
 * Metode ARM: Membaca informasi dari ATAG/FDT yang diberikan
 * oleh bootloader.
 */
ukuran_t mesin_deteksi_ram(void)
{
    ukuran_t total_kb = 0;

#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    /*
     * Metode 1: Baca BDA (BIOS Data Area) pada 0x0413.
     * Berisi jumlah memori konvensional dalam KB.
     * Nilai ini biasanya 640 (0x0280) atau kurang.
     */
    {
        volatile uint16 *penuding_bda;
        penuding_bda = (volatile uint16 *)BDA_ALAMAT;
        /* Offset 0x13 dari awal BDA = alamat 0x413 */
        penuding_bda = (volatile uint16 *)0x0413;
        total_kb = (ukuran_t)(*penuding_bda);
    }

    /*
     * Metode 2: Coba baca CMOS untuk memori ekstended.
     * Register CMOS 0x30-0x31 berisi ukuran memori di atas 1 MB
     * dalam KB. Akses CMOS melalui port 0x70/0x71.
     */
    {
        uint8 rendah, tinggi;
        uint16 ext_kb;

        tulis_port(0x70, 0x30);
        tunda_io();
        rendah = baca_port(0x71);

        tulis_port(0x70, 0x31);
        tunda_io();
        tinggi = baca_port(0x71);

        ext_kb = (uint16)((tinggi << 8) | rendah);
        total_kb += (ukuran_t)ext_kb;
    }

#elif defined(ARSITEK_ARM32)
    /* Pada ARM32, RAM biasanya dimulai dari 0x00000000.
     * Untuk VersatilePB, default 256 MB.
     * Nilai ini seharusnya diperoleh dari ATAG bootloader. */
    total_kb = 256UL * 1024UL;    /* 256 MB default */

#elif defined(ARSITEK_ARM64)
    /* Pada ARM64 (QEMU virt), default 1024 MB.
     * Seharusnya diperoleh dari FDT/DTB bootloader. */
    total_kb = 1024UL * 1024UL;   /* 1024 MB default */
#endif

    return total_kb;
}

/*
 * mesin_cpuid — Baca informasi CPU melalui instruksi CPUID.
 * Hanya tersedia pada arsitektur x86/x64.
 *
 * Parameter:
 *   daun — nomor daun (leaf) CPUID
 *   eax  — pointer untuk menyimpan hasil EAX
 *   ebx  — pointer untuk menyimpan hasil EBX
 *   ecx  — pointer untuk menyimpan hasil ECX
 *   edx  — pointer untuk menyimpan hasil EDX
 */
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
void mesin_cpuid(uint32 daun, uint32 *eax, uint32 *ebx,
                 uint32 *ecx, uint32 *edx)
{
    uint32 a, b, c, d;
    __asm__ volatile (
        "cpuid"
        : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
        : "a"(daun)
    );
    if (eax) *eax = a;
    if (ebx) *ebx = b;
    if (ecx) *ecx = c;
    if (edx) *edx = d;
}
#endif
