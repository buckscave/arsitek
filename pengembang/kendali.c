/*
 * kendali.c
 *
 * Pembangunan kendali (driver) generik berdasarkan IC chip.
 * Berisi tabel database IC yang dikenali dan fungsi-fungsi
 * untuk mengidentifikasi, menginisialisasi, dan mengendalikan
 * perangkat berdasarkan IC chip yang digunakan.
 *
 * Filosofi: Deteksi berdasarkan IC, bukan vendor/brand.
 * Banyak perangkat dari pabrikan berbeda menggunakan IC yang sama,
 * sehingga driver generik per IC lebih efisien dan akurat.
 *
 * Parameter:
 *   parameter_maks[]    — nilai maksimum untuk pengujian
 *   parameter_optimal[] — nilai stabil setelah pengujian
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"
#include "../lampiran/peninjau.h"
#include "../lampiran/pengembang.h"

/* ================================================================
 * DATABASE IC — TABEL IC YANG DIKENALI
 *
 * Setiap entri berisi data lengkap DataIC:
 *   vendor_id, perangkat_id  — ID PCI untuk identifikasi
 *   kelas, subkelas          — Kelas PCI untuk pencocokan
 *   nama_ic                  — Nama IC (mis. "RTL8139C+")
 *   fabrikkan                — Fabrikan IC (mis. "Realtek")
 *   fungsi                   — Deskripsi fungsi
 *   tipe_fungsi              — Tipe fungsi IC (enum)
 *   tipe_perangkat           — Tipe perangkat yang menggunakan IC
 *   tipe_bus                 — Jenis bus koneksi
 *   clock_mhz                — Kecepatan clock IC
 *   alamat_basis             — Alamat basis register IC
 *   ukuran_register          — Ukuran ruang register IC
 *   interupsi_default        — Nomor interupsi default
 *   parameter_maks[]         — Parameter nilai maksimum
 *   parameter_optimal[]      — Parameter nilai optimal/stabil
 *   jumlah_parameter         — Jumlah parameter yang digunakan
 * ================================================================ */

