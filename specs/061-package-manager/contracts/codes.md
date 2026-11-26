# Contract: Catalog Codes (spec 061)

Each gets a `docs/errors.md` entry. A row is added with the task that implements the code. None is
fixable by a text edit: the repair, where there is one, is a command the message names.

| Code | Name | Exit | Fixable | When |
|---|---|---|---|---|
| E0707 | `package_version_conflict` | 65 | no | a selection in which two requirers ask one package at incompatible versions (another major; below 1.0.0 another minor) |
| E0708 | `package_source_clash` | 65 | no | a selection in which one name has two sources, or one source two names |
| E0709 | `package_requirement` | 65 | no | `vo check`/`run`/`test`/`build`: a versioned package's own manifest requires a name the program's manifest lacks or has lower; at the requirer's line |
| E0710 | `package_version_changed` | 65 | no | a selected version's content hash differs from the hash a requirer's manifest pins for that version |
| E0711 | `package_version_unknown` | 65 | no | `vo package` with a source: it has no such version, or no version tags; `--restore`: the pinned version is gone |

## Messages

- E0707: "Package `money` is required at incompatible versions: 2.0.0 by `audit`, 1.4.0 by `report`." Requirers: `this
  command`, `this program` (a direct entry), then packages by name; when none of them asks the selected version, who
  set it is appended (a requirer at a version since replaced, or `this program's manifest`).
- E0708: "Package `money` names two sources: URL (this program), URL (`cash`)." / "One source has two names: `money`
  (this program) and `coins` (`purse`): URL."
- E0709: "Package `report` requires `money` 1.4.0; the manifest has 1.2.0: run `vo package --repair main.vo`." (or "does
  not list it"; the entry file is the one being checked). Planned with US5 and US6: "`pads` has no source in `report`'s
  manifest"; "`money` is required by `report`".
- E0710: "`money` 1.4.0 has other content (sha256:…) than `report` pinned (sha256:…): the version was changed after release."
- E0711: "URL has no version 1.9.0. Nearest: 1.8.2, 2.0.0, 1.8.1, 1.8.0, 1.7.3." (below and above in turn) or "URL has no version tags (`v1.2.3`)."
