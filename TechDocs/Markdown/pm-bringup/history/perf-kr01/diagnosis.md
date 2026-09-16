# Perf KR-01: omitted initialization, retained sampling

## Finding

The user's new report is `KR-01` when Perf starts, after the TrueType correction
allowed startup to proceed. No new guest fault address or register dump was
supplied. The fault identified here is a **definite, independently reproduced
bug in the shipped minimal-Perf profile**, consistent with that report. It is
not a claim that the guest's faulting instruction has been captured this time.

In the supplied kernel, `Library/Kernel/Boot/bootStrings.asm:54` maps
`KS_TE_DIVIDE_BY_ZERO` to `KR-01`. The thread exception handler selects that
message for a divide exception. This follows the kernel's thread-exception path,
so the absence of a loader PMDIAG2 register dump does not contradict the report.

## The mismatch introduced in the earlier build

The full product was assembled with `PM_PERF_MINIMAL=1` for Perf, retaining the
four-meter bring-up variant. Its `Appl/Perf/init.asm:127–129` deliberately skips
`InitHandleStats`. That routine would normally initialize `handlesPerPixel`.
The variable is in `udata`, outside `StartStateData..EndStateData`; the kernel's
`AllocateResource` adds `HAF_ZERO_INIT` for dgroup. Its initial value is therefore
zero, and it is not restored from the application's saved state.

However, the original `Appl/Perf/calc.asm:78–116` still calls every calculator
on each timer sample. This includes `PerfCalcFreeHandles`, even with the Handles
graph disabled. That function reads the free-handle count and then executes:

```asm
mov si, ds:[handlesPerPixel]
div si
```

It does not test either the graph flag or the divisor. The combination of skipped
initialization and unconditional calculation is the defect. Merely hiding the
graph was insufficient. This mismatch was introduced by the earlier profile
changes supplied in this conversation, not by an asserted change to DOSBox-X.

## Match to the shipped machine code

The original full-product `PERF.GEO`, SHA-256
`08ca19b5f2f58cada9000be089e355785b3e0b1e8978e09ff3685610dfce5b72`,
has `PerfCalcStatCode` as resource 4. The resource begins with calls to the four
retained calculators followed by nine legacy calculators. At offset `002C`, it
calls the handle calculator at `02CD`.

At `02DD` it contains `8B 36 54 07` (`MOV SI,[0754]`), followed at `02E1` by
`F7 F6` (`DIV SI`). Dgroup offset `0754` is `handlesPerPixel`.

The native regression harness runs the actual resource, with controlled
`SysStatistics` and `SysGetInfo` calls. It reproduces CPU exception vector 0 at
resource offset `02E1`, with `SI=0000`, on the first sample. Three distinct
fixtures reproduce the same defect. **Selector `0637` in the test transcript is
a harness choice; it is not a reported address from this new guest failure.**

## Correction

`source/perf-kr01.patch` places the nine unsupported calculator calls under
`ifndef PM_PERF_MINIMAL`, matching their initialization and UI configuration.
The minimal variant now calls only:

```text
SysStatistics
PerfCalcContextSwitches
PerfCalcLoadAverage
PerfCalcInterrupts
PerfCalcCPUUsage
return
```

This does not disable Perf, change the four retained calculations, fake counters,
initialize a made-up handle denominator, or suppress a kernel error. The unused
legacy helper routines remain in the resource but are not called by this profile.
A build without the minimal flag retains its original sampling path; actual
before/after nonminimal builds have identical executable payloads.

Only resource 4 differs between the old and corrected minimal executables. All
other resource bytes/relocations, including the application data and UI layout,
are unchanged. No loader, kernel, video, TrueType, INI, font or cache change is
part of this update.

## Verification and limits

All 35 host/package tests passed without skips. The corrected linked sampling
routine passes CPU boundary, low-count calibration, changing-input and disabled-
state tests. One case executes 120 successive calls and checks all four numeric
and graph outputs against an independent integer model of the retained code.
No free-handle query occurs in the corrected sampling path.

The native harness runs 16-bit instructions on the Linux host with controlled
segment memory; it substitutes the GEOS kernel calls. It does **not** boot GEOS,
exercise real kernel counters, draw windows, or test DOSBox-X input/timing.
A prepared installation test applies both diagnostic loaders and the TrueType
fix first, then verifies that only Perf changes and every other existing file is
preserved. The full 342-module import/configuration audit still passes.

The actual guest still needs to be tried. KR-01 by itself cannot exclude another
divide fault elsewhere. This update addresses the reproduced handle-meter defect;
it is not a claim that all legacy Perf arithmetic or the full product is audited.

## Evidence

- `source-evidence.txt`: source excerpts and line numbers.
- `original-calc.dis` / `fixed-calc.dis`: linked sampling resources, disassembled.
- `native-transcript.txt`: actual focused native test output.
- `tests.txt`: complete 35-test result.
- `product-update.json`: one-file update and restoration results.
- `provenance.json`: input/build hashes, header differences and verification scope.
- `nonminimal-comparison.json`: preserved nonminimal executable payload.