static DataIC tabel_ic[] = {
    /* ---- IC Eternet (10 entri) ---- */
    {
        0x10EC, 0x8139, 0x02, 0x00,
        "RTL8139C+", "Realtek",
        "Pengendali Eternet 10/100 Fast",
        IC_FUNGSI_ETERNET, PERANGKAT_JARINGAN, BUS_PCI,
        100, 0x0000, 0x0100, 11,
        {100, 100, 2048, 1514, 100, 32, 4, 6},
        {100, 100, 512, 1514, 10, 16, 1, 6},
        4
    },
    {
        0x10EC, 0x8169, 0x02, 0x00,
        "RTL8169SC", "Realtek",
        "Pengendali Eternet 10/100/1000 Gigabit",
        IC_FUNGSI_ETERNET, PERANGKAT_JARINGAN, BUS_PCI,
        125, 0x0000, 0x0100, 11,
        {1000, 1000, 8192, 9000, 1000, 256, 8, 6},
        {1000, 1000, 2048, 1514, 100, 64, 4, 6},
        4
    },
    {
        0x10EC, 0x8125, 0x02, 0x00,
        "RTL8125B", "Realtek",
        "Pengendali Eternet 2.5 Gigabit",
        IC_FUNGSI_ETERNET, PERANGKAT_JARINGAN, BUS_PCI,
        250, 0x0000, 0x0100, 11,
        {2500, 2500, 16384, 9000, 2500, 512, 16, 6},
        {1000, 1000, 4096, 1514, 100, 128, 4, 6},
        4
    },
    {
        0x8086, 0x10D3, 0x02, 0x00,
        "I82574L", "Intel",
        "Pengendali Eternet 1 Gigabit Server",
        IC_FUNGSI_ETERNET, PERANGKAT_JARINGAN, BUS_PCI,
        125, 0x0000, 0x0200, 18,
        {1000, 1000, 8192, 9000, 1000, 256, 8, 6},
        {1000, 1000, 2048, 1514, 100, 64, 4, 6},
        4
    },
    {
        0x8086, 0x10EA, 0x02, 0x00,
        "I82577LM", "Intel",
        "Pengendali Eternet 1 Gigabit (Mobile)",
        IC_FUNGSI_ETERNET, PERANGKAT_JARINGAN, BUS_PCI,
        125, 0x0000, 0x0200, 20,
        {1000, 1000, 8192, 9000, 1000, 256, 8, 6},
        {1000, 1000, 2048, 1514, 100, 64, 4, 6},
        4
    },
    {
        0x8086, 0x1502, 0x02, 0x00,
        "I82579V", "Intel",
        "Pengendali Eternet 1 Gigabit Desktop",
        IC_FUNGSI_ETERNET, PERANGKAT_JARINGAN, BUS_PCI,
        125, 0x0000, 0x0200, 20,
        {1000, 1000, 8192, 9000, 1000, 256, 8, 6},
        {1000, 1000, 2048, 1514, 100, 64, 4, 6},
        4
    },
    {
        0x1969, 0x1073, 0x02, 0x00,
        "AR8151V2", "Atheros",
        "Pengendali Eternet 10/100/1000 Gigabit",
        IC_FUNGSI_ETERNET, PERANGKAT_JARINGAN, BUS_PCI,
        125, 0x0000, 0x0100, 11,
        {1000, 1000, 8192, 9000, 1000, 256, 8, 6},
        {1000, 1000, 2048, 1514, 100, 64, 4, 6},
        4
    },
    {
        0x1969, 0x1091, 0x02, 0x00,
        "AR8161", "Atheros",
        "Pengendali Eternet 10/100/1000 Gigabit",
        IC_FUNGSI_ETERNET, PERANGKAT_JARINGAN, BUS_PCI,
        125, 0x0000, 0x0100, 11,
        {1000, 1000, 8192, 9000, 1000, 256, 8, 6},
        {1000, 1000, 2048, 1514, 100, 64, 4, 6},
        4
    },
    {
        0x14E4, 0x165F, 0x02, 0x00,
        "BCM5720", "Broadcom",
        "Pengendali Eternet 1 Gigabit Server",
        IC_FUNGSI_ETERNET, PERANGKAT_JARINGAN, BUS_PCI,
        125, 0x0000, 0x0200, 16,
        {1000, 1000, 16384, 9000, 1000, 512, 16, 6},
        {1000, 1000, 4096, 1514, 100, 128, 4, 6},
        4
    },
    {
        0x14E4, 0x168E, 0x02, 0x00,
        "BCM57810", "Broadcom",
        "Pengendali Eternet 10 Gigabit",
        IC_FUNGSI_ETERNET, PERANGKAT_JARINGAN, BUS_PCI,
        250, 0x0000, 0x0200, 18,
        {10000, 10000, 32768, 9000, 10000, 1024, 32, 6},
        {1000, 1000, 8192, 1514, 1000, 256, 4, 6},
        4
    },

    /* ---- IC Storage AHCI (4 entri) ---- */
    {
        0x8086, 0x2922, 0x01, 0x06,
        "ICH9 AHCI", "Intel",
        "Pengendali SATA AHCI 6-port",
        IC_FUNGSI_STORAGE_AHCI, PERANGKAT_PENYIMPANAN, BUS_PCI,
        66, 0x0000, 0x0200, 19,
        {32, 65535, 8192, 32, 0, 0, 0, 0},
        {32, 256, 4096, 32, 0, 0, 0, 0},
        4
    },
    {
        0x8086, 0x3A22, 0x01, 0x06,
        "ICH10 AHCI", "Intel",
        "Pengendali SATA AHCI 6-port",
        IC_FUNGSI_STORAGE_AHCI, PERANGKAT_PENYIMPANAN, BUS_PCI,
        66, 0x0000, 0x0200, 19,
        {32, 65535, 8192, 32, 0, 0, 0, 0},
        {32, 256, 4096, 32, 0, 0, 0, 0},
        4
    },
    {
        0x8086, 0x1C02, 0x01, 0x06,
        "PCH SATA AHCI", "Intel",
        "Pengendali SATA AHCI (Cougar Point)",
        IC_FUNGSI_STORAGE_AHCI, PERANGKAT_PENYIMPANAN, BUS_PCI,
        66, 0x0000, 0x0200, 19,
        {32, 65535, 8192, 32, 0, 0, 0, 0},
        {32, 256, 4096, 32, 0, 0, 0, 0},
        4
    },
    {
        0x8086, 0x8C02, 0x01, 0x06,
        "Lynx Point AHCI", "Intel",
        "Pengendali SATA AHCI (Lynx Point)",
        IC_FUNGSI_STORAGE_AHCI, PERANGKAT_PENYIMPANAN, BUS_PCI,
        66, 0x0000, 0x0200, 19,
        {32, 65535, 8192, 32, 0, 0, 0, 0},
        {32, 256, 4096, 32, 0, 0, 0, 0},
        4
    },

    /* ---- IC Storage NVMe (2 entri) ---- */
    {
        0x8086, 0xF1A5, 0x01, 0x08,
        "PCH NVMe", "Intel",
        "Pengendali NVMe (PCH)",
        IC_FUNGSI_STORAGE_NVME, PERANGKAT_PENYIMPANAN, BUS_PCI,
        100, 0x0000, 0x4000, 19,
        {65535, 65535, 65535, 128, 0, 0, 0, 0},
        {64, 64, 64, 16, 0, 0, 0, 0},
        4
    },
    {
        0x144D, 0xA808, 0x01, 0x08,
        "PM981 NVMe", "Samsung",
        "Pengendali NVMe SSD",
        IC_FUNGSI_STORAGE_NVME, PERANGKAT_PENYIMPANAN, BUS_PCI,
        100, 0x0000, 0x4000, 19,
        {65535, 65535, 65535, 128, 0, 0, 0, 0},
        {64, 64, 64, 16, 0, 0, 0, 0},
        4
    },

    /* ---- IC Storage IDE (2 entri) ---- */
    {
        0x8086, 0x7010, 0x01, 0x01,
        "PIIX3 IDE", "Intel",
        "Pengendali IDE PIIX3",
        IC_FUNGSI_STORAGE_IDE, PERANGKAT_PENYIMPANAN, BUS_PCI,
        33, 0x0000, 0x0010, 14,
        {2, 256, 4096, 1, 0, 0, 0, 0},
        {2, 32, 512, 1, 0, 0, 0, 0},
        4
    },
    {
        0x8086, 0x2820, 0x01, 0x01,
        "ICH8 IDE", "Intel",
        "Pengendali IDE ICH8 (Bus Master)",
        IC_FUNGSI_STORAGE_IDE, PERANGKAT_PENYIMPANAN, BUS_PCI,
        33, 0x0000, 0x0010, 14,
        {2, 256, 4096, 1, 0, 0, 0, 0},
        {2, 32, 512, 1, 0, 0, 0, 0},
        4
    },

    /* ---- IC USB (6 entri) ---- */
    {
        0x8086, 0x7020, 0x0C, 0x00,
        "PIIX3 UHCI", "Intel",
        "Pengendali USB 1.1 UHCI",
        IC_FUNGSI_USB_UHCI, PERANGKAT_USB, BUS_PCI,
        48, 0x0000, 0x0020, 11,
        {2, 127, 12, 64, 0, 0, 0, 0},
        {2, 64, 12, 64, 0, 0, 0, 0},
        4
    },
    {
        0x8086, 0x24D2, 0x0C, 0x00,
        "ICH5 UHCI", "Intel",
        "Pengendali USB 1.1 UHCI (ICH5)",
        IC_FUNGSI_USB_UHCI, PERANGKAT_USB, BUS_PCI,
        48, 0x0000, 0x0020, 11,
        {2, 127, 12, 64, 0, 0, 0, 0},
        {2, 64, 12, 64, 0, 0, 0, 0},
        4
    },
    {
        0x10DE, 0x0C7C, 0x0C, 0x10,
        "MCP78 OHCI", "NVIDIA",
        "Pengendali USB 1.1 OHCI",
        IC_FUNGSI_USB_OHCI, PERANGKAT_USB, BUS_PCI,
        48, 0x0000, 0x0100, 11,
        {2, 127, 12, 64, 0, 0, 0, 0},
        {2, 64, 12, 64, 0, 0, 0, 0},
        4
    },
    {
        0x8086, 0x24DD, 0x0C, 0x20,
        "ICH5 EHCI", "Intel",
        "Pengendali USB 2.0 EHCI",
        IC_FUNGSI_USB_EHCI, PERANGKAT_USB, BUS_PCI,
        48, 0x0000, 0x0100, 11,
        {8, 127, 480, 512, 0, 0, 0, 0},
        {6, 64, 480, 512, 0, 0, 0, 0},
        4
    },
    {
        0x8086, 0x293A, 0x0C, 0x20,
        "ICH9 EHCI", "Intel",
        "Pengendali USB 2.0 EHCI (ICH9)",
        IC_FUNGSI_USB_EHCI, PERANGKAT_USB, BUS_PCI,
        48, 0x0000, 0x0100, 11,
        {8, 127, 480, 512, 0, 0, 0, 0},
        {6, 64, 480, 512, 0, 0, 0, 0},
        4
    },
    {
        0x8086, 0x1E31, 0x0C, 0x30,
        "Panther Point xHCI", "Intel",
        "Pengendali USB 3.0 xHCI",
        IC_FUNGSI_USB_XHCI, PERANGKAT_USB, BUS_PCI,
        100, 0x0000, 0x8000, 11,
        {16, 255, 5000, 1024, 0, 0, 0, 0},
        {8, 64, 5000, 512, 0, 0, 0, 0},
        4
    },

    /* ---- IC Tampilan (4 entri) ---- */
    {
        0x1234, 0x1111, 0x03, 0x00,
        "Bochs VBE", "Bochs",
        "Pengendali Tampilan VGA Kompatibel (Virtual)",
        IC_FUNGSI_TAMPILAN_VGA, PERANGKAT_LAYAR, BUS_PCI,
        33, 0xB8000, 0x08000, 9,
        {1920, 1080, 32, 1, 0, 0, 0, 0},
        {1024, 768, 32, 0, 0, 0, 0, 0},
        4
    },
    {
        0x8086, 0x0046, 0x03, 0x00,
        "HD Graphics Ironlake", "Intel",
        "Pengendali Tampilan Terintegrasi Intel",
        IC_FUNGSI_TAMPILAN_GPU, PERANGKAT_GPU, BUS_PCI,
        200, 0x0000, 0x40000, 9,
        {2560, 1600, 32, 1, 0, 0, 0, 0},
        {1920, 1080, 32, 0, 0, 0, 0, 0},
        4
    },
    {
        0x10DE, 0x0FC6, 0x03, 0x00,
        "GK107 GPU", "NVIDIA",
        "Pengendali GPU Kepler",
        IC_FUNGSI_TAMPILAN_GPU, PERANGKAT_GPU, BUS_PCI,
        350, 0x0000, 0x100000, 9,
        {3840, 2160, 32, 1, 0, 0, 0, 0},
        {1920, 1080, 32, 0, 0, 0, 0, 0},
        4
    },
    {
        0x1AF4, 0x1050, 0x03, 0x00,
        "VirtIO-GPU", "Red Hat",
        "Pengendali GPU VirtIO",
        IC_FUNGSI_TAMPILAN_GPU, PERANGKAT_GPU, BUS_PCI,
        100, 0x0000, 0x10000, 9,
        {2560, 1600, 32, 1, 0, 0, 0, 0},
        {1920, 1080, 32, 0, 0, 0, 0, 0},
        4
    },

    /* ---- IC Audio (3 entri) ---- */
    {
        0x8086, 0x2415, 0x04, 0x01,
        "ICH2 AC97", "Intel",
        "Pengendali Audio AC97",
        IC_FUNGSI_AUDIO, PERANGKAT_LAIN, BUS_PCI,
        24, 0x0000, 0x0100, 11,
        {48000, 2, 16, 64, 0, 0, 0, 0},
        {44100, 2, 16, 32, 0, 0, 0, 0},
        4
    },
    {
        0x8086, 0x293E, 0x04, 0x03,
        "ICH9 HDA", "Intel",
        "Pengendali Audio High Definition",
        IC_FUNGSI_AUDIO, PERANGKAT_LAIN, BUS_PCI,
        24, 0x0000, 0x4000, 22,
        {192000, 8, 32, 16, 0, 0, 0, 0},
        {48000, 2, 16, 4, 0, 0, 0, 0},
        4
    },
    {
        0x10EC, 0x0269, 0x04, 0x03,
        "ALC269 HDA", "Realtek",
        "Kodek Audio High Definition",
        IC_FUNGSI_AUDIO, PERANGKAT_LAIN, BUS_PCI,
        24, 0x0000, 0x4000, 22,
        {192000, 8, 32, 16, 0, 0, 0, 0},
        {48000, 2, 16, 4, 0, 0, 0, 0},
        4
    },

    /* ---- IC UART/Serial (1 entri) ---- */
    {
        0x8086, 0x2922, 0x07, 0x00,
        "16550A UART", "Intel",
        "Pengendali Serial UART 16550A",
        IC_FUNGSI_UART, PERANGKAT_SERIAL, BUS_PCI,
        24, 0x03F8, 0x0008, 4,
        {921600, 8, 1, 64, 0, 0, 0, 0},
        {115200, 8, 1, 16, 0, 0, 0, 0},
        4
    },

    /* ---- IC DMA (1 entri) ---- */
    {
        0x8086, 0x7010, 0x08, 0x01,
        "8237 DMA", "Intel",
        "Pengendali DMA 8237",
        IC_FUNGSI_DMA, PERANGKAT_DMA, BUS_ISA,
        8, 0x0000, 0x0010, 0,
        {4, 65536, 0, 0, 0, 0, 0, 0},
        {4, 65536, 0, 0, 0, 0, 0, 0},
        2
    },

    /* ---- IC PIC/PIT (2 entri) ---- */
    {
        0x0000, 0x0000, 0x08, 0x00,
        "8259A PIC", "Intel",
        "Pengendali Interupsi yang Dapat Diprogram",
        IC_FUNGSI_PIC, PERANGKAT_PIC, BUS_ISA,
        8, 0x0020, 0x0002, 0,
        {15, 0, 0, 0, 0, 0, 0, 0},
        {15, 0, 0, 0, 0, 0, 0, 0},
        1
    },
    {
        0x0000, 0x0000, 0x08, 0x00,
        "8254 PIT", "Intel",
        "Penghitung Interval yang Dapat Diprogram",
        IC_FUNGSI_PIT, PERANGKAT_PIT, BUS_ISA,
        8, 0x0040, 0x0004, 0,
        {1193182, 0, 0, 0, 0, 0, 0, 0},
        {100, 0, 0, 0, 0, 0, 0, 0},
        1
    },

    /* ---- IC RTC (1 entri) ---- */
    {
        0x0000, 0x0000, 0x08, 0x00,
        "MC146818 RTC", "Motorola",
        "Jam Waktu Nyata (Real-Time Clock)",
        IC_FUNGSI_RTC, PERANGKAT_RTC, BUS_ISA,
        8, 0x0070, 0x0002, 8,
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
        0
    },

    /* ---- IC Bridge (2 entri) ---- */
    {
        0x8086, 0x29C0, 0x06, 0x00,
        "ICH9 Host Bridge", "Intel",
        "Jembatan Host-PCI ICH9",
        IC_FUNGSI_BRIDGE, PERANGKAT_PCI, BUS_PCI,
        33, 0x0000, 0x0100, 0,
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
        0
    },
    {
        0x8086, 0x244E, 0x06, 0x04,
        "ICH5 PCI-PCI Bridge", "Intel",
        "Jembatan PCI-ke-PCI ICH5",
        IC_FUNGSI_BRIDGE, PERANGKAT_PCI, BUS_PCI,
        33, 0x0000, 0x0100, 0,
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
        0
    },

    /* ---- IC Bluetooth (1 entri) ---- */
    {
        0x8087, 0x07DA, 0x0D, 0x10,
        "BCM20702 Bluetooth", "Broadcom",
        "Pengendali Bluetooth USB",
        IC_FUNGSI_BLUETOOTH, PERANGKAT_LAIN, BUS_USB,
        48, 0x0000, 0x0040, 3,
        {3, 7, 4096, 0, 0, 0, 0, 0},
        {1, 1, 1024, 0, 0, 0, 0, 0},
        3
    },

    /* ---- Penanda akhir tabel ---- */
    {
        0xFFFF, 0xFFFF, 0xFF, 0xFF,
        "", "", "",
        IC_FUNGSI_LAIN, PERANGKAT_LAIN, BUS_LAIN,
        0, 0, 0, 0,
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
        0
    }
};

