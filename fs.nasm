bpd_end equ 512
fat_size equ (512*256)
fat2_start equ (512+fat_size)
root_size equ (1024 * 16)
times 3 db 0x90
db "MSWIN4.0"
dw 512
db 1
dw 1 ; num of reserved sector
db 2; num of fat
dw 512; root entry count
dw 65520; total num of sector (16);
db 0xf0; media type
dw 256;  num of sector in a FAT;
dw 512; num of sector in track;
dw 2; num head
dd 0 ; num of hidden sec;
dd 0 ;total num of sector (32)
db 0x80; drive num;
db 0 ; reserved;
db 0x29; boot flag
dd 0xbeef;
db "BOOT       "; Volume label;
db "FAT16   "; FAT type

times 510 - ($ - $$) db 0x00
dw 0xaa55
db 0xf0, 0xff
dw 0xffff
dw 0xffff

times fat2_start - ($-$$) db 0x00
db 0xf0, 0xff
dw 0xffff
dw 0xffff
times (fat2_start+fat_size) - ($-$$) db 0x00
; ROOT_START
FAT_ROOT:
db "BOOT    ", "DSK"
db 0x28
db 0x00
db 0x00
dw (0 << 11) | (0 << 5) | (0/2)
dw (0 << 9) | (0 << 5) | (1)
dw (0 << 9) | (0 << 5) | (1)
dw 0x0000;
dw (0 << 11) | (0 << 5) | (0/2)
dw (0 << 9) | (0 << 5) | (1)
dw 0;
dd 0


; Create a text file SPECIAL.TXT
db "SPECIAL ", "TXT"
db 0x20
db 0x00
db 0
dw (0 << 11) | (0 << 5) | (0/2)
dw (0 << 9) | (0 << 5) | (1)
dw (0 << 9) | (0 << 5) | (1)
dw 0x0000;
dw (0 << 11) | (0 << 5) | (0/2)
dw (0 << 9) | (0 << 5) | (1)
dw 2
dd FILE.end - FILE

times (fat2_start+fat_size) + root_size - ($-$$) db 0x00

FILE: db "HELLO"
.end db 0
align 512, db 0x00
times (512 * 63) db 0x00
