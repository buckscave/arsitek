/*
 * notulen.c
 *
 * Implementasi Notulen — pencatat dan logger kernel Arsitek.
 * Notulen berfungsi sebagai "buku catatan" perusahaan:
 * mencatat setiap transaksi, temuan, dan laporan yang
 * dilakukan oleh Peninjau, Penyedia, dan Pengembang.
 *
 * Notulen memiliki dua fungsi utama:
 *   1. Pencatatan log kernel (notulen_catat) — pesan teks
 *      dengan tingkat keparahan (info, peringatan, kesalahan, fatal)
 *   2. Penyimpanan data perangkat (notulen_tambah, dll.) —
 *      database perangkat keras yang terdeteksi
 *
 * Pada tahap awal, output log dikirim ke:
 *   - Layar VGA (jika pigura belum siap)
 *   - Port serial COM1 (untuk debugging)
 *   - Buffer internal (untuk pembacaan ulang)
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"

/* ================================================================
 * DATA INTERNAL
 * ================================================================ */

/* Database perangkat — array statis */
static DataPerangkat catatan[PERANGKAT_MAKSIMUM];
static int total = 0;

/* Buffer log — menyimpan baris-baris notulen */
static char buffer_log[NOTULEN_BARIS_MAKSIMUM][NOTULEN_PANJANG_BARIS];
static int baris_log = 0;

/* Tingkat log minimum yang ditampilkan */
static TingkatNotulen tingkat_minimum = NOTULEN_INFO;

/* Label tingkat log untuk tampilan */
static const char *label_tingkat[] = {
    "INFO",       /* NOTULEN_INFO */
    "PERINGATAN", /* NOTULEN_PERINGATAN */
    "KESALAHAN",  /* NOTULEN_KESALAHAN */
    "FATAL"       /* NOTULEN_FATAL */
};

/* ================================================================
 * OUTPUT LOG
 * ================================================================ */

/*
 * tulis_ke_serial — Kirim satu karakter ke port serial COM1.
 * Digunakan untuk debugging kernel melalui serial console.
 */
static void tulis_ke_serial(char c)
{
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    /* Tunggu sampai transmit buffer kosong */
    uint8 ulangi = 255;
    while (ulangi-- > 0) {
        if ((baca_port(SERIAL_COM1 + 5) & 0x20) != 0) {
            break;
        }
    }
    tulis_port(SERIAL_COM1, (uint8)c);
#else
    TIDAK_DIGUNAKAN(c);
#endif
}

/*
 * tulis_string_ke_serial — Kirim string ke port serial.
 */
static void tulis_string_ke_serial(const char *str)
{
    while (*str) {
        if (*str == '\n') {
            tulis_ke_serial('\r');
        }
        tulis_ke_serial(*str);
        str++;
    }
}

/*
 * inisialisasi_serial — Siapkan port serial COM1.
 * Konfigurasi: 115200 baud, 8N1, tanpa parity.
 */
static void inisialisasi_serial(void)
{
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    tulis_port(SERIAL_COM1 + 1, 0x00);    /* Nonaktifkan interrupt */
    tulis_port(SERIAL_COM1 + 3, 0x80);    /* Aktifkan DLAB */
    tulis_port(SERIAL_COM1 + 0, 0x01);    /* Baud rate rendah = 115200 */
    tulis_port(SERIAL_COM1 + 1, 0x00);    /* Baud rate tinggi */
    tulis_port(SERIAL_COM1 + 3, 0x03);    /* 8 bit, tanpa parity, 1 stop */
    tulis_port(SERIAL_COM1 + 2, 0xC7);    /* Aktifkan FIFO, bersihkan, 14-byte */
    tulis_port(SERIAL_COM1 + 4, 0x0B);    /* IRQ aktif, RTS/DSR set */
#endif
}

/* Penanda apakah serial sudah diinisialisasi */
static int serial_siap = SALAH;

/* ================================================================
 * FUNGSI PENCATATAN LOG
 * ================================================================ */

/*
 * notulen_catat — Catat pesan log ke notulen.
 * Pesan dikirim ke serial console dan disimpan di buffer internal.
 * Jika tingkat pesan >= tingkat_minimum, pesan juga ditampilkan
 * di layar konsol melalui pigura.
 *
 * Format pesan: "[TINGKAT] pesan"
 *
 * Parameter:
 *   tingkat — tingkat keparahan (NOTULEN_INFO .. NOTULEN_FATAL)
 *   pesan   — isi pesan (string null-terminated)
 */
