/*
 * antarmuka.c
 *
 * Pembangunan antarmuka untuk lapisan atas (OS/Kontraktor).
 * Menyediakan API terstandar untuk mengakses perangkat
 * melalui abstraksi yang tidak bergantung pada detail hardware.
 *
 * Setiap antarmuka dikaitkan dengan satu IC chip dan
 * menyediakan operasi baca/tulis/ioctl/status yang generik.
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"
#include "../lampiran/peninjau.h"
#include "../lampiran/pengembang.h"

/* ================================================================
 * KONSTANTA ANTARMUKA
 * ================================================================ */

/* Jumlah maksimum antarmuka yang dapat dibuat */
#define ANTARMUKA_MAKSIMUM    64

/* Status antarmuka */
#define ANTARMUKA_STATUS_TUTUP    0
#define ANTARMUKA_STATUS_BUKA     1
#define ANTARMUKA_STATUS_SIBUK    2
#define ANTARMUKA_STATUS_KESALAHAN 3

/* Kode ioctl umum */
#define IOCTL_RESET              0x0001
#define IOCTL_SET_PARAMETER      0x0002
#define IOCTL_GET_PARAMETER      0x0003
#define IOCTL_DIAGNOSTIK         0x0004
#define IOCTL_SET_KECEPATAN      0x0005
#define IOCTL_GET_IDENTITAS      0x0006

/* ================================================================
 * STRUKTUR ANTARMUKA
 * ================================================================ */

/*
 * Struktur AntarmukaPerangkat — merepresentasikan satu
 * antarmuka perangkat yang dapat diakses oleh lapisan atas.
 */
typedef struct {
    uint32           nomor;           /* Nomor urut antarmuka */
    DataPerangkat   *perangkat;       /* Pointer ke data perangkat */
    DataIC          *ic;              /* Pointer ke data IC */
    uint8            status;          /* Status antarmuka */
    uint8            tipe_fungsi;     /* Tipe fungsi IC (dari DataIC) */
    uint16           jumlah_buka;     /* Berapa kali antarmuka dibuka */
    ukuran_ptr       alamat_register; /* Alamat basis register IC */
    uint32           interupsi;       /* Nomor interupsi */
    uint32           parameter_aktif[8]; /* Parameter yang sedang aktif */
} AntarmukaPerangkat;

/* ================================================================
 * DATA INTERNAL
 * ================================================================ */

/* Tabel antarmuka yang sudah dibuat */
static AntarmukaPerangkat tabel_antarmuka[ANTARMUKA_MAKSIMUM];
static int jumlah_antarmuka = 0;

/* ================================================================
 * FUNGSI PEMBUATAN ANTARMUKA
 * ================================================================ */

/*
 * pengembang_antarmuka_buat — Buat antarmuka untuk perangkat.
 * Menghubungkan perangkat dengan IC yang sudah diidentifikasi
 * dan menyiapkan struktur antarmuka untuk akses lapisan atas.
 *
 * Parameter:
 *   perangkat — pointer ke struktur DataPerangkat
 *   ic        — pointer ke struktur DataIC yang sudah diidentifikasi
 * Mengembalikan nomor antarmuka (>= 0) jika berhasil, STATUS_GAGAL jika gagal.
 */
int pengembang_antarmuka_buat(DataPerangkat *perangkat, DataIC *ic)
{
    AntarmukaPerangkat *ant;
    int i;

    if (perangkat == NULL || ic == NULL) return STATUS_PARAMETAR_SALAH;
    if (jumlah_antarmuka >= ANTARMUKA_MAKSIMUM) return STATUS_GAGAL;

    ant = &tabel_antarmuka[jumlah_antarmuka];

    /* Isi struktur antarmuka */
    isi_memori(ant, 0, sizeof(AntarmukaPerangkat));
    ant->nomor = (uint32)jumlah_antarmuka;
    ant->perangkat = perangkat;
    ant->ic = ic;
    ant->status = ANTARMUKA_STATUS_TUTUP;
    ant->tipe_fungsi = (uint8)ic->tipe_fungsi;
    ant->jumlah_buka = 0;
    ant->alamat_register = perangkat->alamat;
    ant->interupsi = perangkat->interupsi;

    /* Salin parameter optimal dari IC ke parameter aktif */
    for (i = 0; i < ic->jumlah_parameter && i < 8; i++) {
        ant->parameter_aktif[i] = ic->parameter_optimal[i];
    }

    jumlah_antarmuka++;

    /* Catat pembuatan antarmuka */
    {
        char pesan[128];
        salin_string(pesan, "Antarmuka: Dibuat untuk IC ");
        salin_string(pesan + panjang_string(pesan), ic->nama_ic);
        notulen_catat(NOTULEN_INFO, pesan);
    }

    return (int)ant->nomor;
}

