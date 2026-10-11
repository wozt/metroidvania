# Repository working rules

- Write all commits, documentation, code comments, UI text, and diagnostics in English.
- Never stage, commit, push, or transfer ROMs, BIOS images, save files, or
  extracted proprietary data.
- At the end of every meaningful work cycle, run the relevant checks, update
  `docs/PROJECT_STATUS.md`, create a descriptive commit, and push the current
  branch to `origin` over SSH.
- Do not publish releases or rewrite remote history without an explicit request.
- After every feature change, synchronize the registries that describe it in
  the same commit: `data/native_parity/annotations.tsv` for the two original
  reconstructions, `data/dashboard/features.tsv` for assets, tools, story,
  platforms and the Metroidvania layer, and any hand-written document that
  states the feature's scope (`README.md`, `docs/*.md`). Update the notes,
  divergences, limitations, sources, tests, dependencies and next actions to
  match the code and its tests; change a status only when the evidence
  changed, never to raise a percentage, and never declare a feature faithful
  or validated without proof.
- Then regenerate the derived documents (`python3 -m scripts.native_parity`
  for `docs/NATIVE_PARITY_CHECKLIST.md`, `python3 -m scripts.dashboard
  --run-tests` for `dashboard/data.json` and `dashboard/data.js`) and verify
  their consistency with `python3 -m scripts.check_generated`, which runs no
  tests and is enough on its own for documentation-only changes.