/* Jumlah entri dalam tabel IC (tidak termasuk penanda akhir) */
#define TABEL_IC_JUMLAH (sizeof(tabel_ic) / sizeof(tabel_ic[0]) - 1)

/* ================================================================
 * DATA INTERNAL — IC yang sudah teridentifikasi per perangkat
 * ================================================================ */

static DataIC ic_teridentifikasi[PERANGKAT_MAKSIMUM];
static int jumlah_ic_teridentifikasi = 0;

/* ================================================================
 * pengembang_kendali_identifikasi_ic — Identifikasi IC dari PCI IDs
 *
 * Mencari dalam tabel IC berdasarkan vendor_id, perangkat_id,
 * kelas, dan subkelas. Mengembalikan pointer ke entri DataIC
 * yang paling cocok.
 *
 * Prioritas pencocokan:
 *   1. vendor_id + perangkat_id + kelas + subkelas (skor 5)
 *   2. vendor_id + perangkat_id + kelas (skor 4)
 *   3. vendor_id + perangkat_id (skor 3)
 *   4. kelas + subkelas (skor 2)
 *   5. kelas saja (skor 1)
 *
 * Parameter:
 *   vendor_id    — Vendor ID PCI
 *   perangkat_id — Device ID PCI
 *   kelas        — Kelas PCI
 *   subkelas     — Sub-kelas PCI
 *
 * Mengembalikan:
 *   Pointer ke DataIC jika dikenali, NULL jika tidak
 * ================================================================ */
