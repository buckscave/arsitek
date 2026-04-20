/*
 * arsitek.c
 *
 * Titik masuk kernel Arsitek.
 * Fungsi arsitek_mulai() dipanggil oleh kode pemicu (boot)
 * setelah CPU berada di mode eksekusi yang tepat.
 *
 * Alur kernel:
 *   1. Konstruksi — siapkan mesin (GDT, IDT, PIC, PIT, paging)
 *   2. Pigura     — siapkan konsol tampilan
 *   3. Peninjau   — periksa semua perangkat keras
 *                    (melapor ke Arsitek melalui Notulen)
 *   4. Penyedia   — kumpulkan sumber daya untuk setiap perangkat
 *                    (melapor ke Arsitek melalui Notulen)
 *   5. Pengembang — bangun driver dan antarmuka perangkat
 *                    (melapor ke Arsitek melalui Notulen)
 *   6. Siap       — kernel siap, menunggu Kontraktor (OS)
 *
 * Setelah alur selesai, kernel memasuki loop idle.
 * Interupsi ditangani secara asynchronous oleh handler IDT.
 */

#include "lampiran/arsitek.h"
#include "lampiran/mesin.h"
#include "lampiran/pigura.h"

/* ================================================================
 * UTILITAS STRING — dibutuhkan oleh seluruh modul
 * ================================================================ */

void *salin_memori(void *tujuan, const void *sumber, ukuran_t ukuran)
{
    uint8 *d = (uint8 *)tujuan;
    const uint8 *s = (const uint8 *)sumber;
    ukuran_t i;

    if (tujuan == NULL || sumber == NULL) return tujuan;

    for (i = 0; i < ukuran; i++) {
        d[i] = s[i];
    }
    return tujuan;
}

void *isi_memori(void *tujuan, int nilai, ukuran_t ukuran)
{
    uint8 *d = (uint8 *)tujuan;
    uint8 byte_nilai = (uint8)(nilai & 0xFF);
    ukuran_t i;

    if (tujuan == NULL) return tujuan;

    for (i = 0; i < ukuran; i++) {
        d[i] = byte_nilai;
    }
    return tujuan;
}

int bandingkan_memori(const void *a, const void *b, ukuran_t ukuran)
{
    const uint8 *pa = (const uint8 *)a;
    const uint8 *pb = (const uint8 *)b;
    ukuran_t i;

    for (i = 0; i < ukuran; i++) {
        if (pa[i] != pb[i]) {
            return (pa[i] < pb[i]) ? -1 : 1;
        }
    }
    return 0;
}

ukuran_t panjang_string(const char *str)
{
    ukuran_t len = 0;
    if (str == NULL) return 0;
    while (str[len]) len++;
    return len;
}

char *salin_string(char *tujuan, const char *sumber)
{
    ukuran_t i = 0;
    if (tujuan == NULL || sumber == NULL) return tujuan;
    while (sumber[i]) {
        tujuan[i] = sumber[i];
        i++;
    }
    tujuan[i] = '\0';
    return tujuan;
}

int bandingkan_string(const char *a, const char *b)
{
    ukuran_t i = 0;
    if (a == NULL || b == NULL) return -1;
    while (a[i] && b[i] && a[i] == b[i]) i++;
    return (int)((uint8)a[i] - (uint8)b[i]);
}

char *konversi_uint_ke_string(uint32 nilai, char *buffer, int basis)
{
    char sementara[32];
    int posisi = 0;
    int i;

    if (buffer == NULL) return NULL;
    if (basis < 2 || basis > 36) basis = 10;

    if (nilai == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return buffer;
    }

    while (nilai > 0) {
        int digit = (int)(nilai % (uint32)basis);
        sementara[posisi++] = (char)((digit < 10) ? ('0' + digit) : ('a' + digit - 10));
        nilai /= (uint32)basis;
    }

    for (i = 0; i < posisi; i++) {
        buffer[i] = sementara[posisi - 1 - i];
    }
    buffer[posisi] = '\0';

    return buffer;
}

char *konversi_int_ke_string(int32 nilai, char *buffer, int basis)
{
    if (buffer == NULL) return NULL;
    if (basis < 2 || basis > 36) basis = 10;

    if (nilai < 0) {
        buffer[0] = '-';
        konversi_uint_ke_string((uint32)(-nilai), buffer + 1, basis);
    } else {
        konversi_uint_ke_string((uint32)nilai, buffer, basis);
    }

    return buffer;
}