void notulen_catat(TingkatNotulen tingkat, const char *pesan)
{
    char baris[NOTULEN_PANJANG_BARIS];
    int posisi = 0;
    int i;

    if (pesan == NULL) return;

    /* Inisialisasi serial sekali saja */
    if (!serial_siap) {
        inisialisasi_serial();
        serial_siap = BENAR;
    }

    /* Buat format: "[TINGKAT] pesan" */
    baris[posisi++] = '[';
    if (tingkat >= 0 && tingkat <= 3) {
        const char *label = label_tingkat[tingkat];
        while (*label && posisi < NOTULEN_PANJANG_BARIS - 2) {
            baris[posisi++] = *label++;
        }
    }
    baris[posisi++] = ']';
    baris[posisi++] = ' ';

    /* Tambahkan isi pesan */
    i = 0;
    while (pesan[i] && posisi < NOTULEN_PANJANG_BARIS - 2) {
        baris[posisi++] = pesan[i++];
    }
    baris[posisi++] = '\n';
    baris[posisi] = '\0';

    /* Kirim ke serial console */
    tulis_string_ke_serial(baris);

    /* Simpan ke buffer log */
    if (baris_log < NOTULEN_BARIS_MAKSIMUM) {
        salin_string(buffer_log[baris_log], baris);
        baris_log++;
    }

    /* Tampilkan ke konsol pigura jika tingkat cukup */
    if (tingkat >= tingkat_minimum && pigura_siap) {
        pigura_tulis_string(baris);
    }

    /* Jika FATAL, hentikan sistem */
    if (tingkat == NOTULEN_FATAL) {
        mesin_hentikan();
    }
}

/*
 * notulen_catat_perangkat — Catat pesan log terkait perangkat.
 * Sama seperti notulen_catat, tetapi menambahkan nama perangkat.
 */
void notulen_catat_perangkat(TingkatNotulen tingkat, const char *pesan,
                             DataPerangkat *perangkat)
{
    char baris[NOTULEN_PANJANG_BARIS];

    if (pesan == NULL || perangkat == NULL) return;

    salin_string(baris, pesan);
    salin_string(baris + panjang_string(baris), " — ");
    salin_string(baris + panjang_string(baris), perangkat->nama);

    notulen_catat(tingkat, baris);
}

/* ================================================================
 * FUNGSI PENYIMPANAN DATA PERANGKAT
 * ================================================================ */

void notulen_simpan(DataPerangkat daftar[], int jumlah)
{
    int i;
    for (i = 0; i < jumlah && i < PERANGKAT_MAKSIMUM; i++) {
        catatan[i] = daftar[i];
    }
    total = jumlah;
}

void notulen_tambah(DataPerangkat *perangkat)
{
    if (perangkat == NULL) return;
    if (total < PERANGKAT_MAKSIMUM) {
        catatan[total] = *perangkat;
        catatan[total].nomor = (uint32)total;
        total++;
    }
}

void notulen_hapus(unsigned int nomor_port)
{
    int i;
    for (i = 0; i < total; i++) {
        if (catatan[i].port == nomor_port) {
            int j;
            for (j = i; j < total - 1; j++) {
                catatan[j] = catatan[j + 1];
                catatan[j].nomor = (uint32)j;
            }
            total--;
            return;
        }
    }
}

int notulen_baca_semua(DataPerangkat daftar[], int maks)
{
    int jumlah = (total < maks) ? total : maks;
    int i;
    for (i = 0; i < jumlah; i++) {
        daftar[i] = catatan[i];
    }
    return jumlah;
}

int notulen_baca_satu(unsigned int nomor, DataPerangkat *hasil)
{
    if (hasil == NULL) return 0;
    if (nomor < (unsigned int)total) {
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
    if (nomor < (unsigned int)total) {
        catatan[nomor].kondisi = kondisi;
    }
}

/*
 * notulen_tampilkan — Tampilkan seluruh log ke konsol.
 * Digunakan untuk menampilkan riwayat log yang tersimpan.
 */
void notulen_tampilkan(void)
{
    int i;
    for (i = 0; i < baris_log; i++) {
        tulis_string_ke_serial(buffer_log[i]);
        if (pigura_siap) {
            pigura_tulis_string(buffer_log[i]);
        }
    }
}
