# Building the consolidated fork

## Inputs

Use a checkout at the pinned uploaded branch snapshot with this overlay applied,
and the user's OpenWatcom **2020-12-01-Build** snapshot, not a newer release.
The exact archive and compiler-binary SHA-256 hashes are recorded in
`Tools/build/pm-product/reports/inputs.json`.

The build environment needs Linux, Bash, Perl, Python 3.10 or later, GCC and the
pinned compiler. Preserve the repository's `Installed/` makefiles/dependency
metadata. Use a **lowercase source path without spaces** because of the legacy
product tooling. A clean source extraction avoids mixing stale objects or other
product variants. The scripts intentionally reject an existing output directory.

## Full-product build

From the fork's root, for example:

```sh
bash Tools/build/pm-product/scripts/build.sh \
  /home/me/pcgeos-pm \
  /home/me/ow-20201201 \
  /home/me/pmgeos-output
```

On an x86-64 Linux environment unable to execute the static i386 SDK tools,
append `--bridge` to use the included limited host bridge. That bridge only runs
those trusted SDK tools; it is not a DOS emulator or a way to boot GEOS.

The wrapper verifies the pinned compiler binaries, builds the SDK, compiles the
core, runs the selected product graph, builds explicit GDI/FJPEG/Stub32 variants,
then assembles and audits the product. The uploaded branch's `product_flags`
already supplies `PROTECTED_MODE`. **For this repository, `pmake -n` selects
NO_EC; it is not the usual make dry-run option.**

`PM_PERF_MINIMAL=1` is retained for the four-meter Perf and narrow 640x480 RGB565
VGA16 paths. Do not omit that flag expecting the other original Perf measurements
to have been ported. This is a source handoff of the working subset, not the
previously requested all-statistics enhancement.

The local assembler uses your own source-tree product data, fonts, font cache and
HDPMI16 host. None of those binaries/font files is redistributed in this handoff.
The output is a DOS application directory, not a bootable disk image. Its fresh
`START.BAT` starts the DPMI host and loader. Keep the original working runtime
separate while testing a new build.

## Incremental component builds

With the SDK and the relevant import providers already built, and `ROOT_DIR`,
`LOCAL_ROOT`, `WATCOM`, `PATH` and `INCLUDE` set as in `scripts/build.sh`:

```sh
(cd "$ROOT_DIR/Installed/Loader32/Text"; rm -f loader.obj; pmake -n loader.exe)
(cd "$ROOT_DIR/Installed/Appl/Perf"; rm -f perf.obj perf.rdef; pmake -n PM_PERF_MINIMAL=1 part)
(cd "$ROOT_DIR/Installed/Driver/Font/TrueType"; rm -f ttinit.obj; pmake -n part)
(cd "$ROOT_DIR/Installed/Driver/Video/VGAlike/VGA16"; rm -f vga16Manager.obj; pmake -n PM_PERF_MINIMAL=1 part)
```

Those are incremental commands, not a substitute for bootstrapping the SDK and
import libraries. Kernel changes need their own kernel build and dependent
relinks as appropriate; the full wrapper builds the kernel first.

## Handoff checks

```sh
python3 Tools/build/pm-product/scripts/check-source.py --current .
python3 -m unittest discover -s Tools/build/pm-product/tests -p 'test_source_handoff.py' -v
```

The hash check is for the supplied annotated snapshot. It is expected to fail
after you intentionally edit those files, and it is not enforced by the build
wrapper for development forks. The test suite distinguishes source/configuration
checks from native fixture sources and guest tests. Historical native harnesses
are described in `Tools/build/pm-product/tests/native/README.md`.

`git -c core.whitespace=cr-at-eol diff --check` can be useful when reviewing this
legacy mixed-LF/CRLF tree. Existing source formatting is not globally normalized.
The root `README.md`, upstream build defaults, and existing licensing files are
not replaced by this handoff.
