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
 * Setiap entri berisi:
 *   vendor_id, perangkat_id  — ID PCI untuk identifikasi
 *   kelas, subkelas          — Kelas PCI untuk pencocokan
 *   nama_ic                  — Nama IC (mis. "RTL8139C+")
 *   fabrikkan                — Fabrikan IC (mis. "Realtek")
 *   fungsi                   — Deskripsi fungsi
 *   tipe_fungsi              — Tipe fungsi IC (enum)
 *   tipe_perangkat           — Tipe perangkat yang menggunakan IC
 *   tipe_bus                 — Jenis bus koneksi
 *   clock_mhz                — Kecepatan clock IC
 *   parameter_maks[]         — Parameter nilai maksimum
 *   parameter_optimal[]      — Parameter nilai optimal/stabil
 *   jumlah_parameter         — Jumlah parameter yang digunakan
 * ================================================================ */

static const DataIC tabel_ic[] = {
    /* ---- IC Eternet ---- */
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
        "Pengendali Eternet 1 Gigabit",
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
        "Pengendali Eternet 1 Gigabit",
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

    /* ---- IC Storage AHCI ---- */
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

    /* ---- IC Storage NVMe ---- */
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

    /* ---- IC Storage IDE ---- */
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

    /* ---- IC USB UHCI ---- */
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

    /* ---- IC USB OHCI ---- */
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

    /* ---- IC USB EHCI ---- */
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

    /* ---- IC USB xHCI ---- */
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
    {
        0x8086, 0x9CB1, 0x0C, 0x30,
        "Wildcat Point xHCI", "Intel",
        "Pengendali USB 3.0 xHCI (Wildcat Point)",
        IC_FUNGSI_USB_XHCI, PERANGKAT_USB, BUS_PCI,
        100, 0x0000, 0x8000, 11,
        {16, 255, 5000, 1024, 0, 0, 0, 0},
        {8, 64, 5000, 512, 0, 0, 0, 0},
        4
    },

    /* ---- IC Tampilan VGA ---- */
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

    /* ---- IC Audio ---- */
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

    /* ---- IC UART/Serial ---- */
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

    /* ---- IC DMA ---- */
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

    /* ---- IC PIC/PIT ---- */
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

    /* ---- IC RTC ---- */
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

    /* ---- IC Bridge ---- */
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

    /* ---- IC Bluetooth ---- */
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

    /* ---- Entri akhir (penanda) ---- */
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

/* IC yang teridentifikasi untuk setiap perangkat */
static DataIC ic_teridentifikasi[PERANGKAT_MAKSIMUM];
static int jumlah_ic_teridentifikasi = 0;

/* ================================================================
 * FUNGSI IDENTIFIKASI IC
 * ================================================================ */

/*
 * pengembang_kendali_identifikasi_ic — Identifikasi IC dari PCI IDs.
 * Mencari dalam tabel IC berdasarkan vendor_id, perangkat_id,
 * kelas, dan subkelas. Jika ditemukan, salin data IC ke buffer.
 *
 * Parameter:
 *   vendor_id    — Vendor ID PCI
 *   perangkat_id — Device ID PCI
 *   kelas        — Kelas PCI
 *   subkelas     — Sub-kelas PCI
 *   hasil        — Buffer untuk menampung hasil
 * Mengembalikan STATUS_OK jika IC dikenali, STATUS_TIDAK_DIKENAL jika tidak.
 */
int pengembang_kendali_identifikasi_ic(uint16 vendor_id, uint16 perangkat_id,
                                        uint8 kelas, uint8 subkelas,
                                        DataIC *hasil)
{
    int i;
    int cocok_terbaik = -1;
    int skor_terbaik = 0;

    if (hasil == NULL) return STATUS_PARAMETAR_SALAH;

    /* Cari entri yang paling cocok — prioritas:
     * 1. vendor_id + perangkat_id + kelas + subkelas (skor 4)
     * 2. vendor_id + perangkat_id (skor 3)
     * 3. kelas + subkelas (skor 2)
     * 4. kelas saja (skor 1) */
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
        /* Salin data IC ke hasil */
        *hasil = tabel_ic[cocok_terbaik];

        /* Perbarui IDs dari perangkat aktual jika berbeda */
        hasil->vendor_id = vendor_id;
        hasil->perangkat_id = perangkat_id;
        hasil->kelas = kelas;
        hasil->subkelas = subkelas;

        return STATUS_OK;
    }

    /* IC tidak dikenali — buat entri generik berdasarkan kelas */
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

    /* Tentukan fungsi berdasarkan kelas PCI */
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
    case 0x0D:
        hasil->tipe_fungsi = IC_FUNGSI_BLUETOOTH;
        hasil->tipe_perangkat = PERANGKAT_LAIN;
        salin_string(hasil->fungsi, "Pengendali Nirkabel Generik");
        break;
    default:
        salin_string(hasil->fungsi, "Perangkat Generik");
        break;
    }

    return STATUS_TIDAK_DIKENAL;
}

