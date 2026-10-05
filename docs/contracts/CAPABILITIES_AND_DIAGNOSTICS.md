---
status: proposed
owner: usd-avatar-runtime
---

# Capabilities and diagnostics

This proposal develops [design policy sections 14–15](../design/DESIGN_POLICY.md).
It owns runtime negotiation and observation; provider-specific diagnostics
retain their original owner and codes.

The direct prototype implements exact-version capability negotiation and
callback-scoped diagnostics using
[`types.h`](../../include/avatarRuntime/types.h) and
[`diagnostics.h`](../../include/avatarRuntime/diagnostics.h). Candidate semantic
tokens below remain proposals until actual adapters establish their meaning.

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

Revision 1 computes active support as the intersection of selected providers'
supplied tokens and the instance's explicitly bound tokens at the **same
nonzero version**. Required instance/evaluator capabilities must occur in that
intersection; conflicting versions supplied by selected providers are rejected.
Optional bound tokens without a selected provider produce an inactive-capability
warning. `get_capabilities` returns the active set sorted by ID; installation or
registration alone does not enable a feature. The application must declare the
bound set honestly; the runtime cannot infer rig support without a binding
adapter. Output-consumer negotiation is still Runtime Phase C work.

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

Revision 1 delivers synchronous diagnostic callbacks in validation/plan order,
preserves provider `origin`, `code`, `subject`, message and status, and stamps
the active evaluator/instance/frame/phase. Runtime composition records use
`runtime.*` codes. Records and strings are borrowed only during the callback;
consumers copy them to retain them. The sink must avoid blocking I/O and must
not throw or reenter the runtime. Up to `AR_MAX_DIAGNOSTICS` (256) records per
operation are delivered, followed by one `runtime.diagnostics.overflow`
warning if additional records are omitted. The overflow warning uses runtime
origin and does not change the operation status. Malformed provider records
fail the frame. Severity never decides success: callback statuses and state
validation decide it. Missing mappings/stale-source diagnostics still require
actual provider/connector adapters; timings, hashes and trace tooling are future
work.

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
