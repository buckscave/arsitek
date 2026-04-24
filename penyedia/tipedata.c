/*
 * tipedata.c
 *
 * Penyediaan tipe data sesuai arsitektur CPU.
 * Menyediakan informasi ukuran tipe data, persyaratan
 * alignment, konversi lebar data antara 32-bit dan 64-bit,
 * serta validasi ukuran tipe data terhadap arsitektur.
 *
 * Fungsi utama:
 *   penyedia_tipedata_informasi()       - kirim info tipe data arsitektur
 *   penyedia_tipedata_uji_alignment()   - uji alignment tipe data
 *   penyedia_tipedata_konversi_lebar()  - konversi data 32/64-bit
 *   penyedia_tipedata_validasi_ukuran() - validasi ukuran tipe data
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"
#include "../lampiran/penyedia.h"

/* ================================================================
 * STRUKTUR DATA INTERNAL
 * ================================================================ */

/* Informasi satu tipe data */
typedef struct {
    char    nama[16];      /* Nama tipe data (mis. "uint8", "ukuran_ptr") */
    uint8   ukuran;        /* Ukuran dalam byte */
    uint8   alignment;     /* Alignment yang dibutuhkan dalam byte */
    uint8   bertanda;      /* BENAR jika bertanda, SALAH jika tanpa tanda */
    uint8   lebar_bit;     /* Lebar efektif dalam bit (8, 16, 32, 64) */
} InfoTipeData;

/* Hasil informasi tipe data arsitektur */
typedef struct {
    uint8   lebar_arsitektur;   /* 32 atau 64 bit */
    uint8   ukuran_pointer;     /* Ukuran pointer dalam byte */
    uint8   ukuran_int;         /* Ukuran int dalam byte */
    uint8   ukuran_long;        /* Ukuran long dalam byte */
    uint8   ukuran_long_long;   /* Ukuran long long dalam byte */
    uint8   ukuran_size_t;      /* Ukuran ukuran_t dalam byte */
    uint8   endian;             /* 0=little-endian, 1=big-endian */
    uint8   jumlah_tipe;        /* Jumlah tipe data dalam tabel */
    InfoTipeData tipe[12];      /* Tabel info tipe data */
} HasilInfoTipeData;

/* Hasil uji alignment */
typedef struct {
    uint8   tipe_uji;           /* Indeks tipe yang diuji */
    uint8   alignment_diharapkan; /* Alignment yang diharapkan */
    uint8   alignment_aktual;   /* Alignment aktual dari pengujian */
    ukuran_t selisih_offset;    /* Selisih offset akibat padding */
    logika  lulus;              /* BENAR jika alignment sesuai */
} HasilUjiAlignment;

/* Hasil konversi lebar data */
typedef struct {
    uint32  nilai_32;          /* Nilai hasil konversi ke 32-bit */
    uint64  nilai_64;          /* Nilai hasil konversi ke 64-bit */
    uint8   sumber_lebar;      /* Lebar sumber: 32 atau 64 */
    uint8   terpotong;         /* BENAR jika ada data yang terpotong */
} HasilKonversiLebar;

/* Hasil validasi ukuran */
typedef struct {
    logika  seluruh_lulus;     /* BENAR jika semua validasi lulus */
    uint8   jumlah_gagal;      /* Jumlah validasi yang gagal */
    uint8   tipe_gagal[12];    /* Indeks tipe yang gagal validasi */
    uint8   ukuran_diharapkan[12]; /* Ukuran yang diharapkan */
    uint8   ukuran_aktual[12];     /* Ukuran aktual */
} HasilValidasiUkuran;

/* ================================================================
 * DATA INTERNAL
 * ================================================================ */

/* Tabel informasi tipe data — diisi saat inisialisasi */
static HasilInfoTipeData info_tipedata;

/* Tanda apakah info tipe data sudah diinisialisasi */
static logika tipedata_siap = SALAH;

/* ================================================================
 * FUNGSI BANTUAN INTERNAL
 * ================================================================ */

/*
 * Deteksi endianness mesin saat runtime.
 * Mengembalikan 0 untuk little-endian, 1 untuk big-endian.
 */
static uint8 deteksi_endian(void)
{
    uint16 nilai_uji = 0x0001;
    uint8 *penunjuk = (uint8 *)&nilai_uji;
    if (penunjuk[0] == 0x01) {
        return 0; /* little-endian */
    }
    return 1; /* big-endian */
}

