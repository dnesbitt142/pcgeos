; PM-BRINGUP [PM-DIAG2] ADDED 2026-09-16; ChatGPT-assisted project changes.
; PM-BRINGUP [PM-DIAG2] Base archive commit: 30df506fc64720fd82e528acbcb192adbaa48fce.
; PM-BRINGUP [PM-DIAG2] NEW FILE: bounded register/code/stack snapshot and best-effort GEOS resource identification; terminal reporting, not recovery.
; PM-BRINGUP [PM-DIAG2] See PM-BRINGUP.md and TechDocs/Markdown/pm-bringup/CHANGES.md; original notices retained.
; PMDIAG2 -- terminal fault snapshot. No heap allocation, GEOS API calls,
; file writes, or selector allocation. Snapshot target reads are guarded by
; VERR/LAR/LSL plus range checks. These cannot validate page mappings.
; Metadata is best effort, not a stack trace.
;
; Raw frame offsets after PUSHAD and PUSH DS,ES,FS,GS:
; 0 GS, 2 FS, 4 ES, 6 DS, 8 EDI, 12 ESI, 16 EBP, 20 saved ESP,
; 24 EBX, 28 EDX, 32 ECX, 36 EAX, 40 return IP, 42 return CS,
; 44 exception error, 46 IP, 48 CS, 50 FLAGS, 52 SP, 54 SS.

.386p

loaderDiagnosticBusy BooleanByte BB_FALSE
loaderDiagnosticAborted BooleanByte BB_FALSE
loaderDiagnosticRawFrame byte 56 dup (0)
loaderDiagnosticCodeCount word 0
loaderDiagnosticCode byte 16 dup (0)
loaderDiagnosticStackCount word 0
loaderDiagnosticStackData byte 32 dup (0)
loaderDiagnosticCSLimit dword 0
loaderDiagnosticCSRights word 0
loaderDiagnosticCSValid BooleanByte BB_FALSE
loaderDiagnosticHandle word 0
loaderDiagnosticOwner word 0
loaderDiagnosticResource word 0ffffh
loaderDiagnosticName byte 13 dup (0)
loaderDiagnosticNamed BooleanByte BB_FALSE
; Separate from the application and DPMI exception stacks.
loaderDiagnosticStack byte 1024 dup (0)
loaderDiagnosticStackEnd label byte

; Return CX readable bytes from ES:SI, capped by input CX and 16-bit
; addressing. Reject execute-only, absent, system and expand-down segments.
; BX = selector; DS stays the loader data alias. Does not allocate aliases.
LoaderDiagnosticReadable proc near uses eax, edx
	.enter
	verr	bx
	jnz	bad
	.inst	db 66h		; ESP requires explicit 32-bit operand prefix
	lar	ax, bx
	jnz	bad
	; LAR access byte is bits 8..15. VERR doesn't test present on every CPU.
	test	ax, 8000h
	jz	bad
	test	ax, 0800h
	jnz	ordinary
	test	ax, 0400h	; expand-down data requires different bounds
	jnz	bad
ordinary:
	.inst	db 66h
	lsl	dx, bx
	jnz	bad
	clr	eax
	mov	ax, si
	cmp	eax, edx
	ja	bad
	; Clamp segment limit to FFFFh before using 16-bit offsets.
	cmp	edx, 0ffffh
	jbe	limited
	mov	edx, 0ffffh
limited:
	sub	edx, eax
	inc	edx
	clr	eax
	mov	ax, cx
	cmp	eax, edx
	jbe	okay
	mov	cx, dx
okay:
	mov	es, bx
	jmp	done
bad:
	clr	cx
 done:
	.leave
	ret
LoaderDiagnosticReadable endp

