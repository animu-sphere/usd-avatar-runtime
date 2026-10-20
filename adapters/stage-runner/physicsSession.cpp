#include <avatarStageRunner/PhysicsSession.h>
#include <limits>
#include <stdexcept>
#include <utility>

namespace avatarStageRunner {

PhysicsSession::PhysicsSession(ArRuntime runtime, ArPhysicsWorld world,
                               FrameClock::NowFunction now)
    : runtime_(runtime), world_(world), clock_(std::move(now)) {
  if (!runtime || !world)
    throw std::invalid_argument(
        "PhysicsSession requires runtime and world handles");
  if (arGetApi(AR_ABI_VERSION, sizeof(api_), &api_) != AR_OK ||
      arGetPhysicsApi(AR_PHYSICS_API_VERSION, sizeof(physics_), &physics_) !=
          AR_OK)
    throw std::runtime_error(
        "PhysicsSession requires compatible runtime API tables");
  (void)clock_.tick();
}

PhysicsSession::~PhysicsSession() { ReleaseSnapshots(); }

void PhysicsSession::ReleaseSnapshots() noexcept {
  for (auto snapshot : snapshots_)
    if (snapshot)
      (void)api_.release_snapshot(snapshot);
  snapshots_.clear();
}

void PhysicsSession::Play() {
  if (!paused_)
    return;
  (void)clock_.tick();
  paused_ = false;
}

void PhysicsSession::Pause() {
  if (paused_)
    return;
  (void)clock_.tick();
  paused_ = true;
}

ArStatus PhysicsSession::Display(const ArPhysicsInput *inputs, uint32_t count,
                                 const ArDiagnosticSink *diagnostics) {
  return Advance(clock_.tick().count(), inputs, count, diagnostics);
}

ArStatus PhysicsSession::RetryAdvance(const ArPhysicsInput *inputs,
                                      uint32_t count,
                                      const ArDiagnosticSink *diagnostics) {
  (void)clock_.tick();
  return Advance(0, inputs, count, diagnostics);
}

ArStatus PhysicsSession::Advance(double elapsed, const ArPhysicsInput *inputs,
                                 uint32_t count,
                                 const ArDiagnosticSink *diagnostics) {
  if (request_ == std::numeric_limits<uint64_t>::max())
    return AR_INVALID_STATE;
  std::vector<ArSnapshot> next(count, 0);
  ArPhysicsAdvanceDesc desc{AR_HEADER(ArPhysicsAdvanceDesc)};
  desc.request_id = ++request_;
  desc.elapsed_seconds = elapsed;
  desc.paused = paused_;
  desc.inputs = inputs;
  desc.input_count = count;
  result_ = {AR_HEADER(ArPhysicsAdvanceResult)};
  const auto status = physics_.advance_world(
      runtime_, world_, &desc, diagnostics, &result_, next.data());
  if (status == AR_OK) {
    ReleaseSnapshots();
    snapshots_.swap(next);
  }
  return status;
}

ArStatus PhysicsSession::Reset(uint64_t generation,
                               const ArDiagnosticSink *diagnostics) {
  paused_ = true;
  const auto status =
      physics_.reset_world(runtime_, world_, generation, diagnostics);
  clock_.reset();
  (void)clock_.tick();
  if (status == AR_OK) {
    request_ = 0;
    result_ = {AR_HEADER(ArPhysicsAdvanceResult)};
    ReleaseSnapshots();
  }
  return status;
}

} // namespace avatarStageRunner
