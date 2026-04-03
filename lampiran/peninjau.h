/*
 * peninjau.h
 *
 * Peninjau bertugas memeriksa semua perangkat keras.
 * Ia melihat apa yang terpasang, mengidentifikasi tipe,
 * dan memverifikasi apakah perangkat berfungsi baik.
 *
 * Fungsi-fungsi Peninjau:
 *   cek_ram()       → "Ada berapa RAM? Apakah berfungsi?"
 *   cek_usb()       → "Ada USB di port berapa? Tipe apa?"
 *   cek_pci()       → "Ada kartu PCI apa saja?"
 *   cek_layar()     → "Layar terhubung? Resolusi berapa?"
 *   cek_papan_ketik()→ "Papan ketik terhubung?"
 */

#ifndef PENINJAU_H
#define PENINJAU_H

#include "../dalang/dalang.h"

/* === Pemeriksaan Umum === */

/* Periksa semua perangkat yang terpasang */
void peninjau_periksa_semua(void);

/* Periksa perangkat di satu port tertentu (untuk hot-plug) */
int  peninjau_periksa_port(unsigned int nomor_port, DataPerangkat *hasil);

/* === Pemeriksaan Perangkat Spesifik === */

/* Periksa RAM yang terpasang */
int  peninjau_cek_ram(DataPerangkat *hasil);

/* Periksa perangkat USB */
int  peninjau_cek_usb(unsigned int nomor_port, DataPerangkat *hasil);

/* Periksa bus PCI/PCIe */
int  peninjau_cek_pci(DataPerangkat daftar[], int maks);

/* Periksa layar/tampilan */
int  peninjau_cek_layar(DataPerangkat *hasil);

/* Periksa papan ketik */
int  peninjau_cek_papan_ketik(DataPerangkat *hasil);

/* Periksa tetikan (mouse) */
int  peninjau_cek_tetikan(DataPerangkat *hasil);

/* Periksa penyimpanan (HDD/SSD) */
int  peninjau_cek_penyimpanan(DataPerangkat daftar[], int maks);

/* Verifikasi apakah perangkat berfungsi dengan baik */
KondisiPerangkat peninjau_verifikasi(DataPerangkat *perangkat);

#endif