LoaderDiagnosticProbe proc near uses ax, bx, cx, dx, si, di, es
	.enter
	cld
	mov	bx, {word}ds:[loaderDiagnosticFault+2]
	.inst	db 66h
	lsl	ax, bx
	jnz	noCSInfo
	mov	ds:[loaderDiagnosticCSLimit], eax
	.inst	db 66h		; ESP requires explicit 32-bit operand prefix
	lar	ax, bx
	jnz	noCSInfo
	shr	eax, 8
	mov	ds:[loaderDiagnosticCSRights], ax
	mov	ds:[loaderDiagnosticCSValid], BB_TRUE
noCSInfo:
	mov	si, {word}ds:[loaderDiagnosticFault]
	mov	cx, 16
	call	LoaderDiagnosticReadable
	mov	ds:[loaderDiagnosticCodeCount], cx
	mov	di, offset loaderDiagnosticCode
	jcxz	stackProbe
codeLoop:
	mov	al, es:[si]
	mov	ds:[di], al
	inc	si
	inc	di
	loop	codeLoop
stackProbe:
	mov	bx, {word}ds:[loaderDiagnosticRawFrame+54]
	mov	si, {word}ds:[loaderDiagnosticRawFrame+52]
	mov	cx, 32
	call	LoaderDiagnosticReadable
	and	cx, 0fffeh	; print only complete words
	mov	ds:[loaderDiagnosticStackCount], cx
	mov	di, offset loaderDiagnosticStackData
	jcxz	ownerProbe
stackLoop:
	mov	al, es:[si]
	mov	ds:[di], al
	inc	si
	inc	di
	loop	stackLoop
ownerProbe:
	call	LoaderDiagnosticFindOwner
	.leave
	ret
LoaderDiagnosticProbe endp

LoaderDiagnosticFindOwner proc near uses ax, bx, cx, dx, si, di, es
	.enter
	; Loader bounds are stable for this fixed handle-table build. Do not
	; follow kernel pointers before the loader has constructed the table.
	cmp	ds:[loaderDiagnosticStage], 13
	jb	done
	mov	bx, ds:[loaderVars].KLV_dgroupSegment
	mov	si, ds:[loaderVars].KLV_handleTableStart
	mov	di, ds:[loaderVars].KLV_lastHandle
	test	si, 0fh
	jnz	done
	test	di, 0fh
	jnz	done
	cmp	si, 100h
	jb	done
	cmp	di, si
	jbe	done
	mov	cx, di
	sub	cx, si
	mov	dx, cx
	call	LoaderDiagnosticReadable
	cmp	cx, dx
	jne	done
	; Each entry is exactly sizeof(HandleMem) = 16 bytes.
	mov	ax, {word}ds:[loaderDiagnosticFault+2]
handleLoop:
	cmp	es:[si].HM_addr, ax
	je	found
	add	si, size HandleMem
	cmp	si, di
	jb	handleLoop
	jmp	done
found:
	mov	ds:[loaderDiagnosticHandle], si
	mov	bx, es:[si].HM_owner
	mov	ds:[loaderDiagnosticOwner], bx
	test	bx, 0fh
	jnz	done
	cmp	bx, ds:[loaderVars].KLV_handleTableStart
	jb	done
	cmp	bx, di
	jae	done
	; All table reads are inside the span checked above.
	mov	bx, es:[bx].HM_addr
	clr	si
	mov	cx, size GeodeHeader
	call	LoaderDiagnosticReadable
	cmp	cx, size GeodeHeader
	jne	done
	mov	ax, ds:[loaderDiagnosticOwner]
	cmp	es:[GH_geodeHandle], ax
	jne	done
	mov	si, offset GH_geodeName
	mov	di, offset loaderDiagnosticName
	mov	cx, GEODE_NAME_SIZE
	call	copyName
	mov	{byte}ds:[di], '.'
	inc	di
	mov	cx, GEODE_NAME_EXT_SIZE
	call	copyName
	mov	ds:[loaderDiagnosticNamed], BB_TRUE
	; Locate the matched handle in the owner's resource table. The index
	; is the actual GEOS resource ID, not a guessed selector-to-ID formula.
	mov	si, es:[GH_resHandleOff]
	mov	di, es:[GH_resCount]
	cmp	di, 1
	jb	done
	cmp	di, 4096
	ja	done
	mov	cx, di
	shl	cx, 1
	mov	dx, cx
	call	LoaderDiagnosticReadable
	cmp	cx, dx
	jne	done
	clr	dx
	mov	ax, ds:[loaderDiagnosticHandle]
