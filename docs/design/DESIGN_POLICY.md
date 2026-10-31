---
status: accepted
owner: usd-avatar-runtime
---

# Design policy

The rules that decide what belongs in this repository and how its parts relate. It
owns intent, not status or schedule: what exists is in the
[capability matrix](../reference/CAPABILITY_MATRIX.md), and what comes next is in the
[roadmap](../roadmap/current.md).

## Purpose

`usd-avatar-runtime` is the format-independent avatar evaluation IR, lifecycle and
orchestration layer of the ecosystem. It defines how independently owned capabilities
compose into one avatar runtime, rather than reimplementing them.

> Format plugins own meaning. `usd-motion-plugins` owns generic motion mathematics.
> `usd-avatar-runtime` owns evaluation order and orchestration. Renderers own realization.

It must not become a renderer, a VRM or MMD implementation, a tracking SDK, a game engine
or an alternative to OpenUSD composition.

## Data flow

Preserve this direction:

```text
motion-connectors -> input bridge -> AvatarInputFrame
                                          |
motion / VRM / MMD owners -> usd-avatar-runtime
                             lifecycle, scheduler, capabilities, publication
                                          |
                                 EvaluatedAvatarState
                                  /       |       \
                               Hydra  fast path  recording
```

Never invert it so that a renderer calls back into format behavior. The composed USD
stage holds authored state; high-frequency evaluated state stays in runtime buffers, and
only explicit recording or baking writes it back.

## Responsibilities

Runtime owns instances, frame lifecycle and transactions, the evaluation plan,
dependency and phase ordering, capability negotiation, common input and state contracts,
retained snapshots, shared-world barriers, clock and fixed-step coordination, reset and
seek, adapter lifetime, publication and diagnostics. Other owners are listed in
[dependencies](../architecture/DEPENDENCIES.md#1-owners).

Add something here when it coordinates several avatar subsystems, defines cross-format
order, defines a common contract, publishes evaluated state or provides runtime-wide
lifecycle or diagnostics. Do not add it when it is specific to a format, generic motion
mathematics, a device or protocol connector, or a renderer realization.

## Thin adapters

```text
owner representation -> thin adapter -> runtime contract
```

An adapter receives owner values, invokes the owner's algorithm and marshals the result.
Registration, runtime layout and identity checks, lifetime and publication support that
bridge.

Do not duplicate samplers, retargeting, generic skeleton conversion, expression
algorithms, MMD IK or control. Do not embed physics solvers or renderer shader and backend
logic, or spread format branches through output consumers. When an owner API is missing,
extend the owner rather than writing a private substitute.

## Core dependencies

`avatarRuntime` stays free of external provider dependencies, and its C ABI hides the STL.
Integrations are separate optional targets. Finding or linking the core alone must not pull
in OpenUSD, VRM, MMD, Hydra, Jolt or a renderer. A session is a composition above the core:
it keeps Owner evaluators separate and takes no format algorithm into the core.

## Evaluation

The runtime owns the sequence, so that LookAt, expressions, constraints and physics
writing overlapping state have no hidden dependency chains. Plugins do not get their own
update loops. Phase names stay format-independent. Atomic owner calls are not split to fit
a sketch. Evaluators are deterministic for identical inputs, with no renderer or backend
dependency and explicit read and write domains.

Start with deterministic serial evaluation. Later parallelism uses explicit state
dependencies, independent avatars first. There is no singleton mutable state: each
instance owns its bindings, plan and state, and may share only immutable code and metadata.

## Evaluated state

Common state holds resolved effects, not source-format semantics. Do not add core fields
such as `vrmExpressionHappy` or `pmxMaterialMorph`; adapters convert owner results to
common pose, morph and material values, and namespaced format identity may survive only as
provenance.

A failed evaluation never partly updates a committed snapshot. An irreversible external
physics Step is distinct from ordinary rollback and is expressed through invalidation and
reset.

## Publication

```text
                     +-> Hydra adapter
EvaluatedAvatarState +-> fast-path adapter
                     +-> recording publication adapter -> motion owner recorder
```

All paths share one source of truth, identity and values. A fast path optimizes transport
only, and may use minimal-copy views, dirty masks, late publication and packed views. The
runtime manages no renderer resources and does not embed rendering APIs in the core ABI.

Runtime publishes state for recording; it does not implement a generic recorder. Storage,
resampling, compression and clip construction belong to the motion owner.

## Physics

Runtime solely owns boundary scheduling, the evaluation DAG, shared-world barriers, clock
and reset. Hosts supply their loop, inputs and time requests. The physics owner implements
worlds, Step and solving, and format owners implement bone/body coupling and secondary-motion
mathematics.

## ABI and execution mechanisms

Separately versioned repositories meet at a small C boundary: versioned structs, opaque
handles, span-style arrays and stable identifiers, with C++ conveniences above it. Keep it
experimental until the [freeze gates](../roadmap/current.md#abi-freeze) hold; real provider
and consumer evidence corrects the contract before it freezes.

OpenExec is an optional execution mechanism, not the runtime's architecture. Evaluator
cores remain callable directly, so fast paths stay light and tests stay simple.

## Testing

Core tests cover lifecycle, rollback, retained snapshots, isolation, dependency graphs,
malformed registration, capability discovery and ABI compatibility. Adapter tests check
registration, invocation, marshaling, diagnostics and lifetime against known owner values;
generic numerical correctness stays in owner repositories. Diagnostics preserve the owner's
name, version and subject, and never redefine its codes.
