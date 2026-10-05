# Capability matrix

Checked against this repository on 2026-10-05. This is implementation status,
not a promise about sibling repositories or a release/support declaration.
No runtime code or build configuration exists in the current tree.

| Surface | Current status | Owning documentation |
| --- | --- | --- |
| implementation direction | adopted documentation | [design policy](../design/DESIGN_POLICY.md) |
| runtime architecture and layout | documented proposals | [overview](../architecture/OVERVIEW.md), [layout](../architecture/PROJECT_LAYOUT.md) |
| input/evaluated-state contracts | draft; no ABI freeze or headers | [input](../contracts/INPUT_FRAME.md), [state](../contracts/EVALUATED_STATE.md) |
| evaluator registration, phase execution and lifecycle | draft; not implemented | [evaluator](../contracts/EVALUATOR.md), [lifecycle](../architecture/FRAME_LIFECYCLE.md) |
| versioned C ABI | draft; not implemented | [ABI](../contracts/ABI.md) |
| capability negotiation and diagnostics | draft; not implemented | [capabilities/diagnostics](../contracts/CAPABILITIES_AND_DIAGNOSTICS.md) |
| motion/VRM/MMD/connector integration | ownership described; no runtime adapters | [dependencies](../architecture/DEPENDENCIES.md) |
| Hydra state overlay and direct consumer API | proposed; not implemented | [output paths](../architecture/OUTPUT_PATHS.md) |
| OpenExec orchestration adapter | proposed; not implemented | [overview](../architecture/OVERVIEW.md) |
| capture, replay, inspection and benchmarks | proposed; not implemented | [recording/replay](../design/RECORDING_AND_REPLAY.md) |
| multi-avatar execution and parallel scheduling | proposed; not implemented | [overview](../architecture/OVERVIEW.md) |
| OST composition and published runtime packages | not configured | [layout](../architecture/PROJECT_LAYOUT.md) |
| supported native/Web/WASM/XR targets | no runtime validation evidence | [roadmap](../roadmap/current.md) |

Update a row only with implementation and validation evidence from this
repository. A sibling's callable library or published package is an integration
input, not proof that this runtime already composes it.
