VGA_GRAPH_REG = 3CEh
VGA_SEQ_REG = 3C4h

VGA_SQ_MAP_MASK_REG = 02h
VGA_SQ_CHAR_MAP_REG = 03h
VGA_SQ_MEM_MODE_REG = 04h
VGA_GR_MAP_SELECT_REG = 04h
VGA_GR_GRAPH_MODE_REG = 05h
VGA_GR_MISC_REG = 06h

VIDEO_MEMORY_ADDR = 0B8000h

N_FRAMES = 12*8-8

CMD_LEN = 128
CMD_DATA = 129
WHITEONBLACK = 0700h
CHARSIZE = 32
NCHARS = 256

FONT_BASE = 2000h

HEIGHT = 24
WIDTH = 80
LINE = HEIGHT/2

C_TOP_LEFT = 080h
C_TOP = 081h
C_TOP_RIGHT = 082h
C_RIGHT = 83h
C_BOTTOM_RIGHT = 084h
C_BOTTOM = 85h
C_BOTTOM_LEFT = 086h
C_LEFT = 087h

CN_TOP_LEFT = 0DAh
CN_TOP = 0C4h
CN_TOP_RIGHT = 0BFh
CN_BOTTOM_RIGHT = 0D9h
CN_BOTTOM_LEFT = 0C0h
CN_LEFT = 0B3h

org 0x100
start:
    cld

    mov ax, VIDEO_MEMORY_ADDR shr 4
    mov es, ax

    call reprogram_vga
    
    xor bp, bp
.loop:
    call vsync
    mov ax, bp
    mov dx, bp
    lea dx, [edx*3]
    shr ax, 4
    shr dx, 3
    mov dh, al
    add dx, dx
    call draw_frame
    mov dx, bp
    mov ax, bp
    add dx, dx
    mov dh, al
    and dx, 0F07h
    call shift_frame
    cmp bp, 16
    jb @f
    mov dx, bp
    lea dx, [edx*3]
    shr dx, 3
    add dx, dx
    call draw_text
@@:
    inc bp
    cmp bp, N_FRAMES
    jne .loop

    call restore_vga
    ret

VGA_COLOR_IN_STAT = 3DAh
vsync:
; IN: -
; OUT: -
    ; Wait for vbank
    mov dx, VGA_COLOR_IN_STAT
@@:
    in al, dx
    test al, 8
    jz @b
    
    ret

draw_text:
; IN: dx = width of screen
; OUT: -
    push si
    push di
    xor ch, ch
    mov cl, [CMD_LEN]
    mov si, CMD_DATA
    test cx, cx
    jnz .has_args
    mov cx, text.size - 1
    mov si, text
.has_args:
    inc cx
    mov ax, cx
    mov di, (WIDTH*LINE + WIDTH/2)*2
    and cx, 254
    sub di, cx
    mov cx, ax
    sub ax, dx
    js @f
    shr ax, 1
    add di, ax
    add di, ax
    add si, ax
    jmp .skip
@@: mov dx, cx
.skip:
    mov ah, 07h
    jmp @f
.again:
    stosw
@@: lodsb
    dec dx
    jnz .again
.end:
    pop di
    pop si
    ret


draw_frame:
; IN: dl = width of contents
;     dh = height of contents
; OUT: -
    push bx
    and dx, 0xFEFE
    mov cl, WIDTH
    mov al, dh
    mov ch, dh
    mov bx, (WIDTH/2+WIDTH*LINE-1)*2 ; The exact center of the screen
    mul cl
    sub bx, ax
    xor dh, dh
    mov ax, 1
    add al, dl
    sub bx, dx
    add ax, ax

    ; Top border
    mov di, ax
    mov [es:bx+di], word (C_TOP_RIGHT or WHITEONBLACK)
    sub di, 2
    jz .skip0
.inner0:
    mov [es:bx+di], word (C_TOP or WHITEONBLACK)
    sub di, 2
    jnz .inner0
.skip0:
    mov [es:bx+di], word (C_TOP_LEFT or WHITEONBLACK)
    add bx, WIDTH*2

    test ch, ch
    jz .no_lines

    ; First line
    mov di, ax
    mov [es:bx+di], word (C_RIGHT or WHITEONBLACK)
    sub di, 2
    jz .skip1
.inner1:
    mov [es:bx+di], word (' ' or WHITEONBLACK)
    sub di, 2
    jnz .inner1
    mov [es:bx+2], word (' ' or WHITEONBLACK)
.skip1:
    mov [es:bx], word (C_LEFT or WHITEONBLACK)
    add bx, WIDTH*2
    dec ch

    cmp ch, 1
    jbe .one_line

    ; All other lines until the last
.outer2:
    mov di, ax
    mov [es:bx+di], word (C_RIGHT or WHITEONBLACK)
    sub di, 2
    jz .skip2
    mov [es:bx+di], word (' ' or WHITEONBLACK)
    mov [es:bx+2], word (' ' or WHITEONBLACK)
