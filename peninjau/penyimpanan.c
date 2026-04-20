/*
 * penyimpanan.c
 *
 * Pemeriksaan perangkat penyimpanan (HDD/SSD/NVMe).
 * Memindai bus PCI untuk pengendali penyimpanan:
 *   - IDE/PATA (kelas 0x01, subkelas 0x01)
 *   - SATA/AHCI (kelas 0x01, subkelas 0x06)
 *   - NVMe (kelas 0x01, subkelas 0x08)
 *   - SCSI (kelas 0x01, subkelas 0x00)
 *
 * Untuk setiap pengendali yang ditemukan, mencoba membaca
 * informasi disk melalui register PCI BAR.
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"
#include "../lampiran/peninjau.h"

int peninjau_cek_penyimpanan(DataPerangkat daftar[], int maks)
{
    int jumlah = 0;

#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    {
        uint8 bus, dev, fn;

        for (bus = 0; bus < 255 && jumlah < maks; bus++) {
            for (dev = 0; dev < 32 && jumlah < maks; dev++) {
                for (fn = 0; fn < 8 && jumlah < maks; fn++) {
                    uint32 data_kls;
                    uint16 vendor;
                    uint8 kelas, subkelas, prog_if;

                    vendor = peninjau_pci_baca_vendor(bus, dev, fn);
                    if (vendor == PCI_VENDOR_TIDAK_ADA) continue;

                    data_kls = peninjau_pci_baca(bus, dev, fn, 0x08);
                    kelas = (uint8)((data_kls >> 24) & 0xFF);
                    subkelas = (uint8)((data_kls >> 16) & 0xFF);
                    prog_if = (uint8)((data_kls >> 8) & 0xFF);

                    /* Kelas 0x01 = Mass Storage Controller */
                    if (kelas != PCI_KELAS_STORAGE) continue;

                    isi_memori(&daftar[jumlah], 0, sizeof(DataPerangkat));
                    daftar[jumlah].tipe = PERANGKAT_PENYIMPANAN;
                    daftar[jumlah].tipe_bus = BUS_PCI;
                    daftar[jumlah].vendor_id = vendor;
                    daftar[jumlah].perangkat_id = peninjau_pci_baca_perangkat(bus, dev, fn);
                    daftar[jumlah].kelas = kelas;
                    daftar[jumlah].subkelas = subkelas;
                    daftar[jumlah].kondisi = KONDISI_BAIK;
                    daftar[jumlah].port = ((uint32)bus << 16) | ((uint32)dev << 8) | fn;
                    daftar[jumlah].alamat = (ukuran_ptr)peninjau_pci_baca_bar(bus, dev, fn, 0);

                    /* Identifikasi tipe pengendali */
                    switch (subkelas) {
                    case 0x00:
                        salin_string(daftar[jumlah].nama, "Pengendali SCSI");
                        daftar[jumlah].interupsi = peninjau_pci_baca_irq(bus, dev, fn);
                        break;
                    case 0x01:
                        if (prog_if & 0x80) {
                            salin_string(daftar[jumlah].nama, "Pengendali IDE (Bus Master)");
                        } else {
                            salin_string(daftar[jumlah].nama, "Pengendali IDE");
                        }
                        daftar[jumlah].interupsi = peninjau_pci_baca_irq(bus, dev, fn);
                        break;
                    case 0x05:
                        salin_string(daftar[jumlah].nama, "Pengendali ATAPI");
                        break;
                    case 0x06:
                        if (prog_if == 0x01) {
                            salin_string(daftar[jumlah].nama, "Pengendali SATA AHCI");
                        } else {
                            salin_string(daftar[jumlah].nama, "Pengendali SATA");
                        }
                        daftar[jumlah].interupsi = peninjau_pci_baca_irq(bus, dev, fn);
                        break;
                    case 0x08:
                        salin_string(daftar[jumlah].nama, "Pengendali NVMe");
                        daftar[jumlah].interupsi = peninjau_pci_baca_irq(bus, dev, fn);
                        break;
                    default:
                        salin_string(daftar[jumlah].nama, "Pengendali Penyimpanan");
                        break;
                    }

                    /* Simpan detail ke data_khusus */
                    {
                        DataPenyimpanan dp;
                        isi_memori(&dp, 0, sizeof(DataPenyimpanan));
                        salin_string(dp.nama, daftar[jumlah].nama);
                        dp.tipe = (subkelas == 0x08) ? 2 :    /* NVMe */
                                  (subkelas == 0x06) ? 1 : 0;  /* SSD/HDD */
                        dp.bus = BUS_PCI;
                        salin_memori(daftar[jumlah].data_khusus, &dp,
                                     MIN(sizeof(DataPenyimpanan), DATA_KHUSUS_PANJANG));
                    }

                    notulen_tambah(&daftar[jumlah]);
                    jumlah++;
                }
            }
        }
    }
#else
    TIDAK_DIGUNAKAN(daftar);
    TIDAK_DIGUNAKAN(maks);
#endif

    {
        char pesan[64];
        salin_string(pesan, "Peninjau: Ditemukan ");
        konversi_uint_ke_string((uint32)jumlah, pesan + panjang_string(pesan), 10);
        salin_string(pesan + panjang_string(pesan), " pengendali penyimpanan");
        notulen_catat(NOTULEN_INFO, pesan);
    }

    return jumlah;
}