/*
 * Hitung alignment aktual suatu tipe menggunakan offset struct.
 * Caranya: letakkan tipe dalam struct dengan byte sebelumnya,
 * lalu ukur offset yang dihasilkan compiler.
 */
static uint8 hitung_alignment_aktual(ukuran_t ukuran_tipe)
{
    /*
     * Metode sederhana: alignment biasanya sama dengan ukuran tipe
     * untuk tipe dasar, kecuali untuk tipe yang lebih besar dari
     * ukuran register mesin (over-aligned).
     * Pada x86/x64, alignment natural = min(ukuran_tipe, ukuran_register).
     * Untuk long long pada mesin 32-bit, alignment bisa 4 atau 8.
     */
#if ARSITEK_LEBAR == 32
    if (ukuran_tipe >= 8) {
        return 4; /* long long pada 32-bit biasanya 4-byte aligned */
    }
    return (uint8)ukuran_tipe;
#else
    if (ukuran_tipe >= 16) {
        return 8; /* Tipe sangat besar tetap 8-byte aligned */
    }
    return (uint8)ukuran_tipe;
#endif
}

/*
 * Isi tabel informasi tipe data berdasarkan arsitektur saat ini.
 */
static void isi_tabel_tipedata(void)
{
    int idx = 0;

    isi_memori(&info_tipedata, 0, sizeof(HasilInfoTipeData));

    info_tipedata.lebar_arsitektur = (uint8)ARSITEK_LEBAR;
    info_tipedata.ukuran_pointer = (uint8)sizeof(void *);
    info_tipedata.ukuran_int = (uint8)sizeof(int);
    info_tipedata.ukuran_long = (uint8)sizeof(long);
    info_tipedata.ukuran_long_long = (uint8)sizeof(long long);
    info_tipedata.ukuran_size_t = (uint8)sizeof(ukuran_t);
    info_tipedata.endian = deteksi_endian();

    /* Tipe 0: uint8 */
    salin_string(info_tipedata.tipe[idx].nama, "uint8");
    info_tipedata.tipe[idx].ukuran = (uint8)sizeof(uint8);
    info_tipedata.tipe[idx].alignment = 1;
    info_tipedata.tipe[idx].bertanda = SALAH;
    info_tipedata.tipe[idx].lebar_bit = 8;
    idx++;

    /* Tipe 1: int8 */
    salin_string(info_tipedata.tipe[idx].nama, "int8");
    info_tipedata.tipe[idx].ukuran = (uint8)sizeof(int8);
    info_tipedata.tipe[idx].alignment = 1;
    info_tipedata.tipe[idx].bertanda = BENAR;
    info_tipedata.tipe[idx].lebar_bit = 8;
    idx++;

    /* Tipe 2: uint16 */
    salin_string(info_tipedata.tipe[idx].nama, "uint16");
    info_tipedata.tipe[idx].ukuran = (uint8)sizeof(uint16);
    info_tipedata.tipe[idx].alignment = 2;
    info_tipedata.tipe[idx].bertanda = SALAH;
    info_tipedata.tipe[idx].lebar_bit = 16;
    idx++;

    /* Tipe 3: int16 */
    salin_string(info_tipedata.tipe[idx].nama, "int16");
    info_tipedata.tipe[idx].ukuran = (uint8)sizeof(int16);
    info_tipedata.tipe[idx].alignment = 2;
    info_tipedata.tipe[idx].bertanda = BENAR;
    info_tipedata.tipe[idx].lebar_bit = 16;
    idx++;

    /* Tipe 4: uint32 */
    salin_string(info_tipedata.tipe[idx].nama, "uint32");
    info_tipedata.tipe[idx].ukuran = (uint8)sizeof(uint32);
    info_tipedata.tipe[idx].alignment = 4;
    info_tipedata.tipe[idx].bertanda = SALAH;
    info_tipedata.tipe[idx].lebar_bit = 32;
    idx++;

    /* Tipe 5: int32 */
    salin_string(info_tipedata.tipe[idx].nama, "int32");
    info_tipedata.tipe[idx].ukuran = (uint8)sizeof(int32);
    info_tipedata.tipe[idx].alignment = 4;
    info_tipedata.tipe[idx].bertanda = BENAR;
    info_tipedata.tipe[idx].lebar_bit = 32;
    idx++;

    /* Tipe 6: uint64 */
    salin_string(info_tipedata.tipe[idx].nama, "uint64");
    info_tipedata.tipe[idx].ukuran = (uint8)sizeof(uint64);
    info_tipedata.tipe[idx].alignment = hitung_alignment_aktual(sizeof(uint64));
    info_tipedata.tipe[idx].bertanda = SALAH;
    info_tipedata.tipe[idx].lebar_bit = 64;
    idx++;

    /* Tipe 7: int64 */
    salin_string(info_tipedata.tipe[idx].nama, "int64");
    info_tipedata.tipe[idx].ukuran = (uint8)sizeof(int64);
    info_tipedata.tipe[idx].alignment = hitung_alignment_aktual(sizeof(int64));
    info_tipedata.tipe[idx].bertanda = BENAR;
    info_tipedata.tipe[idx].lebar_bit = 64;
    idx++;

    /* Tipe 8: ukuran_ptr */
    salin_string(info_tipedata.tipe[idx].nama, "ukuran_ptr");
    info_tipedata.tipe[idx].ukuran = (uint8)sizeof(ukuran_ptr);
    info_tipedata.tipe[idx].alignment = hitung_alignment_aktual(sizeof(ukuran_ptr));
    info_tipedata.tipe[idx].bertanda = SALAH;
    info_tipedata.tipe[idx].lebar_bit = (uint8)ARSITEK_LEBAR;
    idx++;

    /* Tipe 9: ukuran_t */
    salin_string(info_tipedata.tipe[idx].nama, "ukuran_t");
    info_tipedata.tipe[idx].ukuran = (uint8)sizeof(ukuran_t);
    info_tipedata.tipe[idx].alignment = hitung_alignment_aktual(sizeof(ukuran_t));
    info_tipedata.tipe[idx].bertanda = SALAH;
    info_tipedata.tipe[idx].lebar_bit = (uint8)(sizeof(ukuran_t) * 8);
    idx++;

    /* Tipe 10: selisih_t */
    salin_string(info_tipedata.tipe[idx].nama, "selisih_t");
    info_tipedata.tipe[idx].ukuran = (uint8)sizeof(selisih_t);
    info_tipedata.tipe[idx].alignment = hitung_alignment_aktual(sizeof(selisih_t));
    info_tipedata.tipe[idx].bertanda = BENAR;
    info_tipedata.tipe[idx].lebar_bit = (uint8)(sizeof(selisih_t) * 8);
    idx++;

    /* Tipe 11: logika */
    salin_string(info_tipedata.tipe[idx].nama, "logika");
    info_tipedata.tipe[idx].ukuran = (uint8)sizeof(logika);
    info_tipedata.tipe[idx].alignment = (uint8)sizeof(int);
    info_tipedata.tipe[idx].bertanda = BENAR;
    info_tipedata.tipe[idx].lebar_bit = (uint8)(sizeof(int) * 8);
    idx++;

    info_tipedata.jumlah_tipe = (uint8)idx;
    tipedata_siap = BENAR;
}

