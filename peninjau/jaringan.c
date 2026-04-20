/*
 * jaringan.c
 *
 * Pemeriksaan perangkat jaringan — deteksi pengendali
 * Eternet dan WiFi melalui bus PCI.
 * Memindai kelas PCI 0x02 (Network Controller).
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"
#include "../lampiran/peninjau.h"

int peninjau_cek_jaringan(DataPerangkat daftar[], int maks)
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
                    uint8 kelas, subkelas;

                    vendor = peninjau_pci_baca_vendor(bus, dev, fn);
                    if (vendor == PCI_VENDOR_TIDAK_ADA) continue;

                    data_kls = peninjau_pci_baca(bus, dev, fn, 0x08);
                    kelas = (uint8)((data_kls >> 24) & 0xFF);
                    subkelas = (uint8)((data_kls >> 16) & 0xFF);

                    if (kelas != PCI_KELAS_JARINGAN) continue;

                    isi_memori(&daftar[jumlah], 0, sizeof(DataPerangkat));
                    daftar[jumlah].tipe = PERANGKAT_JARINGAN;
                    daftar[jumlah].tipe_bus = BUS_PCI;
                    daftar[jumlah].vendor_id = vendor;
                    daftar[jumlah].perangkat_id = peninjau_pci_baca_perangkat(bus, dev, fn);
                    daftar[jumlah].kelas = kelas;
                    daftar[jumlah].subkelas = subkelas;
                    daftar[jumlah].kondisi = KONDISI_BAIK;
                    daftar[jumlah].port = ((uint32)bus << 16) | ((uint32)dev << 8) | fn;
                    daftar[jumlah].alamat = (ukuran_ptr)peninjau_pci_baca_bar(bus, dev, fn, 0);
                    daftar[jumlah].interupsi = peninjau_pci_baca_irq(bus, dev, fn);

                    /* Identifikasi tipe */
                    switch (subkelas) {
                    case 0x00:
                        salin_string(daftar[jumlah].nama, "Pengendali Eternet");
                        break;
                    case 0x80:
                        salin_string(daftar[jumlah].nama, "Pengendali Jaringan Lain");
                        break;
                    default:
                        salin_string(daftar[jumlah].nama, "Pengendali Jaringan");
                        break;
                    }

                    /* Simpan detail */
                    {
                        DataJaringan dj;
                        isi_memori(&dj, 0, sizeof(DataJaringan));
                        salin_string(dj.nama, daftar[jumlah].nama);
                        dj.tipe = (subkelas == 0x80) ? 1 : 0;   /* 0=Eternet, 1=WiFi */
                        dj.bus = BUS_PCI;
                        salin_memori(daftar[jumlah].data_khusus, &dj,
                                     MIN(sizeof(DataJaringan), DATA_KHUSUS_PANJANG));
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

    return jumlah;
}