.skip2:
    mov [es:bx], word (C_LEFT or WHITEONBLACK)
    add bx, WIDTH*2
    dec ch
    cmp ch, 1
    jne .outer2

.one_line:
    test ch, ch
    jz .no_lines

    ; Last line
    mov di, ax
    mov [es:bx+di], word (C_RIGHT or WHITEONBLACK)
    sub di, 2
    jz .skip3
.inner3:
    mov [es:bx+di], word (' ' or WHITEONBLACK)
    sub di, 2
    jnz .inner3
    mov [es:bx+2], word (' ' or WHITEONBLACK)
.skip3:
    mov [es:bx], word (C_LEFT or WHITEONBLACK)
    add bx, WIDTH*2
    
.no_lines:
    ; Bottom border
    mov di, ax
    mov [es:bx+di], word (C_BOTTOM_RIGHT or WHITEONBLACK)
    sub di, 2
    jz .skip4
.inner4:
    mov [es:bx+di], word (C_BOTTOM or WHITEONBLACK)
    sub di, 2
    jnz .inner4
.skip4:
    mov [es:bx+di], word (C_BOTTOM_LEFT or WHITEONBLACK)
    pop bx
    ret

shift_frame:
; IN: dl = vertical state (0-7, 0 -- smallest)
;     dh = horizontal state
; OUT: -

    push di
    push si
    push bx

    push dx
    call enable_font_writes
    pop dx
    cmp [font_n], byte 0
    jne @f
    mov ax, (VIDEO_MEMORY_ADDR + CHARSIZE*NCHARS) shr 4
    mov es, ax
@@:
    xor ch, ch

    ; Vertial left
    mov cl, dl
    mov al, 1
    shl al, cl
    mov ah, al
    mov di, FONT_BASE + C_LEFT*CHARSIZE
    mov cl, 16
    rep stosb

    ; Bottom left vertical part
    mov cl, dh
    mov di, FONT_BASE + C_BOTTOM_LEFT*CHARSIZE
    rep stosb
    xor al, al
    mov cl, 16
    sub cl, dh
    rep stosb
    
    ; Top left vertical part
    mov di, FONT_BASE + C_TOP_LEFT*CHARSIZE
    mov cl, 16
    sub cl, dh
    rep stosb
    mov cl, dl
    mov al, 1
    shl al, cl
    mov cl, dh
    rep stosb

    ; Vertial right
    mov cl, dl
    mov al, 0x80
    shr al, cl
    mov ah, al
    mov di, FONT_BASE + C_RIGHT*CHARSIZE
    mov cl, 16/2
    rep stosw

    ; Bottom right vertical part
    mov cl, dh
    mov di, FONT_BASE + C_BOTTOM_RIGHT*CHARSIZE
    rep stosb
    xor al, al
    mov cl, 16
    sub cl, dh
    rep stosb
    
    ; Top right vertical part
    mov di, FONT_BASE + C_TOP_RIGHT*CHARSIZE
    mov cl, 16
    sub cl, dh
    rep stosb
    mov cl, dl
    mov al, 80h
    shr al, cl
    mov cl, dh
    rep stosb

    ; Horizontal parts of corner
    ; pieces

    xor ch, ch
    
    mov di, FONT_BASE+C_BOTTOM_LEFT*CHARSIZE
    mov al, 0xFF
    mov cl, 7
    sub cl, dl
    shr al, cl
    mov cl, dh
    add di, cx
    mov [es:di], al
    mov di, FONT_BASE+C_TOP_LEFT*CHARSIZE
    mov cl, 15
    sub cl, dh
    add di, cx
    mov [es:di], al
    
    mov al, 0xFF
    mov cl, 7
    sub cl, dl
    shl al, cl
    mov cl, dh
    mov di, FONT_BASE+C_BOTTOM_RIGHT*CHARSIZE
    add di, cx
    mov [es:di], al
    mov di, FONT_BASE+C_TOP_RIGHT*CHARSIZE
    mov cl, 15
    sub cl, dh
    add di, cx
    mov [es:di], al

    ; Horizontal top/bottom
    mov di, C_TOP*CHARSIZE-C_BOTTOM*CHARSIZE
    mov si, 15
    mov bx, FONT_BASE+C_BOTTOM*CHARSIZE
    mov dl, dh
    xor dh, dh

@@: cmp dx, si
    sete al
    neg al
    mov [es:di+bx], al
    mov [es:si+bx], al
    inc di
    dec si
    jns @b

    cmp [font_n], byte 0
    je .else
    mov [font_n], byte 0
    mov ax, 3000h or VGA_SQ_CHAR_MAP_REG
    jmp .fi
.else:
    mov [font_n], byte 1
    mov ax, 0500h or VGA_SQ_CHAR_MAP_REG
    push VIDEO_MEMORY_ADDR shr 4
    pop es