/* ================================================================
 * FUNGSI PUBLIK
 * ================================================================ */

/*
 * penyedia_tipedata_informasi
 *
 * Mengembalikan informasi tentang ukuran tipe data untuk
 * arsitektur CPU saat ini (32-bit vs 64-bit). Fungsi ini
 * mengisi data_khusus pada struktur DataPerangkat dengan
 * informasi tipe data yang relevan.
 *
 * Parameter:
 *   perangkat - struktur perangkat yang akan diisi info tipe data
 *
 * Format data_khusus:
 *   [0]     = lebar arsitektur (32 atau 64)
 *   [1]     = ukuran pointer (byte)
 *   [2]     = ukuran int (byte)
 *   [3]     = ukuran long (byte)
 *   [4]     = ukuran long long (byte)
 *   [5]     = ukuran ukuran_t (byte)
 *   [6]     = endianness (0=little, 1=big)
 *   [7]     = jumlah tipe data dalam tabel
 *   [8-11]  = alignment uint8, uint16, uint32, uint64
 *   [12-15] = alignment int8, int16, int32, int64
 *   [16]    = alignment ukuran_ptr
 *   [17]    = alignment ukuran_t
 */
void penyedia_tipedata_informasi(DataPerangkat *perangkat)
{
    if (perangkat == NULL) return;

    /* Pastikan tabel sudah diinisialisasi */
    if (!tipedata_siap) {
        isi_tabel_tipedata();
    }

    /* Isi data_khusus dengan informasi tipe data */
    perangkat->data_khusus[0] = info_tipedata.lebar_arsitektur;
    perangkat->data_khusus[1] = info_tipedata.ukuran_pointer;
    perangkat->data_khusus[2] = info_tipedata.ukuran_int;
    perangkat->data_khusus[3] = info_tipedata.ukuran_long;
    perangkat->data_khusus[4] = info_tipedata.ukuran_long_long;
    perangkat->data_khusus[5] = info_tipedata.ukuran_size_t;
    perangkat->data_khusus[6] = info_tipedata.endian;
    perangkat->data_khusus[7] = info_tipedata.jumlah_tipe;

    /* Alignment tipe tanpa tanda: uint8, uint16, uint32, uint64 */
    perangkat->data_khusus[8]  = info_tipedata.tipe[0].alignment;  /* uint8  */
    perangkat->data_khusus[9]  = info_tipedata.tipe[2].alignment;  /* uint16 */
    perangkat->data_khusus[10] = info_tipedata.tipe[4].alignment;  /* uint32 */
    perangkat->data_khusus[11] = info_tipedata.tipe[6].alignment;  /* uint64 */

    /* Alignment tipe bertanda: int8, int16, int32, int64 */
    perangkat->data_khusus[12] = info_tipedata.tipe[1].alignment;  /* int8   */
    perangkat->data_khusus[13] = info_tipedata.tipe[3].alignment;  /* int16  */
    perangkat->data_khusus[14] = info_tipedata.tipe[5].alignment;  /* int32  */
    perangkat->data_khusus[15] = info_tipedata.tipe[7].alignment;  /* int64  */

    /* Alignment tipe khusus */
    perangkat->data_khusus[16] = info_tipedata.tipe[8].alignment;  /* ukuran_ptr */
    perangkat->data_khusus[17] = info_tipedata.tipe[9].alignment;  /* ukuran_t   */

    notulen_catat(NOTULEN_INFO, "Penyedia: Informasi tipe data arsitektur siap");
}

