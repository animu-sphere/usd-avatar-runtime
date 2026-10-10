---
status: proposed
owner: usd-avatar-runtime
---

# Frame lifecycle

This proposal owns execution boundaries around the
[phase sequence](../contracts/EVALUATOR.md). It applies to direct and OpenExec
execution and every output path.

The direct implementation in
[`runtime.cpp`](../../libs/avatarRuntime/runtime.cpp) exercises this lifecycle
with experimental C callbacks. Reset/transaction rules below are implemented;
checkpoint encoding/restore and real-provider conformance remain RT-O5 work.

## 1. Prepare an instance

Load/register evaluator providers, resolve authored bindings, inspect
capabilities and construct a deterministic plan. Validate dependencies and
output requirements before admitting a frame. Missing required providers,
incompatible contracts or dependency cycles prevent that plan from becoming
active; optional omissions produce structured diagnostics.

Binding/configuration changes create a new generation at a frame boundary.
They must not mutate an evaluator's inputs during an in-flight evaluation.
Topology/joint/material identity changes invalidate affected buffers and
consumer bindings explicitly.

## 2. Execute a frame

1. The host supplies the evaluation instant and clock mapping. The input
   bridge polls/binds connector data and assembles an immutable input snapshot.
2. The runtime selects the instance configuration generation and prior state,
   then prepares a working state buffer.
3. Evaluators execute in the validated order with declared inputs and writes.
   Diagnostics and timings are associated with that instance and frame.
4. Final consolidation validates channel shapes, target identity and finite
   transforms, then creates a publishable evaluated-state snapshot.
5. Output adapters consume that snapshot. Capture associates it with the
   exact input, evaluator/configuration versions and prior-state provenance.
6. The runtime commits the successful frame's state and releases temporary
   resources according to the selected lifetime model.

This describes a logical transaction. The scoped prototype rules below define
buffer ownership and calls; final ABI freeze still needs provider conformance.
Output adapters must not trigger evaluation again.

In revision 4, `evaluate_frame` validates the input and starts a fresh authored
baseline, then calls `begin_frame`/`evaluate` in plan order. Stateful providers
stage private changes during these callbacks. `end_frame(commit=0)` runs in
reverse begin order after any failure, including failure of `begin_frame`
itself. A failed attempt neither advances the last successful frame nor returns
a snapshot; its output handle is zero, allowing correction/retry with the same
frame ID. Stateless providers have only an `evaluate` callback.

Typed gaze identity/space/reference/validity, numeric values and clock mappings
are validated before any `begin_frame`. Malformed observations cannot stage
provider changes. Valid unavailable/stale observations are forwarded unchanged;
the owning adapter supplies its explicit hold/drop policy. Input observations
are borrowed for this call and are not retained as prior state.

After successful validation and all snapshot allocation, the runtime invokes
infallible `end_frame(commit=1)` in plan order and records the prior successful
snapshot. Only then does `evaluate_frame` return a retained immutable snapshot.
Consumer transport runs outside this transaction. A failed consumer can retry
delivery of that retained snapshot; it cannot roll back/re-evaluate the already
committed provider state. This scopes the publication-failure question for the
direct path; future adapters must preserve the same distinction.

## 3. Failure and discontinuities

The proposed default is to publish only a complete valid snapshot. Evaluation
failure produces an explicit failed-frame result; a previous valid snapshot
can remain displayed if the host chooses, carrying its original frame identity.
It must not be labeled as the failed frame's newly evaluated state.

Stateful evaluators need a specified commit/abort mechanism so that failed
frames cannot partially advance filters or simulation. How provider-private
state is checkpointed is `RT-O5`, a contract-freeze question. Consumer failure
after successful evaluation is a publication error, distinct from evaluator
failure; it must not cause a second simulation step for the same frame.

Seek, clock discontinuity, input-source replacement and configuration changes
need explicit reset/restore rules. Reverse-time evaluation is not implied by
accepting a timestamp. A replay starts from a declared initial state or a
checkpoint, rather than from whichever live frame happened to run last.

The prototype accepts strictly increasing nonzero successful frame IDs and
nondecreasing finite evaluation seconds; a failed frame may be retried. Seeking
backwards or changing source/configuration policy requires `reset_instance`
with a strictly larger generation. Reset first creates all replacement provider
states. If creation fails, temporary states are destroyed and the old committed
state/generation survive. Success replaces private state and clears prior-frame
history. `destroy_state` must safely accept a null/partially created state after
failed creation. Changed layouts/plans currently require a new instance;
checkpoint restore is not implemented. Retained snapshots survive either path.

Binding layout ID/version and negotiated active capabilities remain unchanged
by reset and are retained with each snapshot. The selected input revision is
copied into working/published state; prior state retains its own successful
input revision. A failed attempt cannot replace that prior metadata. Layout
changes require a new instance and host-assigned layout ID/version, independently
of a reset generation; see the [state contract](../contracts/EVALUATED_STATE.md#3-snapshot-rules).

## 4. Concurrency

Initially, an instance evaluates one frame at a time and its phase order is
serial. Asynchronous connector intake and output consumption require snapshot
handoff; they cannot expose partially written frame buffers. Shutdown drains
or cancels outstanding work before releasing instances, providers or retained
outputs. Parallel scheduling remains later roadmap work.
