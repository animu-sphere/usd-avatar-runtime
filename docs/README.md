# Documentation

Each subject has one owning document. Design describes intent, contracts
describe proposed shared boundaries, architecture assigns responsibilities,
reference records the current tree, and roadmap tracks incomplete work.

## Design

| Document | Owns |
| --- | --- |
| [Design policy](design/DESIGN_POLICY.md) | adopted implementation direction, original sections 1–22 |
| [Recording and replay](design/RECORDING_AND_REPLAY.md) | capture composition, reproducibility and validation proposals |

## Architecture

| Document | Owns |
| --- | --- |
| [Overview](architecture/OVERVIEW.md) | authored/runtime split, instances and execution layering |
| [Dependencies](architecture/DEPENDENCIES.md) | repository ownership and integration edges |
| [Frame lifecycle](architecture/FRAME_LIFECYCLE.md) | frame boundaries, configuration changes and stateful execution |
| [Output paths](architecture/OUTPUT_PATHS.md) | Hydra, direct consumers, bake/export and parity |
| [Project layout](architecture/PROJECT_LAYOUT.md) | proposed directories, components and build separation |

## Contracts

The contracts describe the target boundary and the scoped experimental
implementation under `include/avatarRuntime/`. Revision 1 is compilable but
is not a frozen wire/ABI representation. Adoption is gated by
[Runtime Phase A](roadmap/current.md#runtime-phase-a--freeze-contracts).

| Document | Owns |
| --- | --- |
| [Input frame](contracts/INPUT_FRAME.md) | assembled input, clock mapping, provenance and semantic intent |
| [Evaluated state](contracts/EVALUATED_STATE.md) | resolved pose, deformation and appearance output |
| [Evaluator](contracts/EVALUATOR.md) | phase sequence, registration, dependencies and evaluator invocation |
| [ABI](contracts/ABI.md) | versioning, memory lifetime, handles and cross-package representation |
| [Capabilities and diagnostics](contracts/CAPABILITIES_AND_DIAGNOSTICS.md) | runtime negotiation and observable evaluation results |

## Implementation and planning

| Document | Owns |
| --- | --- |
| [Capability matrix](reference/CAPABILITY_MATRIX.md) | implementation facts in this repository |
| [Roadmap index](roadmap/README.md) | roadmap navigation and sequence |
| [Current roadmap](roadmap/current.md) | Runtime Phases A–F, acceptance criteria and unresolved decisions |

## Maintenance

[Documentation guidelines](contributing/documentation.md) define category
ownership, metadata, cross-repository references and validation.
[CONTRIBUTING.md](../CONTRIBUTING.md) is the contribution entry point.

The adopted policy is authoritative for architectural direction. Focused
proposals elaborate it without silently freezing new choices. A conflict
with a sibling contract is an integration question to resolve with its owner;
it does not authorize redefining that contract here.
