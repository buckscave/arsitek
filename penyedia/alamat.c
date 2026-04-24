/*
 * alamat.c
 *
 * Penyediaan alamat memori fisik dan I/O.
 * Mengelola peta alamat perangkat dan alokasi region
 * memori untuk pengendali (driver), termasuk pemetaan
 * ruang alamat perangkat MMIO.
 *
 * Fungsi utama:
 *   penyedia_alamat_cari_bebas()      - cari region alamat bebas
 *   penyedia_alamat_alokasi_region()  - alokasi region alamat berurutan
 *   penyedia_alamat_bebaskan_region() - bebaskan region alamat
 *   penyedia_alamat_peta_perangkat()  - petakan ruang alamat perangkat MMIO
 *   penyedia_alamat_daftar_mmio()     - daftar region MMIO yang terpetakan
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"
#include "../lampiran/penyedia.h"

/* ================================================================
 * KONSTANTA INTERNAL
 * ================================================================ */

/* Jumlah bit dalam satu elemen bitmap (sizeof(uint32) * 8) */
#define BIT_PER_ELEMEN     32

/* Jumlah maksimum region yang dapat dilacak */
#define REGION_MAKSIMUM   64

/* Jumlah maksimum region MMIO yang dapat dipetakan */
#define MMIO_MAKSIMUM     32

/* Jumlah elemen bitmap untuk melacak alamat (4096 * 32 = 131072 bit) */
/* Setiap bit mewakili satu halaman 4KB, jadi total 512 MB terlacak */
#define BITMAP_ELEMEN     4096

/* ================================================================
 * STRUKTUR DATA INTERNAL
 * ================================================================ */

/* Satu region alamat yang dialokasikan */
typedef struct {
    ukuran_ptr alamat_mulai;   /* Alamat awal region */
    ukuran_t   ukuran;         /* Ukuran region dalam byte */
    uint32     nomor_perangkat; /* Nomor perangkat pemilik */
    logika     terpakai;       /* BENAR jika slot ini terpakai */
    uint8      tipe_region;    /* 0=memori umum, 1=MMIO, 2=I/O */
} RegionAlamat;

/* Satu region MMIO yang terpetakan */
typedef struct {
    ukuran_ptr alamat_fisik;   /* Alamat fisik perangkat */
    ukuran_ptr alamat_virtual; /* Alamat virtual yang dipetakan */
    ukuran_t   ukuran;         /* Ukuran region MMIO */
    uint32     nomor_perangkat; /* Nomor perangkat pemilik */
    char       nama[32];       /* Nama perangkat MMIO */
    logika     terpakai;       /* BENAR jika slot ini terpakai */
} RegionMMIO;

/* ================================================================
 * DATA INTERNAL
 * ================================================================ */

/* Bitmap alamat — setiap bit mewakili satu halaman 4KB */
static uint32 bitmap_alamat[BITMAP_ELEMEN];

/* Tabel region yang dialokasikan */
static RegionAlamat tabel_region[REGION_MAKSIMUM];

/* Tabel region MMIO yang terpetakan */
static RegionMMIO tabel_mmio[MMIO_MAKSIMUM];

/* Jumlah region yang sedang terpakai */
static int jumlah_region_terpakai = 0;

/* Jumlah region MMIO yang terpetakan */
static int jumlah_mmio_terpakai = 0;

/* Alamat dasar untuk alokasi — mulai setelah kernel */
static ukuran_ptr alamat_dasar_alokasi;

/* ================================================================
 * FUNGSI BANTUAN INTERNAL
 * ================================================================ */

/*
 * Inisialisasi modul alamat.
 * Menandai region yang sudah dipakai (kernel, VGA, dll)
 * dalam bitmap sehingga tidak dialokasikan ulang.
 */
