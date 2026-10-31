# usd-avatar-runtime {tag}

The format-independent avatar evaluation runtime: common C contracts, frame
lifecycle, ordered evaluation, retained state and fixed-tick orchestration, with
no dependency on OpenUSD, Hydra or any format library.

> **Experimental.** The C ABI is at an unfrozen revision. A provider or consumer
> built against another revision is refused and must be rebuilt.

- **Release record:** [docs/releases/{tag}.md](https://github.com/animu-sphere/usd-avatar-runtime/blob/{tag}/docs/releases/{tag}.md)
- **Capability matrix:** [CAPABILITY_MATRIX.md](https://github.com/animu-sphere/usd-avatar-runtime/blob/{tag}/docs/reference/CAPABILITY_MATRIX.md)
- **ABI:** [ABI.md](https://github.com/animu-sphere/usd-avatar-runtime/blob/{tag}/docs/contracts/ABI.md)

{changelog}

## Consuming the core from another repository

The core is published as a `.tar.zst` asset below and as a digest-pinned OpenStrata
artifact in `ghcr.io/animu-sphere/usd-avatar-runtime`, tagged
`avatarRuntime-{version}-<target>`. Paste this under `requires.libraries`:

{pins}

## Artifacts

| artifact | contents |
| --- | --- |
| `avatarRuntime-{version}-<target>.tar.zst` | the installed core for `<target>` |
| `avatarRuntime-{version}-<target>.manifest.json` | its OpenStrata manifest |
| `external-library-pins.md` / `.json` | the pin table above, as a file |
| `usd-avatar-runtime-{version}-src.tar.gz` | source archive at this tag, including the optional adapters |
| `SHA256SUMS` | SHA-256 checksums of every file above |

## SHA-256 checksums

```text
{checksums}
```
