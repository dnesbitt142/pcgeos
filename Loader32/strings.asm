; PM-BRINGUP [PM-DIAG2] CHANGED 2026-09-16; ChatGPT-assisted project changes.
; PM-BRINGUP [PM-DIAG2] Base archive commit: 30df506fc64720fd82e528acbcb192adbaa48fce.
; PM-BRINGUP [PM-DIAG2] Replace the misleading strings-file fallback with per-error messages and PMDIAG2 output; exit DOS with failure status.
; PM-BRINGUP [PM-DIAG2] See PM-BRINGUP.md and TechDocs/Markdown/pm-bringup/CHANGES.md; original notices retained.
COMMENT @----------------------------------------------------------------------

	Copyright (c) GeoWorks 1991 -- All Rights Reserved

PROJECT:	PC GEOS
MODULE:		Loader
FILE:		strings.asm

ROUTINES:
	Name			Description
	----			-----------
   	ReadStringsFile		Read in the strings file

REVISION HISTORY:
	Name	Date		Description
	----	----		-----------
	Tony	1/91		Initial version

DESCRIPTION:

	$Id: strings.asm,v 1.1 97/04/04 17:26:44 newdeal Exp $

------------------------------------------------------------------------------@

stringsFileName	char	"geos.str",0
stringsFileRead	BooleanByte	BB_FALSE


COMMENT @----------------------------------------------------------------------

FUNCTION:	ReadStringsFile

DESCRIPTION:	Read the kernel strings file both into the buffer in this
		segment.

CALLED BY:	INTERNAL

PASS:
	ds, es - loader segment

RETURN:
	stringsFileRead - true if strings file can be read

DESTROYED:
	ax, bx, cx, dx, si, di, bp

REGISTER/STACK USAGE:

PSEUDO CODE/STRATEGY:

KNOWN BUGS/SIDE EFFECTS/CAVEATS/IDEAS:

REVISION HISTORY:
	Name	Date		Description
	----	----		-----------
	Tony	11/90		Initial version

------------------------------------------------------------------------------@

ReadStringsFile	proc	near

	;USE LOWEST COMMON DENOMINATOR FOR ACCESS FLAGS. DOS 2.X doesn't like
	; sharing modes or inheritance bits...

	mov	ax, (MSDOS_OPEN_FILE shl 8) or FA_READ_ONLY
	mov	dx, offset stringsFileName
	int	21h
if	REQUIRE_STRINGS_FILE
	ERROR_C	LS_CANNOT_OPEN_STRINGS_FILE
else
	jc	done
endif
	mov	bx, ax

	; read strings into buffer
	mov	ah, MSDOS_READ_FILE
	mov	cx, MAX_STRING_FILE_SIZE
	mov	dx, STR_BUFFER
	int	21h				;ax = bytes read
if	REQUIRE_STRINGS_FILE
	ERROR_C	LS_CANNOT_OPEN_STRINGS_FILE
else
	jc	done
endif

	; store 0 at end
	mov	si, dx
	add	si, ax
	mov	{byte} ds:[si], 0

	mov	ah, MSDOS_CLOSE_FILE
	int	21h
if	REQUIRE_STRINGS_FILE
	ERROR_C	LS_CANNOT_OPEN_STRINGS_FILE
else
	jc	done
endif

	; 1) replace the CR,LF pairs with just CR
	; 2) remove comments

	mov	si, STR_BUFFER
	mov	di, si
	mov	cx, NUMBER_OF_STRINGS_IN_STRINGS_FILE

startOfLineLoop:
	lodsb
	cmp	al, '#'				;if a comment then skip it
	jz	skipComment
	stosb
	tst	al				;if NULL then done
	jz	compactDone
	cmp	al, C_CR			;if CR then skip LF
	jz	skipLF

	; middle of line -- copy until CR, LF

middleOfLineLoop:
	lodsb
	stosb
	tst	al
	jz	compactDone
	cmp	al, C_CR
	jnz	middleOfLineLoop
	dec	cx				;if done then exit
	jcxz	compactDoneStoreNull
skipLF:
	lodsb
	jmp	startOfLineLoop

	; comment - skip it

skipComment:
	lodsb
	tst	al
if	REQUIRE_STRINGS_FILE
	ERROR_Z	LS_CANNOT_OPEN_STRINGS_FILE
else
	jz	done
endif
	cmp	al, C_LF
	jnz	skipComment
	jmp	startOfLineLoop

compactDoneStoreNull:
	clr	al
	stosb
compactDone:

	tst	cx
