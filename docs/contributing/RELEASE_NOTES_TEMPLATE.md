# usd-avatar-runtime {tag}

The format-independent avatar evaluation runtime: common C contracts, frame
lifecycle, ordered evaluation, retained state and fixed-tick orchestration. The
core has no dependency on OpenUSD, Hydra or any format library.

> **Experimental.** The C ABI is at an unfrozen revision. A provider or consumer
> built against another revision is refused and must be rebuilt.

- **Release record:** [docs/releases/{tag}.md](https://github.com/animu-sphere/usd-avatar-runtime/blob/{tag}/docs/releases/{tag}.md)
- **Capability matrix:** [CAPABILITY_MATRIX.md](https://github.com/animu-sphere/usd-avatar-runtime/blob/{tag}/docs/reference/CAPABILITY_MATRIX.md)
- **ABI:** [ABI.md](https://github.com/animu-sphere/usd-avatar-runtime/blob/{tag}/docs/contracts/ABI.md)

{changelog}

## Consuming a package from another repository

Each package is published as a `.tar.zst` asset below and as a digest-pinned
OpenStrata artifact in `ghcr.io/animu-sphere/usd-avatar-runtime`, tagged
`<package>-{version}-<target>`. Pin `avatarRuntime` for the dependency-free core, or
`avatarVrmSession` for the core with the VRM session, not both. Paste the entry
under `requires.libraries`:

{pins}

## Artifacts

| artifact | contents |
| --- | --- |
| `avatarRuntime-{version}-<target>.tar.zst` | the installed core for `<target>` |
| `avatarVrmSession-{version}-<target>.tar.zst` | the core with the native VRM session, for `<target>` |
| `<package>-{version}-<target>.manifest.json` | its OpenStrata manifest |
| `external-library-pins.md` / `.json` | the pin table above, as a file |
| `usd-avatar-runtime-{version}-src.tar.gz` | source archive at this tag, including every optional adapter |
| `SHA256SUMS` | SHA-256 checksums of every file above |

## SHA-256 checksums

```text
{checksums}
```