/* ================================================================
 * FUNGSI BACA/TULIS ANTARMUKA
 * ================================================================ */

/*
 * pengembang_antarmuka_baca — Baca data dari perangkat.
 * Melakukan operasi baca melalui register IC.
 *
 * Parameter:
 *   nomor      — nomor antarmuka
 *   offset     — offset register atau alamat baca
 *   buffer     — buffer penampung data
 *   ukuran     — jumlah byte yang dibaca
 * Mengembalikan jumlah byte yang berhasil dibaca, atau STATUS_GAGAL.
 */
int pengembang_antarmuka_baca(unsigned int nomor, uint32 offset,
                               void *buffer, ukuran_t ukuran)
{
    AntarmukaPerangkat *ant;

    if (nomor >= (unsigned int)jumlah_antarmuka) return STATUS_TIDAK_ADA;
    if (buffer == NULL) return STATUS_PARAMETAR_SALAH;

    ant = &tabel_antarmuka[nomor];

    if (ant->status == ANTARMUKA_STATUS_TUTUP) return STATUS_GAGAL;

    /* Baca dari register IC */
    if (ukuran == 4) {
        uint32 nilai;
        nilai = pengembang_kendali_baca_register(ant->perangkat, offset);
        salin_memori(buffer, &nilai, 4);
    } else if (ukuran == 2) {
        uint32 nilai;
        nilai = pengembang_kendali_baca_register(ant->perangkat, offset & 0xFC);
        nilai = (nilai >> ((offset & 0x03) * 8)) & 0xFFFF;
        salin_memori(buffer, &nilai, 2);
    } else if (ukuran == 1) {
        uint32 nilai;
        nilai = pengembang_kendali_baca_register(ant->perangkat, offset & 0xFC);
        nilai = (nilai >> ((offset & 0x03) * 8)) & 0xFF;
        salin_memori(buffer, &nilai, 1);
    } else {
        /* Baca multi-register */
        ukuran_t i;
        for (i = 0; i < ukuran; i += 4) {
            uint32 nilai;
            nilai = pengembang_kendali_baca_register(ant->perangkat, offset + i);
            salin_memori((uint8 *)buffer + i, &nilai,
                         MIN(4, (ukuran_t)(ukuran - i)));
        }
    }

    return (int)ukuran;
}

/*
 * pengembang_antarmuka_tulis — Tulis data ke perangkat.
 * Melakukan operasi tulis melalui register IC.
 *
 * Parameter:
 *   nomor      — nomor antarmuka
 *   offset     — offset register atau alamat tulis
 *   buffer     — buffer data yang akan ditulis
 *   ukuran     — jumlah byte yang ditulis
 * Mengembalikan jumlah byte yang berhasil ditulis, atau STATUS_GAGAL.
 */
