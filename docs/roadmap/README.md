# Roadmap

[Current work](current.md) owns the incomplete implementation sequence and
acceptance criteria. It refines
[design policy section 20](../design/DESIGN_POLICY.md) using the adopted
[near-term direction](../design/NEAR_TERM_PLAN.md).

The adopted [boundary cleanup policy](../design/BOUNDARY_POLICY.md) adds
[cleanup workstreams](current.md#boundary-cleanup-workstreams) for motion
validation, USD/motion ownership, VRM state validation, MMD, publication and
ABI stabilization. Their A–F labels are separate from the Runtime Phases below.

1. Runtime Phase A — freeze common contracts.
2. Runtime Phase B — integrate motion, VRM, MMD and connector providers.
3. Runtime Phase C — `hydra-toon` fast-path first, then Hydra publication and parity.
4. Runtime Phase D — optional OpenExec wrappers over stable cores.
5. Runtime Phase E — replay, profiling and multi-avatar runtime quality.
6. Runtime Phase F — reduced Web/WASM/XR application profiles.

These are implementation milestones, not version numbers or released features.
All phases remain open. A/B overlap: real motion/VRM integration, an early
`hydra-toon` consumer, then real MMD integration drive contract correction before
the Phase A freeze candidate. Full Hydra publication comes afterward.
Evidence milestones A/B/C and the seven implementation steps are defined in
the [current roadmap](current.md#near-term-implementation-sequence); they are
not substitutes for Runtime Phase acceptance. Synthetic tests or documentation
alone cannot freeze a contract.
See the [capability matrix](../reference/CAPABILITY_MATRIX.md) for current facts.
