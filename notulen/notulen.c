/*
 * notulen.c
 *
 * Implementasi Notulen — pencatat data perangkat.
 */

#include "notulen.h"

static DataPerangkat catatan[PERANGKAT_MAKSIMUM];
static int total = 0;

void notulen_simpan(DataPerangkat daftar[], int jumlah)
{
    for (int i = 0; i < jumlah && i < PERANGKAT_MAKSIMUM; i++) {
        catatan[i] = daftar[i];
    }
    total = jumlah;
}

void notulen_tambah(DataPerangkat *perangkat)
{
    if (total < PERANGKAT_MAKSIMUM) {
        catatan[total] = *perangkat;
        catatan[total].nomor = total;
        total++;
    }
}

void notulen_hapus(unsigned int nomor_port)
{
    for (int i = 0; i < total; i++) {
        if (catatan[i].port == nomor_port) {
            /* Geser semua perangkat setelahnya */
            for (int j = i; j < total - 1; j++) {
                catatan[j] = catatan[j + 1];
                catatan[j].nomor = j;
            }
            total--;
            return;
        }
    }
}

int notulen_baca_semua(DataPerangkat daftar[], int maks)
{
    int jumlah = (total < maks) ? total : maks;
    for (int i = 0; i < jumlah; i++) {
        daftar[i] = catatan[i];
    }
    return jumlah;
}

int notulen_baca_satu(unsigned int nomor, DataPerangkat *hasil)
{
    if (nomor < total) {
        *hasil = catatan[nomor];
        return 1;
    }
    return 0;
}

int notulen_hitung(void)
{
    return total;
}

void notulen_perbarui_kondisi(unsigned int nomor, KondisiPerangkat kondisi)
{
    if (nomor < total) {
        catatan[nomor].kondisi = kondisi;
    }
}
