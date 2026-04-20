;; ================================================================
;; pemicu.asm — Kode pemicu (boot) untuk arsitektur x86 (IA-32)
;;
;; Titik masuk pertama kernel Arsitek.
;; Dimuat oleh BIOS pada alamat 0x7C00 dalam mode nyata 16-bit.
;;
;; Sektor boot (512 byte) hanya berisi kode esensial:
;;   1. Inisialisasi segmen dan tumpukan
;;   2. Aktifkan garis A20
;;   3. Muat sektor 2+ dari disk ke memori (0x7E00)
;;   4. Muat GDT dan beralih ke mode terlindung 32-bit
;;   5. Lompat jauh ke kode 32-bit di 0x7E00
;;
;; Kode 32-bit (setelah tanda tangan boot) menangani:
;;   - Salin kernel ke alamat 1 MB
;;   - Lompat ke titik masuk kernel
;; ================================================================

[BITS 16]
[ORG 0x7C00]

;; ================================================================
;; SEKTOR BOOT (maks 510 byte + 2 byte tanda tangan)
;; ================================================================

pemicu_mulai:
    ;; ------ Inisialisasi segmen dan tumpukan ------
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00              ; Tumpukan tumbuh ke bawah dari alamat boot

    ;; ------ Simpan nomor drive boot ------
    mov [nomor_drive], dl

    ;; ------ Aktifkan garis A20 ------
    call aktifkan_a20

    ;; ------ Muat kernel dari disk ------
    ;; Sektor 2+ dimuat ke 0x7E00 (tepat setelah sektor boot)
    ;; Kode 32-bit dan kernel C ada di sektor-sektor ini

    ;; Tahap 1: 64 sektor ke 0x7E00 (32 KB)
    mov ax, 0x0000
    mov es, ax
    mov bx, 0x7E00
    mov ah, 0x02
    mov al, 64
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [nomor_drive]
    int 0x13
    jc kesalahan_disk

    ;; Tahap 2: 64 sektor ke 0x0FE00 (32 KB)
    mov ax, 0x0000
    mov es, ax
    mov bx, 0xFE00
    mov ah, 0x02
    mov al, 64
    mov ch, 1
    mov cl, 2
    mov dh, 0
    mov dl, [nomor_drive]
    int 0x13
    jc kesalahan_disk

    ;; ------ Matikan interupsi ------
    cli

    ;; ------ Muat GDT ------
    lgdt [penuding_gdt]

    ;; ------ Beralih ke mode terlindung ------
    mov eax, cr0
    or eax, 0x00000001          ; Set bit PE (Protection Enable)
    mov cr0, eax

    ;; ------ Lompat jauh ke kode 32-bit ------
    ;; mode_terlindung berada di alamat 0x7E00 (ORG 0x7C00 + 512)
    ;; yang sesuai dengan lokasi kode 32-bit di memori
    jmp 0x08:mode_terlindung

;; ================================================================
;; Fungsi: Aktifkan garis A20 melalui pengendali papan ketik
;; ================================================================
aktifkan_a20:
    call .tunggu_perintah
    mov al, 0xAD
    out 0x64, al
    call .tunggu_perintah
    mov al, 0xD0
    out 0x64, al
    call .tunggu_data
    in al, 0x60
    push ax
    call .tunggu_perintah
    mov al, 0xD1
    out 0x64, al
    call .tunggu_perintah
    pop ax
    or al, 0x02
    out 0x60, al
    call .tunggu_perintah
    mov al, 0xAE
    out 0x64, al
    call .tunggu_perintah
    ret

.tunggu_perintah:
    in al, 0x64
    test al, 0x02
    jnz .tunggu_perintah
    ret

.tunggu_data:
    in al, 0x64
    test al, 0x01
    jz .tunggu_data
    ret

;; ================================================================
;; Penanganan kesalahan disk
;; ================================================================
kesalahan_disk:
    cli
    hlt
    jmp kesalahan_disk

;; ================================================================
;; DATA SEKTOR BOOT
;; ================================================================

nomor_drive:       db 0

;; ================================================================
;; GDT (Tabel Deskriptor Global) — minimalis
;; ================================================================
gdt_mulai:
    ;; Entri 0: Deskriptor NULL (wajib)
    dd 0x00000000
    dd 0x00000000

    ;; Entri 1: Segmen Kode Kernel (ring 0, 32-bit)
    dw 0xFFFF                   ; Batas bawah
    dw 0x0000                   ; Dasar bawah
    db 0x00                     ; Dasar tengah
    db 10011010b                ; Present, DPL=0, Code, Readable
    db 11001111b                ; 4K granularity, 32-bit, batas atas
    db 0x00                     ; Dasar atas

    ;; Entri 2: Segmen Data Kernel (ring 0, 32-bit)
    dw 0xFFFF                   ; Batas bawah
    dw 0x0000                   ; Dasar bawah
    db 0x00                     ; Dasar tengah
    db 10010010b                ; Present, DPL=0, Data, Writable
    db 11001111b                ; 4K granularity, 32-bit, batas atas
    db 0x00                     ; Dasar atas
gdt_akhir:

penuding_gdt:
    dw gdt_akhir - gdt_mulai - 1   ; Batas GDT
    dd gdt_mulai                    ; Alamat dasar GDT

;; ================================================================
;; Padding dan tanda tangan boot sector
;; ================================================================
times 510 - ($ - $$) db 0       ; Isi sisa dengan nol
dw 0xAA55                       ; Tanda tangan boot sector

;; ================================================================
;; KODE MODE TERLINDUNG 32-BIT
;;
;; Kode ini dimuat dari sektor 2+ ke alamat 0x7E00.
;; Tugasnya menyalin kernel C ke alamat 1 MB dan melompat ke sana.
;; ================================================================
[BITS 32]

mode_terlindung:
    ;; ------ Atur segmen mode terlindung ------
    mov ax, 0x10                ; Selektor data kernel (GDT entri 2)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ;; ------ Atur tumpukan kernel ------
    mov esp, 0x90000            ; Tumpukan di 576 KB

    ;; ------ Salin kernel dari memori konvensional ke 1 MB ------
    ;; Sumber: alamat kernel_data_mulai (0x7E00 + offset kode 32-bit)
    ;; Tujuan: 0x100000 (1 MB)
    ;; Ukuran: 64 KB (atau sebanyak yang dimuat dari disk)
    ;;
    ;; Kernel C (arsitek.bin) diletakkan tepat setelah kode 32-bit
    ;; dalam citra disk. Saat dimuat ke memori, kernel dimulai dari
    ;; alamat kernel_data_mulai.

    mov esi, kernel_data_mulai  ; Alamat sumber (kernel C)
    mov edi, 0x100000           ; Alamat tujuan (1 MB)
    mov ecx, (64 * 1024) / 4   ; 64 KB dalam satuan dword
    cld
    rep movsd                   ; Salin!

    ;; ------ Lompat ke titik masuk kernel ------
    ;; Fungsi pertama kernel: arsitek_mulai() di alamat 0x100000
    jmp 0x100000

    ;; ------ Jika kernel kembali (seharusnya tidak) ------
    cli
    hlt
    jmp $

;; Penanda: data kernel C dimulai di sini dalam citra disk.
;; Label ini digunakan untuk menghitung alamat sumber saat menyalin.
kernel_data_mulai:
