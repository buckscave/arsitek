/*
 * peninjau.c
 *
 * Modul Peninjau — inspektur perangkat keras utama.
 * Mengoordinasikan seluruh pemeriksaan perangkat keras
 * dan melaporkan hasilnya ke Arsitek melalui Notulen.
 *
 * Alur Peninjau:
 *   1. Periksa CPU (via CPUID pada x86, register MIDR pada ARM)
 *   2. Periksa RAM (via BDA/CMOS pada x86, ATAG/FDT pada ARM)
 *   3. Enumerasi bus PCI (x86/x64)
 *   4. Periksa layar/tampilan (VGA/VBE)
 *   5. Periksa papan ketik (8042 controller)
 *   6. Periksa tetikus (PS/2 aux port)
 *   7. Periksa penyimpanan (IDE/AHCI via PCI)
 *   8. Periksa jaringan (Ethernet via PCI)
 *   9. Periksa USB (via PCI class 0x0C03)
 *  10. Periksa DMA (8237 controller)
 *
 * CATATAN PENTING tentang duplikasi notulen:
 *   Perangkat penyimpanan dan jaringan adalah perangkat PCI
 *   yang sudah ditemukan saat enumerasi PCI umum (langkah 3).
 *   Fungsi peninjau_cek_penyimpanan() dan peninjau_cek_jaringan()
 *   TIDAK memanggil notulen_tambah() karena perangkat PCI
 *   sudah dicatat saat enumerasi PCI. Pemanggilan notulen_tambah()
 *   di penyimpanan.c dan jaringan.c telah dihapus untuk
 *   menghindari entri ganda (double-entry) di notulen.
 *
 * Setelah selesai, Peninjau memberikan laporan lengkap
 * kepada Arsitek, yang disampaikan melalui Notulen.
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"
#include "../lampiran/peninjau.h"

/* Penanda global: apakah peninjau sudah selesai */
int peninjau_siap = SALAH;

/* Buffer data perangkat yang ditemukan peninjau */
static DataPerangkat daftar_perangkat[PERANGKAT_MAKSIMUM];
static int jumlah_perangkat = 0;

/* ================================================================
 * FUNGSI UTAMA
 * ================================================================ */

/*
 * peninjau_periksa_semua — Periksa seluruh perangkat keras.
 * Dipanggil oleh arsitek_mulai() setelah konstruksi selesai.
 * Menjalankan semua sub-inspeksi secara berurutan.
 *
 * Perangkat penyimpanan dan jaringan TIDAK ditambahkan ke
 * notulen secara terpisah karena sudah tercakup dalam
 * enumerasi PCI umum. Pemanggilan notulen_tambah() hanya
 * dilakukan di sini untuk perangkat non-PCI.
 */
void peninjau_periksa_semua(void)
{
    DataPerangkat hasil;
    int jumlah_pci;
    int i;

    notulen_catat(NOTULEN_INFO, "Peninjau: Memulai pemeriksaan perangkat keras...");
    jumlah_perangkat = 0;

    /* 1. Periksa CPU */
    isi_memori(&hasil, 0, sizeof(DataPerangkat));
    if (peninjau_cek_cpu(&hasil) == STATUS_OK) {
        notulen_tambah(&hasil);
        daftar_perangkat[jumlah_perangkat++] = hasil;
    }

    /* 2. Periksa RAM */
    isi_memori(&hasil, 0, sizeof(DataPerangkat));
    if (peninjau_cek_ram(&hasil) == STATUS_OK) {
        notulen_tambah(&hasil);
        daftar_perangkat[jumlah_perangkat++] = hasil;
    }

    /* 3. Enumerasi PCI — ini inti dari deteksi perangkat.
     * Semua perangkat PCI (termasuk penyimpanan dan jaringan)
     * sudah ditambahkan ke notulen oleh fungsi isi_data_pci()
     * di pci.c. Pemanggilan notulen_tambah() di penyimpanan.c
     * dan jaringan.c telah dihapus untuk mencegah duplikasi. */
    {
        DataPerangkat daftar_pci[PCI_MAKSIMUM];
        jumlah_pci = peninjau_cek_pci(daftar_pci, PCI_MAKSIMUM);
        for (i = 0; i < jumlah_pci && jumlah_perangkat < PERANGKAT_MAKSIMUM; i++) {
            /* Perangkat PCI sudah ditambahkan ke notulen oleh
             * isi_data_pci() di pci.c, jadi tidak perlu
             * memanggil notulen_tambah() lagi di sini. */
            daftar_perangkat[jumlah_perangkat++] = daftar_pci[i];
        }
    }

    /* 4. Periksa layar */
    isi_memori(&hasil, 0, sizeof(DataPerangkat));
    if (peninjau_cek_layar(&hasil) == STATUS_OK) {
        notulen_tambah(&hasil);
        daftar_perangkat[jumlah_perangkat++] = hasil;
    }

    /* 5. Periksa papan ketik */
    isi_memori(&hasil, 0, sizeof(DataPerangkat));
    if (peninjau_cek_papan_ketik(&hasil) == STATUS_OK) {
        notulen_tambah(&hasil);
        daftar_perangkat[jumlah_perangkat++] = hasil;
    }

    /* 6. Periksa tetikus */
    isi_memori(&hasil, 0, sizeof(DataPerangkat));
    if (peninjau_cek_tetikus(&hasil) == STATUS_OK) {
        notulen_tambah(&hasil);
        daftar_perangkat[jumlah_perangkat++] = hasil;
    }

    /* 7. Periksa penyimpanan.
     * Perangkat penyimpanan adalah perangkat PCI yang sudah
     * dicatat oleh enumerasi PCI di langkah 3. Fungsi
     * peninjau_cek_penyimpanan() TIDAK lagi memanggil
     * notulen_tambah() untuk menghindari duplikasi. */
    {
        DataPerangkat daftar_penyimpanan[PENYIMPANAN_MAKSIMUM];
        int jumlah = peninjau_cek_penyimpanan(daftar_penyimpanan,
                                               PENYIMPANAN_MAKSIMUM);
        for (i = 0; i < jumlah && jumlah_perangkat < PERANGKAT_MAKSIMUM; i++) {
            daftar_perangkat[jumlah_perangkat++] = daftar_penyimpanan[i];
        }
    }

    /* 8. Periksa jaringan.
     * Perangkat jaringan adalah perangkat PCI yang sudah
     * dicatat oleh enumerasi PCI di langkah 3. Fungsi
     * peninjau_cek_jaringan() TIDAK lagi memanggil
     * notulen_tambah() untuk menghindari duplikasi. */
    {
        DataPerangkat daftar_jaringan[8];
        int jumlah = peninjau_cek_jaringan(daftar_jaringan, 8);
        for (i = 0; i < jumlah && jumlah_perangkat < PERANGKAT_MAKSIMUM; i++) {
            daftar_perangkat[jumlah_perangkat++] = daftar_jaringan[i];
        }
    }

    /* 9. Periksa USB */
    isi_memori(&hasil, 0, sizeof(DataPerangkat));
    if (peninjau_cek_usb(0, &hasil) == STATUS_OK) {
        notulen_tambah(&hasil);
        daftar_perangkat[jumlah_perangkat++] = hasil;
    }

    /* 10. Periksa DMA */
    isi_memori(&hasil, 0, sizeof(DataPerangkat));
    if (peninjau_cek_dma(&hasil) == STATUS_OK) {
        notulen_tambah(&hasil);
        daftar_perangkat[jumlah_perangkat++] = hasil;
    }

    /* Catat ringkasan */
    {
        char pesan[80];
        salin_string(pesan, "Peninjau: Pemeriksaan selesai. Ditemukan ");
        konversi_uint_ke_string((uint32)jumlah_perangkat,
                                pesan + panjang_string(pesan), 10);
        salin_string(pesan + panjang_string(pesan), " perangkat");
        notulen_catat(NOTULEN_INFO, pesan);
    }

    peninjau_siap = BENAR;
}

/*
 * peninjau_periksa_port — Periksa satu port tertentu.
 * Digunakan untuk hot-plug detection.
 */
int peninjau_periksa_port(unsigned int nomor_port, DataPerangkat *hasil)
{
    if (hasil == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* Untuk saat ini, hanya mendukung pengecekan PCI */
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    {
        uint8 bus      = (uint8)((nomor_port >> 16) & 0xFF);
        uint8 perangkat = (uint8)((nomor_port >> 8) & 0xFF);
        uint8 fungsi   = (uint8)(nomor_port & 0xFF);
        uint16 vendor_id;

        /* Baca vendor ID */
        vendor_id = peninjau_pci_baca_vendor(bus, perangkat, fungsi);
        if (vendor_id == PCI_VENDOR_TIDAK_ADA) {
            return STATUS_TIDAK_ADA;
        }

        return peninjau_cek_pci(hasil, 1) >= 1 ? STATUS_OK : STATUS_GAGAL;
    }
#else
    TIDAK_DIGUNAKAN(nomor_port);
    TIDAK_DIGUNAKAN(hasil);
    return STATUS_TIDAK_ADA;
#endif
}

/*
 * peninjau_verifikasi — Verifikasi kondisi perangkat.
 * Memeriksa apakah perangkat berfungsi dengan baik
 * berdasarkan informasi yang tercatat.
 */
KondisiPerangkat peninjau_verifikasi(DataPerangkat *perangkat)
{
    if (perangkat == NULL) {
        return KONDISI_TIDAK_DIKENAL;
    }

    /* Jika perangkat sudah ditandai rusak, kembalikan apa adanya */
    if (perangkat->kondisi == KONDISI_RUSAK) {
        return KONDISI_RUSAK;
    }

    /* Untuk perangkat PCI, cek ulang vendor ID */
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    if (perangkat->tipe_bus == BUS_PCI) {
        uint8 bus      = (uint8)((perangkat->port >> 16) & 0xFF);
        uint8 dev      = (uint8)((perangkat->port >> 8) & 0xFF);
        uint8 fungsi   = (uint8)(perangkat->port & 0xFF);
        uint16 vendor  = peninjau_pci_baca_vendor(bus, dev, fungsi);

        if (vendor == PCI_VENDOR_TIDAK_ADA || vendor != perangkat->vendor_id) {
            return KONDISI_TERPUTUS;
        }
    }
#endif

    return KONDISI_BAIK;
}