int pengembang_antarmuka_tulis(unsigned int nomor, uint32 offset,
                                const void *buffer, ukuran_t ukuran)
{
    AntarmukaPerangkat *ant;

    if (nomor >= (unsigned int)jumlah_antarmuka) return STATUS_TIDAK_ADA;
    if (buffer == NULL) return STATUS_PARAMETAR_SALAH;

    ant = &tabel_antarmuka[nomor];

    if (ant->status == ANTARMUKA_STATUS_TUTUP) return STATUS_GAGAL;

    /* Tulis ke register IC */
    if (ukuran == 4) {
        uint32 nilai;
        salin_memori(&nilai, buffer, 4);
        pengembang_kendali_tulis_register(ant->perangkat, offset, nilai);
    } else if (ukuran == 2) {
        uint32 nilai;
        uint16 data;
        salin_memori(&data, buffer, 2);
        nilai = pengembang_kendali_baca_register(ant->perangkat, offset & 0xFC);
        nilai &= ~(0xFFFF << ((offset & 0x03) * 8));
        nilai |= ((uint32)data << ((offset & 0x03) * 8));
        pengembang_kendali_tulis_register(ant->perangkat, offset & 0xFC, nilai);
    } else if (ukuran == 1) {
        uint32 nilai;
        uint8 data;
        salin_memori(&data, buffer, 1);
        nilai = pengembang_kendali_baca_register(ant->perangkat, offset & 0xFC);
        nilai &= ~(0xFF << ((offset & 0x03) * 8));
        nilai |= ((uint32)data << ((offset & 0x03) * 8));
        pengembang_kendali_tulis_register(ant->perangkat, offset & 0xFC, nilai);
    } else {
        /* Tulis multi-register */
        ukuran_t i;
        for (i = 0; i < ukuran; i += 4) {
            uint32 nilai = 0;
            salin_memori(&nilai, (const uint8 *)buffer + i,
                         MIN(4, (ukuran_t)(ukuran - i)));
            pengembang_kendali_tulis_register(ant->perangkat, offset + i, nilai);
        }
    }

    return (int)ukuran;
}

/* ================================================================
 * FUNGSI IOCTL
 * ================================================================ */

/*
 * pengembang_antarmuka_ioctl — Operasi kontrol perangkat.
 * Menyediakan operasi khusus seperti reset, set parameter,
 * diagnostik, dan identifikasi.
 *
 * Parameter:
 *   nomor      — nomor antarmuka
 *   perintah   — kode perintah ioctl
 *   argumen    — argumen perintah (tergantung perintah)
 * Mengembalikan STATUS_OK jika berhasil.
 */
int pengembang_antarmuka_ioctl(unsigned int nomor, uint32 perintah,
                                uint32 argumen)
{
    AntarmukaPerangkat *ant;

    if (nomor >= (unsigned int)jumlah_antarmuka) return STATUS_TIDAK_ADA;

    ant = &tabel_antarmuka[nomor];

    switch (perintah) {
    case IOCTL_RESET:
        /* Reset IC */
        if (ant->ic != NULL) {
            return pengembang_kendali_reset_ic(ant->perangkat, ant->ic);
        }
        break;

    case IOCTL_SET_PARAMETER:
        /* Set parameter IC ke nilai tertentu */
        {
            int nomor_param = (int)((argumen >> 16) & 0xFF);
            uint32 nilai = argumen & 0xFFFF;
            if (ant->ic != NULL) {
                int hasil;
                hasil = pengembang_kendali_set_parameter(ant->ic, nomor_param, nilai);
                if (hasil == STATUS_OK && nomor_param < 8) {
                    ant->parameter_aktif[nomor_param] = nilai;
                }
                return hasil;
            }
        }
        break;

    case IOCTL_GET_PARAMETER:
        /* Baca parameter aktif */
        {
            int nomor_param = (int)(argumen & 0xFF);
            if (nomor_param < 8) {
                return (int)ant->parameter_aktif[nomor_param];
            }
        }
        return STATUS_PARAMETAR_SALAH;

    case IOCTL_DIAGNOSTIK:
        /* Jalankan diagnostik IC */
        if (ant->ic != NULL) {
            return pengembang_kendali_diagnostik(ant->perangkat, ant->ic);
        }
        break;

    case IOCTL_SET_KECEPATAN:
        /* Atur kecepatan (untuk jaringan/serial) */
        if (ant->ic != NULL && ant->ic->jumlah_parameter > 0) {
            if (argumen <= ant->ic->parameter_maks[0]) {
                ant->parameter_aktif[0] = argumen;
                return STATUS_OK;
            }
            /* Kecepatan melebihi maksimum — gunakan optimal */
            ant->parameter_aktif[0] = ant->ic->parameter_optimal[0];
            return STATUS_OK;
        }
        break;

    case IOCTL_GET_IDENTITAS:
        /* Kembalikan identitas IC */
        if (ant->ic != NULL) {
            return (int)((uint32)ant->ic->vendor_id |
                         ((uint32)ant->ic->perangkat_id << 16));
        }
        break;

    default:
        return STATUS_TIDAK_DIKENAL;
    }

    return STATUS_OK;
}

/* ================================================================
 * FUNGSI STATUS
 * ================================================================ */

