# Historical native test fixture sources

These are the C test harnesses written for the earlier PMDIAG2, TrueType and
Perf KR-01 updates. They execute selected trusted 16-bit compiled paths using
controlled Linux/x86-64 descriptors, memory, registers and intercepted calls.
They are **not** DOSBox-X or physical-machine tests.

They are included as source for reviewers, not as runnable end-to-end tests in
this font-free/binary-free overlay. Loader fixtures require the matching linked
loader and symbol-derived entry offsets. TrueType and Perf fixtures require
extracted code/data resources and expected offsets from the corresponding update
binaries. Some offsets are hard-coded for those builds and must be regenerated
or checked after functional code changes. A zero exit from compilation alone is
not proof that the fixture was executed.

The original binary-update archives already supplied to the user contain the
Python drivers, matching binaries/symbols, and complete fixture invocations.
Historical result transcripts and provenance are preserved under
`TechDocs/Markdown/pm-bringup/history/`. They have not been relabelled as fresh
results from this source-only handoff.