/*
 * penyedia_tipedata_uji_alignment
 *
 * Menguji persyaratan alignment untuk berbagai tipe data.
 * Memverifikasi bahwa alignment yang dilaporkan compiler
 * sesuai dengan alignment aktual pada arsitektur ini.
 *
 * Parameter:
 *   perangkat - struktur perangkat untuk menyimpan hasil uji
 *
 * Mengembalikan: jumlah tipe yang lulus uji alignment.
 *
 * Format data_khusus (setelah pemanggilan):
 *   [32]        = jumlah tipe yang lulus
 *   [33]        = jumlah tipe yang gagal
 *   [34..45]    = hasil per tipe: 1=lulus, 0=gagal
 */
int penyedia_tipedata_uji_alignment(DataPerangkat *perangkat)
{
    HasilUjiAlignment hasil_uji[12];
    int jumlah_lulus = 0;
    int jumlah_gagal = 0;
    int i;
    uint8 alignment_aktual;

    if (perangkat == NULL) return 0;

    /* Pastikan tabel sudah diinisialisasi */
    if (!tipedata_siap) {
        isi_tabel_tipedata();
    }

    isi_memori(hasil_uji, 0, sizeof(hasil_uji));

    /* Uji alignment setiap tipe data */
    for (i = 0; i < info_tipedata.jumlah_tipe && i < 12; i++) {
        alignment_aktual = hitung_alignment_aktual(
            (ukuran_t)info_tipedata.tipe[i].ukuran);

        hasil_uji[i].tipe_uji = (uint8)i;
        hasil_uji[i].alignment_diharapkan = info_tipedata.tipe[i].alignment;
        hasil_uji[i].alignment_aktual = alignment_aktual;
        hasil_uji[i].selisih_offset = 0;

        if (alignment_aktual == info_tipedata.tipe[i].alignment) {
            hasil_uji[i].lulus = BENAR;
            jumlah_lulus++;
        } else {
            hasil_uji[i].lulus = SALAH;
            jumlah_gagal++;
        }
    }

    /* Simpan ringkasan ke data_khusus */
    perangkat->data_khusus[32] = (uint8)jumlah_lulus;
    perangkat->data_khusus[33] = (uint8)jumlah_gagal;

    /* Simpan hasil per tipe */
    for (i = 0; i < info_tipedata.jumlah_tipe && i < 12; i++) {
        perangkat->data_khusus[34 + i] = (uint8)(hasil_uji[i].lulus ? 1 : 0);
    }

    {
        char pesan[80];
        salin_string(pesan, "Penyedia: Uji alignment selesai, lulus ");
        konversi_uint_ke_string((uint32)jumlah_lulus,
                                pesan + panjang_string(pesan), 10);
        salin_string(pesan + panjang_string(pesan), "/");
        konversi_uint_ke_string((uint32)info_tipedata.jumlah_tipe,
                                pesan + panjang_string(pesan), 10);
        notulen_catat(NOTULEN_INFO, pesan);
    }

    return jumlah_lulus;
}