/*
 * pengembang_antarmuka_status — Dapatkan status perangkat.
 * Mengembalikan kondisi terkini perangkat melalui antarmuka.
 *
 * Parameter:
 *   nomor — nomor antarmuka
 * Mengembalikan kode status (KONDISI_BAIK, dll.)
 */
int pengembang_antarmuka_status(unsigned int nomor)
{
    AntarmukaPerangkat *ant;

    if (nomor >= (unsigned int)jumlah_antarmuka) return STATUS_TIDAK_ADA;

    ant = &tabel_antarmuka[nomor];

    /* Verifikasi perangkat masih terhubung */
    if (ant->perangkat != NULL) {
        KondisiPerangkat kondisi;
        kondisi = peninjau_verifikasi(ant->perangkat);
        return (int)kondisi;
    }

    return (int)KONDISI_TIDAK_DIKENAL;
}

/* ================================================================
 * FUNGSI BUKA/TUTUP
 * ================================================================ */

/*
 * pengembang_antarmuka_buka — Buka antarmuka perangkat.
 * Menandai antarmuka sebagai aktif dan menyiapkan akses.
 *
 * Parameter:
 *   nomor — nomor antarmuka
 * Mengembalikan STATUS_OK jika berhasil.
 */
int pengembang_antarmuka_buka(unsigned int nomor)
{
    AntarmukaPerangkat *ant;

    if (nomor >= (unsigned int)jumlah_antarmuka) return STATUS_TIDAK_ADA;

    ant = &tabel_antarmuka[nomor];

    if (ant->status == ANTARMUKA_STATUS_BUKA) {
        /* Sudah dibuka — tingkatkan jumlah buka */
        ant->jumlah_buka++;
        return STATUS_OK;
    }

    if (ant->status == ANTARMUKA_STATUS_SIBUK) {
        return STATUS_SIBUK;
    }

    if (ant->status == ANTARMUKA_STATUS_KESALAHAN) {
        return STATUS_GAGAL;
    }

    /* Buka antarmuka */
    ant->status = ANTARMUKA_STATUS_BUKA;
    ant->jumlah_buka = 1;

    /* Set parameter ke nilai optimal saat pertama kali dibuka */
    if (ant->ic != NULL) {
        int i;
        for (i = 0; i < ant->ic->jumlah_parameter && i < 8; i++) {
            ant->parameter_aktif[i] = ant->ic->parameter_optimal[i];
        }
    }

    return STATUS_OK;
}

/*
 * pengembang_antarmuka_tutup — Tutup antarmuka perangkat.
 * Menandai antarmuka sebagai tidak aktif jika semua
 * pengguna telah menutupnya.
 *
 * Parameter:
 *   nomor — nomor antarmuka
 * Mengembalikan STATUS_OK jika berhasil.
 */
int pengembang_antarmuka_tutup(unsigned int nomor)
{
    AntarmukaPerangkat *ant;

    if (nomor >= (unsigned int)jumlah_antarmuka) return STATUS_TIDAK_ADA;

    ant = &tabel_antarmuka[nomor];

    if (ant->status != ANTARMUKA_STATUS_BUKA) {
        return STATUS_OK;    /* Sudah ditutup */
    }

    if (ant->jumlah_buka > 1) {
        ant->jumlah_buka--;
        return STATUS_OK;
    }

    /* Tutup antarmuka */
    ant->status = ANTARMUKA_STATUS_TUTUP;
    ant->jumlah_buka = 0;

    return STATUS_OK;
}

/* ================================================================
 * FUNGSI DAFTAR
 * ================================================================ */

/*
 * pengembang_antarmuka_daftar — Daftar semua antarmuka.
 * Mengisi array dengan nomor antarmuka yang tersedia.
 *
 * Parameter:
 *   daftar — array penampung nomor antarmuka
 *   maks   — jumlah maksimum entri
 * Mengembalikan jumlah antarmuka yang terdaftar.
 */
int pengembang_antarmuka_daftar(uint32 daftar[], int maks)
{
    int i;
    int jumlah = (jumlah_antarmuka < maks) ? jumlah_antarmuka : maks;

    for (i = 0; i < jumlah; i++) {
        daftar[i] = tabel_antarmuka[i].nomor;
    }

    return jumlah;
}
