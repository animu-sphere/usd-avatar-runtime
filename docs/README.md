# Documentation

Each subject has one owning document. [Guidelines](contributing/documentation.md) define
what belongs in each category.

| To find | Read |
| --- | --- |
| what exists and how far it reaches | [capability matrix](reference/CAPABILITY_MATRIX.md) |
| what comes next and how it is accepted | [roadmap](roadmap/current.md) |
| what belongs in this repository | [design policy](design/DESIGN_POLICY.md) and [dependencies](architecture/DEPENDENCIES.md) |
| what was measured, when and with what limits | [evidence log](reports/evidence-log.md) |
| what each release shipped and how one is cut | [release records](releases/README.md) |

## Design

| Document | Owns |
| --- | --- |
| [Design policy](design/DESIGN_POLICY.md) | ownership rules, thin adapters, core dependencies and publication principles |
| [Recording and replay](design/RECORDING_AND_REPLAY.md) | proposed capture contents, reproducibility and validation |

## Contracts

Experimental boundaries under `include/avatarRuntime/`. Revision 5 compiles and is tested,
and is not frozen; see the [freeze gates](roadmap/current.md#abi-freeze).

| Document | Owns |
| --- | --- |
| [Input frame](contracts/INPUT_FRAME.md) | assembled input, clock mapping, provenance and semantic intent |
| [Evaluated state](contracts/EVALUATED_STATE.md) | resolved pose, deformation, appearance and snapshot identity |
| [Evaluator](contracts/EVALUATOR.md) | phases, registration, dependencies and the shared-world barrier |
| [ABI](contracts/ABI.md) | versioning, memory lifetime, handles and representation |
| [Capabilities and diagnostics](contracts/CAPABILITIES_AND_DIAGNOSTICS.md) | negotiation and observable results |

## Architecture

| Document | Owns |
| --- | --- |
| [Overview](architecture/OVERVIEW.md) | authored/runtime split, instances and execution layering |
| [Dependencies](architecture/DEPENDENCIES.md) | repository ownership and integration edges |
| [Project layout](architecture/PROJECT_LAYOUT.md) | targets, directories and packaging |
| [Frame lifecycle](architecture/FRAME_LIFECYCLE.md) | frame boundaries, physics clock, reset, seek and unload |
| [Output paths](architecture/OUTPUT_PATHS.md) | Hydra, direct consumers, bake and export, parity |

Component boundaries:

| Area | Documents |
| --- | --- |
| VRM | [session](architecture/VRM_SESSION.md), [LookAt and expression adapter](architecture/VRM_ADAPTER.md), [spring evaluator](architecture/VRM_SPRING_ADAPTER.md); USD bindings for [Humanoid](architecture/VRM_USD_BINDING.md), [LookAt](architecture/VRM_LOOKAT_USD_BINDING.md), [expression](architecture/VRM_EXPRESSION_USD_BINDING.md) and [spring](architecture/VRM_SPRING_USD_BINDING.md) |
| MMD | [session](architecture/MMD_SESSION.md), [control and coupling adapter](architecture/MMD_ADAPTER.md) |
| Motion and USD | [motion adapters](architecture/MOTION_ADAPTER.md), [motion clip binding](architecture/MOTION_USD_BINDING.md), [skeleton binding](architecture/USD_BINDING.md), [live pose output](architecture/USD_POSE_OUTPUT.md) |
| Physics hosts | [Stage Runner session](architecture/STAGE_RUNNER_SESSION.md), [USD physics resources](architecture/PHYSICS_USD_RESOURCES.md) |

## Other

- Guide: [live MMD physics in usdview](guides/live-mmd-physics.md).
- [Documentation guidelines](contributing/documentation.md) and
  [CONTRIBUTING.md](../CONTRIBUTING.md) for maintenance.
- [Code of Conduct](../CODE_OF_CONDUCT.md), [Security Policy](../SECURITY.md) and
  [Apache-2.0 license](../LICENSE).