resourceLoop:
	cmp	es:[si], ax
	je	gotResource
	add	si, 2
	inc	dx
	cmp	dx, di
	jb	resourceLoop
	jmp	done
 gotResource:
	mov	ds:[loaderDiagnosticResource], dx
 done:
	.leave
	ret
copyName:
	mov	al, es:[si]
	cmp	al, 20h
	jb	badChar
	cmp	al, 7eh
	ja	badChar
	jmp	storeChar
badChar:
	mov	al, '?'
storeChar:
	mov	ds:[di], al
	inc	si
	inc	di
	loop	copyName
	retn
LoaderDiagnosticFindOwner endp

; DOS output helpers preserve caller state. EAX variant prints all 32 bits.
LoaderDiagnosticText proc near uses ax
	.enter
	mov	ah, 09h
	int	21h
	.leave
	ret
LoaderDiagnosticText endp
LoaderDiagnosticChar proc near uses ax
	.enter
	mov	ah, 02h
	int	21h
	.leave
	ret
LoaderDiagnosticChar endp
LoaderDiagnosticHexDword proc near
	push	eax
	shr	eax, 16
	call	LoaderDiagnosticHexWord
	pop	eax
	call	LoaderDiagnosticHexWord
	ret
LoaderDiagnosticHexDword endp
LoaderDiagnosticHexByte proc near uses ax, bx, cx, dx
	.enter
	mov	bl, al
	mov	cx, 2
byteLoop:
	rol	bl, 4
	mov	dl, bl
	and	dl, 0fh
	add	dl, '0'
	cmp	dl, '9'
	jbe	digit
	add	dl, 'A'-'9'-1
digit:
	call	LoaderDiagnosticChar
	loop	byteLoop
	.leave
	ret
LoaderDiagnosticHexByte endp

reg32Labels word offset labelEAX, offset labelEBX, offset labelECX, offset labelEDX, offset labelESI, offset labelEDI, offset labelEBP
reg32FrameOffsets word 36, 24, 32, 28, 12, 8, 16
reg16Labels word offset labelFlags, offset labelDS, offset labelES, offset labelFS, offset labelGS, offset labelSS, offset labelSP
reg16FrameOffsets word 50, 6, 4, 2, 0, 54, 52
labelEAX char 13,10,'EAX=$'
labelEBX char ' EBX=$'
labelECX char ' ECX=$'
labelEDX char ' EDX=$'
labelESI char 13,10,'ESI=$'
labelEDI char ' EDI=$'
labelEBP char ' EBP=$'
labelFlags char ' FLAGS=$'
labelDS char 13,10,'DS=$'
labelES char ' ES=$'
labelFS char ' FS=$'
labelGS char ' GS=$'
labelSS char ' SS:SP=$'
labelSP char ':$'
labelCode char 13,10,'code @CS:IP: $'
labelStack char 13,10,'stack @SS:SP: $'
labelStack2 char 13,10,'stack +0010:  $'
labelUnavailable char '<unavailable>$'
labelGeode char 13,10,'geode=$'
labelUnresolved char '<unresolved>$'
labelHandle char ' handle=$'
labelOwner char ' owner=$'
labelResource char ' resource=$'
labelCSLimit char 13,10,'CS limit=$'
labelCSRights char ' access=$'
labelPartial char 13,10,'Snapshot aborted; first fault preserved.$'

LoaderDiagnosticPrint proc near uses ax, bx, cx, dx, si, di
	.enter
	clr	si
	mov	cx, 7