static void inisialisasi_alamat(void)
{
    ukuran_t halaman_kernel;
    ukuran_t halaman_vga;
    ukuran_t i;

    /* Bersihkan bitmap */
    isi_memori(bitmap_alamat, 0, sizeof(bitmap_alamat));

    /* Bersihkan tabel region dan MMIO */
    isi_memori(tabel_region, 0, sizeof(tabel_region));
    isi_memori(tabel_mmio, 0, sizeof(tabel_mmio));

    /* Tentukan alamat dasar alokasi berdasarkan arsitektur */
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    alamat_dasar_alokasi = 0x00200000; /* Mulai setelah 2 MB */
#elif defined(ARSITEK_ARM32)
    alamat_dasar_alokasi = 0x00200000;
#else
    alamat_dasar_alokasi = 0x00200000;
#endif

    /* Tandai halaman kernel sebagai terpakai */
    halaman_kernel = KERNEL_ALAMAT_DASAR / HALAMAN_4KB;
    /* Kernel biasanya menggunakan sekitar 1 MB — tandai 256 halaman */
    for (i = 0; i < 256; i++) {
        uint32 idx = (uint32)(halaman_kernel + i) / BIT_PER_ELEMEN;
        uint32 bit = (uint32)(halaman_kernel + i) % BIT_PER_ELEMEN;
        if (idx < BITMAP_ELEMEN) {
            BIT_SET(bitmap_alamat[idx], bit);
        }
    }

    /* Tandai halaman VGA sebagai terpakai (0xB8000 - 0xBFFFF) */
    halaman_vga = VGA_ALAMAT / HALAMAN_4KB;
    for (i = 0; i < 8; i++) {
        uint32 idx = (uint32)(halaman_vga + i) / BIT_PER_ELEMEN;
        uint32 bit = (uint32)(halaman_vga + i) % BIT_PER_ELEMEN;
        if (idx < BITMAP_ELEMEN) {
            BIT_SET(bitmap_alamat[idx], bit);
        }
    }

    jumlah_region_terpakai = 0;
    jumlah_mmio_terpakai = 0;
}

/*
 * Tandai bit dalam bitmap sebagai terpakai (1).
 * Parameter halaman: nomor halaman awal
 * Parameter jumlah: jumlah halaman yang ditandai
 */
static void tandai_terpakai(ukuran_t halaman, ukuran_t jumlah)
{
    ukuran_t i;
    for (i = 0; i < jumlah; i++) {
        uint32 idx = (uint32)(halaman + i) / BIT_PER_ELEMEN;
        uint32 bit = (uint32)(halaman + i) % BIT_PER_ELEMEN;
        if (idx < BITMAP_ELEMEN) {
            BIT_SET(bitmap_alamat[idx], bit);
        }
    }
}

/*
 * Tandai bit dalam bitmap sebagai bebas (0).
 * Parameter halaman: nomor halaman awal
 * Parameter jumlah: jumlah halaman yang dibebaskan
 */
static void tandai_bebas(ukuran_t halaman, ukuran_t jumlah)
{
    ukuran_t i;
    for (i = 0; i < jumlah; i++) {
        uint32 idx = (uint32)(halaman + i) / BIT_PER_ELEMEN;
        uint32 bit = (uint32)(halaman + i) % BIT_PER_ELEMEN;
        if (idx < BITMAP_ELEMEN) {
            BIT_HAPUS(bitmap_alamat[idx], bit);
        }
    }
}

/*
 * Periksa apakah halaman tertentu bebas (0) dalam bitmap.
 */
static logika halaman_bebas(ukuran_t halaman)
{
    uint32 idx = (uint32)halaman / BIT_PER_ELEMEN;
    uint32 bit = (uint32)halaman % BIT_PER_ELEMEN;
    if (idx >= BITMAP_ELEMEN) return SALAH;
    return !BIT_CEK(bitmap_alamat[idx], bit);
}

/*
 * Cari slot kosong dalam tabel region.
 * Mengembalikan indeks slot, atau -1 jika penuh.
 */
static int cari_slot_region(void)
{
    int i;
    for (i = 0; i < REGION_MAKSIMUM; i++) {
        if (!tabel_region[i].terpakai) {
            return i;
        }
    }
    return -1;
}

/*
 * Cari slot kosong dalam tabel MMIO.
 * Mengembalikan indeks slot, atau -1 jika penuh.
 */
static int cari_slot_mmio(void)
{
    int i;
    for (i = 0; i < MMIO_MAKSIMUM; i++) {
        if (!tabel_mmio[i].terpakai) {
            return i;
        }
    }
    return -1;
}

