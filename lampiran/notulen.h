/*
 * notulen.h
 *
 * Notulen bertugas menyimpan semua data perangkat.
 * Ia adalah "buku catatan" perusahaan.
 *
 * Fungsi Notulen:
 *   - Menyimpan daftar perangkat
 *   - Menambah perangkat baru
 *   - Menghapus perangkat yang dicabut
 *   - Membaca data perangkat
 *   - Memperbarui kondisi perangkat
 */

#ifndef NOTULEN_H
#define NOTULEN_H

#include "../dalang/dalang.h"

/* === Penyimpanan === */

/* Simpan seluruh daftar perangkat */
void notulen_simpan(DataPerangkat daftar[], int jumlah);

/* Tambah satu perangkat baru ke catatan */
void notulen_tambah(DataPerangkat *perangkat);

/* Hapus perangkat dari catatan */
void notulen_hapus(unsigned int nomor_port);

/* === Pembacaan === */

/* Baca seluruh daftar perangkat yang tercatat */
int  notulen_baca_semua(DataPerangkat daftar[], int maks);

/* Baca data satu perangkat berdasarkan nomor urut */
int  notulen_baca_satu(unsigned int nomor, DataPerangkat *hasil);

/* Hitung jumlah perangkat yang tercatat */
int  notulen_hitung(void);

/* === Pembaruan === */

/* Perbarui kondisi sebuah perangkat */
void notulen_perbarui_kondisi(unsigned int nomor, KondisiPerangkat kondisi);

#endif
