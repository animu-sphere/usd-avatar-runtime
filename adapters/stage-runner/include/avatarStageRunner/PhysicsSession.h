#pragma once

#include <avatarRuntime/physics.h>
#include <usd_stage_runner/runtime/frame_clock.h>
#include <vector>

namespace avatarStageRunner {

// Single-threaded host binding over an existing runtime world. The caller owns
// runtime/world/providers and must keep them alive until this session is gone.
// This class owns only its clock and retained output, never a fixed
// accumulator.
class PhysicsSession {
public:
  using FrameClock = usd_stage_runner::runtime::FrameClock;
  PhysicsSession(
      ArRuntime runtime, ArPhysicsWorld world,
      FrameClock::NowFunction now = [] {
        return std::chrono::steady_clock::now();
      });
  ~PhysicsSession();
  PhysicsSession(const PhysicsSession &) = delete;
  PhysicsSession &operator=(const PhysicsSession &) = delete;

  void Play();
  void Pause();
  bool IsPaused() const noexcept { return paused_; }
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
  // independently before the next successful advance/reset or destruction.
  const std::vector<ArSnapshot> &Snapshots() const noexcept {
    return snapshots_;
  }
  const ArPhysicsAdvanceResult &LastAdvance() const noexcept { return result_; }

private:
  ArStatus Advance(double elapsed, const ArPhysicsInput *, uint32_t,
                   const ArDiagnosticSink *);
  void ReleaseSnapshots() noexcept;
  ArRuntime runtime_;
  ArPhysicsWorld world_;
  ArRuntimeApi api_{};
  ArPhysicsApi physics_{};
  FrameClock clock_;
  bool paused_ = true;
  uint64_t request_ = 0;
  std::vector<ArSnapshot> snapshots_;
  ArPhysicsAdvanceResult result_{AR_HEADER(ArPhysicsAdvanceResult)};
};

} // namespace avatarStageRunner
