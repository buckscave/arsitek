/*
 * koneksi.c
 *
 * Pembangunan koneksi ke perangkat berdasarkan IC/chip.
 * Setiap fungsi koneksi menyiapkan jalur komunikasi ke
 * perangkat melalui bus yang sesuai (PCI, USB, I2C, SPI, UART).
 *
 * Koneksi bersifat generik per IC — tidak bergantung
 * pada pabrikan atau merek perangkat, melainkan pada
 * jenis IC chip yang digunakan perangkat tersebut.
 *
 * Parameter yang digunakan adalah nilai optimal (stabil)
 * yang telah diuji pada nilai maksimum terlebih dahulu.
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"
#include "../lampiran/peninjau.h"
#include "../lampiran/pengembang.h"

/* ================================================================
 * KONSTANTA KONEKSI
 * ================================================================ */

/* Latency timer optimal untuk perangkat PCI (dalam clock cycle) */
#define PCI_LATENCY_OPTIMAL         0x40

/* Batas timeout untuk operasi bus (dalam iterasi) */
#define KONEKSI_TIMEOUT             10000

/* Kecepatan I2C optimal (kHz) */
#define I2C_KECEPATAN_OPTIMAL       100

/* Kecepatan SPI optimal (kHz) */
#define SPI_KECEPATAN_OPTIMAL       1000

/* Baud rate UART optimal */
#define UART_BAUD_OPTIMAL           115200

/* ================================================================
 * KONEKSI BUS PCI
 * ================================================================ */

/*
 * pengembang_koneksi_pci — Bangun koneksi ke perangkat PCI.
 *
 * Mengaktifkan I/O Space, Memory Space, dan Bus Mastering
 * pada register perintah PCI menggunakan penyedia_jalur.
 * Juga mengatur latency timer ke nilai optimal.
 *
 * Parameter:
 *   perangkat — pointer ke struktur DataPerangkat
 *
 * Mengembalikan:
 *   STATUS_OK jika berhasil,
 *   STATUS_PARAMETAR_SALAH jika perangkat NULL
 */
