/*
 * antarmuka.c
 *
 * Pembangunan antarmuka untuk lapisan atas (OS/Kontraktor).
 * Menyediakan API terstandar untuk mengakses perangkat
 * melalui abstraksi yang tidak bergantung pada detail hardware.
 *
 * Setiap antarmuka dikaitkan dengan satu IC chip dan
 * menyediakan operasi baca/tulis/ioctl/status yang generik.
 * Struktur AntarmukaPerangkat didefinisikan dalam pengembang.h.
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"
#include "../lampiran/peninjau.h"
#include "../lampiran/pengembang.h"

/* ================================================================
 * KONSTANTA ANTARMUKA
 * ================================================================ */

/* Status kunci antarmuka */
#define ANTARMUKA_BEBAS         0   /* Antarmuka tidak terkunci */
#define ANTARMUKA_TERKUNCI     1   /* Antarmuka sedang digunakan */

/* Kode ioctl umum */
#define IOCTL_RESET              0x0001
#define IOCTL_SET_PARAMETER      0x0002
#define IOCTL_GET_PARAMETER      0x0003
#define IOCTL_DIAGNOSTIK         0x0004
#define IOCTL_SET_KECEPATAN      0x0005
#define IOCTL_GET_IDENTITAS      0x0006

/* ================================================================
 * DATA INTERNAL
 *
 * Tabel statis untuk melacak semua antarmuka yang telah dibuat.
 * Maksimum ANTARMUKA_MAKSIMUM (32) antarmuka per sistem.
 * ================================================================ */

static AntarmukaPerangkat tabel_antarmuka[ANTARMUKA_MAKSIMUM];
static int jumlah_antarmuka = 0;

/* ================================================================
 * pengembang_antarmuka_buat — Buat antarmuka untuk perangkat
 *
 * Menghubungkan perangkat dengan IC yang sudah diidentifikasi
 * dan menyiapkan struktur AntarmukaPerangkat untuk akses
 * oleh lapisan atas. Mengisi semua field antarmuka dari
 * data IC dan perangkat.
 *
 * Parameter:
 *   perangkat  — pointer ke DataPerangkat
 *   ic         — pointer ke DataIC yang sudah diidentifikasi
 *   antarmuka  — pointer ke AntarmukaPerangkat yang akan diisi
 *
 * Mengembalikan:
 *   STATUS_OK jika berhasil,
 *   STATUS_PARAMETAR_SALAH jika parameter NULL,
 *   STATUS_GAGAL jika tabel penuh
 * ================================================================ */
