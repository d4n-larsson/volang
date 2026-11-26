# Contract: Catalog Codes (spec 061)

Each gets a `docs/errors.md` entry. A row is added with the task that implements the code. None is
fixable by a text edit: the repair, where there is one, is a command the message names.

| Code | Name | Exit | Fixable | When |
|---|---|---|---|---|
| E0711 | `package_version_unknown` | 65 | no | `vo package` with a source: it has no such version, or no version tags; `--restore`: the pinned version is gone |

## Messages (planned for the codes not yet in the table)

- E0707 `package_version_conflict`: "Package `money` is required at incompatible versions: 1.2.0 by `report`, 2.0.0 by `ledger`."
- E0708 `package_source_clash`: "Package `money` names two sources: URL (this program), URL (`report`)." / "One source has two names: `money` and `cash`."
- E0709 `package_requirement`: "Package `report` requires `money` 1.4.0; the manifest has 1.2.0: run `vo package --repair ENTRY`." (or "does not list it"; "`pads` has no source in `report`'s manifest"; "`money` is required by `report`")
- E0710 `package_version_changed`: "`money` 1.4.0 has other content (sha256:…) than `report` pinned (sha256:…): the version was changed after release."
- E0711: "URL has no version 1.9.0. Nearest: 1.8.2, 2.0.0, 1.8.1, 1.8.0, 1.7.3." (below and above in turn) or "URL has no version tags (`v1.2.3`)."