/*
 * Cari region berdasarkan alamat awal.
 * Mengembalikan indeks region, atau -1 jika tidak ditemukan.
 */
static int cari_region_berdasarkan_alamat(ukuran_ptr alamat)
{
    int i;
    for (i = 0; i < REGION_MAKSIMUM; i++) {
        if (tabel_region[i].terpakai &&
            tabel_region[i].alamat_mulai == alamat) {
            return i;
        }
    }
    return -1;
}

/* ================================================================
 * FUNGSI PUBLIK
 * ================================================================ */

/*
 * penyedia_alamat_cari_bebas
 *
 * Mencari region alamat bebas dalam bitmap yang cukup besar
 * untuk menampung jumlah halaman yang diminta.
 *
 * Parameter:
 *   jumlah_halaman - jumlah halaman 4KB yang dibutuhkan
 *   alamat_mulai   - batas bawah alamat pencarian (0 = gunakan default)
 *
 * Mengembalikan: alamat awal region bebas, atau 0 jika tidak ditemukan.
 */
ukuran_ptr penyedia_alamat_cari_bebas(ukuran_t jumlah_halaman,
                                       ukuran_ptr alamat_mulai)
{
    ukuran_t halaman_mulai;
    ukuran_t halaman_maks;
    ukuran_t ditemukan_berurutan;
    ukuran_t i;
    ukuran_t halaman_akhir;

    /* Inisialisasi jika belum */
    if (jumlah_region_terpakai == 0 && jumlah_mmio_terpakai == 0) {
        inisialisasi_alamat();
    }

    if (jumlah_halaman == 0) return 0;

    /* Tentukan batas pencarian */
    if (alamat_mulai != 0) {
        halaman_mulai = (ukuran_t)alamat_mulai / HALAMAN_4KB;
    } else {
        halaman_mulai = (ukuran_t)alamat_dasar_alokasi / HALAMAN_4KB;
    }

    halaman_maks = (ukuran_t)BITMAP_ELEMEN * BIT_PER_ELEMEN;

    /* Cari region berurutan yang bebas */
    ditemukan_berurutan = 0;
    for (i = halaman_mulai; i < halaman_maks; i++) {
        if (halaman_bebas(i)) {
            ditemukan_berurutan++;
            if (ditemukan_berurutan >= jumlah_halaman) {
                /* Ditemukan — kembalikan alamat awal region */
                halaman_akhir = i + 1;
                return (ukuran_ptr)((halaman_akhir - jumlah_halaman) * HALAMAN_4KB);
            }
        } else {
            ditemukan_berurutan = 0;
        }
    }

    /* Tidak ditemukan region yang cukup besar */
    notulen_catat(NOTULEN_PERINGATAN,
                  "Penyedia: Tidak ada region alamat bebas yang cukup");
    return 0;
}

/*
 * penyedia_alamat_alokasi_region
 *
 * Mengalokasikan region alamat berurutan dari bitmap.
 * Region yang dialokasikan dicatat dalam tabel region.
 *
 * Parameter:
 *   ukuran          - ukuran region dalam byte (akan dibulatkan ke halaman)
 *   nomor_perangkat - nomor perangkat yang meminta alokasi
 *   tipe_region     - tipe region: 0=memori umum, 1=MMIO, 2=I/O
 *
 * Mengembalikan: alamat awal region yang dialokasikan, atau 0 jika gagal.
 */
