# Implementation and verification status

This file is the current handoff summary. Files in `history/` preserve older
reports exactly as supplied and may describe failures that were subsequently
fixed; read them as historical evidence, not as the latest project status.

| Item | State at handoff |
|---|---|
| Loader/GPMI, kernel selector forwarding, restricted VBE startup | Implemented source included; compiled in earlier candidate builds. |
| Full-product build graph and local assembly profile | Included, with the 18 recorded exclusions; not an exhaustive product certification. |
| PMDIAG1 error-message correction | Included as part of final PMDIAG2 source. |
| PMDIAG2 GPF register/code/stack/resource capture | Implemented; the user supplied screenshots showing it running. |
| TrueType existing-family uninitialized pointer | Fixed; user reported that update resolved the crash. |
| Perf first-update KR-01 | Fixed in the four-meter profile; user reported that update works. |
| All measurements from original Perf in protected mode | **Not implemented or verified.** The four-meter profile remains selected. |
| Non-fatal application exceptions / crash containment | **Discussion only; no implementation.** |
| QR-encoded crash record or robust graphics renderer | **Design discussion only; no implementation.** |
| Crash-file, serial-port or audible diagnostics | **Not implemented.** |
| Latest Celeron/FreeDOS hardware GPF | **Open; no fix included.** |

## Hardware report still to investigate

The user reported a P3 Celeron running FreeDOS and a memory manager described as
“Jrmmex”. The photograph shows the DPMI host becoming resident, followed by:

```text
PMDIAG2 error=0x000C stage=0x000F
fault CS:IP=00E7:079E exception-error=0x0000
geode=geos    .kern handle=14F0 owner=14D0 resource=0003
CS limit=0000CFFF access=00FB
code @CS:IP: 26 89 37 1F CB 3B F5 75 F7 26 89 0F 1F CB 3B F5
```

The prompt returns in `...\PMGEOS\SYSTEM\FS`. These are observations from the
photograph, not a diagnosed cause. In particular, stage `000F` means kernel
handoff was reached; it does not establish which later operation caused the
fault. Neither the memory manager nor the filesystem driver has been proven to
be responsible. Do not describe the earlier TrueType or Perf fix as a correction
for this different fault.

## What “works” means here

The user's confirmations are valuable real-guest evidence for the two reported
fixes. They are not a claim that every desktop application, printer, filesystem,
DPMI host or hardware configuration was tested. No new DOSBox-X or physical PC
boot is performed merely by packaging these files. Handoff-specific source and
build checks are listed separately in `VALIDATION.md`.

## Follow-on work, not part of this archive

Restore the legacy Perf memory/heap/fragmentation/handle/swap/PPP measurements only
after their data sources and invariants have been validated in this protected-mode
kernel. Removing the profile flag alone does not establish that the original
measurements are correct or safe. Investigate the physical-machine kernel GPF
independently. Implement and test any QR diagnostic renderer or exception-recovery
mechanism as separately reviewable changes rather than labelling a design as code.
