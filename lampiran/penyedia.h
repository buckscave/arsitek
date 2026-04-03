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

#include "../dalang/dalang.h"

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

#endif
