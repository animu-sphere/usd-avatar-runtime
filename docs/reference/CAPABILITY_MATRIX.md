# Capability matrix

What exists in the current tree, and how far each part reaches. It is status, not a
promise about sibling repositories. Measurements and their limits are in the dated
[evidence log](../reports/evidence-log.md), which also holds the sections that used to be
on this page, under the same anchors.

The C ABI is experimental revision 5, not frozen. Released packages target `cy2026` /
`usd` on Windows x86_64 and Linux x86_64; see the [release records](../releases/README.md).

## Delivery state

A working adapter is not a shipped feature. **Adapter** is source-built against installed
owners. **Package** ships in the `avatarVrmSession` package and the session selects it.
**Renderer** means a renderer consumes it from this repository's output.

| Feature | Adapter | Package | Renderer | Open |
| --- | --- | --- | --- | --- |
| Motion clip pose | yes; seven real clips on one avatar | yes | `hydra-toon` resident check, headless; no pixels compared | broader avatars and motion |
| Spring secondary motion | yes; owner parity on real VRM 0.x and 1.0 avatars | yes | through pose, as above | scaled spring rigs are refused; shared-world composition |
| Expression | yes; owner parity on real VRM 0.x and 1.0 avatars with generated weights and generated clip channels | yes, selected explicitly, from the host or the clip | probe transport only | captured channels; real-avatar material binds; shared-mesh targets |
| LookAt | yes; owner parity with bone and Expression types on real avatars with a generated gaze point and generated clip gaze | yes, selected explicitly, from the host or the clip | probe transport only | captured gaze |
| Node constraints | no | no | no | [deferred past v0.3.0](../architecture/VRM_SESSION.md#node-constraints); the owner has no evaluator |
| Native input expression and gaze | explicit attribute selection; generated fixtures | yes, sampled per tick in the session | no | captured input; automatic discovery |
| Live UsdSkel pose, morph, UV, material output | yes | yes, optional for hosts | usdview | discardable session layer, not stage authoring |
| MMD control and coupling | yes; generated PMX/VMD/Jolt | no | probe transport | owner library unpublished; representative assets |
| Native MMD session | yes; bounded private-asset headless runs | no | `hydra-toon` viewport, [generated material delivery](https://github.com/animu-sphere/hydra-toon/blob/main/docs/reports/renderer/111-2026-10-29-live-mmd-materials.md) | full-motion and representative acceptance; several avatars |
| Live usdview MMD host | yes; [guide](../guides/live-mmd-physics.md) | no | usdview | full PMX/VMD |
| Stage Runner session, USD physics resources | yes; generated | no | no | automatic world and participant binding; application wiring |
| Hydra publication | no | no | no | not started |
| Recording publication, inspect and replay hosts, benchmarks | no | no | no | not started |
| OpenExec | no | no | no | not started |
| Connector intake, multi-source selection | no | no | no | not started |

## Core

These ship in both packages and have no external provider dependency.

| Surface | Implemented | Open | Owning document |
| --- | --- | --- | --- |
| Input and state | attributed scalars; typed gaze observations with space, validity and clocks; dense snapshots with layout identity, capabilities and source-sample provenance; scalar to vec4 material values; primary vertex UVs | resolved expression and gaze records; other deformation channels | [input](../contracts/INPUT_FRAME.md), [state](../contracts/EVALUATED_STATE.md) |
| Evaluators and lifecycle | direct serial plan; dependency, cycle and write validation; commit, abort and reset; retained snapshots | checkpoint restore; provider phase conformance | [evaluator](../contracts/EVALUATOR.md), [lifecycle](../architecture/FRAME_LIFECYCLE.md) |
| Physics table | world membership, selective removal, pre/Step/post barrier, bounded fixed ticks, pause, group reset, invalidation after Step | display interpolation; automatic body and constraint teardown | [evaluator](../contracts/EVALUATOR.md#5-shared-world-physics-orchestration), [lifecycle](../architecture/FRAME_LIFECYCLE.md#5-physics-clock-and-discontinuities) |
| `FixedTickSession` | seeding, bounded catch-up and backlog, pause, reset, transactional retry, retained output | | [lifecycle](../architecture/FRAME_LIFECYCLE.md#native-backend-free-fixed-tick-session) |
| ABI | revision and size validation, scoped handles, retain and release, separately built C provider and consumer | freeze; cross-toolchain evidence | [ABI](../contracts/ABI.md) |
| Capabilities and diagnostics | exact-version intersection, required support, ordered bounded diagnostics, provider provenance | semantic vocabulary; output negotiation; retained diagnostics | [capabilities and diagnostics](../contracts/CAPABILITIES_AND_DIAGNOSTICS.md) |
| Direct consumer API | retained snapshots with layout and value identity, under the [consumer contract](../contracts/EVALUATED_STATE.md#4-consumer-contract); renderer-side [resident matching](../architecture/OUTPUT_PATHS.md#resident-target-matching) | consumer cost measurements | [output paths](../architecture/OUTPUT_PATHS.md) |

Several independent instances are tested under serial execution. Parallel scheduling is
not implemented.

## Where it was measured

| Category | Windows x86_64 | Linux x86_64 | macOS arm64 |
| --- | --- | --- | --- |
| Core unit and installed consumer | hosted | hosted | hosted plain CMake; not a release target |
| `avatarVrmSession` package and consumer | pull request and release lanes | pull request and release lanes | no |
| Adapter integration outside the session | local only | no | no |
| Real-asset end-to-end | local only | no | no |
| Renderer consumption | local only, `lookdev` renderer with `usd` package | no | no |

Web and XR are not targets.

Update this page in the change that alters the state, and add the measurement to the
[evidence log](../reports/evidence-log.md). A sibling's callable library or published
package is an integration input, not proof that this runtime already composes it.