if	REQUIRE_STRINGS_FILE
	ERROR_NZ	LS_CANNOT_OPEN_STRINGS_FILE
else
	jnz	done
endif

	mov	ds:[stringsFileRead], BB_TRUE

ife	REQUIRE_STRINGS_FILE
done:
endif

	.leave
	ret

ReadStringsFile	endp


COMMENT @----------------------------------------------------------------------

FUNCTION:	LoaderError

DESCRIPTION:	Handle a loader error

CALLED BY:	UTILITY

PASS:
	ax - KernelStrings

RETURN:

DESTROYED:

REGISTER/STACK USAGE:

PSEUDO CODE/STRATEGY:

KNOWN BUGS/SIDE EFFECTS/CAVEATS/IDEAS:

REVISION HISTORY:
	Name	Date		Description
	----	----		-----------
	Tony	1/91		Initial version

------------------------------------------------------------------------------@
crlfString	char	13, 10, '$'

; PM-BRINGUP [PM-DIAG2] CHANGED error reporting: numeric reason/stage plus snapshot; DOS exit status is now 1.
LoaderError	proc	near
	LoaderDS
	LoaderES
	push	ax
	mov	ax, es
	and	ax, ax
	jne	validSeg
	; Must still be in real mode.
	mov	ax, kcode
	mov	es, ax
	mov	ds, ax
validSeg:
	pop	ax

	; This is a terminal error path. Keep the original ordinal on the stack
	; because both PrintString and DOS output may overwrite AX.
	push	ax
	mov	dx, offset crlfString
	mov	ah, MSDOS_DISPLAY_STRING
	int	21h
	mov	ax, LS_ERROR_PREFIX
	call	PrintString
	pop	ax
	push	ax
	call	PrintString

	mov	dx, offset diagnosticErrorPrefix
	mov	ah, MSDOS_DISPLAY_STRING
	int	21h
	pop	ax
	call	LoaderDiagnosticHexWord
	mov	dx, offset diagnosticStagePrefix
	mov	ah, MSDOS_DISPLAY_STRING
	int	21h
	mov	ax, ds:[loaderDiagnosticStage]
	call	LoaderDiagnosticHexWord

	tst	ds:[loaderDiagnosticFaultValid]
	jz	noFault
	mov	dx, offset diagnosticFaultPrefix
	mov	ah, MSDOS_DISPLAY_STRING
	int	21h
	mov	ax, {word} ds:[loaderDiagnosticFault+2]
	call	LoaderDiagnosticHexWord
	mov	dl, ':'
	mov	ah, 02h                  ; DOS character output
	int	21h
	mov	ax, {word} ds:[loaderDiagnosticFault]
	call	LoaderDiagnosticHexWord
	mov	dx, offset diagnosticFaultErrorPrefix
	mov	ah, MSDOS_DISPLAY_STRING
	int	21h
	mov	ax, ds:[loaderDiagnosticFaultError]
	call	LoaderDiagnosticHexWord
	call	LoaderDiagnosticPrint
noFault:
	mov	dx, offset crlfString
	mov	ah, MSDOS_DISPLAY_STRING
	int	21h

	; Report a nonzero DOS exit status instead of the previous false success.
	mov	ax, 4c01h
	int	21h
	.unreached
LoaderError	endp

; Pure hexadecimal formatting; does not require a loaded strings file or heap.
; PM-BRINGUP [PM-DIAG2] ADDED allocation-free hexadecimal formatter for the terminal error path.
LoaderDiagnosticHexWord proc near uses ax, bx, cx, dx
	.enter
	mov	bx, ax
	mov	cx, 4
hexLoop:
	rol	bx, 4
	mov	dl, bl
	and	dl, 0fh
	add	dl, '0'
	cmp	dl, '9'
	jbe	hexDigit
	add	dl, 'A'-'9'-1
hexDigit:
	push	bx, cx
	mov	ah, 02h
	int	21h
	pop	bx, cx
	loop	hexLoop
	.leave
	ret
LoaderDiagnosticHexWord endp



COMMENT @----------------------------------------------------------------------

FUNCTION:	PrintString

DESCRIPTION:	Print a string from the strings file

CALLED BY:	LoaderError

PASS:
	ds, es - loader segment
	ax - KernelStrings

RETURN:
	none

DESTROYED:
	ax, bx, cx, dx, si, di, bp

REGISTER/STACK USAGE:

PSEUDO CODE/STRATEGY:

KNOWN BUGS/SIDE EFFECTS/CAVEATS/IDEAS:

REVISION HISTORY:
	Name	Date		Description
	----	----		-----------
	Tony	1/91		Initial version

------------------------------------------------------------------------------@
; PM-BRINGUP [PM-DIAG2] CHANGED fallback selection: every valid LoaderStrings ordinal has a matching built-in message.
PrintString	proc	near
	; Bounds check also covers errors not included in external GEOS.STR.
	cmp	ax, LS_CANNOT_LOAD_XIP_KERNEL_WITH_STANDARD_LOADER
	jne	notXIP
	mov	si, offset cannotLoadXIPKernel
	jmp	print
notXIP:
	cmp	ax, NUMBER_OF_STRINGS_IN_STRINGS_FILE
	jb	knownError
	mov	si, offset unknownLoaderError
	jmp	print
knownError:
	tst	ds:[stringsFileRead]
	jnz	lookInBuffer

	; Each LoaderStrings value has its own built-in message. In particular,
	; an INI, memory, kernel or protection fault is NOT a GEOS.STR error.
	mov	bx, ax
	shl	bx, 1
	mov	si, ds:[loaderDefaultStrings][bx]
	jmp	print

lookInBuffer:
	mov	si, STR_BUFFER

	; skip strings before the one we want
	clr	cx
	mov	cl, al			; cx = # strings to skip
	jcxz	print			;if no more strings to skip then got it

skipLineLoop:
	lodsb
	cmp	al, C_CR
	jnz	skipLineLoop
	loop	skipLineLoop

	; print it
print:
	mov	dx, si
	mov	ah, MSDOS_DISPLAY_STRING
	int	21h
	ret
PrintString	endp

	; Built-in English diagnostics used when no external strings were read.
; PM-BRINGUP [PM-DIAG2] ADDED ordered defaults and diagnostic state; the existing external-string path is retained.
loaderDefaultStrings label word
	word offset systemError
	word offset cannotLocateKernelString
	word offset cannotOpenStringsFile
	word offset cannotOpenIniFile
	word offset cannotReadIniFile
	word offset corruptIniFile
	word offset iniFileTooLarge
	word offset cannotLoadKernel
	word offset notEnoughMemory
	word offset malformedPath
	word offset invalidMemoryArgument
	word offset cantStartGPMI
	word offset generalProtectionFault
if LOAD_DOS_EXTENDER
	word offset cantLoadDOSExtender
endif
CheckHack <(($ - loaderDefaultStrings) / (size word)) eq NUMBER_OF_STRINGS_IN_STRINGS_FILE>

systemError char "System Error: $"
NEC <cannotLocateKernelString char "cannot locate geos.ini or kernel geos.geo$" >
EC <cannotLocateKernelString char "cannot locate geosec.ini or kernel geosec.geo$" >
cannotOpenStringsFile char "cannot load strings file (geos.str)$"
cannotOpenIniFile char "cannot open system configuration file (geos.ini)$"
cannotReadIniFile char "cannot read system configuration file (geos.ini)$"
corruptIniFile char "system configuration file (geos.ini) is corrupt$"
iniFileTooLarge char "system configuration file (geos.ini) is too large$"
cannotLoadKernel char "cannot load kernel (SYSTEM\\GEOS.GEO)$"
notEnoughMemory char "not enough memory for GEOS loader/kernel startup$"
malformedPath char "illegal path specification$"
invalidMemoryArgument char "invalid /mNNN or /rNNN argument$"
cantStartGPMI char "GEOS requires DPMI$"
generalProtectionFault char "general protection fault in protected mode$"
if LOAD_DOS_EXTENDER
cantLoadDOSExtender char "Cannot load DOS Extender (dosext.exe)$"
endif
cannotLoadXIPKernel char "cannot load XIP kernel with standard loader$"
unknownLoaderError char "unrecognized loader error (see numeric code below)$"

diagnosticErrorPrefix char 13, 10, "PMDIAG2 error=0x$"
diagnosticStagePrefix char " stage=0x$"
diagnosticFaultPrefix char 13, 10, "fault CS:IP=$"
diagnosticFaultErrorPrefix char " exception-error=0x$"

; Stage 0 includes startup before the writable loader alias is established.
; 000F means the loader reached kernel handoff, not which later geode failed.
loaderDiagnosticStage word 0
loaderDiagnosticFaultValid BooleanByte BB_FALSE
loaderDiagnosticFault dword 0
loaderDiagnosticFaultError word 0

; PM-BRINGUP [PM-DIAG2] ADDED snapshot helper include; no QR renderer, file logging or application recovery is implemented.
include fault.asm