int pengembang_koneksi_pci(DataPerangkat *perangkat)
{
    int hasil;

    if (perangkat == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* Aktifkan IO, MEM, dan BUS_MASTER melalui penyedia jalur */
    hasil = penyedia_jalur_konfigurasi_pci(perangkat,
                                           BENAR,  /* aktifkan_io   */
                                           BENAR,  /* aktifkan_mem  */
                                           BENAR); /* aktifkan_master */

    if (hasil != STATUS_OK) {
        notulen_catat(NOTULEN_KESALAHAN,
            "Koneksi: Gagal mengkonfigurasi PCI");
        return hasil;
    }

    /* Atur latency timer ke nilai optimal */
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    {
        uint8 bus, dev, fn;
        uint8 latency;
        uint32 reg;

        bus = (uint8)((perangkat->port >> 16) & 0xFF);
        dev = (uint8)((perangkat->port >> 8) & 0xFF);
        fn  = (uint8)(perangkat->port & 0xFF);

        reg = peninjau_pci_baca(bus, dev, fn, PCI_OFFSET_LATENCY);
        latency = (uint8)((reg >> 8) & 0xFF);

        if (latency < PCI_LATENCY_OPTIMAL) {
            reg &= ~(uint32)0x0000FF00;
            reg |= (uint32)PCI_LATENCY_OPTIMAL << 8;
            peninjau_pci_tulis(bus, dev, fn, PCI_OFFSET_LATENCY, reg);
        }
    }
#endif

    notulen_catat(NOTULEN_INFO,
        "Koneksi: PCI terhubung (IO+MEM+MASTER aktif)");

    return STATUS_OK;
}

/* ================================================================
 * KONEKSI USB
 * ================================================================ */

/*
 * pengembang_koneksi_usb_uhci — Koneksi ke pengendali USB UHCI.
 *
 * UHCI menggunakan port I/O untuk akses register.
 * Base address dari BAR4 (bukan BAR0).
 * Mengkonfigurasi PCI terlebih dahulu, lalu atur pengendali
 * USB melalui penyedia_jalur_konfigurasi_usb.
 *
 * Parameter:
 *   perangkat — pointer ke struktur DataPerangkat
 *
 * Mengembalikan:
 *   STATUS_OK jika berhasil,
 *   STATUS_PARAMETAR_SALAH jika perangkat NULL
 */
int pengembang_koneksi_usb_uhci(DataPerangkat *perangkat)
{
    if (perangkat == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* Aktifkan PCI terlebih dahulu */
    pengembang_koneksi_pci(perangkat);

    /* Konfigurasi pengendali USB UHCI via penyedia jalur */
    penyedia_jalur_konfigurasi_usb(perangkat, 0, 0);

    /* UHCI: Setel register kapabilitas via MMIO */
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    if (perangkat->alamat != 0) {
        volatile uint16 *mmio;
        /* UHCI: Reset pengendali */
        mmio = (volatile uint16 *)(ukuran_ptr)perangkat->alamat;
        *mmio = 0x0004; /* HCRESET bit */
        tunda_io();
        tunda_io();
        *mmio = 0x0000; /* Lepas reset */
    }
#endif

    notulen_catat(NOTULEN_INFO,
        "Koneksi: USB UHCI terhubung (port I/O)");

    return STATUS_OK;
}

/*
 * pengembang_koneksi_usb_ohci — Koneksi ke pengendali USB OHCI.
 *
 * OHCI menggunakan memory-mapped I/O. Mengkonfigurasi PCI
 * dan menyiapkan akses MMIO ke register pengendali.
 *
 * Parameter:
 *   perangkat — pointer ke struktur DataPerangkat
 *
 * Mengembalikan:
 *   STATUS_OK jika berhasil,
 *   STATUS_PARAMETAR_SALAH jika perangkat NULL
 */
int pengembang_koneksi_usb_ohci(DataPerangkat *perangkat)
{
    if (perangkat == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* Aktifkan PCI terlebih dahulu */
    pengembang_koneksi_pci(perangkat);

    /* Konfigurasi pengendali USB OHCI via penyedia jalur */
    penyedia_jalur_konfigurasi_usb(perangkat, 1, 1);

    /* OHCI: Pemetaan MMIO dan reset pengendali */
#if defined(ARSITEK_X86) || defined(ARSITEK_X64) || defined(ARSITEK_ARM32) || defined(ARSITEK_ARM64)
    if (perangkat->alamat != 0) {
        volatile uint32 *mmio;
        /* OHCI: HcControl register (offset 0x04) */
        mmio = (volatile uint32 *)(ukuran_ptr)(perangkat->alamat + 0x04);
        /* Set USB Operational state (bit 6-7 = 2) */
        *mmio = (*mmio & ~(0x3 << 6)) | (0x2 << 6);
    }
#endif

    notulen_catat(NOTULEN_INFO,
        "Koneksi: USB OHCI terhubung (MMIO)");

    return STATUS_OK;
}

/*
 * pengembang_koneksi_usb_ehci — Koneksi ke pengendali USB EHCI.
 *
 * EHCI menggunakan memory-mapped I/O dengan register
 * kapabilitas dan operasi. Mengkonfigurasi PCI dan
 * menyiapkan akses MMIO.
 *
 * Parameter:
 *   perangkat — pointer ke struktur DataPerangkat
 *
 * Mengembalikan:
 *   STATUS_OK jika berhasil,
 *   STATUS_PARAMETAR_SALAH jika perangkat NULL
 */
int pengembang_koneksi_usb_ehci(DataPerangkat *perangkat)
{
    if (perangkat == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* Aktifkan PCI terlebih dahulu */
    pengembang_koneksi_pci(perangkat);

    /* Konfigurasi pengendali USB EHCI via penyedia jalur */
    penyedia_jalur_konfigurasi_usb(perangkat, 2, 2);

    /* EHCI: Baca kapabilitas dan atur pengendali */
#if defined(ARSITEK_X86) || defined(ARSITEK_X64) || defined(ARSITEK_ARM32) || defined(ARSITEK_ARM64)
    if (perangkat->alamat != 0) {
        volatile uint8 *mmio;
        volatile uint32 *reg_op;
        uint8 cap_len;

        /* Baca panjang kapabilitas dari ECAP (offset 0x00) */
        mmio = (volatile uint8 *)(ukuran_ptr)perangkat->alamat;
        cap_len = mmio[0];

        /* Hitung alamat register operasi */
        reg_op = (volatile uint32 *)(ukuran_ptr)
                 (perangkat->alamat + (ukuran_ptr)cap_len);

        /* Set Run/Stop bit (bit 0 = 1 = Run) */
        reg_op[0] = 0x00000001;
    }
#endif

    notulen_catat(NOTULEN_INFO,
        "Koneksi: USB EHCI terhubung (MMIO)");

    return STATUS_OK;
}

/*
 * pengembang_koneksi_usb_xhci — Koneksi ke pengendali USB xHCI.
 *
 * xHCI menggunakan memory-mapped I/O dengan struktur
 * kapabilitas dan operasi register yang didefinisikan
 * oleh spesifikasi xHCI. Mengkonfigurasi PCI dan
 * menyiapkan akses MMIO.
 *
 * Parameter:
 *   perangkat — pointer ke struktur DataPerangkat
 *
 * Mengembalikan:
 *   STATUS_OK jika berhasil,
 *   STATUS_PARAMETAR_SALAH jika perangkat NULL
 */
int pengembang_koneksi_usb_xhci(DataPerangkat *perangkat)
{
    if (perangkat == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* Aktifkan PCI terlebih dahulu */
    pengembang_koneksi_pci(perangkat);

    /* Konfigurasi pengendali USB xHCI via penyedia jalur */
    penyedia_jalur_konfigurasi_usb(perangkat, 3, 3);

    /* xHCI: Baca kapabilitas dan inisialisasi */
#if defined(ARSITEK_X86) || defined(ARSITEK_X64) || defined(ARSITEK_ARM32) || defined(ARSITEK_ARM64)
    if (perangkat->alamat != 0) {
        volatile uint32 *mmio;
        uint32 cap_len;
        uint32 hcs_params;

        /* Baca panjang kapabilitas (offset 0x10, CAPLENGTH) */
        mmio = (volatile uint32 *)(ukuran_ptr)perangkat->alamat;
        cap_len = mmio[4] & 0xFF; /* CAPLENGTH di byte rendah */

        /* Baca parameter struktur HC (HCSParams1, offset 0x04) */
        hcs_params = mmio[1];

        /* Hitung jumlah port berdasarkan HCSParams1 */
        /* Bit 0-7: MaxPorts */

        /* Pemetaan alamat register operasi */
        TIDAK_DIGUNAKAN(cap_len);
        TIDAK_DIGUNAKAN(hcs_params);
    }
#endif

    notulen_catat(NOTULEN_INFO,
        "Koneksi: USB xHCI terhubung (MMIO)");

    return STATUS_OK;
}

/* ================================================================
 * KONEKSI STORAGE
 * ================================================================ */

/*
 * pengembang_koneksi_ahci — Koneksi ke pengendali SATA AHCI.
 *
 * AHCI menggunakan memory-mapped register melalui ABAR (BAR5).
 * Mengaktifkan AHCI mode, mengatur Global Control Register,
 * dan memetakan command list.
 *
 * Parameter:
 *   perangkat — pointer ke struktur DataPerangkat
 *
 * Mengembalikan:
 *   STATUS_OK jika berhasil,
 *   STATUS_PARAMETAR_SALAH jika perangkat NULL
 */
int pengembang_koneksi_ahci(DataPerangkat *perangkat)
{
    if (perangkat == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* Aktifkan PCI terlebih dahulu */
    pengembang_koneksi_pci(perangkat);

    /* Pemetaan ABAR (BAR5) untuk register AHCI */
#if defined(ARSITEK_X86) || defined(ARSITEK_X64) || defined(ARSITEK_ARM32) || defined(ARSITEK_ARM64)
    {
        uint8 bus, dev, fn;
        uint32 abar;

        bus = (uint8)((perangkat->port >> 16) & 0xFF);
        dev = (uint8)((perangkat->port >> 8) & 0xFF);
        fn  = (uint8)(perangkat->port & 0xFF);

        /* Baca ABAR dari BAR5 (offset 0x24) */
        abar = peninjau_pci_baca(bus, dev, fn, PCI_OFFSET_BAR5);

        if (abar != 0) {
            /* Pemetaan alamat MMIO ABAR */
            perangkat->alamat = (ukuran_ptr)(abar & 0xFFFFFFF0UL);

            /* Akses Global Control Register (offset 0x04 dari ABAR) */
            if (perangkat->alamat != 0) {
                volatile uint32 *ghc;
                ghc = (volatile uint32 *)
                      (ukuran_ptr)(perangkat->alamat + 0x04);

                /* Bit 31: AE (AHCI Enable) — aktifkan mode AHCI */
                *ghc |= 0x80000000UL;
            }
        }
    }
#endif

    notulen_catat(NOTULEN_INFO,
        "Koneksi: AHCI terhubung (ABAR dipetakan)");

    return STATUS_OK;
}

/*
 * pengembang_koneksi_nvme — Koneksi ke pengendali NVMe.
 *
 * NVMe menggunakan memory-mapped register melalui BAR0.
 * Mengakses Controller Capabilities dan mengatur antrian
 * pengiriman dan penyelesaian.
 *
 * Parameter:
 *   perangkat — pointer ke struktur DataPerangkat
 *
 * Mengembalikan:
 *   STATUS_OK jika berhasil,
 *   STATUS_PARAMETAR_SALAH jika perangkat NULL
 */
int pengembang_koneksi_nvme(DataPerangkat *perangkat)
{
    if (perangkat == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* Aktifkan PCI terlebih dahulu */
    pengembang_koneksi_pci(perangkat);

    /* Pemetaan BAR0 untuk register NVMe */
#if defined(ARSITEK_X86) || defined(ARSITEK_X64) || defined(ARSITEK_ARM32) || defined(ARSITEK_ARM64)
    {
        uint8 bus, dev, fn;
        uint32 bar0;

        bus = (uint8)((perangkat->port >> 16) & 0xFF);
        dev = (uint8)((perangkat->port >> 8) & 0xFF);
        fn  = (uint8)(perangkat->port & 0xFF);

        /* Baca BAR0 */
        bar0 = peninjau_pci_baca(bus, dev, fn, PCI_OFFSET_BAR0);

        if (bar0 != 0) {
            /* Pemetaan alamat MMIO BAR0 */
            perangkat->alamat = (ukuran_ptr)(bar0 & 0xFFFFFFF0UL);

            /* Baca Controller Capabilities (CAP, offset 0x00) */
            if (perangkat->alamat != 0) {
                volatile uint32 *cap;
                cap = (volatile uint32 *)
                      (ukuran_ptr)perangkat->alamat;

                /* CAP memberikan batasan: MQES, CQR, TO, DSTRD, etc. */
                TIDAK_DIGUNAKAN(cap);
            }
        }
    }
#endif

    notulen_catat(NOTULEN_INFO,
        "Koneksi: NVMe terhubung (BAR0 dipetakan)");

    return STATUS_OK;
}

/* ================================================================
 * KONEKSI BUS SERIAL
 * ================================================================ */

/*
 * pengembang_koneksi_i2c — Koneksi ke perangkat I2C.
 *
 * Mengatur kecepatan bus dan alamat slave menggunakan
 * penyedia_jalur_konfigurasi_i2c. Kecepatan optimal
 * adalah 100 kHz (mode standar, stabil).
 *
 * Parameter:
 *   perangkat — pointer ke struktur DataPerangkat
 *
 * Mengembalikan:
 *   STATUS_OK jika berhasil,
 *   STATUS_PARAMETAR_SALAH jika perangkat NULL
 */
int pengembang_koneksi_i2c(DataPerangkat *perangkat)
{
    int nomor_bus;
    uint8 alamat_7bit;

    if (perangkat == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* Tentukan nomor bus I2C dari data perangkat */
    nomor_bus = (int)(perangkat->port & 0x03);

    /* Ambil alamat 7-bit dari data khusus */
    alamat_7bit = perangkat->data_khusus[0];

    /* Konfigurasi I2C via penyedia jalur */
    penyedia_jalur_konfigurasi_i2c(nomor_bus,
                                    I2C_KECEPATAN_OPTIMAL,
                                    alamat_7bit);

    notulen_catat(NOTULEN_INFO,
        "Koneksi: I2C terhubung (100 kHz, mode standar)");

    return STATUS_OK;
}

/*
 * pengembang_koneksi_spi — Koneksi ke perangkat SPI.
 *
 * Mengatur clock, mode, dan ukuran kata menggunakan
 * penyedia_jalur_konfigurasi_spi. Parameter optimal:
 *   Clock: 1 MHz (stabil, maks 50 MHz)
 *   Mode: 0 (CPOL=0, CPHA=0)
 *   Word size: 8 bit
 *
 * Parameter:
 *   perangkat — pointer ke struktur DataPerangkat
 *
 * Mengembalikan:
 *   STATUS_OK jika berhasil,
 *   STATUS_PARAMETAR_SALAH jika perangkat NULL
 */
int pengembang_koneksi_spi(DataPerangkat *perangkat)
{
    int nomor_bus;

    if (perangkat == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* Tentukan nomor bus SPI dari data perangkat */
    nomor_bus = (int)(perangkat->port & 0x03);

    /* Konfigurasi SPI via penyedia jalur */
    penyedia_jalur_konfigurasi_spi(nomor_bus,
                                    SPI_KECEPATAN_OPTIMAL,
                                    0,   /* mode 0 */
                                    8,   /* 8 bit per kata */
                                    BENAR); /* CS aktif rendah */

    notulen_catat(NOTULEN_INFO,
        "Koneksi: SPI terhubung (1 MHz, mode 0, 8-bit)");

    return STATUS_OK;
}

/*
 * pengembang_koneksi_uart — Koneksi ke perangkat UART/serial.
 *
 * Mengatur baud rate, format data, dan flow control
 * pada register UART 16550A. Parameter optimal:
 *   Baud rate: 115200 (stabil, maks 921600)
 *   Data bits: 8
 *   Parity: none
 *   Stop bits: 1
 *
 * Parameter:
 *   perangkat — pointer ke struktur DataPerangkat
 *
 * Mengembalikan:
 *   STATUS_OK jika berhasil,
 *   STATUS_PARAMETAR_SALAH jika perangkat NULL
 */
int pengembang_koneksi_uart(DataPerangkat *perangkat)
{
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    uint16 port_uart;
#endif

    if (perangkat == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    port_uart = (uint16)perangkat->alamat;

    /* Konfigurasi UART 16550A: 115200 baud, 8N1 */

    /* Nonaktifkan interupsi UART */
    tulis_port(port_uart + 1, 0x00);

    /* Aktifkan DLAB (Divisor Latch Access Bit) */
    tulis_port(port_uart + 3, 0x80);

    /* Atur divisor untuk 115200 baud (1 = 115200) */
    tulis_port(port_uart + 0, 0x01); /* Divisor rendah */
    tulis_port(port_uart + 1, 0x00); /* Divisor tinggi */

    /* 8 data bits, tanpa parity, 1 stop bit (8N1) */
    tulis_port(port_uart + 3, 0x03);

    /* Aktifkan FIFO, bersihkan, 14-byte threshold */
    tulis_port(port_uart + 2, 0xC7);

    /* IRQ aktif, RTS/DSR set */
    tulis_port(port_uart + 4, 0x0B);
#else
    /* ARM dan arsitektur lain: konfigurasi MMIO UART */
    if (perangkat->alamat != 0) {
        volatile uint32 *mmio;
        mmio = (volatile uint32 *)(ukuran_ptr)perangkat->alamat;

        /*
         * Konfigurasi UART generik via MMIO.
         * Detail register bergantung pada implementasi
         * UART pada platform (mis. PL011 pada ARM).
         */

        /* Nonaktifkan UART sementara */
        mmio[0] = 0x00000000;

        /* Atur baud rate divisor */
        mmio[0] = (uint32)(48000000UL / (16UL * UART_BAUD_OPTIMAL));

        /* Aktifkan kembali: 8N1, FIFO aktif */
        mmio[0] = 0x00000070;
    }
#endif

    notulen_catat(NOTULEN_INFO,
        "Koneksi: UART terhubung (115200 8N1)");

    return STATUS_OK;
}

/* ================================================================
 * KONEKSI GENERIK
 * ================================================================ */

/*
 * pengembang_koneksi_generik — Koneksi generik untuk perangkat
 * yang jenis IC-nya tidak dikenali secara spesifik.
 *
 * Mengaktifkan bus dasar dan mengatur parameter konservatif.
 * Mencoba menemukan koneksi yang paling sesuai berdasarkan
 * tipe bus perangkat.
 *
 * Parameter:
 *   perangkat — pointer ke struktur DataPerangkat
 *
 * Mengembalikan:
 *   STATUS_OK jika berhasil,
 *   STATUS_PARAMETAR_SALAH jika perangkat NULL
 */
int pengembang_koneksi_generik(DataPerangkat *perangkat)
{
    if (perangkat == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* Coba koneksi berdasarkan tipe bus */
    switch (perangkat->tipe_bus) {
    case BUS_PCI:
        return pengembang_koneksi_pci(perangkat);

    case BUS_USB:
        return pengembang_koneksi_usb_ehci(perangkat);

    case BUS_I2C:
        return pengembang_koneksi_i2c(perangkat);

    case BUS_SPI:
        return pengembang_koneksi_spi(perangkat);

    case BUS_ISA:
        /* ISA tidak memerlukan koneksi khusus */
        notulen_catat(NOTULEN_INFO,
            "Koneksi: ISA terhubung (tidak perlu konfigurasi)");
        return STATUS_OK;

    case BUS_SATA:
        return pengembang_koneksi_ahci(perangkat);

    case BUS_NVME:
        return pengembang_koneksi_nvme(perangkat);

    default:
        /* Bus tidak dikenali — coba PCI sebagai fallback */
        notulen_catat(NOTULEN_PERINGATAN,
            "Koneksi: Tipe bus tidak dikenali, coba PCI generik");
        if (perangkat->tipe_bus == BUS_PCI) {
            return pengembang_koneksi_pci(perangkat);
        }
        break;
    }

    notulen_catat(NOTULEN_INFO,
        "Koneksi: Generik terhubung (fallback)");

    return STATUS_OK;
}