int pengembang_antarmuka_buat(DataPerangkat *perangkat, DataIC *ic,
                               AntarmukaPerangkat *antarmuka)
{
    if (perangkat == NULL || ic == NULL || antarmuka == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    if (jumlah_antarmuka >= ANTARMUKA_MAKSIMUM) {
        notulen_catat(NOTULEN_KESALAHAN,
            "Antarmuka: Tabel antarmuka penuh");
        return STATUS_GAGAL;
    }

    /* Isi struktur antarmuka */
    isi_memori(antarmuka, 0, sizeof(AntarmukaPerangkat));
    antarmuka->perangkat   = perangkat;
    antarmuka->ic          = ic;
    antarmuka->terkunci    = ANTARMUKA_BEBAS;
    antarmuka->jumlah_buka = 0;
    antarmuka->kondisi     = KONDISI_BAIK;
    antarmuka->alamat_basis = ic->alamat_basis;
    antarmuka->tipe_bus    = ic->tipe_bus;

    /* Simpan juga ke tabel internal */
    tabel_antarmuka[jumlah_antarmuka] = *antarmuka;
    jumlah_antarmuka++;

    /* Catat pembuatan antarmuka ke notulen */
    {
        char pesan[128];
        salin_string(pesan, "Antarmuka: Dibuat untuk IC ");
        salin_string(pesan + panjang_string(pesan), ic->nama_ic);
        notulen_catat(NOTULEN_INFO, pesan);
    }

    return STATUS_OK;
}

/* ================================================================
 * pengembang_antarmuka_baca — Baca data dari perangkat
 *
 * Melakukan operasi baca melalui register IC. Mendukung
 * baca 1 byte, 2 byte, 4 byte, dan multi-register.
 *
 * Parameter:
 *   antarmuka — pointer ke AntarmukaPerangkat
 *   offset    — offset register atau alamat baca
 *   buffer    — buffer penampung data (uint8*)
 *   panjang   — jumlah byte yang akan dibaca
 *
 * Mengembalikan:
 *   Jumlah byte yang berhasil dibaca (positif),
 *   STATUS_GAGAL jika antarmuka tidak aktif,
 *   STATUS_PARAMETAR_SALAH jika parameter NULL
 * ================================================================ */
int pengembang_antarmuka_baca(AntarmukaPerangkat *antarmuka,
                               uint32 offset,
                               uint8 *buffer,
                               ukuran_t panjang)
{
    if (antarmuka == NULL || buffer == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* Pastikan antarmuka pernah dibuka */
    if (antarmuka->jumlah_buka == 0) {
        notulen_catat(NOTULEN_KESALAHAN,
            "Antarmuka: Baca gagal — antarmuka belum dibuka");
        return STATUS_GAGAL;
    }

    /* Pastikan perangkat tersedia */
    if (antarmuka->perangkat == NULL) {
        return STATUS_GAGAL;
    }

    /* Baca berdasarkan ukuran data */
    if (panjang == 4) {
        uint32 nilai;
        nilai = pengembang_kendali_baca_register(
                    antarmuka->perangkat, offset);
        salin_memori(buffer, &nilai, 4);
    } else if (panjang == 2) {
        uint32 nilai;
        nilai = pengembang_kendali_baca_register(
                    antarmuka->perangkat, offset & 0xFC);
        nilai = (nilai >> ((offset & 0x03) * 8)) & 0xFFFF;
        salin_memori(buffer, &nilai, 2);
    } else if (panjang == 1) {
        uint32 nilai;
        nilai = pengembang_kendali_baca_register(
                    antarmuka->perangkat, offset & 0xFC);
        nilai = (nilai >> ((offset & 0x03) * 8)) & 0xFF;
        salin_memori(buffer, &nilai, 1);
    } else {
        /* Baca multi-register */
        ukuran_t i;
        for (i = 0; i < panjang; i += 4) {
            uint32 nilai;
            ukuran_t sisa;
            ukuran_t salin;

            nilai = pengembang_kendali_baca_register(
                        antarmuka->perangkat, offset + (uint32)i);
            sisa = panjang - i;
            salin = (sisa < 4) ? sisa : 4;
            salin_memori(buffer + i, &nilai, salin);
        }
    }

    return (int)panjang;
}

/* ================================================================
 * pengembang_antarmuka_tulis — Tulis data ke perangkat
 *
 * Melakukan operasi tulis melalui register IC. Mendukung
 * tulis 1 byte, 2 byte, 4 byte, dan multi-register.
 * Untuk tulis 1/2 byte, dilakukan read-modify-write
 * untuk menjaga byte lain dalam register yang sama.
 *
 * Parameter:
 *   antarmuka — pointer ke AntarmukaPerangkat
 *   offset    — offset register atau alamat tulis
 *   buffer    — buffer data yang akan ditulis (const uint8*)
 *   panjang   — jumlah byte yang akan ditulis
 *
 * Mengembalikan:
 *   Jumlah byte yang berhasil ditulis (positif),
 *   STATUS_GAGAL jika antarmuka tidak aktif,
 *   STATUS_PARAMETAR_SALAH jika parameter NULL
 * ================================================================ */
int pengembang_antarmuka_tulis(AntarmukaPerangkat *antarmuka,
                                uint32 offset,
                                const uint8 *buffer,
                                ukuran_t panjang)
{
    if (antarmuka == NULL || buffer == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* Pastikan antarmuka pernah dibuka */
    if (antarmuka->jumlah_buka == 0) {
        notulen_catat(NOTULEN_KESALAHAN,
            "Antarmuka: Tulis gagal — antarmuka belum dibuka");
        return STATUS_GAGAL;
    }

    /* Pastikan perangkat tersedia */
    if (antarmuka->perangkat == NULL) {
        return STATUS_GAGAL;
    }

    /* Tulis berdasarkan ukuran data */
    if (panjang == 4) {
        uint32 nilai;
        salin_memori(&nilai, buffer, 4);
        pengembang_kendali_tulis_register(
            antarmuka->perangkat, offset, nilai);
    } else if (panjang == 2) {
        uint32 nilai;
        uint16 data;
        salin_memori(&data, buffer, 2);
        nilai = pengembang_kendali_baca_register(
                    antarmuka->perangkat, offset & 0xFC);
        nilai &= ~(0xFFFF << ((offset & 0x03) * 8));
        nilai |= ((uint32)data << ((offset & 0x03) * 8));
        pengembang_kendali_tulis_register(
            antarmuka->perangkat, offset & 0xFC, nilai);
    } else if (panjang == 1) {
        uint32 nilai;
        uint8 data;
        salin_memori(&data, buffer, 1);
        nilai = pengembang_kendali_baca_register(
                    antarmuka->perangkat, offset & 0xFC);
        nilai &= ~(0xFF << ((offset & 0x03) * 8));
        nilai |= ((uint32)data << ((offset & 0x03) * 8));
        pengembang_kendali_tulis_register(
            antarmuka->perangkat, offset & 0xFC, nilai);
    } else {
        /* Tulis multi-register */
        ukuran_t i;
        for (i = 0; i < panjang; i += 4) {
            uint32 nilai;
            ukuran_t sisa;
            ukuran_t salin;

            nilai = 0;
            sisa = panjang - i;
            salin = (sisa < 4) ? sisa : 4;
            salin_memori(&nilai, buffer + i, salin);
            pengembang_kendali_tulis_register(
                antarmuka->perangkat,
                offset + (uint32)i, nilai);
        }
    }

    return (int)panjang;
}

/* ================================================================
 * pengembang_antarmuka_ioctl — Operasi kendali khusus perangkat
 *
 * Menyediakan operasi khusus seperti reset, atur parameter,
 * diagnostik, dan identifikasi. Kode perintah ioctl ditentukan
 * oleh konstanta IOCTL_* di atas.
 *
 * Parameter:
 *   antarmuka — pointer ke AntarmukaPerangkat
 *   perintah  — kode perintah ioctl
 *   argumen   — argumen perintah (tergantung perintah)
 *
 * Mengembalikan:
 *   STATUS_OK jika berhasil,
 *   STATUS_TIDAK_DIKENAL jika perintah tidak dikenali,
 *   STATUS_PARAMETAR_SALAH jika parameter NULL
 * ================================================================ */
int pengembang_antarmuka_ioctl(AntarmukaPerangkat *antarmuka,
                                uint32 perintah,
                                uint32 argumen)
{
    if (antarmuka == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    switch (perintah) {
    case IOCTL_RESET:
        /* Reset IC */
        if (antarmuka->ic != NULL && antarmuka->perangkat != NULL) {
            return pengembang_kendali_reset_ic(
                antarmuka->ic, antarmuka->perangkat);
        }
        return STATUS_GAGAL;

    case IOCTL_SET_PARAMETER:
        /* Atur parameter IC ke nilai tertentu */
        if (antarmuka->ic != NULL && antarmuka->perangkat != NULL) {
            return pengembang_kendali_set_parameter(
                antarmuka->ic, antarmuka->perangkat);
        }
        return STATUS_GAGAL;

    case IOCTL_GET_PARAMETER:
        /* Baca parameter aktif — kembalikan melalui argumen */
        if (antarmuka->ic != NULL && argumen < 8) {
            return (int)antarmuka->ic->parameter_optimal[argumen];
        }
        return STATUS_PARAMETAR_SALAH;

    case IOCTL_DIAGNOSTIK:
        /* Jalankan diagnostik IC */
        if (antarmuka->ic != NULL && antarmuka->perangkat != NULL) {
            return pengembang_kendali_diagnostik(
                antarmuka->ic, antarmuka->perangkat);
        }
        return STATUS_GAGAL;

    case IOCTL_SET_KECEPATAN:
        /* Atur kecepatan (untuk jaringan/serial) */
        if (antarmuka->ic != NULL &&
            antarmuka->ic->jumlah_parameter > 0) {
            if (argumen <= antarmuka->ic->parameter_maks[0]) {
                pengembang_kendali_tulis_register(
                    antarmuka->perangkat,
                    antarmuka->alamat_basis, argumen);
                return STATUS_OK;
            }
            /* Kecepatan melebihi maksimum — gunakan optimal */
            pengembang_kendali_tulis_register(
                antarmuka->perangkat,
                antarmuka->alamat_basis,
                antarmuka->ic->parameter_optimal[0]);
            return STATUS_OK;
        }
        return STATUS_GAGAL;

    case IOCTL_GET_IDENTITAS:
        /* Kembalikan identitas IC (vendor_id | perangkat_id << 16) */
        if (antarmuka->ic != NULL) {
            return (int)((uint32)antarmuka->ic->vendor_id |
                         ((uint32)antarmuka->ic->perangkat_id << 16));
        }
        return STATUS_GAGAL;

    default:
        return STATUS_TIDAK_DIKENAL;
    }
}

/* ================================================================
 * pengembang_antarmuka_status — Dapatkan status perangkat
 *
 * Memeriksa kondisi perangkat melalui peninjau dan
 * mengembalikan status terkini antarmuka.
 *
 * Parameter:
 *   antarmuka — pointer ke AntarmukaPerangkat
 *
 * Mengembalikan:
 *   KondisiPerangkat (KONDISI_BAIK, KONDISI_RUSAK, dll.)
 *   KONDISI_TIDAK_DIKENAL jika parameter NULL
 * ================================================================ */
KondisiPerangkat pengembang_antarmuka_status(
    AntarmukaPerangkat *antarmuka)
{
    if (antarmuka == NULL) {
        return KONDISI_TIDAK_DIKENAL;
    }

    /* Jika antarmuka belum dibuka, kembalikan kondisi dari data */
    if (antarmuka->perangkat == NULL) {
        return KONDISI_TERPUTUS;
    }

    /* Verifikasi perangkat masih terhubung */
    if (antarmuka->jumlah_buka > 0) {
        KondisiPerangkat kondisi;
        kondisi = peninjau_verifikasi(antarmuka->perangkat);
        antarmuka->kondisi = kondisi;
        return kondisi;
    }

    /* Kembalikan kondisi yang tersimpan */
    return antarmuka->kondisi;
}

/* ================================================================
 * pengembang_antarmuka_buka — Buka perangkat untuk digunakan
 *
 * Menandai antarmuka sebagai aktif dan menyiapkan akses.
 * Jika antarmuka sudah dibuka, tingkatkan jumlah_buka
 * (mendukung pembukaan berulang dari lapisan berbeda).
 *
 * Parameter:
 *   antarmuka — pointer ke AntarmukaPerangkat
 *
 * Mengembalikan:
 *   STATUS_OK jika berhasil,
 *   STATUS_SIBUK jika antarmuka sedang digunakan (terkunci),
 *   STATUS_PARAMETAR_SALAH jika parameter NULL
 * ================================================================ */
int pengembang_antarmuka_buka(AntarmukaPerangkat *antarmuka)
{
    if (antarmuka == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* Cek apakah antarmuka terkunci */
    if (antarmuka->terkunci == ANTARMUKA_TERKUNCI) {
        return STATUS_SIBUK;
    }

    /* Cek kondisi perangkat */
    if (antarmuka->kondisi == KONDISI_RUSAK) {
        notulen_catat(NOTULEN_KESALAHAN,
            "Antarmuka: Perangkat rusak, tidak dapat dibuka");
        return STATUS_GAGAL;
    }

    /* Tingkatkan jumlah buka */
    antarmuka->jumlah_buka++;

    /* Terapkan parameter optimal saat pertama kali dibuka */
    if (antarmuka->jumlah_buka == 1 && antarmuka->ic != NULL) {
        ic_isi_parameter_optimal(antarmuka->ic, antarmuka->perangkat);
    }

    /* Kunci antarmuka selama digunakan */
    antarmuka->terkunci = ANTARMUKA_TERKUNCI;
    antarmuka->kondisi = KONDISI_BAIK;

    return STATUS_OK;
}

/* ================================================================
 * pengembang_antarmuka_tutup — Tutup perangkat
 *
 * Menandai antarmuka sebagai tidak aktif jika semua
 * pengguna telah menutupnya. Jumlah buka dikurangi;
 * jika mencapai nol, antarmuka dilepas kuncinya.
 *
 * Parameter:
 *   antarmuka — pointer ke AntarmukaPerangkat
 *
 * Mengembalikan:
 *   STATUS_OK jika berhasil,
 *   STATUS_PARAMETAR_SALAH jika parameter NULL
 * ================================================================ */
int pengembang_antarmuka_tutup(AntarmukaPerangkat *antarmuka)
{
    if (antarmuka == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* Jika belum pernah dibuka, tidak perlu ditutup */
    if (antarmuka->jumlah_buka == 0) {
        return STATUS_OK;
    }

    /* Kurangi jumlah buka */
    antarmuka->jumlah_buka--;

    /* Jika tidak ada lagi yang membuka, lepaskan kunci */
    if (antarmuka->jumlah_buka == 0) {
        antarmuka->terkunci = ANTARMUKA_BEBAS;
        antarmuka->kondisi = KONDISI_SIAGA;
    }

    return STATUS_OK;
}

/* ================================================================
 * pengembang_antarmuka_daftar — Daftar semua antarmuka
 *
 * Mengisi array dengan struktur AntarmukaPerangkat yang
 * mewakili semua antarmuka perangkat yang tersedia.
 *
 * Parameter:
 *   daftar — array penampung AntarmukaPerangkat
 *   maks   — jumlah maksimum entri yang dapat ditampung
 *
 * Mengembalikan:
 *   Jumlah antarmuka yang terdaftar (positif),
 *   STATUS_PARAMETAR_SALAH jika daftar NULL
 * ================================================================ */
int pengembang_antarmuka_daftar(AntarmukaPerangkat daftar[], int maks)
{
    int i;
    int jumlah;

    if (daftar == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* Tentukan jumlah yang akan disalin */
    jumlah = (jumlah_antarmuka < maks) ? jumlah_antarmuka : maks;

    /* Salin setiap entri antarmuka */
    for (i = 0; i < jumlah; i++) {
        daftar[i] = tabel_antarmuka[i];
    }

    return jumlah;
}