reg32Loop:
	mov	dx, ds:[reg32Labels+si]
	call	LoaderDiagnosticText
	mov	bx, ds:[reg32FrameOffsets+si]
	mov	eax, {dword}ds:[loaderDiagnosticRawFrame+bx]
	call	LoaderDiagnosticHexDword
	add	si, 2
	loop	reg32Loop
	clr	si
	mov	cx, 7
reg16Loop:
	mov	dx, ds:[reg16Labels+si]
	call	LoaderDiagnosticText
	mov	bx, ds:[reg16FrameOffsets+si]
	mov	ax, {word}ds:[loaderDiagnosticRawFrame+bx]
	call	LoaderDiagnosticHexWord
	add	si, 2
	loop	reg16Loop
	mov	dx, offset labelCode
	call	LoaderDiagnosticText
	mov	cx, ds:[loaderDiagnosticCodeCount]
	jcxz	noCode
	mov	si, offset loaderDiagnosticCode
codePrint:
	mov	al, ds:[si]
	call	LoaderDiagnosticHexByte
	mov	dl, ' '
	call	LoaderDiagnosticChar
	inc	si
	loop	codePrint
	jmp	printStack
noCode:
	mov	dx, offset labelUnavailable
	call	LoaderDiagnosticText
printStack:
	mov	dx, offset labelStack
	call	LoaderDiagnosticText
	mov	cx, ds:[loaderDiagnosticStackCount]
	shr	cx, 1
	jcxz	noStack
	mov	si, offset loaderDiagnosticStackData
stackPrint:
	cmp	si, offset loaderDiagnosticStackData+16
	jne	notSecond
	mov	dx, offset labelStack2
	call	LoaderDiagnosticText
notSecond:
	mov	ax, ds:[si]
	call	LoaderDiagnosticHexWord
	mov	dl, ' '
	call	LoaderDiagnosticChar
	add	si, 2
	loop	stackPrint
	jmp	printOwner
noStack:
	mov	dx, offset labelUnavailable
	call	LoaderDiagnosticText
printOwner:
	mov	dx, offset labelGeode
	call	LoaderDiagnosticText
	tst	ds:[loaderDiagnosticNamed]
	jz	unresolved
	mov	si, offset loaderDiagnosticName
	mov	cx, 13
namePrint:
	mov	dl, ds:[si]
	call	LoaderDiagnosticChar
	inc	si
	loop	namePrint
	jmp	ownerIds
unresolved:
	mov	dx, offset labelUnresolved
	call	LoaderDiagnosticText
ownerIds:
	mov	dx, offset labelHandle
	call	LoaderDiagnosticText
	mov	ax, ds:[loaderDiagnosticHandle]
	call	LoaderDiagnosticHexWord
	mov	dx, offset labelOwner
	call	LoaderDiagnosticText
	mov	ax, ds:[loaderDiagnosticOwner]
	call	LoaderDiagnosticHexWord
	mov	dx, offset labelResource
	call	LoaderDiagnosticText
	mov	ax, ds:[loaderDiagnosticResource]
	call	LoaderDiagnosticHexWord
	mov	dx, offset labelCSLimit
	call	LoaderDiagnosticText
	tst	ds:[loaderDiagnosticCSValid]
	jz	noLimit
	mov	eax, ds:[loaderDiagnosticCSLimit]
	call	LoaderDiagnosticHexDword
	mov	dx, offset labelCSRights
	call	LoaderDiagnosticText
	mov	ax, ds:[loaderDiagnosticCSRights]
	call	LoaderDiagnosticHexWord
	jmp	checkAborted
noLimit:
	mov	dx, offset labelUnavailable
	call	LoaderDiagnosticText
checkAborted:
	tst	ds:[loaderDiagnosticAborted]
	jz	done
	mov	dx, offset labelPartial
	call	LoaderDiagnosticText
done:
	.leave
	ret
LoaderDiagnosticPrint endp
