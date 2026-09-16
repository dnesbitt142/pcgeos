# Protected-mode full-product build helpers

**PM-BRINGUP: newly added repository-local tooling**, adapted from the full-product
package previously supplied in this conversation. Start with the root
`PM-BRINGUP.md` and `TechDocs/Markdown/pm-bringup/BUILDING.md`.

The runtime source changes are already in the normal `Appl/`, `Driver/`,
`Library/`, `Loader32/`, and product-manifest paths. There is no duplicate
`changes/` tree and the build wrapper does not overwrite source files.

`source/Product.mk` is the selected graph derived from upstream
`Installed/Makefile`; `source/product-exclusions.json` records the 18 manifest
exceptions. `scripts/product_config.py` preserves the final full-product profile,
including the enabled spooler. `scripts/assemble.py` uses the upstream product
assembler and rejects unexpected missing files even if the legacy assembler
returns success. `scripts/audit.py` checks load-time imports and explicit startup
loads; this is not execution testing.

`reports/inputs.json` pins the compiler and source inputs. `source-manifest.json`
identifies the 17 consolidated project files and their previous source snapshots.
`scripts/check-source.py` is read-only and does not require Git, a compiler, or a
network connection. `tests/test_source_handoff.py` exercises the handoff's source
and host-helper contracts. Historical native fixture sources are kept separately
and require matched binary resources; they are not quietly counted as guest tests.
