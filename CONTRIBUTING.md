# Contributing

Thanks for taking an interest in `usd-avatar-runtime`. Documentation, tests,
build fixes and clear bug reports are all useful contributions. Small pull
requests do not need prior permission.

## Before you start

- Search existing issues and pull requests first.
- Use the [bug report template](.github/ISSUE_TEMPLATE/bug_report.md) or
  [feature request template](.github/ISSUE_TEMPLATE/feature_request.md).
- Discuss substantial behavior or contract changes in an issue before a large
  implementation. Identify the owning repository; a short proposal is enough.
- Report security problems privately through [SECURITY.md](SECURITY.md).
- Read the [Code of Conduct](CODE_OF_CONDUCT.md) and respect the licenses and
  privacy of contributors and asset creators. Do not commit avatars, textures,
  motions, captures or screenshots without redistribution permission; prefer
  generated fixtures.

## Ownership and design

Read the [design policy](docs/design/DESIGN_POLICY.md),
[dependency boundary](docs/architecture/DEPENDENCIES.md) and
[documentation guidelines](docs/contributing/documentation.md) first.

Before implementing a feature, identify its owner. Common contracts,
cross-format ordering, runtime lifecycle and state publication belong here.
VRM/MMD semantics, generic motion algorithms, device adapters and rendering
belong to their respective repositories.

Adapters receive owner values, invoke owner algorithms and marshal/publish
common state. Keep runtime layout, identity and lifetime checks here; generic
motion validation, skeleton/rest conversion and USD motion interpretation belong
to `usd-motion-plugins`. Extend missing owner APIs instead of implementing
private substitutes. Recording is a publication adapter to the owner recorder.

Contract drafts are under [docs/contracts/](docs/README.md#contracts).
Changes should resolve a named open question or include an explicit rationale
and acceptance evidence. A draft is not a published ABI.

Implementation changes should update the owning architecture/contract page,
the capability matrix, the evidence log and the open roadmap work together. Keep renderer and
OpenExec dependencies out of reusable evaluator logic. Begin with serial,
ordered evaluation and instance-local state.

Validation should target the changed boundary: deterministic frame evaluation,
adapter conformance, direct/OpenExec equivalence, output parity or replay as
appropriate. Do not claim a supported target, published package or working
command without evidence from that target or artifact.

## Working locally

The [README build instructions](README.md#build) cover the dependency-free
runtime configure, build and test commands. Run the focused tests for the
boundary you change, including optional adapters when affected. The
[evidence log](docs/reports/evidence-log.md) records validation
coverage and limits; do not claim checks that you could not run.

For documentation changes, verify relative links and fragments, metadata,
category indexes, and the distinction between intent and implementation.
Repository documents are in English; machine-local paths and unredistributable
avatar or capture assets do not belong in committed documentation.

## Pull requests

Keep each pull request focused on one understandable change. The
[pull request template](.github/PULL_REQUEST_TEMPLATE.md) asks for the reason,
affected ownership or compatibility boundary, documentation updates and checks
run. Explain any checks that were unavailable. Draft pull requests are welcome
for early feedback.

Unless explicitly stated otherwise, contributions submitted for inclusion are
under the project's [Apache-2.0 license](LICENSE). Third-party dependencies and
assets retain their own terms; only submit material you have permission to share.
