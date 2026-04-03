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
 */

#ifndef PENGEMBANG_H
#define PENGEMBANG_H

#include "../dalang/dalang.h"

/* === Pembangunan Umum === */

/* Bangun semua koneksi dan kendali untuk semua perangkat */
void pengembang_bangun_semua(DataPerangkat daftar[], int jumlah);

/* Bangun koneksi untuk satu perangkat */
int  pengembang_bangun_koneksi(DataPerangkat *perangkat);

/* === Kendali (Driver) === */

/* Bangun kendali (driver) untuk perangkat */
int  pengembang_bangun_kendali(DataPerangkat *perangkat);

/* Pasang kendali ke perangkat (dipanggil oleh API) */
int  pengembang_pasang_kendali(unsigned int nomor);

/* Copot kendali dari perangkat */
void pengembang_copot_kendali(unsigned int nomor);

/* Bersihkan kendali ketika perangkat dicabut */
void pengembang_bersihkan_kendali(unsigned int nomor_port);

/* === Antarmuka === */

/* Bangun antarmuka untuk lapisan atas */
int  pengembang_bangun_antarmuka(DataPerangkat *perangkat);

#endif
