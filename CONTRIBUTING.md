# Contributing

Read the [design policy](docs/design/DESIGN_POLICY.md),
[dependency boundary](docs/architecture/DEPENDENCIES.md) and
[documentation guidelines](docs/contributing/documentation.md) first.

Before implementing a feature, identify its owner. Common contracts,
cross-format ordering, runtime lifecycle and state publication belong here.
VRM/MMD semantics, generic motion algorithms, device adapters and rendering
belong to their respective repositories.

Contract drafts are under [docs/contracts/](docs/README.md#contracts).
Changes should resolve a named open question or include an explicit rationale
and acceptance evidence. A draft is not a published ABI.

Implementation changes should update the owning architecture/contract page,
the capability matrix and the open roadmap work together. Keep renderer and
OpenExec dependencies out of reusable evaluator logic. Begin with serial,
ordered evaluation and instance-local state.

Validation should target the changed boundary: deterministic frame evaluation,
adapter conformance, direct/OpenExec equivalence, output parity or replay as
appropriate. Do not claim a supported target, published package or working
command without evidence from that target or artifact.

For documentation changes, verify relative links and fragments, metadata,
category indexes, and the distinction between intent and implementation.
Repository documents are in English; machine-local paths and unredistributable
avatar or capture assets do not belong in committed documentation.