ukuran_ptr penyedia_alamat_alokasi_region(ukuran_t ukuran,
                                           uint32 nomor_perangkat,
                                           uint8 tipe_region)
{
    ukuran_t jumlah_halaman;
    ukuran_ptr alamat_hasil;
    int slot;

    /* Inisialisasi jika belum */
    if (jumlah_region_terpakai == 0 && jumlah_mmio_terpakai == 0) {
        inisialisasi_alamat();
    }

    if (ukuran == 0) return 0;

    /* Bulatkan ukuran ke atas ke kelipatan halaman */
    jumlah_halaman = ukuran / HALAMAN_4KB;
    if (ukuran % HALAMAN_4KB != 0) {
        jumlah_halaman++;
    }

    /* Cari region bebas */
    alamat_hasil = penyedia_alamat_cari_bebas(jumlah_halaman, 0);
    if (alamat_hasil == 0) return 0;

    /* Cari slot dalam tabel region */
    slot = cari_slot_region();
    if (slot < 0) {
        notulen_catat(NOTULEN_KESALAHAN,
                      "Penyedia: Tabel region penuh, tidak dapat mengalokasi");
        return 0;
    }

    /* Tandai halaman sebagai terpakai dalam bitmap */
    tandai_terpakai((ukuran_t)alamat_hasil / HALAMAN_4KB, jumlah_halaman);

    /* Catat dalam tabel region */
    tabel_region[slot].alamat_mulai = alamat_hasil;
    tabel_region[slot].ukuran = jumlah_halaman * HALAMAN_4KB;
    tabel_region[slot].nomor_perangkat = nomor_perangkat;
    tabel_region[slot].tipe_region = tipe_region;
    tabel_region[slot].terpakai = BENAR;

    jumlah_region_terpakai++;

    {
        char pesan[80];
        salin_string(pesan, "Penyedia: Alokasi region di 0x");
        konversi_uint_ke_string((uint32)alamat_hasil,
                                pesan + panjang_string(pesan), 16);
        salin_string(pesan + panjang_string(pesan), " sebesar ");
        konversi_uint_ke_string((uint32)(jumlah_halaman * HALAMAN_4KB),
                                pesan + panjang_string(pesan), 10);
        salin_string(pesan + panjang_string(pesan), " byte");
        notulen_catat(NOTULEN_INFO, pesan);
    }

    return alamat_hasil;
}

/*
 * penyedia_alamat_bebaskan_region
 *
 * Membebaskan region alamat yang sebelumnya dialokasikan.
 * Menandai halaman sebagai bebas dalam bitmap dan menghapus
 * catatan dari tabel region.
 *
 * Parameter:
 *   alamat - alamat awal region yang akan dibebaskan
 *
 * Mengembalikan: STATUS_OK jika berhasil,
 *                STATUS_TIDAK_ADA jika region tidak ditemukan.
 */
int penyedia_alamat_bebaskan_region(ukuran_ptr alamat)
{
    int slot;
    ukuran_t halaman_awal;
    ukuran_t jumlah_halaman;

    if (alamat == 0) return STATUS_PARAMETAR_SALAH;

    /* Cari region dalam tabel */
    slot = cari_region_berdasarkan_alamat(alamat);
    if (slot < 0) {
        notulen_catat(NOTULEN_PERINGATAN,
                      "Penyedia: Region alamat tidak ditemukan untuk dibebaskan");
        return STATUS_TIDAK_ADA;
    }

    /* Hitung jumlah halaman */
    halaman_awal = (ukuran_t)tabel_region[slot].alamat_mulai / HALAMAN_4KB;
    jumlah_halaman = tabel_region[slot].ukuran / HALAMAN_4KB;

    /* Tandai halaman sebagai bebas */
    tandai_bebas(halaman_awal, jumlah_halaman);

    /* Hapus catatan dari tabel */
    tabel_region[slot].alamat_mulai = 0;
    tabel_region[slot].ukuran = 0;
    tabel_region[slot].nomor_perangkat = 0;
    tabel_region[slot].tipe_region = 0;
    tabel_region[slot].terpakai = SALAH;

    jumlah_region_terpakai--;

    {
        char pesan[80];
        salin_string(pesan, "Penyedia: Region di 0x");
        konversi_uint_ke_string((uint32)alamat,
                                pesan + panjang_string(pesan), 16);
        salin_string(pesan + panjang_string(pesan), " dibebaskan");
        notulen_catat(NOTULEN_INFO, pesan);
    }

    return STATUS_OK;
}

/*
 * penyedia_alamat_peta_perangkat
 *
 * Memetakan ruang alamat perangkat (MMIO) ke ruang alamat
 * yang dapat diakses oleh pengendali. Pada arsitektur x86/x64
 * dengan paging, ini berarti membuat pemetaan page table.
 * Pada konfigurasi sederhana, cukup mencatat alamat fisik
 * dan mengizinkan akses langsung.
 *
 * Parameter:
 *   alamat_fisik    - alamat fisik perangkat di bus
 *   ukuran          - ukuran region MMIO dalam byte
 *   nomor_perangkat - nomor perangkat yang memetakan
 *   nama            - nama perangkat untuk identifikasi
 *
 * Mengembalikan: alamat virtual yang dipetakan, atau 0 jika gagal.
 */
