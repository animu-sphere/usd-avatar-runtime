# Release records

Each released version gets an immutable record here. Current capability belongs
to the [capability matrix](../reference/CAPABILITY_MATRIX.md), and unfinished work
to the [roadmap](../roadmap/current.md).

| Version | Record | Scope |
| --- | --- | --- |
| v0.1.0 | [v0.1.0.md](v0.1.0.md) | The dependency-free core with experimental ABI revision 5, and the release lane |

## How a release is cut

A release ships the libraries `openstrata.toml` names as release members. Optional
adapters stay source-built until their owners' packages can be pinned.

1. Run [`release.yml`](../../.github/workflows/release.yml) by dispatch on `main`
   and inspect its dry-run bundle.
2. Date the `CHANGELOG.md` section and add the record. `VERSION`,
   `openstrata.toml` and `libs/avatarRuntime/openstrata.library.yaml` must agree.
3. Tag that commit `vX.Y.Z`. The workflow pushes to GHCR and drafts the release.
4. Inspect the draft and its pins, then publish it.

A change to the ABI revision takes a new minor version while the version is 0.x.