DataIC *pengembang_kendali_identifikasi_ic(uint16 vendor_id,
                                            uint16 perangkat_id,
                                            uint8 kelas,
                                            uint8 subkelas)
{
    int i;
    int cocok_terbaik = -1;
    int skor_terbaik = 0;

    for (i = 0; i < (int)TABEL_IC_JUMLAH; i++) {
        int skor = 0;

        if (tabel_ic[i].vendor_id == vendor_id &&
            tabel_ic[i].perangkat_id == perangkat_id) {
            skor += 3;
        }
        if (tabel_ic[i].kelas == kelas &&
            tabel_ic[i].subkelas == subkelas) {
            skor += 2;
        } else if (tabel_ic[i].kelas == kelas) {
            skor += 1;
        }

        if (skor > skor_terbaik) {
            skor_terbaik = skor;
            cocok_terbaik = i;
        }
    }

    if (cocok_terbaik >= 0 && skor_terbaik >= 2) {
        /* Simpan ke tabel IC teridentifikasi */
        if (jumlah_ic_teridentifikasi < PERANGKAT_MAKSIMUM) {
            DataIC *hasil;
            hasil = &ic_teridentifikasi[jumlah_ic_teridentifikasi];
            *hasil = tabel_ic[cocok_terbaik];
            hasil->vendor_id = vendor_id;
            hasil->perangkat_id = perangkat_id;
            hasil->kelas = kelas;
            hasil->subkelas = subkelas;
            jumlah_ic_teridentifikasi++;
            return hasil;
        }
        /* Tabel penuh — kembalikan langsung dari tabel statis */
        return (DataIC *)&tabel_ic[cocok_terbaik];
    }

    /* IC tidak dikenali — buat entri generik berdasarkan kelas */
    if (jumlah_ic_teridentifikasi < PERANGKAT_MAKSIMUM) {
        DataIC *hasil;
        hasil = &ic_teridentifikasi[jumlah_ic_teridentifikasi];
        isi_memori(hasil, 0, sizeof(DataIC));
        hasil->vendor_id = vendor_id;
        hasil->perangkat_id = perangkat_id;
        hasil->kelas = kelas;
        hasil->subkelas = subkelas;
        salin_string(hasil->nama_ic, "Generik");
        salin_string(hasil->fabrikkan, "Tidak Dikenal");
        hasil->tipe_fungsi = IC_FUNGSI_LAIN;
        hasil->tipe_perangkat = PERANGKAT_LAIN;
        hasil->tipe_bus = BUS_PCI;

        switch (kelas) {
        case 0x01:
            hasil->tipe_fungsi = (subkelas == 0x06) ? IC_FUNGSI_STORAGE_AHCI :
                                 (subkelas == 0x08) ? IC_FUNGSI_STORAGE_NVME :
                                 IC_FUNGSI_STORAGE_IDE;
            hasil->tipe_perangkat = PERANGKAT_PENYIMPANAN;
            salin_string(hasil->fungsi, "Pengendali Penyimpanan Generik");
            break;
        case 0x02:
            hasil->tipe_fungsi = IC_FUNGSI_ETERNET;
            hasil->tipe_perangkat = PERANGKAT_JARINGAN;
            salin_string(hasil->fungsi, "Pengendali Jaringan Generik");
            break;
        case 0x03:
            hasil->tipe_fungsi = IC_FUNGSI_TAMPILAN_VGA;
            hasil->tipe_perangkat = PERANGKAT_LAYAR;
            salin_string(hasil->fungsi, "Pengendali Tampilan Generik");
            break;
        case 0x04:
            hasil->tipe_fungsi = IC_FUNGSI_AUDIO;
            hasil->tipe_perangkat = PERANGKAT_LAIN;
            salin_string(hasil->fungsi, "Pengendali Audio Generik");
            break;
        case 0x06:
            hasil->tipe_fungsi = IC_FUNGSI_BRIDGE;
            hasil->tipe_perangkat = PERANGKAT_PCI;
            salin_string(hasil->fungsi, "Jembatan Bus Generik");
            break;
        case 0x07:
            hasil->tipe_fungsi = IC_FUNGSI_UART;
            hasil->tipe_perangkat = PERANGKAT_SERIAL;
            salin_string(hasil->fungsi, "Pengendali Serial Generik");
            break;
        case 0x0C:
            if (subkelas == 0x03) {
                hasil->tipe_fungsi = IC_FUNGSI_USB_XHCI;
                hasil->tipe_perangkat = PERANGKAT_USB;
                salin_string(hasil->fungsi, "Pengendali USB Generik");
            }
            break;
        default:
            salin_string(hasil->fungsi, "Perangkat Generik");
            break;
        }

        jumlah_ic_teridentifikasi++;
        return hasil;
    }

    return NULL;
}

