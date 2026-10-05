---
status: proposed
owner: usd-avatar-runtime
---

# Capabilities and diagnostics

This proposal develops [design policy sections 14–15](../design/DESIGN_POLICY.md).
It owns runtime negotiation and observation; provider-specific diagnostics
retain their original owner and codes.

## 1. Capabilities

Capability tokens have explicit versions. The policy's candidate vocabulary is:

```text
avatar.pose.humanoid
avatar.lookAt
avatar.expression
avatar.morph
avatar.secondaryMotion
avatar.appearance.materialOverride
```

These names are proposals, not a registered ABI enumeration. Freeze their
meaning, version compatibility and mandatory/optional requirement rules in
Runtime Phase A.

Distinguish provider support, capabilities of the bound avatar, the active
evaluation plan and what an output consumer can represent. Installing an
expression evaluator does not prove that a particular avatar has expression
bindings; a valid material override does not prove that a renderer supports
the affected input.

Negotiation returns the active capability set and reasons for unavailable
features. Missing required support prevents the plan/output binding from
activating. Optional unsupported outputs follow an explicit host policy and
produce diagnostics. Never rely on compile-time VRM/MMD assumptions alone.

## 2. Diagnostic context

A proposed record carries stable code and severity, origin/provider identity,
instance/frame, phase/evaluator, affected source/channel/target, readable
message and optional typed details. Record evaluation status separately from
severity; a warning alone must not ambiguously mean failed execution.

Preserve provider codes/provenance when aggregating. Runtime composition
errors have a distinct namespace. Numeric diagnostic IDs and serialized shapes
are not selected yet.

Required cases include missing mappings, dropped/stale input, unresolved
intent, unsupported outputs, invalid contracts, dependency cycles, conflicting
writes and non-finite transforms. Deterministic ordering and overflow/drop
behavior must be specified; logging must not block the frame on external I/O.

## 3. Observation hooks

Expose optional phase/evaluator timings, frame status, state hashes and a trace:

```text
input/source -> evaluator/phase -> affected channels -> resolved state
```

Profiling clocks measure execution but do not influence evaluated values.
Hash definitions identify the contract/layout versions and canonical encoding;
floating-point platform differences must not be hidden by an unspecified hash.
Logical equality/numeric comparison remains necessary alongside hashes.

Replay records evaluator diagnostics separately from transport/consumer
diagnostics. The former reproduce evaluation; the latter explain publication
and display behavior. See [recording and replay](../design/RECORDING_AND_REPLAY.md).