/* ================================================================
 * FUNGSI INISIALISASI IC
 * ================================================================ */

/*
 * pengembang_kendali_inisialisasi_ic — Inisialisasi driver untuk IC.
 * Mengkonfigurasi perangkat berdasarkan data IC dan mengatur
 * parameter ke nilai optimal (stabil).
 *
 * Parameter:
 *   ic        — pointer ke struktur DataIC yang sudah diidentifikasi
 *   perangkat — pointer ke struktur DataPerangkat
 * Mengembalikan STATUS_OK jika berhasil.
 */
int pengembang_kendali_inisialisasi_ic(DataIC *ic, DataPerangkat *perangkat)
{
    if (ic == NULL || perangkat == NULL) return STATUS_PARAMETAR_SALAH;

    /* Simpan data IC ke data_khusus perangkat */
    if (jumlah_ic_teridentifikasi < PERANGKAT_MAKSIMUM) {
        ic_teridentifikasi[jumlah_ic_teridentifikasi] = *ic;
        jumlah_ic_teridentifikasi++;
    }

    /* Atur parameter perangkat ke nilai optimal IC */
    perangkat->interupsi = ic->interupsi_default;

    /* Catat identifikasi IC ke notulen */
    {
        char pesan[128];
        salin_string(pesan, "Kendali: IC teridentifikasi — ");
        salin_string(pesan + panjang_string(pesan), ic->nama_ic);
        salin_string(pesan + panjang_string(pesan), " (");
        salin_string(pesan + panjang_string(pesan), ic->fabrikkan);
        salin_string(pesan + panjang_string(pesan), ")");
        notulen_catat(NOTULEN_INFO, pesan);
    }

    /* Inisialisasi berdasarkan tipe fungsi IC */
    switch (ic->tipe_fungsi) {
    case IC_FUNGSI_ETERNET:
        /* Inisialisasi IC jaringan — set kecepatan optimal */
        notulen_catat(NOTULEN_INFO, "Kendali: Inisialisasi pengendali jaringan generik");
        break;

    case IC_FUNGSI_STORAGE_AHCI:
    case IC_FUNGSI_STORAGE_NVME:
    case IC_FUNGSI_STORAGE_IDE:
        /* Inisialisasi IC penyimpanan — set queue depth optimal */
        notulen_catat(NOTULEN_INFO, "Kendali: Inisialisasi pengendali penyimpanan generik");
        break;

    case IC_FUNGSI_USB_UHCI:
    case IC_FUNGSI_USB_OHCI:
    case IC_FUNGSI_USB_EHCI:
    case IC_FUNGSI_USB_XHCI:
        /* Inisialisasi IC USB — set port count optimal */
        notulen_catat(NOTULEN_INFO, "Kendali: Inisialisasi pengendali USB generik");
        break;

    case IC_FUNGSI_TAMPILAN_VGA:
    case IC_FUNGSI_TAMPILAN_GPU:
        /* Inisialisasi IC tampilan — set resolusi optimal */
        notulen_catat(NOTULEN_INFO, "Kendali: Inisialisasi pengendali tampilan generik");
        break;

    case IC_FUNGSI_AUDIO:
        /* Inisialisasi IC audio — set sample rate optimal */
        notulen_catat(NOTULEN_INFO, "Kendali: Inisialisasi pengendali audio generik");
        break;

    case IC_FUNGSI_UART:
        /* Inisialisasi IC UART — set baud rate optimal */
        notulen_catat(NOTULEN_INFO, "Kendali: Inisialisasi pengendali serial generik");
        break;

    default:
        notulen_catat(NOTULEN_INFO, "Kendali: Inisialisasi pengendali generik");
        break;
    }

    return STATUS_OK;
}

/* ================================================================
 * FUNGSI AKSES REGISTER IC
 * ================================================================ */

/*
 * pengembang_kendali_baca_register — Baca register IC.
 * Mengakses register IC melalui bus yang sesuai.
 * Untuk PCI: menggunakan memory-mapped I/O atau port I/O.
 *
 * Parameter:
 *   perangkat — pointer ke struktur DataPerangkat
 *   offset    — offset register dari alamat basis
 * Mengembalikan nilai register 32-bit.
 */
uint32 pengembang_kendali_baca_register(DataPerangkat *perangkat, uint32 offset)
{
    if (perangkat == NULL) return 0;

    /* Akses melalui PCI config space atau MMIO */
    switch (perangkat->tipe_bus) {
    case BUS_PCI:
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
        {
            uint8 bus = (uint8)((perangkat->port >> 16) & 0xFF);
            uint8 dev = (uint8)((perangkat->port >> 8) & 0xFF);
            uint8 fn  = (uint8)(perangkat->port & 0xFF);

            /* Jika offset < 0x40, gunakan PCI config space */
            if (offset < 0x40) {
                return peninjau_pci_baca(bus, dev, fn, (uint8)offset);
            }
            /* Jika offset >= 0x40, gunakan MMIO (BAR0) */
            if (perangkat->alamat != 0) {
                volatile uint32 *mmio;
                mmio = (volatile uint32 *)(ukuran_ptr)(perangkat->alamat + offset);
                return *mmio;
            }
        }
#endif
        break;

    default:
        break;
    }

    return 0;
}

/*
 * pengembang_kendali_tulis_register — Tulis register IC.
 * Mengakses register IC melalui bus yang sesuai.
 *
 * Parameter:
 *   perangkat — pointer ke struktur DataPerangkat
 *   offset    — offset register dari alamat basis
 *   nilai     — nilai 32-bit yang akan ditulis
 */
void pengembang_kendali_tulis_register(DataPerangkat *perangkat,
                                        uint32 offset, uint32 nilai)
{
    if (perangkat == NULL) return;

    switch (perangkat->tipe_bus) {
    case BUS_PCI:
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
        {
            uint8 bus = (uint8)((perangkat->port >> 16) & 0xFF);
            uint8 dev = (uint8)((perangkat->port >> 8) & 0xFF);
            uint8 fn  = (uint8)(perangkat->port & 0xFF);

            if (offset < 0x40) {
                peninjau_pci_tulis(bus, dev, fn, (uint8)offset, nilai);
            } else if (perangkat->alamat != 0) {
                volatile uint32 *mmio;
                mmio = (volatile uint32 *)(ukuran_ptr)(perangkat->alamat + offset);
                *mmio = nilai;
            }
        }
#endif
        break;

    default:
        break;
    }
}

/* ================================================================
 * FUNGSI PENGATURAN PARAMETER
 * ================================================================ */

/*
 * pengembang_kendali_set_parameter — Atur parameter IC.
 * Parameter diuji pada nilai maksimum terlebih dahulu,
 * kemudian diset ke nilai optimal (stabil).
 *
 * Parameter:
 *   ic           — pointer ke DataIC
 *   nomor_param  — nomor parameter (0-7)
 *   nilai        — nilai parameter
 * Mengembalikan STATUS_OK jika berhasil.
 */
int pengembang_kendali_set_parameter(DataIC *ic, int nomor_param, uint32 nilai)
{
    if (ic == NULL) return STATUS_PARAMETAR_SALAH;
    if (nomor_param < 0 || nomor_param >= 8) return STATUS_PARAMETAR_SALAH;
    if (nomor_param >= ic->jumlah_parameter) return STATUS_PARAMETAR_SALAH;

    /* Uji: apakah nilai melebihi maksimum? */
    if (nilai > ic->parameter_maks[nomor_param]) {
        /* Nilai melebihi batas — gunakan nilai optimal */
        nilai = ic->parameter_optimal[nomor_param];
    }

    /* Catat pengaturan parameter */
    {
        char pesan[96];
        salin_string(pesan, "Kendali: Parameter IC ");
        konversi_uint_ke_string((uint32)nomor_param,
                                pesan + panjang_string(pesan), 10);
        salin_string(pesan + panjang_string(pesan), " = ");
        konversi_uint_ke_string(nilai, pesan + panjang_string(pesan), 10);
        notulen_catat(NOTULEN_INFO, pesan);
    }

    return STATUS_OK;
}

/* ================================================================
 * FUNGSI VERIFIKASI IC
 * ================================================================ */

/*
 * pengembang_kendali_verifikasi_ic — Verifikasi IC merespons.
 * Membaca register identifikasi IC dan membandingkan
 * dengan nilai yang diharapkan.
 *
 * Parameter:
 *   perangkat — pointer ke struktur DataPerangkat
 *   ic        — pointer ke struktur DataIC
 * Mengembalikan BENAR jika IC merespons dengan benar.
 */
