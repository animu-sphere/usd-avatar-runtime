---
status: proposed
owner: usd-avatar-runtime
---

# Frame lifecycle

This proposal owns execution boundaries around the
[phase sequence](../contracts/EVALUATOR.md). It applies to direct and OpenExec
execution and every output path.

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

This describes a logical transaction; buffer allocation, output retention and
ABI call names remain open. Output adapters must not trigger evaluation again.

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

## 4. Concurrency

Initially, an instance evaluates one frame at a time and its phase order is
serial. Asynchronous connector intake and output consumption require snapshot
handoff; they cannot expose partially written frame buffers. Shutdown drains
or cancels outstanding work before releasing instances, providers or retained
outputs. Parallel scheduling remains later roadmap work.
