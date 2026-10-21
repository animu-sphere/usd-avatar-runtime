#pragma once

#include <avatarRuntime/physics.h>
#include <usd_stage_runner/runtime/frame_clock.h>
#include <vector>

namespace avatarStageRunner {

// Single-threaded host binding. Runtime outlives the session. World callback
// resources outlive registration; evaluator lifetimes follow the core API.
// It owns its clock and retained output and can optionally own the runtime
// world registration. Solver resources remain caller-owned. No accumulator.
class PhysicsSession {
public:
  using FrameClock = usd_stage_runner::runtime::FrameClock;
  PhysicsSession(
      ArRuntime runtime, ArPhysicsWorld world,
      FrameClock::NowFunction now = [] {
        return std::chrono::steady_clock::now();
      });
  // Create and own a runtime world registration. Descriptor data is consumed
  // during construction; world callbacks/resources live until successful Close
  // or destruction. Evaluator instances/registrations remain caller-owned.
  // Registration failure throws std::runtime_error without taking membership.
  PhysicsSession(
      ArRuntime runtime, const ArPhysicsWorldDesc &world,
      FrameClock::NowFunction now = [] {
        return std::chrono::steady_clock::now();
      },
      const ArDiagnosticSink *diagnostics = nullptr);
  ~PhysicsSession();
  PhysicsSession(const PhysicsSession &) = delete;
  PhysicsSession &operator=(const PhysicsSession &) = delete;

  void Play();
  void Pause();
  bool IsPaused() const noexcept { return paused_; }
  bool IsOpen() const noexcept { return world_ != 0; }
  // Pauses and releases output. An owned registration is removed before
  // callback resources can be unloaded; a borrowed world stays registered.
  // Idempotent; a failed removal preserves output and leaves the session open.
  // Closed sessions reject Display/RetryAdvance/Reset; Play/Pause do nothing.
  ArStatus Close() noexcept;
  // Synchronously removes scheduler membership and this member's display
  // reference. Does not read the clock, pause, reset or touch solver resources.
  // Detach external bodies/commands before the next Display/Reset; subsequent
  // requests must cover the remaining members. Provider code remains borrowed.
  ArStatus RemoveParticipant(ArInstance instance) noexcept;
  ArStatus Display(const ArPhysicsInput *inputs, uint32_t count,
                   const ArDiagnosticSink *diagnostics = nullptr);
  // Retry simulation after a pre-Step failure with a NEW request and zero
  // elapsed. Renderer delivery retries read Snapshots() instead of calling
  // this.
  ArStatus RetryAdvance(const ArPhysicsInput *inputs, uint32_t count,
                        const ArDiagnosticSink *diagnostics = nullptr);
  // Pauses even on refusal. A successful group reset clears output/history;
  // a refusal keeps the last displayable snapshots and propagates its status.
  ArStatus Reset(uint64_t generation,
                 const ArDiagnosticSink *diagnostics = nullptr);

  // Borrowed handles in the last successful request's input order. Retain
  // independently before advance/reset/member removal/close or destruction.
  const std::vector<ArSnapshot> &Snapshots() const noexcept {
    return snapshots_;
  }
  const ArPhysicsAdvanceResult &LastAdvance() const noexcept { return result_; }

private:
  void Initialize();
  ArStatus Advance(double elapsed, const ArPhysicsInput *, uint32_t,
                   const ArDiagnosticSink *);
  void ReleaseSnapshots() noexcept;
  ArRuntime runtime_;
  ArPhysicsWorld world_ = 0;
  bool ownsWorld_ = false;
  ArRuntimeApi api_{};
  ArPhysicsApi physics_{};
  ArPhysicsMembershipApi membership_{};
  FrameClock clock_;
  bool paused_ = true;
  uint64_t request_ = 0;
  std::vector<ArSnapshot> snapshots_;
  std::vector<ArInstance> snapshotInstances_;
  ArPhysicsAdvanceResult result_{AR_HEADER(ArPhysicsAdvanceResult)};
};

} // namespace avatarStageRunner
