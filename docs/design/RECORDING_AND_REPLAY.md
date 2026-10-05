---
status: proposed
owner: usd-avatar-runtime
---

# Recording and replay

This proposal develops [design policy section 13](DESIGN_POLICY.md). It adds
runtime evaluation provenance around motion recording, not a parallel motion
format.

## 1. Capture contents

| Content | Purpose |
| --- | --- |
| assembled inputs and selected stream samples | reproduce exactly what evaluation consumed |
| source clocks, mappings, actors and timestamps | reproduce input selection and time placement |
| authored binding/configuration identity | reproduce the avatar/layout used |
| provider identities, versions and active plan | identify the algorithms and ordering |
| initial state, explicit seeds and checkpoints | reproduce stateful evaluators and discontinuities |
| evaluated snapshots and diagnostics | regression baseline and debugging evidence |
| contract/encoding versions and state hashes | validate compatibility and corruption |

Reuse `usd-motion-plugins`' motion recording primitives and persistence contract
for motion samples; reference the
[motion contract](https://github.com/animu-sphere/usd-motion-plugins/blob/main/docs/design/MOTION_CONTRACT.md).
The runtime envelope relates inputs, results and provenance. Its encoding and
checkpoint model remain undecided (`RT-O8`). Raw device packet capture stays
with connector tooling; selected semantic input is the runtime replay boundary.

## 2. Replay modes

**Evaluation replay** restores the declared configuration/initial state and
runs the recorded input sequence through the captured evaluator versions.
Compare resolved state, channel identity and deterministic diagnostics with
the recorded results. A missing version/configuration must produce an explicit
compatibility failure or clearly declared non-baseline run.

**Output replay** publishes captured evaluated snapshots without executing
evaluators. It is useful for Hydra/direct-consumer parity and renderer
performance experiments. It does not prove evaluator determinism.

Keep timings as observations, rather than values that drive replay. Repeatable
numeric behavior is scoped to stated versions, configuration and platform
rules; cross-platform comparison uses explicit tolerances instead of promising
unmeasured bitwise equality.

## 3. Acceptance evidence

Cover missing/zero input distinctions, source replacement, seek/reset,
failed-frame commit/abort, configuration rebinding and multi-avatar isolation.
Compare direct/OpenExec execution from identical initial state when OpenExec
is enabled. Feed the same resolved snapshots through both output adapters.

Fixtures must be redistributable and carry provenance. A benchmark records
hardware, target/runtime versions and instrumentation settings; it separates
evaluation cost, publication cost and renderer/display cost.
