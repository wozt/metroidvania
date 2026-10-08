# ROM requirements

Only two revisions are currently accepted:

| Game | Region | Observed size | SHA-1 |
|---|---:|---:|---|
| Castlevania: Aria of Sorrow | USA | 8,388,608 bytes | `abd71fe01ebb201bcc133074db1dd8c5253776c7` |
| Metroid: Zero Mission | USA | 8,388,608 bytes | `5de8536afe1f0078ee6fe1089f890e8c7aa0a6e8` |

These fingerprints are documented by the upstream repositories and were
confirmed against both local files on 2026-10-08. Filenames are not evidence;
only content hashes are accepted.

Validation scripts read ROMs locally. They perform no uploads, telemetry, or
requests containing ROM bytes. Future extracted artifacts must stay under the
Git-ignored `extracted/` directory. The `no_proprietary_files` check rejects ROM
extensions and large files outside submodules.

Every importer must apply these checks before extraction or code generation.
Other regions remain unsupported until they have dedicated symbols, audits,
and tests.
