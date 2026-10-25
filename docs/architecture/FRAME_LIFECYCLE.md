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

## 5. Physics clock and discontinuities

The 2026-10-20 physics orchestration direction assigns clock/fixed-step
coordination, reset/seek and final publication to this runtime. This section
defines lifecycle requirements. The optional experimental physics table now
implements the bounded clock/group recovery described below. The optional
[MMD adapter](MMD_ADAPTER.md) composes owner continuations with group reset;
display interpolation and broader host integration remain open.
The [evaluator contract](../contracts/EVALUATOR.md#5-shared-world-physics-orchestration)
owns the pre/Step/post barrier order.

The host supplies elapsed time, pause/seek requests and avatar state changes.
Runtime accumulates elapsed simulation time, determines the number of due
fixed ticks and runs the barrier for each tick. Fixed tick duration, maximum
catch-up tick count and excess-time policy must be configurable and observable.
The experimental binding requires explicit positive `fixed_step_seconds` and
`max_catch_up_ticks`; it supplies no hidden timestep default. Excess time is
retained and returned as `pending_seconds`. Neither the host nor an individual
avatar adapter runs a second accumulator for the same world.

Simulation tick time and display evaluation time are separate. A display
request can require zero, one or several fixed ticks. Interpolation/publication
reads committed simulation results and produces an immutable display snapshot;
it must not mutate solver state or advance a world to satisfy a renderer retry.
Interpolation method and tick/display provenance representation require
contract review under RT-O4/RT-O6; no new revision-5 fields are implied.

Required discontinuity behavior before physics integration:

| Event | Required lifecycle policy |
| --- | --- |
| pause / resume | pause adds no simulation time and performs no Step; resume must not catch up time spent paused; state/input changes still follow validated frame boundaries |
| reset | invalidate affected simulation history and accumulator; recreate/reinitialize adapter and physics state from declared bindings/pose before resuming ticks; retained snapshots keep their original identity |
| seek / loop wrap / clock discontinuity | explicitly invalidate affected physics and interpolation history; start from a declared reset state or supported checkpoint; a backward jump is not a negative Step |
| avatar state or binding change | apply at a safe boundary; determine whether a value update, physics reinitialization or structural rebinding is required; preserve generation/layout rules above |
| avatar unload | remove it from barrier membership and detach its bodies/coupling resources only after in-flight work drains; invalidate affected interpolation/bindings and preserve unrelated participants |
| physics disabled | bypass physics adapters/barriers; static and motion-only evaluation still consolidates and publishes the same state contract |

For shared worlds, define the reset/seek scope with the physics owner: an
avatar-local reset cannot silently restore the entire world or leave coupled
participants with incompatible history. Adapter references must outlive
in-flight callbacks, while published snapshots retain values without keeping
mutable world handles. Exact destruction order and checkpoint/velocity restore
are RT-O5/RT-O6 decisions to validate. Full replay to an arbitrary seek time
is outside the physics MVP.

A failure after `Step` cannot be treated as an ordinary per-instance abort
unless the world supports validated rollback. Specify world-group commit,
recovery/invalidation and same-tick retry rules before admitting non-idempotent
steps (RT-O5/RT-O6). Never publish mixed pre/post states or step again merely
because a consumer failed; the last valid snapshot can remain displayed with
its original identity.

### Experimental fixed-tick requests

The optional [Stage Runner session](STAGE_RUNNER_SESSION.md) wraps this table
with a measured host clock, playback transitions and retained display output.
It introduces no accumulator or physics/backend dependency into core.
Its optional owned registration permits whole-world close before external
resource unload. This removes every member and requires explicit instance
reset before reuse. Selective removal uses the separately negotiated membership
extension below.

`advance_world` accepts nonnegative finite elapsed time, pause state and exactly
one input template per participant. Runtime preserves observations and input
revision, and assigns the participant generation, tick ID and evaluation seconds
for each due tick. Motion evaluators therefore sample at the fixed tick time;
the supplied observations are held across catch-up ticks with their original
source clocks/provenance. Hosts must explicitly assemble other per-tick input
policies; no resampling or freshness policy is invented here.

Tick IDs start at one. Simulation time is `start_seconds + tick_id * fixed_step_seconds`.
A request runs zero through the configured maximum ticks, returning the last
tick/time and backlog. Pause discards that request's elapsed time and does not
process existing backlog. Resume adds only the elapsed time explicitly supplied
after pause. Every accepted request needs a strictly increasing nonzero
`request_id`, preventing duplicate elapsed-time submission with the same ID.
Input validation precedes acceptance. Once accepted, even a failed pre-Step
attempt retains elapsed time; retry with a new request ID and zero elapsed.
Completed earlier ticks in a multi-tick request remain committed.

Successful requests return retained snapshots in input-array order. A zero-tick
request returns the last committed snapshots, or zero before the first tick.
Repeated paused requests with zero elapsed can retrieve retained state without
simulation. These are tick snapshots; display interpolation and separate
display-frame provenance remain unimplemented. Consumers retry delivery using
retained handles, never by resubmitting elapsed time.

Failure after the world Step callback starts, including Step refusal, post
evaluation, allocation or commit failure, invalidates the entire group and
returns no snapshots. Prior committed snapshots remain valid. Further advances
are rejected until `reset_world`; runtime never assumes backend rollback.
An infallible provider lifecycle violation likewise requires group reset.

`arGetPhysicsMembershipApi` negotiates `ArPhysicsMembershipApi` version 1,
independently of the unchanged core and physics tables. Its
`remove_participant(runtime, world, instance)` removes one participant at a
serialized call boundary. It invokes no callback and preserves the world's
tick, simulation time, backlog, request history, remaining instance generations
and committed snapshots. The detached instance is invalidated for evaluation
or rebinding until an explicit increasing-generation reset; destruction is
immediately permitted. Retained snapshots keep their values and identities.
Removing the last avatar leaves a valid world for non-avatar simulation.

An invalid handle, non-member or invalidated world refuses without membership
changes. Removal cannot recover a poisoned group, including a poisoned member:
reset the group or close it. Callback reentry returns `AR_BUSY`. The exclusive
host driver must remove external body/constraint resources, pending commands,
body snapshots and input rows before its next advance or reset. The core owns
only scheduler membership; it never infers those resources. Cross-avatar
constraints require an explicit host teardown policy. Provider registrations,
code and shared `user_data` still outlive the runtime; removing an instance is
not an evaluator-unregister API.

`reset_world` first creates all replacement provider states with a generation
greater than every participant's. Creation failure leaves the old group intact.
It then invokes the external reset callback for the entire solver, including
non-avatar participants. A failed callback leaves the group invalid. Success
installs the new provider states and clears tick/request history, backlog and
prior states, returning simulation time to the declared start. Retained
snapshots keep their original generation and values. Seek/loop wrap may use
this explicit restart; arbitrary-time replay and checkpoint restore are open.

Evidence: `contracts.physics` covers two participants, a same-phase pre/post
dependency, zero/one/multiple ticks, catch-up backlog, pause, retained snapshots,
reentry, direct-call exclusion, pre-Step retry, Step/post failure and group reset.
The existing core C provider/consumer, transaction and installed-consumer tests
remain required. This validates runtime scheduling with constructed callbacks;
it does not establish MMD coupling, Jolt or real-avatar conformance.