.fi:
    ; Switch font location
    ; (for double buffering)
    mov dx, VGA_SEQ_REG
    out dx, ax
    
    call disable_font_writes
    pop bx
    pop si
    pop di
    ret

reprogram_vga:
; IN: -
; OUT: -
    ; Save VGA registers states
    cli
    push di
    push si

    call swap_es_ds
    mov di, saved_vga_regs
    mov dx, VGA_SEQ_REG
    mov cl, VGA_SQ_MAP_MASK_REG
    mov ch, VGA_SQ_MEM_MODE_REG
@@:
    mov al, cl
    inc cl
    out dx, al
    inc dx
    in al, dx
    dec dx
    stosb
    cmp cl, ch
    jbe @b
    cmp ch, VGA_GR_MISC_REG
    je @f
    dec cl
    mov ch, VGA_GR_MISC_REG
    mov dx, VGA_GRAPH_REG
    jmp @b
@@:
    ; Reprogram VGA video
    mov dx, VGA_GRAPH_REG
    out dx, ax
    mov ax, 0100h or VGA_GR_GRAPH_MODE_REG
    out dx, ax
    mov ax, 0C00h or VGA_GR_MISC_REG
    out dx, ax
    mov ax, 0200h or VGA_GR_MAP_SELECT_REG
    out dx, ax

    ; Reprogram VGA sequencer
    mov dx, VGA_SEQ_REG
    ; Enable writing to planes 0 and 2
    mov ax, 0500h or VGA_SQ_MAP_MASK_REG
    out dx, ax
    mov ax, 0000h or VGA_SQ_CHAR_MAP_REG
    out dx, ax
    mov ax, 0600h or VGA_SQ_MEM_MODE_REG
    out dx, ax

    ; Save unmodified characters
    mov cx, NCHARS*CHARSIZE/2
    xor si, si
    mov di, saved_chars
    rep movsw
    call swap_es_ds

    ; Copy font to the new location
    mov si, saved_chars
    mov di, FONT_BASE
    mov cx, NCHARS*CHARSIZE/2
    rep movsw
    mov si, saved_chars
    mov di, FONT_BASE + CHARSIZE*NCHARS
    mov cx, NCHARS*CHARSIZE/2
    rep movsw

    ; Switch font source to new font base
    mov ax, 3000h or VGA_SQ_CHAR_MAP_REG
    out dx, ax

    call disable_font_writes

    pop si
    pop di
    sti
    ret

enable_font_writes:
    mov dx, VGA_GRAPH_REG
    mov ax, 0C00h or VGA_GR_MISC_REG
    out dx, ax
    ret

disable_font_writes:
    mov dx, VGA_GRAPH_REG
    mov ax, 0E00h or VGA_GR_MISC_REG
    out dx, ax
    ret

restore_vga:
; IN: -
; OUT: -

    cli
    push si
    push di

    ; Replace custom box-drawing
    ; characters with regular ones
    mov si, WIDTH*HEIGHT*2
.replace_loop:
    mov al, [es:si]
    cmp al, C_BOTTOM
    je .d1
    cmp al, C_TOP
    jne @f
.d1:mov [es:si], byte CN_TOP
@@: cmp al, C_LEFT
    je .d2
    cmp al, C_RIGHT
    jne @f
.d2:mov [es:si], byte CN_LEFT
@@: cmp al, C_TOP_LEFT
    jne @f
    mov [es:si], byte CN_TOP_LEFT
@@: cmp al, C_TOP_RIGHT
    jne @f
    mov [es:si], byte CN_TOP_RIGHT
@@: cmp al, C_BOTTOM_LEFT
    jne @f
    mov [es:si], byte CN_BOTTOM_LEFT
@@: cmp al, C_BOTTOM_RIGHT
    jne @f
    mov [es:si], byte CN_BOTTOM_RIGHT
@@: sub si, 2
    jns .replace_loop

    ; Restore original characters
    call enable_font_writes
    mov dx, VGA_SEQ_REG
    mov ax, 0400h or VGA_SQ_MAP_MASK_REG
    out dx, ax
    mov cx, NCHARS*CHARSIZE/2
    xor di, di
    mov si, saved_chars
    rep movsw
    call disable_font_writes

    ; Restore VGA registers
    mov si, saved_vga_regs
    mov dx, VGA_SEQ_REG
    mov al, VGA_SQ_MAP_MASK_REG
    mov cl, VGA_SQ_MEM_MODE_REG
@@:
    mov ah, [si]
    inc si
    out dx, ax
    inc al
    cmp al, cl
    jbe @b
    cmp cl, VGA_GR_MISC_REG
    je @f
    dec al
    mov cl, VGA_GR_MISC_REG
    mov dx, VGA_GRAPH_REG
    jmp @b
@@:
    pop di
    pop si
    sti
    ret

swap_es_ds:
    push es
    push ds
    pop es
    pop ds
    ret

font_n db 0
text db "Some text.. more text", 0
.size = $ - text

align 2
saved_vga_regs rb 6
saved_chars:
