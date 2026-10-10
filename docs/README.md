# Documentation

Each subject has one owning document. Design describes intent, contracts
describe proposed shared boundaries, architecture assigns responsibilities,
reference records the current tree, and roadmap tracks incomplete work.

## Design

| Document | Owns |
| --- | --- |
| [Design policy](design/DESIGN_POLICY.md) | adopted implementation direction, original sections 1–22 |
| [Near-term direction](design/NEAR_TERM_PLAN.md) | adopted 2026-10-05 priorities: VRM first, fast-path consumer, MMD validation before freeze |
| [Boundary cleanup policy](design/BOUNDARY_POLICY.md) | adopted 2026-10-06 ownership rules, motion/USD cleanup, thin adapters and publication boundary; source sections 1–21 |
| [Recording and replay](design/RECORDING_AND_REPLAY.md) | capture composition, reproducibility and validation proposals |

## Architecture

| Document | Owns |
| --- | --- |
| [Overview](architecture/OVERVIEW.md) | authored/runtime split, instances and execution layering |
| [Dependencies](architecture/DEPENDENCIES.md) | repository ownership and integration edges |
| [Frame lifecycle](architecture/FRAME_LIFECYCLE.md) | frame boundaries, configuration changes and stateful execution |
| [Output paths](architecture/OUTPUT_PATHS.md) | Hydra, direct consumers, bake/export and parity |
| [Project layout](architecture/PROJECT_LAYOUT.md) | proposed directories, components and build separation |
| [Scoped VRM adapter](architecture/VRM_ADAPTER.md) | optional owner LookAt/expression registration, mapping, lifetime and evidence limits |
| [Scoped motion adapters](architecture/MOTION_ADAPTER.md) | clip sampling/retarget registration, MotionPoseInputBridge, installed-owner validation delegation and owned diagnostic reports |
| [Scoped USD motion clip binding](architecture/MOTION_USD_BINDING.md) | direct typed owner clip/source-rest integration, compatible StageClip wrapper and parity tool |
| [Scoped USD skeleton binding](architecture/USD_BINDING.md) | current baseline/Humanoid/placement binding; planned split of motion conversion from runtime identity/layout |
| [Scoped VRM USD Humanoid binding](architecture/VRM_USD_BINDING.md) | owner schema discovery, skeleton relationship resolution, standard role mapping and local asset evidence limits |
| [Scoped VRM USD LookAt binding](architecture/VRM_LOOKAT_USD_BINDING.md) | owner range-map extraction, explicit head/eye/rest binding, adapter configuration and real-avatar evidence limits |
| [Scoped VRM USD Expression binding](architecture/VRM_EXPRESSION_USD_BINDING.md) | owner expression/output extraction, canonical morph/material baseline, LookAt composition and evidence limits |

## Contracts

The contracts describe the target boundary and the scoped experimental
implementation under `include/avatarRuntime/`. Revision 3 is compilable but
is not a frozen wire/ABI representation. Adoption is gated by
[Runtime Phase A](roadmap/current.md#runtime-phase-a--freeze-contracts).

| Document | Owns |
| --- | --- |
| [Input frame](contracts/INPUT_FRAME.md) | assembled input, clock mapping, provenance and semantic intent |
| [Evaluated state](contracts/EVALUATED_STATE.md) | central IR: resolved pose, deformation, expression/gaze results, appearance and snapshot identity/provenance |
| [Evaluator](contracts/EVALUATOR.md) | phase sequence, registration, dependencies and evaluator invocation |
| [ABI](contracts/ABI.md) | versioning, memory lifetime, handles and cross-package representation |
| [Capabilities and diagnostics](contracts/CAPABILITIES_AND_DIAGNOSTICS.md) | runtime negotiation and observable evaluation results |

## Implementation and planning

| Document | Owns |
| --- | --- |
| [Capability matrix](reference/CAPABILITY_MATRIX.md) | implementation facts in this repository |
| [Roadmap index](roadmap/README.md) | roadmap navigation and sequence |
| [Current roadmap](roadmap/current.md) | boundary cleanup workstreams, Runtime Phases A–F, acceptance criteria and unresolved decisions |

## Maintenance

[Documentation guidelines](contributing/documentation.md) define category
ownership, metadata, cross-repository references and validation.
[CONTRIBUTING.md](../CONTRIBUTING.md) is the contribution entry point.

The adopted design policy is the architectural baseline; the near-term
direction refines its implementation sequence, and the boundary cleanup policy
refines ownership and migration priorities. Focused
proposals elaborate it without silently freezing new choices. A conflict
with a sibling contract is an integration question to resolve with its owner;
it does not authorize redefining that contract here.
