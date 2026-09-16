; PM-BRINGUP [PM-VBE] ADDED 2026-09-16; ChatGPT-assisted project changes.
; PM-BRINGUP [PM-VBE] Base archive commit: 30df506fc64720fd82e528acbcb192adbaa48fce.
; PM-BRINGUP [PM-VBE] NEW FILE: banked VBE 0111h, 640x480 RGB565 only; validate BIOS mode data and map/free DOS window selectors.
; PM-BRINGUP [PM-VBE] See PM-BRINGUP.md and TechDocs/Markdown/pm-bringup/CHANGES.md; original notices retained.
; Minimal protected-mode VBE path, September 2026.
; Only banked 640x480 RGB565 (VBE 0111h), with a readable/writable 64KiB
; window A, is accepted.  Do not pass protected selectors to the BIOS.
; Existing rasterizers remain unchanged; this is not a substitute display.

; AX/BX/CX/DX inputs, AX return; other registers preserved. No real-mode
; pointer arguments. Zero SS:SP requests the DPMI host's real-mode stack.
PMPerfVideoBIOS proc near
        uses bx, cx, dx, si, di, es
inAX    local word push ax
inBX    local word push bx
inCX    local word push cx
inDX    local word push dx
regs    local PMRealModeRegister
        .enter
        segmov es, ss
        lea di, regs
        clr ax
        mov cx, size PMRealModeRegister / 2
        cld
        rep stosw
        mov ax, inAX
        mov regs.PMRMR_eax.low, ax
        mov ax, inBX
        mov regs.PMRMR_ebx.low, ax
        mov ax, inCX
        mov regs.PMRMR_ecx.low, ax
        mov ax, inDX
        mov regs.PMRMR_edx.low, ax
        lea di, regs
        mov bx, VIDEO_BIOS
        clr cx
        call SysRealInterrupt
        mov ax, regs.PMRMR_eax.low
        .leave
        ret
PMPerfVideoBIOS endp

VidTestVESA proc near
        uses bx, cx, dx, si, di, ds, es
regs    local PMRealModeRegister
realSeg local word
dosSel  local word
        .enter
        cmp ax, VD_VESA_640x480_16
        jne absent
        mov bx, (size VESAModeInfo + 15) / 16
        call SysAllocDOSBlock
        jc absent
        mov realSeg, ax
        mov dosSel, dx
        ; Zero the entire allocation: VBE defines a 256-byte response;
        ; the historical GEOS structure reserves 512 bytes.
        mov es, dx
        clr ax, di
        mov cx, size VESAModeInfo / 2
        cld
        rep stosw
        segmov es, ss
        lea di, regs
        mov cx, size PMRealModeRegister / 2
        rep stosw
        mov regs.PMRMR_eax.low, 04f01h
        mov regs.PMRMR_ecx.low, VM_640x480_16
        mov ax, realSeg
        mov regs.PMRMR_es, ax
        lea di, regs
        mov bx, VIDEO_BIOS
        clr cx
        call SysRealInterrupt
        jc freeAbsent
        cmp regs.PMRMR_eax.low, 004fh
        jne freeAbsent
        mov ds, dosSel
        clr si
        ; Require the supported graphics mode, not merely a mode entry.
        mov ax, ds:[VMI_modeAttr]
        and ax, mask VMA_SUPPORTED or mask VMA_GRAPHICS
        cmp ax, mask VMA_SUPPORTED or mask VMA_GRAPHICS
        jne freeAbsent
        cmp ds:[VMI_Xres], 640
        jne freeAbsent
        cmp ds:[VMI_Yres], 480
        jne freeAbsent
        cmp ds:[VMI_bitsPerPixel], 16
        jne freeAbsent
        cmp ds:[VMI_nplanes], 1
        jne freeAbsent
        cmp ds:[VMI_memModel], 6       ; VBE direct color
        jne freeAbsent
        cmp {byte}ds:[1fh], 5         ; red size/position
        jne freeAbsent
        cmp {byte}ds:[20h], 11
        jne freeAbsent
        cmp {byte}ds:[21h], 6         ; green size/position
        jne freeAbsent
        cmp {byte}ds:[22h], 5
        jne freeAbsent
        cmp {byte}ds:[23h], 5         ; blue size/position
        jne freeAbsent
        cmp {byte}ds:[24h], 0
        jne freeAbsent
        mov al, ds:[VMI_winAAttr]
        and al, 7
        cmp al, 7                    ; present, readable, writable
        jne freeAbsent
        cmp ds:[VMI_winSize], 64
        jne freeAbsent
        cmp ds:[VMI_winASeg], 0
        je freeAbsent
        cmp ds:[VMI_scanSize], 1280
        jb freeAbsent
        ; The bank increment must be integral, nonzero, and <=64.
        mov bx, ds:[VMI_winGran]
        tst bx
        jz freeAbsent
        cmp bx, 64
        ja freeAbsent
        mov ax, 64
        clr dx
        div bx
        tst dx
        jnz freeAbsent
        segmov es, fs
        mov di, offset modeInfo
        mov cx, size VESAModeInfo / 2
        rep movsw
        ; A real-mode far function pointer cannot be called as PM code.
        clr fs:[modeInfo].VMI_winFunc.segment
        clr fs:[modeInfo].VMI_winFunc.offset
        mov fs:[vesaMode], VM_640x480_16
        clr fs:[hostIfVersion]
        mov dx, dosSel               ; DPMI 0101h takes DX, not AX
        segmov ds, fs                ; don't leave the freed selector loaded
        call SysFreeDOSBlock
        mov ax, DP_PRESENT
        clc
        jmp done