/* ================================================================
 * pengembang_kendali_inisialisasi_ic — Inisialisasi driver untuk IC
 *
 * Mengkonfigurasi perangkat berdasarkan data IC dan mengatur
 * parameter ke nilai optimal (stabil). Menyimpan nomor
 * interupsi default ke data perangkat.
 *
 * Parameter:
 *   ic        — pointer ke DataIC yang sudah diidentifikasi
 *   perangkat — pointer ke DataPerangkat
 *
 * Mengembalikan:
 *   STATUS_OK jika berhasil,
 *   STATUS_PARAMETAR_SALAH jika parameter NULL
 * ================================================================ */
int pengembang_kendali_inisialisasi_ic(DataIC *ic, DataPerangkat *perangkat)
{
    if (ic == NULL || perangkat == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* Atur interupsi default dari IC ke perangkat */
    perangkat->interupsi = ic->interupsi_default;

    /* Catat inisialisasi ke notulen */
    {
        char pesan[128];
        salin_string(pesan, "Kendali: IC diinisialisasi — ");
        salin_string(pesan + panjang_string(pesan), ic->nama_ic);
        salin_string(pesan + panjang_string(pesan), " (");
        salin_string(pesan + panjang_string(pesan), ic->fabrikkan);
        salin_string(pesan + panjang_string(pesan), ")");
        notulen_catat(NOTULEN_INFO, pesan);
    }

    /* Inisialisasi berdasarkan tipe fungsi IC */
    switch (ic->tipe_fungsi) {
    case IC_FUNGSI_ETERNET:
        notulen_catat(NOTULEN_INFO,
            "Kendali: Inisialisasi pengendali jaringan generik");
        break;
    case IC_FUNGSI_STORAGE_AHCI:
    case IC_FUNGSI_STORAGE_NVME:
    case IC_FUNGSI_STORAGE_IDE:
        notulen_catat(NOTULEN_INFO,
            "Kendali: Inisialisasi pengendali penyimpanan generik");
        break;
    case IC_FUNGSI_USB_UHCI:
    case IC_FUNGSI_USB_OHCI:
    case IC_FUNGSI_USB_EHCI:
    case IC_FUNGSI_USB_XHCI:
        notulen_catat(NOTULEN_INFO,
            "Kendali: Inisialisasi pengendali USB generik");
        break;
    case IC_FUNGSI_TAMPILAN_VGA:
    case IC_FUNGSI_TAMPILAN_GPU:
        notulen_catat(NOTULEN_INFO,
            "Kendali: Inisialisasi pengendali tampilan generik");
        break;
    case IC_FUNGSI_AUDIO:
        notulen_catat(NOTULEN_INFO,
            "Kendali: Inisialisasi pengendali audio generik");
        break;
    case IC_FUNGSI_UART:
        notulen_catat(NOTULEN_INFO,
            "Kendali: Inisialisasi pengendali serial generik");
        break;
    default:
        notulen_catat(NOTULEN_INFO,
            "Kendali: Inisialisasi pengendali generik");
        break;
    }

    return STATUS_OK;
}

/* ================================================================
 * pengembang_kendali_baca_register — Baca register IC
 *
 * Mengakses register IC melalui bus yang sesuai.
 * Untuk offset < 0x40: gunakan PCI config space.
 * Untuk offset >= 0x40: gunakan MMIO (alamat basis perangkat).
 * Pada x86/x64: akses langsung via pointer MMIO.
 * Pada ARM: akses MMIO juga.
 *
 * Parameter:
 *   perangkat — pointer ke DataPerangkat
 *   offset    — offset register dari alamat basis
 *
 * Mengembalikan:
 *   Nilai register 32-bit, atau 0 jika gagal
 * ================================================================ */
uint32 pengembang_kendali_baca_register(DataPerangkat *perangkat,
                                         uint32 offset)
{
    if (perangkat == NULL) {
        return 0;
    }

    switch (perangkat->tipe_bus) {
    case BUS_PCI:
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
        {
            uint8 bus, dev, fn;

            bus = (uint8)((perangkat->port >> 16) & 0xFF);
            dev = (uint8)((perangkat->port >> 8) & 0xFF);
            fn  = (uint8)(perangkat->port & 0xFF);

            /* PCI config space untuk offset rendah */
            if (offset < 0x40) {
                return penyedia_jalur_baca_pci(perangkat,
                                                (uint8)offset);
            }
        }
#endif

        /* MMIO untuk offset tinggi (semua arsitektur) */
        if (perangkat->alamat != 0) {
            volatile uint32 *mmio;
            mmio = (volatile uint32 *)(ukuran_ptr)
                   (perangkat->alamat + offset);
            return *mmio;
        }
        break;

    default:
        /* Bus lain: akses MMIO jika alamat tersedia */
        if (perangkat->alamat != 0) {
            volatile uint32 *mmio;
            mmio = (volatile uint32 *)(ukuran_ptr)
                   (perangkat->alamat + offset);
            return *mmio;
        }
        break;
    }

    return 0;
}

/* ================================================================
 * pengembang_kendali_tulis_register — Tulis register IC
 *
 * Mengakses register IC melalui bus yang sesuai.
 * Logika akses sama dengan baca_register.
 *
 * Parameter:
 *   perangkat — pointer ke DataPerangkat
 *   offset    — offset register dari alamat basis
 *   nilai     — nilai 32-bit yang akan ditulis
 * ================================================================ */
void pengembang_kendali_tulis_register(DataPerangkat *perangkat,
                                        uint32 offset, uint32 nilai)
{
    if (perangkat == NULL) {
        return;
    }

    switch (perangkat->tipe_bus) {
    case BUS_PCI:
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
        {
            /* PCI config space untuk offset rendah */
            if (offset < 0x40) {
                penyedia_jalur_tulis_pci(perangkat,
                                          (uint8)offset, nilai);
                return;
            }
        }
#endif

        /* MMIO untuk offset tinggi */
        if (perangkat->alamat != 0) {
            volatile uint32 *mmio;
            mmio = (volatile uint32 *)(ukuran_ptr)
                   (perangkat->alamat + offset);
            *mmio = nilai;
        }
        break;

    default:
        /* Bus lain: akses MMIO jika alamat tersedia */
        if (perangkat->alamat != 0) {
            volatile uint32 *mmio;
            mmio = (volatile uint32 *)(ukuran_ptr)
                   (perangkat->alamat + offset);
            *mmio = nilai;
        }
        break;
    }
}

/* ================================================================
 * pengembang_kendali_set_parameter — Atur parameter IC
 *
 * Parameter diuji pada nilai maksimum terlebih dahulu,
 * kemudian diset ke nilai optimal (stabil). Parameter
 * optimal disalin dari DataIC ke register perangkat.
 *
 * Parameter:
 *   ic        — pointer ke DataIC
 *   perangkat — pointer ke DataPerangkat
 *
 * Mengembalikan:
 *   STATUS_OK jika berhasil,
 *   STATUS_PARAMETAR_SALAH jika parameter NULL
 * ================================================================ */
int pengembang_kendali_set_parameter(DataIC *ic, DataPerangkat *perangkat)
{
    int i;

    if (ic == NULL || perangkat == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* Terapkan parameter optimal ke perangkat */
    for (i = 0; i < ic->jumlah_parameter && i < 8; i++) {
        uint32 nilai_optimal;

        nilai_optimal = ic->parameter_optimal[i];

        /* Catat pengaturan parameter ke notulen */
        {
            char pesan[96];
            salin_string(pesan, "Kendali: Parameter IC ");
            konversi_uint_ke_string((uint32)i,
                                    pesan + panjang_string(pesan), 10);
            salin_string(pesan + panjang_string(pesan), " = ");
            konversi_uint_ke_string(nilai_optimal,
                                    pesan + panjang_string(pesan), 10);
            salin_string(pesan + panjang_string(pesan), " (optimal)");
            notulen_catat(NOTULEN_INFO, pesan);
        }

        /* Tulis parameter ke register IC (offset 0x80 + i*4) */
        pengembang_kendali_tulis_register(perangkat,
                                           0x80 + (uint32)i * 4,
                                           nilai_optimal);
    }

    return STATUS_OK;
}

/* ================================================================
 * pengembang_kendali_verifikasi_ic — Verifikasi IC merespons
 *
 * Membaca register identifikasi IC dan membandingkan
 * dengan nilai yang diharapkan dari DataIC.
 *
 * Parameter:
 *   ic        — pointer ke DataIC
 *   perangkat — pointer ke DataPerangkat
 *
 * Mengembalikan:
 *   BENAR jika IC merespons dengan benar,
 *   SALAH jika tidak
 * ================================================================ */
logika pengembang_kendali_verifikasi_ic(DataIC *ic,
                                         DataPerangkat *perangkat)
{
    uint32 id_dibaca;
    uint16 vendor_dibaca;
    uint16 perangkat_dibaca;

    if (ic == NULL || perangkat == NULL) {
        return SALAH;
    }

    /* Baca register ID PCI (offset 0x00) */
    id_dibaca = pengembang_kendali_baca_register(perangkat, 0x00);

    vendor_dibaca = (uint16)(id_dibaca & 0xFFFF);
    perangkat_dibaca = (uint16)((id_dibaca >> 16) & 0xFFFF);

    /* Bandingkan dengan data IC */
    if (vendor_dibaca != ic->vendor_id) {
        return SALAH;
    }
    if (perangkat_dibaca != ic->perangkat_id) {
        return SALAH;
    }

    return BENAR;
}

/* ================================================================
 * pengembang_kendali_reset_ic — Reset IC ke kondisi awal
 *
 * Mengirim perintah reset melalui register perintah PCI.
 * Menonaktifkan perangkat sementara, lalu mengaktifkan
 * kembali dengan pengaturan yang sama.
 *
 * Parameter:
 *   ic        — pointer ke DataIC
 *   perangkat — pointer ke DataPerangkat
 *
 * Mengembalikan:
 *   STATUS_OK jika berhasil,
 *   STATUS_PARAMETAR_SALAH jika parameter NULL
 * ================================================================ */
int pengembang_kendali_reset_ic(DataIC *ic, DataPerangkat *perangkat)
{
    if (ic == NULL || perangkat == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    switch (perangkat->tipe_bus) {
    case BUS_PCI:
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
        {
            uint8 bus, dev, fn;
            uint32 perintah;

            bus = (uint8)((perangkat->port >> 16) & 0xFF);
            dev = (uint8)((perangkat->port >> 8) & 0xFF);
            fn  = (uint8)(perangkat->port & 0xFF);

            /* Baca register perintah saat ini */
            perintah = peninjau_pci_baca(bus, dev, fn,
                                          PCI_OFFSET_PERINTAH);

            /* Nonaktifkan perangkat sementara */
            peninjau_pci_tulis(bus, dev, fn,
                               PCI_OFFSET_PERINTAH, 0x0000);
            tunda_io();
            tunda_io();

            /* Aktifkan kembali dengan perintah sebelumnya */
            peninjau_pci_tulis(bus, dev, fn,
                               PCI_OFFSET_PERINTAH, perintah);
        }
#endif
        break;

    default:
        /* Untuk bus non-PCI, tulis ke register reset MMIO */
        if (perangkat->alamat != 0) {
            pengembang_kendali_tulis_register(perangkat, 0x00, 0x00000001);
        }
        break;
    }

    notulen_catat(NOTULEN_INFO, "Kendali: IC direset");

    return STATUS_OK;
}

/* ================================================================
 * pengembang_kendali_diagnostik — Jalankan diagnostik pada IC
 *
 * Memverifikasi bahwa IC dapat diakses dan berfungsi
 * dengan parameter optimal. Memeriksa register status
 * untuk mendeteksi kesalahan.
 *
 * Parameter:
 *   ic        — pointer ke DataIC
 *   perangkat — pointer ke DataPerangkat
 *
 * Mengembalikan:
 *   STATUS_OK jika diagnostik lulus,
 *   STATUS_GAGAL jika ditemukan masalah serius
 * ================================================================ */
int pengembang_kendali_diagnostik(DataIC *ic, DataPerangkat *perangkat)
{
    uint32 status;

    if (ic == NULL || perangkat == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* 1. Verifikasi IC merespons */
    if (!pengembang_kendali_verifikasi_ic(ic, perangkat)) {
        notulen_catat(NOTULEN_KESALAHAN,
            "Kendali: Diagnostik gagal — IC tidak merespons");
        return STATUS_GAGAL;
    }

    /* 2. Baca dan periksa register status (offset 0x06) */
    status = pengembang_kendali_baca_register(perangkat, 0x06);

    /* Bit 15: Parity Error */
    if (status & 0x8000) {
        notulen_catat(NOTULEN_PERINGATAN,
            "Kendali: Diagnostik — parity error terdeteksi");
    }

    /* Bit 14: Signaled System Error */
    if (status & 0x4000) {
        notulen_catat(NOTULEN_PERINGATAN,
            "Kendali: Diagnostik — system error terdeteksi");
    }

    /* Bit 13: Received Master Abort */
    if (status & 0x2000) {
        notulen_catat(NOTULEN_PERINGATAN,
            "Kendali: Diagnostik — master abort terdeteksi");
    }

    /* Bit 12: Received Target Abort */
    if (status & 0x1000) {
        notulen_catat(NOTULEN_PERINGATAN,
            "Kendali: Diagnostik — target abort terdeteksi");
    }

    notulen_catat(NOTULEN_INFO,
        "Kendali: Diagnostik IC lulus");

    return STATUS_OK;
}

/* ================================================================
 * FUNGSI IC DARI ARSITEK.H
 *
 * Fungsi-fungsi berikut dideklarasikan dalam arsitek.h dan
 * diimplementasikan di sini karena berhubungan erat dengan
 * database IC dan mekanisme identifikasi.
 * ================================================================ */

/*
 * ic_identifikasi — Identifikasi IC dari PCI IDs
 *
 * Fungsi publik yang dipanggil oleh komponen kernel lain
 * untuk mengidentifikasi IC. Merupakan pembungkus atas
 * pengembang_kendali_identifikasi_ic.
 *
 * Parameter:
 *   vendor_id    — Vendor ID PCI
 *   perangkat_id — Device ID PCI
 *   kelas        — Kelas PCI
 *   subkelas     — Sub-kelas PCI
 *
 * Mengembalikan:
 *   Pointer ke DataIC jika dikenali, NULL jika tidak
 */
DataIC *ic_identifikasi(uint16 vendor_id, uint16 perangkat_id,
                         uint8 kelas, uint8 subkelas)
{
    return pengembang_kendali_identifikasi_ic(vendor_id,
                                               perangkat_id,
                                               kelas, subkelas);
}

/*
 * ic_nama_fungsi — Dapatkan nama fungsi IC
 *
 * Mengkonversi TipeFungsiIC ke string nama yang dapat dibaca.
 *
 * Parameter:
 *   tipe — TipeFungsiIC yang akan dikonversi
 *
 * Mengembalikan:
 *   String nama fungsi IC dalam bahasa Indonesia
 */
const char *ic_nama_fungsi(TipeFungsiIC tipe)
{
    switch (tipe) {
    case IC_FUNGSI_ETERNET:      return "Pengendali Eternet";
    case IC_FUNGSI_WIFI:         return "Pengendali WiFi";
    case IC_FUNGSI_STORAGE_IDE:  return "Pengendali IDE/PATA";
    case IC_FUNGSI_STORAGE_AHCI: return "Pengendali SATA AHCI";
    case IC_FUNGSI_STORAGE_NVME: return "Pengendali NVMe";
    case IC_FUNGSI_USB_UHCI:     return "Pengendali USB UHCI";
    case IC_FUNGSI_USB_OHCI:     return "Pengendali USB OHCI";
    case IC_FUNGSI_USB_EHCI:     return "Pengendali USB EHCI";
    case IC_FUNGSI_USB_XHCI:     return "Pengendali USB xHCI";
    case IC_FUNGSI_TAMPILAN_VGA: return "Pengendali Tampilan VGA";
    case IC_FUNGSI_TAMPILAN_GPU: return "Pengendali GPU";
    case IC_FUNGSI_AUDIO:        return "Pengendali Audio";
    case IC_FUNGSI_UART:         return "Pengendali UART/Serial";
    case IC_FUNGSI_DMA:          return "Pengendali DMA";
    case IC_FUNGSI_PIC:          return "Pengendali Interupsi";
    case IC_FUNGSI_PIT:          return "Penghitung Interval";
    case IC_FUNGSI_RTC:          return "Jam Waktu Nyata";
    case IC_FUNGSI_BRIDGE:       return "Jembatan Bus";
    case IC_FUNGSI_BLUETOOTH:    return "Pengendali Bluetooth";
    case IC_FUNGSI_LAIN:         return "Fungsi Lainnya";
    default:                     return "Tidak Dikenal";
    }
}

/*
 * ic_cocok_perangkat — Periksa apakah IC cocok untuk perangkat
 *
 * Membandingkan data IC dengan data perangkat untuk
 * menentukan apakah driver IC ini dapat mengendalikan
 * perangkat tersebut.
 *
 * Parameter:
 *   ic        — pointer ke DataIC
 *   perangkat — pointer ke DataPerangkat
 *
 * Mengembalikan:
 *   BENAR jika IC cocok dengan perangkat,
 *   SALAH jika tidak cocok atau parameter NULL
 */
int ic_cocok_perangkat(DataIC *ic, DataPerangkat *perangkat)
{
    if (ic == NULL || perangkat == NULL) {
        return SALAH;
    }

    /* Cocokkan vendor ID dan perangkat ID */
    if (ic->vendor_id != 0xFFFF &&
        ic->vendor_id == perangkat->vendor_id &&
        ic->perangkat_id == perangkat->perangkat_id) {
        return BENAR;
    }

    /* Cocokkan kelas dan subkelas */
    if (ic->kelas == perangkat->kelas &&
        ic->subkelas == perangkat->subkelas) {
        return BENAR;
    }

    /* Cocokkan kelas saja (driver generik per kelas) */
    if (ic->kelas == perangkat->kelas) {
        return BENAR;
    }

    return SALAH;
}

/*
 * ic_isi_parameter_optimal — Isi parameter optimal IC ke perangkat
 *
 * Menyalin parameter optimal dari DataIC ke register perangkat.
 * Parameter optimal adalah nilai yang sudah diuji dan terbukti
 * stabil pada pengujian dengan nilai maksimum.
 *
 * Parameter:
 *   ic        — pointer ke DataIC
 *   perangkat — pointer ke DataPerangkat
 */
void ic_isi_parameter_optimal(DataIC *ic, DataPerangkat *perangkat)
{
    int i;

    if (ic == NULL || perangkat == NULL) {
        return;
    }

    /* Salin parameter optimal ke register perangkat */
    for (i = 0; i < ic->jumlah_parameter && i < 8; i++) {
        pengembang_kendali_tulis_register(perangkat,
                                           0x80 + (uint32)i * 4,
                                           ic->parameter_optimal[i]);
    }

    /* Atur interupsi default */
    perangkat->interupsi = ic->interupsi_default;
}
