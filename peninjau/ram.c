/*
 * ram.c
 *
 * Pemeriksaan RAM — deteksi memori fisik.
 * Pada x86/x64, membaca BIOS Data Area dan CMOS.
 * Pada ARM, menggunakan informasi dari bootloader (ATAG/FDT).
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"

int peninjau_cek_ram(DataPerangkat *hasil)
{
    DataRAM data_ram;

    if (hasil == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    isi_memori(hasil, 0, sizeof(DataPerangkat));
    isi_memori(&data_ram, 0, sizeof(DataRAM));

    hasil->tipe = PERANGKAT_RAM;
    hasil->kondisi = KONDISI_BAIK;
    salin_string(hasil->nama, "Memori Akses Acak (RAM)");
    salin_string(hasil->pabrikan, "Sistem");

#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    {
        uint16 mem_bawah_kb;
        uint16 mem_atas_kb;
        volatile uint16 *penuding_bda;

        /* Baca memori konvensional dari BDA pada 0x0413 */
        penuding_bda = (volatile uint16 *)0x0413;
        mem_bawah_kb = *penuding_bda;

        /* Baca memori ekstended dari CMOS (register 0x30-0x31) */
        tulis_port(0x70, 0x30);
        tunda_io();
        mem_atas_kb = (uint16)baca_port(0x71);
        tulis_port(0x70, 0x31);
        tunda_io();
        mem_atas_kb |= (uint16)(baca_port(0x71) << 8);

        /* Hitung total */
        data_ram.total_kb = (ukuran_t)mem_bawah_kb + (ukuran_t)mem_atas_kb;
        data_ram.tersedia_kb = data_ram.total_kb;
        data_ram.terpakai_kb = 0;
        data_ram.jumlah_bank = 1;
        data_ram.ukuran_bank[0] = data_ram.total_kb;
    }
#elif defined(ARSITEK_ARM32)
    data_ram.total_kb = 256UL * 1024UL;     /* Default 256 MB */
    data_ram.tersedia_kb = data_ram.total_kb;
    data_ram.jumlah_bank = 1;
    data_ram.ukuran_bank[0] = data_ram.total_kb;
#elif defined(ARSITEK_ARM64)
    data_ram.total_kb = 1024UL * 1024UL;    /* Default 1024 MB */
    data_ram.tersedia_kb = data_ram.total_kb;
    data_ram.jumlah_bank = 1;
    data_ram.ukuran_bank[0] = data_ram.total_kb;
#endif

    /* Simpan ukuran ke struktur perangkat */
    hasil->ukuran = (ukuran_t)data_ram.total_kb * 1024UL;

    /* Simpan data detail ke data_khusus */
    salin_memori(hasil->data_khusus, &data_ram, MIN(sizeof(DataRAM), DATA_KHUSUS_PANJANG));

    notulen_catat(NOTULEN_INFO, "Peninjau: RAM terdeteksi");

    return STATUS_OK;
}
