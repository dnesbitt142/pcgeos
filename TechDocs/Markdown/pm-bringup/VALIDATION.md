# Validation performed for this source handoff

## Source integrity

The 17 final source files were recovered from the supplied full-product and
incremental update archives. Each is annotated with `PM-BRINGUP` comments.
Removing only those newly added full-line comments and normalizing CRLF/LF yields
exactly the previously supplied source text. Original and final hashes are in
`Tools/build/pm-product/source-manifest.json`.

The 32 handoff tests passed, with no skips. They cover source hashes, comment-only
identity, documented line endings, refusal of unexpected source/symlink paths,
configuration editing, retained TrueType/Perf corrections, PMDIAG2 selection,
source-helper syntax, compiler pins, and native harness **compilation only**.
See `validation/source-tests.txt`. They are not 32 GEOS execution/boot tests.

## Rebuilt affected components

Using the uploaded branch and OpenWatcom 2020-12-01 snapshot, the SDK and necessary
import providers were built. The following five affected components were then
explicitly rebuilt from the annotated sources:

| Component | Result against the previously supplied corrected binary |
|---|---|
| PMDIAG2 Loader32 | Entire executable byte-for-byte identical. |
| Kernel | Same length; all bytes from offset 344 onward identical. |
| VGA16 | Same length; all bytes from offset 344 onward identical. |
| TrueType | Same length; all bytes from offset 344 onward identical. |
| Perf | Same length; all bytes from offset 344 onward identical. |

For the GEOS modules only small header differences remain; their exact offsets
and all hashes are recorded in `validation/affected-build-comparison.json`.
No new binary is shipped in this source-only handoff. Debug-symbol line numbers
can change with annotations even though linked executable payloads do not.

The final forced rebuild completed with no recognized fatal diagnostics under
the supplied log checker. Compiler/assembler warnings remain in
`validation/affected-targets-rebuild.log` and are not hidden.

An initial validation recipe included an unnecessary AnsiC build before its Math
import provider existed. The legacy tool returned success despite a missing
`math.ldf` diagnostic; the log check caught it. That extra step was removed from
the focused check, and all five actual affected targets were rebuilt explicitly.
See `validation/initial-validation-issue.txt`. This is not described as a clean
first-attempt full-product build. Full logs are retained in the separate
`pcgeos-pm-source-validation.zip` evidence archive.

## Patch and packaging checks

See `validation/patch-check.json` for the actual patch round-trip check. The patch
is generated against original files read from the uploaded source archive, with
new-path collisions checked against that archive. Patch application is checked
in a clean baseline checkout, and the result is compared with every overlay file.
Reverse application is also checked. This validates the exact pinned snapshot,
not an uninspected newer GitHub branch head.

The ZIP is a source/document overlay with the repository's directory layout. No
font files, font caches, GEOS executables, compiler executables, object files,
debug symbols, DOS image or `.git` directory is included. The source, patch and
archive checks are distinct from execution testing.

## Not performed in this handoff

No new DOSBox-X or physical PC boot; no all-product application/printing test;
no rerun of every historical native execution fixture; no reconstruction of all
344 full-product executables; no investigation/fix of the latest Celeron kernel
GPF. The adapted full-product wrapper is included, but the whole product graph
and assembly were not rerun in this packaging turn. The user's earlier reports
of working TrueType and Perf fixes remain recorded separately in `STATUS.md`.
