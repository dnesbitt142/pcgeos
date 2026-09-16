# Protected-mode PC/GEOS: consolidated source handoff

This is a **source-only overlay**, arranged using the existing PC/GEOS repository
paths. It is not another runtime update, a complete copy of the repository, or a
bootable distribution. Apply it to your fork of `665-protected-mode-2026`.

**Exact baseline:** uploaded source archive commit `30df506fc64720fd82e528acbcb192adbaa48fce`.
The commit is recorded in that ZIP's archive comment. The live GitHub branch head
was not verified for this handoff. A newer or locally edited branch may require a
manual merge; do not overwrite it blindly.

## What is consolidated

The final implemented versions of the full-product source changes, **PMDIAG2**,
the TrueType existing-family pointer fix, and the **Perf KR-01 fix** are included.
There are **17 project-source paths: 15 modified existing files and 2 new files**.
Repository-local build helpers, test sources, provenance and documentation are
also added. You do not need to apply the older incremental source patches first.

The working Perf variant still has **four meters**: CPU, load average, interrupts
and context switches. The display path is **640x480, banked VBE 0x111, RGB565**,
not classic 16-colour VGA.

**Not implemented:** all-original-meter protected-mode Perf, QR/crash-screen
rendering, crash-file/serial logging, or non-fatal application recovery. The
reported Celeron/FreeDOS kernel GPF at `00E7:079E` is still unresolved. No source
change in this archive claims to fix that hardware-specific report.

## Applying the handoff

Keep your working runtime and checkout backed up. Use a clean development branch.
The separately supplied `pcgeos-pm-consolidated.patch` represents the same overlay
against the exact baseline above and includes both modifications and new files.

From your fork's root:

```sh
git status --short
git switch -c pm-bringup-consolidated
git apply --check /path/to/pcgeos-pm-consolidated.patch
git apply --whitespace=nowarn /path/to/pcgeos-pm-consolidated.patch
python3 Tools/build/pm-product/scripts/check-source.py --current .
git diff --stat
git status --short
```

Stop if `git status` initially shows changes you have not saved, or if
`git apply --check` fails. The patch is not meant to overwrite intermediate
PMDIAG1/PMDIAG2 edits already in a fork. A separate worktree at the pinned commit
is an alternative when you need a clean comparison:

```sh
git worktree add -b pm-bringup-baseline ../pcgeos-pm-baseline 30df506fc64720fd82e528acbcb192adbaa48fce
```

Then apply the patch in that worktree and merge/review as appropriate. The patch
has normal `a/` and `b/` paths, so it is applied from the repository root without
an extra directory prefix. It is a diff, not a `git am` mail-format patch.

**Overlay alternative:** unpack `pcgeos-pm-source-overlay.zip`, inspect
`pcgeos-pm-source-overlay/`, and copy its *contents* into the root of a clean
baseline checkout. For example, from outside both directories:

```sh
python3 pcgeos-pm-source-overlay/Tools/build/pm-product/scripts/check-source.py \
  --baseline /path/to/pcgeos
cp -a pcgeos-pm-source-overlay/. /path/to/pcgeos/
python3 /path/to/pcgeos/Tools/build/pm-product/scripts/check-source.py \
  --current /path/to/pcgeos
```

Use **either** the patch or the copy method, not both. The read-only baseline
checker verifies the 17 project-source paths; `git apply --check` additionally
checks all added helper/document paths. No tool commits, pushes, or contacts
GitHub on your behalf. Review and stage the resulting diff yourself.

## Comments and attribution

All 17 project-source files contain searchable **`PM-BRINGUP [CHANGE-ID]`**
comments. Existing files have a change summary at the top and explanations next
to the changed code. Entirely new modules are explicitly labelled **NEW FILE**.
New build/test helpers are also labelled. Original source and licensing notices
are retained; these comments identify ChatGPT-assisted project modifications,
not an upstream endorsement or an attribution of the original code.

The annotations add **no runtime instructions or data** relative to the most
recent supplied source for each file. `Loader32/strings.asm` has its original
CRLF line endings restored; other project files retain their baseline newline
style. The manifest records the original, previously supplied and annotated
source hashes, plus a normalized-text hash for checking the comment-only change.

## Build and review references

- [File-by-file changes](TechDocs/Markdown/pm-bringup/CHANGES.md)
- [Build instructions](TechDocs/Markdown/pm-bringup/BUILDING.md)
- [Current status and unresolved work](TechDocs/Markdown/pm-bringup/STATUS.md)
- [Handoff validation](TechDocs/Markdown/pm-bringup/VALIDATION.md)
- [Source provenance](Tools/build/pm-product/source-manifest.json)
- [Product build tooling](Tools/build/pm-product/README.md)

The pinned compiler is the supplied **OpenWatcom 2020-12-01 snapshot**. This
handoff contains no compiler, GEOS executables, object files, fonts/font caches,
HDPMI16 executable, DOS image, or generated debug symbols. The existing repository
continues to supply unchanged source files and its original license notices.
The build helper can assemble a runtime using your local repository inputs.

Historical verification records are preserved under
`TechDocs/Markdown/pm-bringup/history/`. They are labelled historical: they do not
become new guest tests merely because they are included in this source archive.