freeAbsent:
        segmov ds, fs
        mov dx, dosSel
        call SysFreeDOSBlock
absent:
        mov ax, DP_NOT_PRESENT
        stc
done:
        .leave
        ret
VidTestVESA endp

VidSetVESA proc near
        uses ax, bx, cx, dx
        .enter
        mov ax, VD_VESA_640x480_16
        call VidTestVESA
        jc failed
        mov ax, fs:[G_mainScreenBuffer]
        tst ax
        jnz mapped
        mov ax, fs:[modeInfo].VMI_winASeg
        mov cx, 0ffffh
        call SysMapRealSegment
        jc failed
        mov fs:[G_mainScreenBuffer], ax
mapped:
        ; Set the mode, not just query its information. Bit 14 is clear:
        ; the existing driver is banked, not a linear-framebuffer driver.
        mov ax, 04f02h
        mov bx, VM_640x480_16
        clr cx, dx
        call PMPerfVideoBIOS
        jc unmapFailed
        cmp ax, 004fh
        jne unmapFailed
        mov ax, fs:[G_mainScreenBuffer]
        mov fs:[writeSegment], ax
        mov fs:[readSegment], ax
        mov fs:[modeInfo].VMI_winASeg, ax
        clr fs:[writeWindow]
        clr fs:[readWindow]
        mov fs:[DriverTable].VDI_pageW, 640
        mov fs:[DriverTable].VDI_pageH, 480
        mov ax, fs:[modeInfo].VMI_scanSize
        mov fs:[DriverTable].VDI_bpScan, ax
        mov fs:[DriverTable].VDI_vRes, 72
        mov fs:[DriverTable].VDI_hRes, 72
        mov fs:[DriverTable].VDI_displayType, VGA24_DISPLAY_TYPE
        mov fs:[curWinEnd], 0ffffh
        mov ax, 64
        clr dx
        div fs:[modeInfo].VMI_winGran
        mov fs:[nextWinInc], ax
        ; Force first use to select a bank, even when that bank is zero.
        mov fs:[curWinPage], 0ffffh
        mov fs:[curWinPageSrc], 0ffffh
        clc
        jmp done
unmapFailed:
        mov ax, fs:[G_mainScreenBuffer]
        call SysUnmapRealSegment
        clr fs:[G_mainScreenBuffer]
failed:
        stc
done:
        .leave
        ret
VidSetVESA endp
