/*
 * pengembang.h
 *
 * Pengembang bertugas membangun koneksi, kendali (driver),
 * dan antarmuka untuk setiap perangkat.
 *
 * Setelah Penyedia selesai mengumpulkan sumber daya,
 * Pengembang menggunakan sumber daya itu untuk:
 *   - Membangun koneksi ke perangkat
 *   - Membangun kendali (driver) agar perangkat bisa dipakai
 *   - Membangun antarmuka agar lapisan atas bisa berbicara
 *
 * Deteksi perangkat keras berbasis IC/chip, bukan vendor/merk.
 * Satu IC chip digunakan oleh banyak kartu dari pabrikan berbeda,
 * sehingga pendeteksian berbasis IC menghasilkan driver generik
 * yang lebih akurat dan dapat dipakai ulang.
 */

#ifndef PENGEMBANG_H
#define PENGEMBANG_H

#include "arsitek.h"

/* ================================================================
 * STRUKTUR ANTARMUKA PERANGKAT
 *
 * Menyimpan state antarmuka untuk satu perangkat yang sudah
 * diidentifikasi IC-nya dan siap digunakan oleh lapisan atas.
 * ================================================================ */

#define ANTARMUKA_MAKSIMUM  32

typedef struct {
    DataPerangkat   *perangkat;       /* Pointer ke data perangkat */
    DataIC          *ic;              /* Pointer ke data IC teridentifikasi */
    uint32           terkunci;        /* Status kunci (0=bebas, 1=terkunci) */
    uint32           jumlah_buka;     /* Jumlah kali perangkat dibuka */
    KondisiPerangkat kondisi;         /* Kondisi antarmuka saat ini */
    uint32           alamat_basis;    /* Alamat basis register IC */
    TipeBus          tipe_bus;        /* Jenis bus koneksi */
} AntarmukaPerangkat;

/* ================================================================
 * PEMBANGUNAN UMUM
 * ================================================================ */

/* Bangun semua koneksi dan kendali untuk semua perangkat */
void pengembang_bangun_semua(DataPerangkat daftar[], int jumlah);

/* Bangun koneksi untuk satu perangkat */
int  pengembang_bangun_koneksi(DataPerangkat *perangkat);

/* ================================================================
 * KENDALI (DRIVER)
 * ================================================================ */

/* Bangun kendali (driver) untuk perangkat */
int  pengembang_bangun_kendali(DataPerangkat *perangkat);

/* Pasang kendali ke perangkat (dipanggil oleh API) */
int  pengembang_pasang_kendali(unsigned int nomor);

/* Copot kendali dari perangkat */
void pengembang_copot_kendali(unsigned int nomor);

/* Bersihkan kendali ketika perangkat dicabut */
void pengembang_bersihkan_kendali(unsigned int nomor_port);

/* ================================================================
 * ANTARMUKA
 * ================================================================ */

/* Bangun antarmuka untuk lapisan atas */
int  pengembang_bangun_antarmuka(DataPerangkat *perangkat);

/* ================================================================
 * KONEKSI — Pembangunan koneksi ke perangkat berdasarkan IC/chip
 * ================================================================ */

/* Koneksi ke bus PCI — aktifkan bus master, memori, IO, latency timer */
int pengembang_koneksi_pci(DataPerangkat *perangkat);

/* Koneksi ke pengendali USB UHCI */
int pengembang_koneksi_usb_uhci(DataPerangkat *perangkat);

/* Koneksi ke pengendali USB OHCI */
int pengembang_koneksi_usb_ohci(DataPerangkat *perangkat);

/* Koneksi ke pengendali USB EHCI */
int pengembang_koneksi_usb_ehci(DataPerangkat *perangkat);

/* Koneksi ke pengendali USB xHCI */
int pengembang_koneksi_usb_xhci(DataPerangkat *perangkat);

/* Koneksi ke pengendali AHCI */
int pengembang_koneksi_ahci(DataPerangkat *perangkat);

/* Koneksi ke pengendali NVMe */
int pengembang_koneksi_nvme(DataPerangkat *perangkat);

/* Koneksi ke bus I2C */
int pengembang_koneksi_i2c(DataPerangkat *perangkat);

/* Koneksi ke bus SPI */
int pengembang_koneksi_spi(DataPerangkat *perangkat);

/* Koneksi ke UART/serial */
int pengembang_koneksi_uart(DataPerangkat *perangkat);

/* Koneksi generik untuk perangkat yang tidak dikenali */
int pengembang_koneksi_generik(DataPerangkat *perangkat);

/* ================================================================
 * KENDALI IC — Pembangunan driver generik berdasarkan IC chip
 * ================================================================ */

/* Identifikasi IC dari vendor_id, perangkat_id, kelas, dan subkelas PCI */
DataIC *pengembang_kendali_identifikasi_ic(uint16 vendor_id, uint16 perangkat_id,
                                            uint8 kelas, uint8 subkelas);

/* Inisialisasi driver untuk IC spesifik */
int pengembang_kendali_inisialisasi_ic(DataIC *ic, DataPerangkat *perangkat);

/* Baca register IC */
uint32 pengembang_kendali_baca_register(DataPerangkat *perangkat, uint32 offset);

/* Tulis register IC */
void pengembang_kendali_tulis_register(DataPerangkat *perangkat, uint32 offset,
                                        uint32 nilai);

/* Set parameter IC (uji dengan maks, lalu set optimal) */
int pengembang_kendali_set_parameter(DataIC *ic, DataPerangkat *perangkat);

/* Verifikasi IC merespons dengan benar */
logika pengembang_kendali_verifikasi_ic(DataIC *ic, DataPerangkat *perangkat);

/* Reset IC ke kondisi awal */
int pengembang_kendali_reset_ic(DataIC *ic, DataPerangkat *perangkat);

/* Jalankan diagnostik pada IC */
int pengembang_kendali_diagnostik(DataIC *ic, DataPerangkat *perangkat);

/* ================================================================
 * ANTARMUKA — Pembangunan antarmuka untuk lapisan atas (OS/Kontraktor)
 * ================================================================ */

/* Buat struktur antarmuka perangkat */
int pengembang_antarmuka_buat(DataPerangkat *perangkat, DataIC *ic,
                               AntarmukaPerangkat *antarmuka);

/* Baca dari perangkat melalui antarmuka */
int pengembang_antarmuka_baca(AntarmukaPerangkat *antarmuka, uint32 offset,
                               uint8 *buffer, ukuran_t panjang);

/* Tulis ke perangkat melalui antarmuka */
int pengembang_antarmuka_tulis(AntarmukaPerangkat *antarmuka, uint32 offset,
                                const uint8 *buffer, ukuran_t panjang);

/* Operasi kendali khusus perangkat (ioctl) */
int pengembang_antarmuka_ioctl(AntarmukaPerangkat *antarmuka, uint32 perintah,
                                uint32 argumen);

/* Dapatkan status perangkat melalui antarmuka */
KondisiPerangkat pengembang_antarmuka_status(AntarmukaPerangkat *antarmuka);

/* Buka perangkat untuk digunakan */
int pengembang_antarmuka_buka(AntarmukaPerangkat *antarmuka);

/* Tutup perangkat */
int pengembang_antarmuka_tutup(AntarmukaPerangkat *antarmuka);

/* Daftar semua antarmuka perangkat yang tersedia */
int pengembang_antarmuka_daftar(AntarmukaPerangkat daftar[], int maks);

#endif
