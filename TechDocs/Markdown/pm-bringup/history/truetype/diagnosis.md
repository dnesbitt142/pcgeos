# Diagnosis of the PMDIAG2 TrueType fault

## Observed in the user's screenshot

```text
PMDIAG2 error=0x000C stage=0x000F
fault CS:IP=0637:0419 exception-error=0x0010
EAX=0000031A EBX=00000010 ECX=00000637 EDX=000004CF
ESI=00000000 EDI=00001D70 EBP=0000144E FLAGS=3202
DS=060F ES=04CF FS=0000 GS=0000 SS:SP=00C7:140A
code @CS:IP: 8E C3 89 D9 26 03 4F 1F 89 4E D2 89 D9 26 03 4F
geode=truetype.drvr handle=2130 owner=20C0 resource=0006
CS limit=00000AAF access=00FA
C:\PMGEOS\USERDATA\FONT\TTF>
```

This is the user's guest observation. The native test transcripts are separate,
controlled regression fixtures and are not additional DOSBox-X observations.

## Identification against the exact shipped driver

The shipped full-product driver has SHA-256
`548a1729804011f345264967b5a328446fa667bdf4137367f929f26a88ef663c`.
Its resource 6 starts at physical file offset `0x1E56` and contains `0x0AAB`
bytes. At resource offset `0x0419`, all 16 supplied instruction bytes match
exactly. The first instruction, `8E C3`, is `MOV ES,BX`.

The original driver was rebuilt from the uploaded protected-mode source and
OpenWatcom snapshot. All bytes from executable file offset 344 through EOF,
including resource tables, code and relocations, match the shipped driver.
Only header fields differ; the exact offsets/hashes are in `provenance.json`.
The source-annotated `original-ttinit.lst` identifies the matching instruction
as part of `ProcessFont()` in `ttinit_TEXT`.

## Source cause

`ProcessFont()` declares `FontInfo *fontInfo` without initialization. The
new-family branch assigns it before use. In the existing-family branch,
original `ttinit.c` lines 463–464 instead compute:

```c
outlineData = (OutlineDataEntry*) (((byte*)fontInfo) + fontInfo->FI_outlineTab);
outlineDataEnd = (OutlineDataEntry*) (((byte*)fontInfo) + fontInfo->FI_outlineEnd);
```

without first resolving the current family's FontInfo chunk. A later assignment
in the same branch occurs too late to protect these reads. A value assigned on
a previous function invocation or on the other branch does not initialize this
local pointer on the failing path.

Immediately before the fault, the code asks `LMemDeref()` for the availability
list, using chunk handle `sizeof(LMemBlockHeader)`, or `0x0010`. The kernel C
wrapper returns the far pointer in DX:AX and leaves the chunk handle in the
scratch register BX. The uninitialized-pointer use compiled into a selector
load from that BX. This accounts specifically for the screenshot's `BX=0010`
and the failure at `MOV ES,BX`; it is not a diagnosis based only on the current
directory or a generic protection-fault message.

## Correction

The patch changes only this existing-family initialization sequence:

```c
fontInfoChunk = availEntries[availIndex].FAE_infoHandle;
fontInfo = LMemDerefHandles( fontInfoBlock, fontInfoChunk );
outlineData = (OutlineDataEntry*) (((byte*)fontInfo) + fontInfo->FI_outlineTab);
outlineDataEnd = (OutlineDataEntry*) (((byte*)fontInfo) + fontInfo->FI_outlineEnd);
```

The fixed machine code makes that additional dereference before reading either
outline-range field. TrueType remains enabled; no fonts, caches, INI settings,
loader, kernel or other product modules are replaced.

The directory observation is consistent with this path: `TrueType_InitFonts()`
uses `FilePushDir()` and `FileSetCurrentPath(SP_FONT, "TTF")`, processes the
files, and eventually calls `FilePopDir()`. A fatal error while processing
interrupts that normal return/cleanup sequence.

## What the tests establish—and what they do not

The native harness loads the actual linked driver code resource and data
resource into controlled 16-bit LDT segments on x86-64 Linux. It enters the
existing-family branch at offset `0x040B` with a synthetic function frame and
font-information heap. Imported LMem calls are intercepted; their relevant
arguments, far-pointer result and BX clobber are modeled from the kernel source.
The driver's local style/weight mapping and iteration instructions execute.

The unmodified code produces the same `0637:0419`, `BX=0010`, error `0010`
failure. The corrected code reaches the expected duplicate or new-style path
in eight fixture cases. These tests stop at the next allocation or face-close
call. They do not implement the GEOS memory manager, enumerate real font files,
exercise allocation failures, render glyphs, or boot DOSBox-X.

The package/update tests additionally prepare the actual full product and apply
both earlier diagnostic updates, then this driver update and restoration.
There are 28 passing tests and no skips; see `tests.txt` and
`product-update.json`. The initial fixture mistakes and their correction are
documented in the README and retained logs.

**The exact uninitialized-pointer defect has been corrected and regression
checked. Successful complete guest startup and working Perf still need a new
DOSBox-X run.**
