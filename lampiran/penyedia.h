/*
 * penyedia.h
 *
 * Penyedia bertugas mengumpulkan sumber daya yang dibutuhkan
 * untuk setiap perangkat yang sudah diperiksa oleh Peninjau.
 *
 * Sumber daya yang disediakan:
 *   - Tipe data sesuai arsitektur (32-bit atau 64-bit)
 *   - Alamat memori dan I/O
 *   - Jalur interupsi
 *   - Jalur bus (PCI, USB, I2C, SPI)
 *   - Ruang memori untuk kendali/driver
 */

#ifndef PENYEDIA_H
#define PENYEDIA_H

#include "arsitek.h"

/* === Penyediaan Umum === */

/* Kumpulkan semua sumber daya untuk semua perangkat */
void penyedia_kumpulkan_semua(DataPerangkat daftar[], int jumlah);

/* Kumpulkan sumber daya untuk satu perangkat */
void penyedia_kumpulkan(DataPerangkat *perangkat, int jumlah);

/* === Penyediaan Spesifik === */

/* Siapkan tipe data sesuai arsitektur mesin (32-bit atau 64-bit) */
void penyedia_sediakan_tipedata(DataPerangkat *perangkat);

/* Siapkan jalur bus untuk perangkat */
int  penyedia_sediakan_bus(DataPerangkat *perangkat, const char *jenis_bus);

/* Siapkan alamat memori fisik untuk perangkat */
int  penyedia_sediakan_alamat(DataPerangkat *perangkat, unsigned long long ukuran);

/* Siapkan jalur interupsi untuk perangkat */
int  penyedia_sediakan_interupsi(DataPerangkat *perangkat, unsigned int nomor_interupsi);

/* Alokasi ruang memori untuk kendali perangkat */
void *penyedia_alokasi_memori(unsigned long long ukuran);

/* Bebaskan ruang memori yang sudah tidak dipakai */
void penyedia_bebaskan_memori(void *alamat, unsigned long long ukuran);

/* === Penyediaan Alamat (alamat.c) === */

/* Cari region alamat bebas dalam bitmap */
ukuran_ptr penyedia_alamat_cari_bebas(ukuran_t jumlah_halaman,
                                       ukuran_ptr alamat_mulai);

/* Alokasi region alamat berurutan dari bitmap */
ukuran_ptr penyedia_alamat_alokasi_region(ukuran_t ukuran,
                                           uint32 nomor_perangkat,
                                           uint8 tipe_region);

/* Bebaskan region alamat yang sebelumnya dialokasikan */
int penyedia_alamat_bebaskan_region(ukuran_ptr alamat);

/* Petakan ruang alamat perangkat MMIO */
ukuran_ptr penyedia_alamat_peta_perangkat(ukuran_ptr alamat_fisik,
                                           ukuran_t ukuran,
                                           uint32 nomor_perangkat,
                                           const char *nama);

/* Daftar region MMIO yang terpetakan */
int penyedia_alamat_daftar_mmio(DataPerangkat daftar[], int maks);

/* === Penyediaan Jalur Bus === */

/* Konfigurasi bus PCI untuk perangkat */
int   penyedia_jalur_konfigurasi_pci(DataPerangkat *perangkat, logika aktifkan_io, logika aktifkan_mem, logika aktifkan_master);

/* Konfigurasi bus USB untuk perangkat */
int   penyedia_jalur_konfigurasi_usb(DataPerangkat *perangkat, uint8 tipe_pengendali, int nomor_bus);

/* Konfigurasi bus I2C */
int   penyedia_jalur_konfigurasi_i2c(int nomor_bus, uint32 kecepatan_khz, uint8 alamat_7bit);

/* Konfigurasi bus SPI */
int   penyedia_jalur_konfigurasi_spi(int nomor_bus, uint32 kecepatan_khz, uint8 mode, uint8 bit_per_kata, logika cs_aktif_rendah);

/* Baca register konfigurasi PCI */
uint32 penyedia_jalur_baca_pci(DataPerangkat *perangkat, uint8 offset);

/* Tulis register konfigurasi PCI */
int   penyedia_jalur_tulis_pci(DataPerangkat *perangkat, uint8 offset, uint32 nilai);

/* Status bus perangkat */
int   penyedia_jalur_status_bus(DataPerangkat *perangkat, uint32 *status);

/* === Penyediaan Jalur Interupsi (Sela) === */

/* Cari nomor interupsi bebas */
int penyedia_sela_cari_bebas(void);

/* Alokasi jalur interupsi untuk perangkat */
int penyedia_sela_alokasi(DataPerangkat *perangkat, unsigned int nomor_interupsi);

/* Bebaskan jalur interupsi */
int penyedia_sela_bebaskan(uint8 nomor_irq);

/* Konfigurasi jalur interupsi */
int penyedia_sela_konfigurasi(DataPerangkat *perangkat, uint8 nomor_irq, uint8 tipe_pemicu);

/* Mask (sembunyikan) jalur interupsi */
int penyedia_sela_mask(uint8 nomor_irq);

/* Unmask (tampilkan) jalur interupsi */
int penyedia_sela_unmask(uint8 nomor_irq);

/* === Penyediaan Tipe Data === */

/* Tampilkan informasi tipe data perangkat */
void   penyedia_tipedata_informasi(DataPerangkat *perangkat);

/* Uji alignment perangkat */
int    penyedia_tipedata_uji_alignment(DataPerangkat *perangkat);

/* Konversi nilai dengan lebar bit tertentu */
int    penyedia_tipedata_konversi_lebar(uint64 nilai_sumber, uint8 lebar_sumber, logika bertanda, void *hasil);

/* Validasi ukuran perangkat */
logika penyedia_tipedata_validasi_ukuran(DataPerangkat *perangkat);

#endif /* PENYEDIA_H */