/* ================================================================
 * TITIK MASUK KERNEL
 * ================================================================ */

/*
 * arsitek_mulai — Titik masuk utama kernel.
 * Dipanggil oleh kode pemicu setelah CPU siap.
 *
 * Arsitek mengoordinasikan seluruh alur kernel:
 *   Konstruksi → Pigura → Peninjau → Penyedia → Pengembang → Idle
 */
void arsitek_mulai(void)
{
    /* ---- Tahap 1: Konstruksi ---- */
    /* Siapkan mesin: GDT, IDT, PIC, PIT, paging */
    mesin_siapkan();

    /* ---- Tahap 2: Pigura ---- */
    /* Siapkan konsol tampilan agar kita bisa melihat output */
    pigura_mulai();

    /* Tampilkan banner kernel */
    pigura_hapus_layar();
    pigura_set_warna(VGA_HIJAU_TERANG, VGA_HITAM);
    pigura_tulis_string("========================================\n");
    pigura_tulis_string("  Kernel Arsitek v0.1\n");
    pigura_tulis_string("  Arsitektur: " ARSITEK_NAMA "\n");
    pigura_tulis_string("  Lebar bit:  ");
    {
        char buffer[8];
        konversi_uint_ke_string((uint32)ARSITEK_LEBAR, buffer, 10);
        pigura_tulis_string(buffer);
    }
    pigura_tulis_string("-bit\n");
    pigura_tulis_string("========================================\n\n");
    pigura_set_warna(VGA_PUTIH, VGA_HITAM);

    notulen_catat(NOTULEN_INFO, "Arsitek: Kernel dimulai");

    /* ---- Tahap 3: Peninjau ---- */
    /* Periksa semua perangkat keras */
    notulen_catat(NOTULEN_INFO, "Arsitek: Memanggil Peninjau...");
    peninjau_periksa_semua();

    /* ---- Tahap 4: Penyedia ---- */
    /* Kumpulkan sumber daya untuk semua perangkat */
    notulen_catat(NOTULEN_INFO, "Arsitek: Memanggil Penyedia...");
    {
        DataPerangkat daftar[PERANGKAT_MAKSIMUM];
        int jumlah = notulen_baca_semua(daftar, PERANGKAT_MAKSIMUM);
        penyedia_kumpulkan_semua(daftar, jumlah);
    }

    /* ---- Tahap 5: Pengembang ---- */
    /* Bangun driver dan antarmuka untuk setiap perangkat */
    notulen_catat(NOTULEN_INFO, "Arsitek: Memanggil Pengembang...");
    {
        DataPerangkat daftar[PERANGKAT_MAKSIMUM];
        int jumlah = notulen_baca_semua(daftar, PERANGKAT_MAKSIMUM);
        pengembang_bangun_semua(daftar, jumlah);
    }

    /* ---- Tahap 6: Siap ---- */
    notulen_catat(NOTULEN_INFO, "Arsitek: Semua subsistem siap");
    notulen_catat(NOTULEN_INFO, "Arsitek: Menunggu Kontraktor (OS)...");

    pigura_set_warna(VGA_HIJAU_TERANG, VGA_HITAM);
    pigura_tulis_string("\nArsitek: Kernel siap. Menunggu Kontraktor...\n");
    pigura_set_warna(VGA_PUTIH, VGA_HITAM);

    /* Aktifkan interupsi */
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    aktifkan_interupsi();
#endif

    /* Loop idle — tunggu interupsi */
    while (1) {
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
        hentikan_cpu();
#elif defined(ARSITEK_ARM32) || defined(ARSITEK_ARM64)
        __asm__ volatile ("wfi");
#endif
    }
}

/* ================================================================
 * IMPLEMENTASI memcpy UNTUK ARM (freestanding)
 *
 * Compiler ARM menghasilkan panggilan ke memcpy bahkan untuk
 * penugasan struktur. Karena kita menggunakan -nostdlib,
 * kita harus menyediakan implementasi sendiri.
 * ================================================================ */
#ifdef __arm__
void *memcpy(void *tujuan, const void *sumber, unsigned long ukuran)
{
    return salin_memori(tujuan, sumber, (ukuran_t)ukuran);
}
#endif

#ifdef __aarch64__
void *memcpy(void *tujuan, const void *sumber, unsigned long ukuran)
{
    return salin_memori(tujuan, sumber, (ukuran_t)ukuran);
}
#endif