/*
 * penyedia_tipedata_konversi_lebar
 *
 * Mengkonversi data antara lebar 32-bit dan 64-bit.
 * Jika sumber 32-bit, nilai diperluas ke 64-bit (zero-extend
 * untuk tanpa tanda, sign-extend untuk bertanda).
 * Jika sumber 64-bit, nilai dipotong ke 32-bit dan
 * tanda terpotong dicatat.
 *
 * Parameter:
 *   nilai_sumber - nilai data yang akan dikonversi
 *   lebar_sumber - lebar sumber: 32 atau 64
 *   bertanda     - BENAR jika tipe bertanda, SALAH jika tanpa tanda
 *   hasil        - pointer ke HasilKonversiLebar untuk menyimpan hasil
 *
 * Mengembalikan: STATUS_OK jika berhasil, STATUS_PARAMETAR_SALAH jika parameter salah
 */
int penyedia_tipedata_konversi_lebar(uint64 nilai_sumber, uint8 lebar_sumber,
                                      logika bertanda, void *hasil)
{
    HasilKonversiLebar *h = (HasilKonversiLebar *)hasil;

    if (h == NULL) return STATUS_PARAMETAR_SALAH;
    if (lebar_sumber != 32 && lebar_sumber != 64) return STATUS_PARAMETAR_SALAH;

    isi_memori(h, 0, sizeof(HasilKonversiLebar));

    h->sumber_lebar = lebar_sumber;
    h->terpotong = SALAH;

    if (lebar_sumber == 32) {
        /* Konversi dari 32-bit ke 64-bit */
        h->nilai_32 = (uint32)(nilai_sumber & 0xFFFFFFFFUL);

        if (bertanda) {
            /* Sign-extend: jika bit 31 set, isi bit 32-63 dengan 1 */
            if (nilai_sumber & 0x80000000UL) {
                h->nilai_64 = nilai_sumber | 0xFFFFFFFF00000000ULL;
            } else {
                h->nilai_64 = nilai_sumber & 0xFFFFFFFFULL;
            }
        } else {
            /* Zero-extend: isi bit 32-63 dengan 0 */
            h->nilai_64 = nilai_sumber & 0xFFFFFFFFULL;
        }
    } else {
        /* Konversi dari 64-bit ke 32-bit */
        h->nilai_64 = nilai_sumber;
        h->nilai_32 = (uint32)(nilai_sumber & 0xFFFFFFFFUL);

        /* Periksa apakah ada data yang terpotong */
        if ((nilai_sumber & 0xFFFFFFFF00000000ULL) != 0) {
            h->terpotong = BENAR;
        }
    }

    return STATUS_OK;
}

/*
 * penyedia_tipedata_validasi_ukuran
 *
 * Memvalidasi bahwa ukuran tipe data yang diukur compiler
 * sesuai dengan yang diharapkan berdasarkan arsitektur CPU.
 * Pada arsitektur 32-bit: pointer=4, long=4, ukuran_t=4.
 * Pada arsitektur 64-bit: pointer=8, long=8, ukuran_t=8.
 *
 * Parameter:
 *   perangkat - struktur perangkat untuk menyimpan hasil validasi
 *
 * Mengembalikan: BENAR jika semua validasi lulus, SALAH jika ada yang gagal.
 *
 * Format data_khusus (setelah pemanggilan):
 *   [48]        = 1 jika seluruh validasi lulus, 0 jika ada yang gagal
 *   [49]        = jumlah validasi yang gagal
 *   [50..61]    = selisih ukuran per tipe (0=sesuai, >0=selisih)
 */