ukuran_ptr penyedia_alamat_peta_perangkat(ukuran_ptr alamat_fisik,
                                           ukuran_t ukuran,
                                           uint32 nomor_perangkat,
                                           const char *nama)
{
    int slot;

    /* Inisialisasi jika belum */
    if (jumlah_region_terpakai == 0 && jumlah_mmio_terpakai == 0) {
        inisialisasi_alamat();
    }

    if (alamat_fisik == 0 || ukuran == 0) return 0;

    /* Cari slot MMIO kosong */
    slot = cari_slot_mmio();
    if (slot < 0) {
        notulen_catat(NOTULEN_KESALAHAN,
                      "Penyedia: Tabel MMIO penuh, tidak dapat memetakan perangkat");
        return 0;
    }

    /* Pada tahap ini, kita menggunakan identity mapping —
     * alamat virtual = alamat fisik. Paging penuh akan
     * diimplementasikan oleh modul pengembang. */
    tabel_mmio[slot].alamat_fisik = alamat_fisik;
    tabel_mmio[slot].alamat_virtual = alamat_fisik; /* identity mapping */
    tabel_mmio[slot].ukuran = ukuran;
    tabel_mmio[slot].nomor_perangkat = nomor_perangkat;
    tabel_mmio[slot].terpakai = BENAR;

    /* Salin nama perangkat */
    if (nama != NULL) {
        int j;
        for (j = 0; j < 31 && nama[j] != '\0'; j++) {
            tabel_mmio[slot].nama[j] = nama[j];
        }
        tabel_mmio[slot].nama[j] = '\0';
    } else {
        salin_string(tabel_mmio[slot].nama, "MMIO Tidak Bernama");
    }

    /* Tandai halaman MMIO sebagai terpakai dalam bitmap */
    {
        ukuran_t halaman_awal = (ukuran_t)alamat_fisik / HALAMAN_4KB;
        ukuran_t jumlah_halaman = ukuran / HALAMAN_4KB;
        if (ukuran % HALAMAN_4KB != 0) {
            jumlah_halaman++;
        }
        tandai_terpakai(halaman_awal, jumlah_halaman);
    }

    jumlah_mmio_terpakai++;

    {
        char pesan[80];
        salin_string(pesan, "Penyedia: MMIO dipetakan di 0x");
        konversi_uint_ke_string((uint32)alamat_fisik,
                                pesan + panjang_string(pesan), 16);
        notulen_catat(NOTULEN_INFO, pesan);
    }

    return tabel_mmio[slot].alamat_virtual;
}

/*
 * penyedia_alamat_daftar_mmio
 *
 * Mengembalikan daftar region MMIO yang telah dipetakan.
 * Hasil disimpan dalam array DataPerangkat yang disediakan pemanggil.
 *
 * Parameter:
 *   daftar - array penampung hasil (alamat dan ukuran per region)
 *   maks   - jumlah maksimum entri yang dapat ditampung
 *
 * Mengembalikan: jumlah region MMIO yang terdaftar.
 */
int penyedia_alamat_daftar_mmio(DataPerangkat daftar[], int maks)
{
    int i;
    int jumlah = 0;

    /* Inisialisasi jika belum */
    if (jumlah_region_terpakai == 0 && jumlah_mmio_terpakai == 0) {
        inisialisasi_alamat();
    }

    if (daftar == NULL || maks <= 0) return 0;

    for (i = 0; i < MMIO_MAKSIMUM && jumlah < maks; i++) {
        if (tabel_mmio[i].terpakai) {
            daftar[jumlah].nomor = tabel_mmio[i].nomor_perangkat;
            daftar[jumlah].alamat = tabel_mmio[i].alamat_virtual;
            daftar[jumlah].ukuran = tabel_mmio[i].ukuran;
            salin_string(daftar[jumlah].nama, tabel_mmio[i].nama);
            jumlah++;
        }
    }

    return jumlah;
}