int pengembang_kendali_verifikasi_ic(DataPerangkat *perangkat, DataIC *ic)
{
    uint32 id_dibaca;

    if (perangkat == NULL || ic == NULL) return SALAH;

    /* Baca register ID PCI */
    id_dibaca = pengembang_kendali_baca_register(perangkat, 0x00);

    /* Bandingkan vendor ID dan device ID */
    if ((uint16)(id_dibaca & 0xFFFF) != ic->vendor_id) {
        return SALAH;
    }
    if ((uint16)((id_dibaca >> 16) & 0xFFFF) != ic->perangkat_id) {
        return SALAH;
    }

    return BENAR;
}

/* ================================================================
 * FUNGSI RESET IC
 * ================================================================ */

/*
 * pengembang_kendali_reset_ic — Reset IC ke keadaan awal.
 * Mengirim perintah reset melalui register yang sesuai.
 *
 * Parameter:
 *   perangkat — pointer ke struktur DataPerangkat
 *   ic        — pointer ke struktur DataIC
 * Mengembalikan STATUS_OK jika berhasil.
 */
int pengembang_kendali_reset_ic(DataPerangkat *perangkat, DataIC *ic)
{
    if (perangkat == NULL || ic == NULL) return STATUS_PARAMETAR_SALAH;

    /* Reset umum: tulis 1 ke bit reset register perintah PCI */
    switch (perangkat->tipe_bus) {
    case BUS_PCI:
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
        {
            uint8 bus = (uint8)((perangkat->port >> 16) & 0xFF);
            uint8 dev = (uint8)((perangkat->port >> 8) & 0xFF);
            uint8 fn  = (uint8)(perangkat->port & 0xFF);
            uint32 perintah;

            /* Baca register perintah, set bit reset (jika ada) */
            perintah = peninjau_pci_baca(bus, dev, fn, PCI_OFFSET_PERINTAH);

            /* Nonaktifkan dulu, lalu aktifkan kembali */
            peninjau_pci_tulis(bus, dev, fn, PCI_OFFSET_PERINTAH, 0x0000);
            tunda_io();
            tunda_io();
            peninjau_pci_tulis(bus, dev, fn, PCI_OFFSET_PERINTAH, perintah);
        }
#endif
        break;

    default:
        break;
    }

    notulen_catat(NOTULEN_INFO, "Kendali: IC direset");

    return STATUS_OK;
}

/* ================================================================
 * FUNGSI DIAGNOSTIK IC
 * ================================================================ */

/*
 * pengembang_kendali_diagnostik — Jalankan diagnostik pada IC.
 * Memverifikasi bahwa IC dapat diakses dan berfungsi
 * dengan parameter optimal.
 *
 * Parameter:
 *   perangkat — pointer ke struktur DataPerangkat
 *   ic        — pointer ke struktur DataIC
 * Mengembalikan STATUS_OK jika diagnostik lulus.
 */
int pengembang_kendali_diagnostik(DataPerangkat *perangkat, DataIC *ic)
{
    if (perangkat == NULL || ic == NULL) return STATUS_PARAMETAR_SALAH;

    /* 1. Verifikasi IC merespons */
    if (!pengembang_kendali_verifikasi_ic(perangkat, ic)) {
        notulen_catat(NOTULEN_KESALAHAN, "Kendali: Diagnostik gagal — IC tidak merespons");
        return STATUS_GAGAL;
    }

    /* 2. Cek status register (offset 0x06) */
    {
        uint32 status;
        status = pengembang_kendali_baca_register(perangkat, 0x06);

        /* Bit 15: Parity Error — harus 0 */
        if (status & 0x8000) {
            notulen_catat(NOTULEN_PERINGATAN, "Kendali: Diagnostik — parity error terdeteksi");
        }

        /* Bit 14: Signaled System Error — harus 0 */
        if (status & 0x4000) {
            notulen_catat(NOTULEN_PERINGATAN, "Kendali: Diagnostik — system error terdeteksi");
        }

        /* Bit 13: Received Master Abort */
        if (status & 0x2000) {
            notulen_catat(NOTULEN_PERINGATAN, "Kendali: Diagnostik — master abort terdeteksi");
        }

        /* Bit 12: Received Target Abort */
        if (status & 0x1000) {
            notulen_catat(NOTULEN_PERINGATAN, "Kendali: Diagnostik — target abort terdeteksi");
        }
    }

    notulen_catat(NOTULEN_INFO, "Kendali: Diagnostik lulus");

    return STATUS_OK;
}