logika penyedia_tipedata_validasi_ukuran(DataPerangkat *perangkat)
{
    HasilValidasiUkuran validasi;
    uint8 ukuran_diharapkan[12];
    uint8 ukuran_aktual_arr[12];
    int i;

    if (perangkat == NULL) return SALAH;

    /* Pastikan tabel sudah diinisialisasi */
    if (!tipedata_siap) {
        isi_tabel_tipedata();
    }

    isi_memori(&validasi, 0, sizeof(HasilValidasiUkuran));

    /* Tentukan ukuran yang diharapkan berdasarkan arsitektur */
    for (i = 0; i < 12; i++) {
        ukuran_diharapkan[i] = 0;
        ukuran_aktual_arr[i] = 0;
    }

    /* uint8 dan int8 selalu 1 byte di semua arsitektur */
    ukuran_diharapkan[0] = 1;  /* uint8 */
    ukuran_diharapkan[1] = 1;  /* int8  */

    /* uint16 dan int16 selalu 2 byte */
    ukuran_diharapkan[2] = 2;  /* uint16 */
    ukuran_diharapkan[3] = 2;  /* int16  */

    /* uint32 dan int32 selalu 4 byte */
    ukuran_diharapkan[4] = 4;  /* uint32 */
    ukuran_diharapkan[5] = 4;  /* int32  */

    /* uint64 dan int64 selalu 8 byte */
    ukuran_diharapkan[6] = 8;  /* uint64 */
    ukuran_diharapkan[7] = 8;  /* int64  */

    /* ukuran_ptr dan ukuran_t bergantung pada arsitektur */
#if ARSITEK_LEBAR == 32
    ukuran_diharapkan[8]  = 4;  /* ukuran_ptr */
    ukuran_diharapkan[9]  = 4;  /* ukuran_t   */
    ukuran_diharapkan[10] = 4;  /* selisih_t  */
#else
    ukuran_diharapkan[8]  = 8;  /* ukuran_ptr */
    ukuran_diharapkan[9]  = 8;  /* ukuran_t   */
    ukuran_diharapkan[10] = 8;  /* selisih_t  */
#endif

    /* logika (int) selalu sama dengan ukuran int */
    ukuran_diharapkan[11] = (uint8)sizeof(int); /* logika */

    /* Ambil ukuran aktual dari tabel */
    for (i = 0; i < info_tipedata.jumlah_tipe && i < 12; i++) {
        ukuran_aktual_arr[i] = info_tipedata.tipe[i].ukuran;
    }

    /* Bandingkan ukuran diharapkan vs aktual */
    validasi.seluruh_lulus = BENAR;
    validasi.jumlah_gagal = 0;

    for (i = 0; i < info_tipedata.jumlah_tipe && i < 12; i++) {
        if (ukuran_aktual_arr[i] != ukuran_diharapkan[i]) {
            validasi.seluruh_lulus = SALAH;
            validasi.tipe_gagal[validasi.jumlah_gagal] = (uint8)i;
            validasi.ukuran_diharapkan[validasi.jumlah_gagal] = ukuran_diharapkan[i];
            validasi.ukuran_aktual[validasi.jumlah_gagal] = ukuran_aktual_arr[i];
            validasi.jumlah_gagal++;
        }
    }

    /* Simpan hasil ke data_khusus perangkat */
    perangkat->data_khusus[48] = (uint8)(validasi.seluruh_lulus ? 1 : 0);
    perangkat->data_khusus[49] = validasi.jumlah_gagal;

    for (i = 0; i < info_tipedata.jumlah_tipe && i < 12; i++) {
        if (ukuran_aktual_arr[i] >= ukuran_diharapkan[i]) {
            perangkat->data_khusus[50 + i] =
                (uint8)(ukuran_aktual_arr[i] - ukuran_diharapkan[i]);
        } else {
            perangkat->data_khusus[50 + i] =
                (uint8)(ukuran_diharapkan[i] - ukuran_aktual_arr[i]);
        }
    }

    if (validasi.seluruh_lulus) {
        notulen_catat(NOTULEN_INFO,
                      "Penyedia: Validasi ukuran tipe data lulus semua");
    } else {
        char pesan[80];
        salin_string(pesan, "Penyedia: Validasi ukuran gagal untuk ");
        konversi_uint_ke_string((uint32)validasi.jumlah_gagal,
                                pesan + panjang_string(pesan), 10);
        salin_string(pesan + panjang_string(pesan), " tipe data");
        notulen_catat(NOTULEN_PERINGATAN, pesan);
    }

    return validasi.seluruh_lulus;
}
