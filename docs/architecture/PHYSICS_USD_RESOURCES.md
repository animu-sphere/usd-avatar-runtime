---
status: binding
owner: usd-avatar-runtime
---

# Host-owned USD physics scene resources

The optional `avatarPhysicsUsdResources` target supplies
[`avatarPhysics::SceneResources`](../../adapters/physics-usd/include/avatarPhysics/SceneResources.h)
and [`avatarPhysics::StageWorld`](../../adapters/physics-usd/include/avatarPhysics/StageWorld.h).
`SceneResources` owns a resource set inside a borrowed neutral `PhysicsWorld`,
using a snapshot from the installed physics owner's
[`readPhysicsScene`](https://github.com/animu-sphere/usd-physics-plugins/blob/main/docs/design/USD_BRIDGE_CONTRACT.md).
The reader owns USD validation and declarations; the backend owns execution;
this host adapter owns path/handle maps and ordered resource lifetime.
`StageWorld` adds whole-stage discovery and world ownership through a
caller-selected factory.

## Build and installation

Enable `AVATAR_BUILD_PHYSICS_USD_RESOURCES=ON` with installed `physicsUsd` 0.1
and compatible `physicsCore`/OpenUSD dependencies on `CMAKE_PREFIX_PATH`.
No backend, format provider, Stage Runner or renderer is required by this
target. Core-only and session-only package lookup import no physics target.

```cmake
find_package(AvatarRuntime 0.1.0 EXACT CONFIG REQUIRED COMPONENTS physics_usd)
target_link_libraries(host PRIVATE AvatarRuntime::avatarPhysicsUsdResources)
```

This is an optional C++17 host boundary, without a new core C table or ABI
freeze. Install the selected backend separately in applications that need it.

## Construction

The caller creates the solver with its chosen world settings and passes a
`PhysicsScene` value. All bodies in that snapshot form one resource set. Body
paths must be unique absolute prim paths. Each enabled joint resolves its
nonempty endpoints within that set; an empty endpoint denotes world. Joints
between resource sets reject before world mutation. Enabled joints require
the optional neutral `SixDofConstraints` capability, including fixed joints
whose local frames must be preserved. Disabled joints create no constraint.
Joint conversion delegates to the owner's `toSixDofConstraint`.

An optional `BodyPolicy` prepares body descriptors before resource creation.
It may select neutral filters, contact coefficients, damping, mass, motion type
or initial transforms. Shape assignment belongs to the resource set and a
policy-supplied shape handle rejects. Policy must not mutate the borrowed world.
Source-specific interpretation stays in the caller's format adapter. Standard
reader admission rules and unsupported format metadata remain unchanged.

An optional `JointPolicy` prepares each enabled joint's converted neutral
six-DOF descriptor once, before backend mutation. It may configure springs
or other neutral properties but cannot change endpoint handles. The adapter
validates its result, then replaces provisional endpoints with the actual
body handles during resource creation. Disabled joints skip this callback.
Policy failure creates no resources; callbacks and descriptors are not retained
after construction. `StageWorld` forwards both optional policies. Format
semantics and experimental opt-in selection remain the caller's responsibility.

Bookkeeping allocation, path/endpoint validation, descriptor preparation and
owner conversion precede backend mutation. Construction then creates shapes,
bodies and enabled constraints. A backend exception or invalid returned handle
rolls back resources already created by this constructor, leaving other sets
alone. This relies on the backend's exception-safety and live-handle destruction
contract, and on exclusive host access. The constructor retains no snapshot,
stage or policy callback. `Bodies()` borrows the current path/handle map.

Scene gravity/settings are not applied to an existing world. Select them when
constructing that world. `SceneResources` creates neither a solver nor runtime
instances/world registrations, and never steps or edits USD.

## Whole-stage world binding

`StageWorld(stage, factory, policy)` reads the complete stage through the installed
owner reader, then passes its optional `SceneSettings` snapshot to a required
`WorldFactory`. Owner validation failures propagate unchanged, before the factory
is invoked. The reader admits at most one active scene; the adapter neither picks
the first of several scenes nor filters unsupported declarations. No scene
declaration passes an empty optional so the caller can choose world defaults.

The factory chooses the backend, applies the supplied gravity/settings and returns
an exclusively owned, empty neutral world. It must not retain borrowed settings
references. A missing factory or null result rejects; a factory exception
propagates. The adapter creates one complete `SceneResources` set in that world.
Policy or construction failure rolls back resources and destroys the new world.
No partial binding is exposed. No factory, policy or stage reference is retained;
stage edits require explicit close and reconstruction at a synchronous boundary.

`World()` and `Resources()` borrow the owned world and its complete resource set.
`Settings()` retains the owner snapshot, including after close. `Close()` first
closes resources and then destroys the world, is idempotent, and makes both
borrowed pointers null on success. A teardown refusal preserves the world and
remaining resource set for retry. Resolve refusals before destruction; as with
`SceneResources`, destructor cleanup relies on live-handle destruction succeeding.

This whole-stage scope allows joints between any admitted body paths in the
snapshot, including different avatar roots, because every constraint and endpoint
shares one lifetime. It exposes no selective participant/resource removal. Close
or reconstruct the complete scope for topology changes. Hosts requiring selective
avatar unload still use separate resource sets with no cross-set constraints.
Detach all runtime registrations and discard external handle/command references
before closing the world. The adapter creates no runtime instance, membership,
clock, fixed tick, renderer resource or USD writeback. It never calls Step.

## Unload, close and reset

Keep the borrowed world alive through destruction. Do not externally destroy
owned handles or attach constraints owned outside this resource set. A host
requiring cross-avatar constraints needs a separate explicit lifetime policy;
this component does not claim to manage them.

At an exclusive synchronous boundary, first detach the participant from the
runtime barrier, then call `Close`, discard pending commands/results/input rows
and destroy the runtime instance before the next advance/reset. Whole-world
shutdown closes the session registration before closing resource sets and
releasing the solver. Group reset closes all old sets before rebuilding the
solver and surviving path/handle maps.

`Close` destroys constraints before bodies and bodies before shapes. Successful
close empties `Bodies()` and is idempotent. On a backend refusal it returns false
and stops before dependent resources are destroyed; the host must resolve the
refusal and retry before advancing or releasing the world. Successful earlier
destruction is retained, so close is not transactional rollback. Destruction
attempts close automatically; use explicit checked close when refusal needs
handling. Under the exclusive ownership contract, live-handle teardown must
succeed, including constructor rollback.

The adapter does not remove scheduler membership or reset providers. Retained
CPU/renderer output has its own lifetime and requires none of these resources.
See the [session contract](STAGE_RUNNER_SESSION.md) for those separate rules.

## Scope

The [capability matrix](../reference/CAPABILITY_MATRIX.md#scoped-physics-scene-resource-evidence-2026-10-21)
records resource-set failure, installed-consumer and generated MMD/Jolt evidence.
The [stage-world evidence](../reference/CAPABILITY_MATRIX.md#scoped-stage-world-binding-evidence-2026-10-21)
records the separate whole-stage scope. Automatic application discovery, backend
selection, PMX/avatar configuration, selective unload with cross-avatar
constraints, source-specific spring/material semantics, resident
rendering, Hydra agreement and representative L3 acceptance remain open.
