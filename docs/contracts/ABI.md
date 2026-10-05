---
status: proposed
owner: usd-avatar-runtime
---

# Cross-package ABI

This document develops [design policy section 9](../design/DESIGN_POLICY.md).
The direct implementation exposes experimental revision 3 through
[`api.h`](../../include/avatarRuntime/api.h). `arGetApi` is the only exported
runtime entry point; it fills the caller's `ArRuntimeApi` function table.
This is a tested prototype, not an ecosystem ABI freeze. Owner motion/gaze mapping,
checkpoint restore and real-provider conformance remain Runtime Phase A gates.
Under the [near-term direction](../design/NEAR_TERM_PLAN.md#2-contract-validation-before-freeze),
Phase A/B integration precedes freeze: real VRM and MMD evidence must inform
the revision, phase model, state representation, capability meanings and
lifecycle. Separately compiled synthetic providers alone do not establish
a freeze candidate.

## 1. Representation

Implemented revision-3 rules:

- Windows uses `__cdecl` and default compiler packing. Descriptors/views begin
  with `uint32_t struct_size` and `abi_version`; fixed value records and writer
  tables are versioned by their enclosing contract. All required fields of
  revision 3 must fit. Unknown larger tails are ignored/left untouched; any
  different revision is rejected before reading the descriptor body.
- Initialize descriptors to zero, then set `AR_HEADER(Type)`. `arGetApi` takes
  an explicit revision and output capacity. No packed structs, STL, USD types,
  exceptions or renderer handles occur in the public headers.
- Arrays use fixed-width counts and borrowed pointers. Counts above 1,048,576
  are rejected. Identifiers are nonempty UTF-8 strings, at most 1,024 bytes,
  without ASCII control characters. Strings are NUL terminated. Callers must
  supply readable memory of the declared size; these checks are not a memory
  sandbox for a malformed pointer.
- Runtime, instance and snapshot handles are nonzero `uint64_t` IDs, never
  recycled during the loaded library's lifetime. Wrong-kind, released, stale
  and cross-runtime instance handles return `AR_INVALID_HANDLE`. A process-wide
  handle table manages lifetime; mutable avatar/provider state is per instance.
- Registration copies descriptor identities, dependencies and capabilities;
  instance creation copies the authored output layout and initial values.
  Provider `user_data` and callback code remain borrowed.

Revision 2 introduced fields in `ArInstanceDesc`, `ArInputFrame` and `ArStateView` for
required binding layout ID/version, input revision and snapshot-associated
active capabilities. Revision 3 appends typed gaze observations to `ArInputFrame`
and preserves that snapshot metadata. Revisions 1 and 2 are rejected at table
and descriptor/view negotiation before reading their bodies. This is an
experimental breaking change, not a compatible tail extension or an ABI freeze.
Providers/consumers must rebuild with revision-3
headers and supply nonempty layout ID/nonzero version on instance creation.
The function table has the same operations. ABI revision is independent of
the experimental CMake package version.

Before freeze, review the selected layout/alignment, calling convention and
minimum-size rules with real provider adapters. C++ conveniences remain a layer
above this table, never an exported binary class boundary.

`ArGazeInput` is a fixed value record versioned by its enclosing input frame,
like `ArScalarInput`; it has no nested header. Its array, identity/reference
strings and values are borrowed for the same synchronous evaluation lifetime.
All known revision-3 fields must fit even when `gaze_count` is zero. No gaze
input pointer is retained by the runtime or snapshot. The
[input contract](INPUT_FRAME.md#revision-3-gaze-observations) owns its space,
validity, direction and absence rules.

Existing motion and format libraries may use C++/OpenUSD values internally.
Provider adapters marshal those values into versioned views; the ABI must
preserve the owner's semantics without exposing its binary class layout or
inventing a second motion model.

## 2. Lifetime and calls

The prototype implements the following lifetime model:

| Boundary | Prototype rule |
| --- | --- |
| input views | borrowed synchronously through `evaluate_frame`; no retention or asynchronous intake |
| evaluated outputs | successful evaluation returns one owned snapshot reference; `retain_snapshot`/`release_snapshot` manage additional references; views last until the last reference is released |
| callbacks | synchronous on the caller's thread; runtime API calls from provider/diagnostic callbacks return `AR_BUSY`; writer/diagnostic hooks are callback scoped |
| allocation | runtime buffers are released by runtime functions; providers create/destroy their private state using their own allocator |
| provider unload | code and `user_data` stay valid until runtime destruction completes; there is no unregister/unload API yet |
| instance shutdown | API calls serialize; destruction waits for earlier evaluation, destroys private state and preserves independently retained output snapshots |

Snapshot storage contains copied values/identities and no provider-private
pointers. It survives instance/runtime destruction; keep the runtime library
loaded until the final snapshot release. Capability views are borrowed until
instance destruction and remain unchanged by reset. Capabilities obtained
through a retained snapshot instead share that snapshot's lifetime, as do
layout identity/version and input revision. The snapshot never borrows its
capability strings from a provider or destroyed instance. Hosts must coordinate
release/destruction with readers. All API calls are serialized in this initial
implementation, including calls for different runtime objects; parallel
scheduling and cancellation are future work. A callback must not wait for a
different thread to call the runtime API. The stateless `arGetApi` table query
does not take this lock and may be used from callbacks.

No allocation may require freeing with another package's incompatible allocator.
No exception may cross a C boundary. Validation failures and evaluator refusals
return defined statuses with structured diagnostics.

Runtime entry points catch allocation/implementation exceptions. Provider and
diagnostic callbacks must not throw. Unexpected evaluator exceptions are caught
as a defensive measure and abort the transaction. `end_frame` and destruction
are contractually infallible; an exception during commit/abort poisons the
instance and requires reset. Revision 3 does not promise recovery from an
arbitrary provider violating its lifecycle contract.

## 3. Compatibility and portability

Define ABI compatibility independently from provider semantic versions and
capability versions. Negotiation must reject incompatible mandatory layouts
before accessing data. Optional extensions are discoverable, not assumed.

The contract tests compile the runtime, a C11 provider DLL and C11 consumer
separately. An additional external CMake build uses only installed runtime
headers, the exported package target and its shared library. A C++20 test also
includes the public headers. Evidence currently covers Windows x64/MSVC only;
it does not establish cross-toolchain or cross-platform compatibility (`RT-O7`).
Optional features are not inferred from ABI success. Unknown evaluator flags,
state domains and typed material kinds are rejected. Semantic capability
versions are independent from `AR_ABI_VERSION` and use exact matching.

WASM-friendly representation is a design objective. It still needs decisions
about pointer width, host memory, optional dependencies and provider loading;
native dynamic loading cannot be presumed to work unchanged in a browser.
