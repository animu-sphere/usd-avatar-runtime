---
status: binding
owner: usd-avatar-runtime
---

# Scoped VRM registration adapter

The optional `avatarVrmAdapter` target connects the installed
[`vrmRig` owner library](https://github.com/animu-sphere/usd-vrm-plugins/tree/main/libs/vrmRig)
to the experimental revision-3 runtime. This document owns the implemented
adapter boundary. It does not establish real-avatar or milestone acceptance.

## Build and installation

The default `avatarRuntime` target has no provider dependencies. Enable
`AVATAR_BUILD_VRM_ADAPTER` to resolve `vrmRig` using its installed CMake package;
its transitive dependencies include `motionCore` and OpenUSD value libraries.
No sibling source directory is compiled into this runtime.

In an x64 developer shell with the owner's dependency DLLs on `PATH`:

```sh
cmake -S . -B build/vrm -G Ninja -DCMAKE_BUILD_TYPE=Release -DAVATAR_BUILD_VRM_ADAPTER=ON -DCMAKE_PREFIX_PATH="<vrmRig-install>;<motionCore-install>;<usd-install>"
cmake --build build/vrm
ctest --test-dir build/vrm --output-on-failure
cmake --install build/vrm --prefix install/vrm
```

Consumers request the optional installed component explicitly:

```cmake
find_package(AvatarRuntime 0.1.0 EXACT CONFIG REQUIRED COMPONENTS vrm)
target_link_libraries(host PRIVATE AvatarRuntime::avatarVrmAdapter)
```

Finding the core package without `COMPONENTS vrm` does not resolve owner
dependencies. The adapter has a C++ configuration API above the common C ABI;
this is not a new cross-toolchain C++ ABI guarantee.

## Configuration and lifetime

[`ExpressionAdapterConfig`](../../adapters/vrm/include/avatarVrm/ExpressionAdapter.h)
copies the owner's `ExpressionRig`, optional expression-type `LookAtRig`,
layout ID/version, evaluator ID, input selections and output bindings.
The host currently supplies these values; USD stage binding is still pending.
`Descriptor()` supplies the registration table. Keep the adapter alive until
runtime destruction, because registration borrows its immutable `user_data`.
Multiple instances can share that configuration without sharing frame state.
Different avatar bindings require different adapter objects/evaluator IDs.

Each input mapping selects exactly `(source, actor, channel)` and names the
owner expression verbatim. The adapter does not invent a semantic vocabulary.
Duplicate selections or multiple mappings to one expression are rejected at
construction. Absence contributes nothing; an explicit scalar zero reaches
the owner resolver and writes its affected targets at zero. Finite double
weights exceeding float range saturate during marshalling, then the owner
applies its own clamp and diagnostic.

Morph bindings map opaque owner targets to `(mesh_id, target_id)`. Material
bindings use the owner's canonical color-slot table, including separate RGB
and alpha inputs where specified. Authored baseline material values must be
present in `ArMaterialInput.value` even with `overridden=0`; the adapter reads
the current working values as the owner resolver's base. Earlier material
writes therefore participate through explicit scheduler ordering.

Every frame verifies layout identity/version and all declared rig effect
bindings, including material input types, before owner evaluation. Missing
targets and unsupported slots fail publication rather than dropping effects.
The host remains responsible for assigning honest layout identities.

## Execution and supported gaze

One stateless callback in `AR_PHASE_EXPRESSIONS` executes the scoped sequence:

```text
selected gaze -> owner LookAt -> named contributions
                                      |
selected scalars ----------------------+
                                      |
                         owner ExpressionResolver
                                      |
                      common morph/material writes
```

The callback reads material values and, when LookAt is enabled, pose. It writes
deformation/material domains. Earlier pose phases precede it; `after` names
additional required predecessors. LookAt contributions are resolved exactly
once, with no intermediate state shared across frames. This is scoped RT-O3
evidence; bone-driven gaze and actual motion/MMD plans remain unvalidated.

LookAt currently accepts selected **runtime-world points** with valid
observations. It derives head world position/orientation from the working
parent-local rig, including root placement, using OpenUSD value operations.
Head ancestry must have unit scale within `1e-6`; owner positions/targets must
fit finite float values. Missing head joints, scaled ancestry, joint-local
points and directions produce explicit failures. Bone-type LookAt is rejected
at construction. These restrictions do not narrow the general input contract.
Absent gaze produces no contribution. Stale/unavailable selected gaze is
diagnosed and contributes nothing; the complete snapshot returns unwritten
effects to baseline. It does not hold the previous gaze silently.

When valid LookAt reports a name also mapped by a scalar, the LookAt value
replaces that scalar and an informational precedence diagnostic identifies
the collision. The owner resolver subsequently applies the rig's expression
arbitration. Scalar look expressions remain usable when gaze is absent or
unavailable. This is an explicit adapter policy, not a core arbitration rule.

## Diagnostics and evidence limits

Diagnostics identify `usd-vrm-plugins.vrmRig` as origin; the runtime stamps the
evaluator, instance, frame and phase. Adapter codes report layout, target,
head, space/range and availability failures. Owner unresolved, clamped,
suppressed and warning results are forwarded with their named subjects or
warning text. Provider failure returns `AR_PROVIDER_ERROR` at the runtime
boundary; the diagnostic retains the underlying adapter status.

The tests exercise installed owner algorithms with constructed rigs and test
input, numeric direct-owner parity at `1e-6`, isolation, reset, absence/zero,
rollback/retry and retained output lifetime. A separately configured installed
consumer repeats the boundary checks without source-tree include paths.
Versions and target evidence are recorded in the
[capability matrix](../reference/CAPABILITY_MATRIX.md).

Real VRM asset binding, Humanoid/motion/connector integration, bone LookAt,
joint-local/direction gaze, resolved expression/gaze records and renderer
consumption remain in the [roadmap](../roadmap/current.md). No milestone,
ABI freeze, pixel parity or renderer support follows from these tests.
