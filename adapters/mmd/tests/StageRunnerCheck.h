#pragma once
#include <array>
#include <usd_stage_runner/runtime/frame_clock.h>

// Test host composition only. FrameClock measures display time; avatar-runtime
// alone owns the world's accumulator, barrier, tick IDs and input holding.
class StageRunnerCheck {
  using FrameClock = usd_stage_runner::runtime::FrameClock;
  Fixture &fixture_;
  FrameClock::Clock::time_point now_{};
  FrameClock clock_{[this] { return now_; }};
  bool paused_ = true;

public:
  explicit StageRunnerCheck(Fixture &fixture) : fixture_(fixture) {
    (void)clock_.tick();
  }
  void Pause(bool paused) {
    // Anchor both transitions, so neither paused wall time nor time before a
    // pause can leak into the first resumed display request.
    (void)clock_.tick();
    paused_ = paused;
  }
  ArStatus Display(std::chrono::milliseconds elapsed) {
    now_ += elapsed;
    return fixture_.advance(clock_.tick().count(), paused_);
  }
  void Reset(uint64_t generation) {
    CHECK(fixture_.physics.reset_world(fixture_.runtime, fixture_.world,
                                       generation, nullptr) == AR_OK);
    clock_.reset();
    (void)clock_.tick();
    paused_ = true;
  }
};

void stageRunner(Assets &assets) {
  std::unique_ptr<MmdToonCheck> consumers[2];
  {
    Fixture f(assets, true);
    StageRunnerCheck host(f);
    const auto owner = assets.config();
    uint64_t generation = 1;
    ArScalarInput held{"host", "actor", "probe:level", 0, -2, 2, 4};
    f.input.scalars = &held;
    f.input.scalar_count = 1;
    f.input.input_revision = 17;
    auto checkFinal = [&] {
      for (int i = 0; i < 2; ++i) {
        ArStateView view{AR_HEADER(ArStateView)};
        CHECK(f.api.get_snapshot(f.snapshots[i], &view) == AR_OK);
        CHECK(view.frame_id == uint64_t(f.steps));
        CHECK(view.generation == generation);
        CHECK(view.input_revision == f.input.input_revision);
        CHECK(std::abs(view.evaluation_seconds - f.steps / 120.) < 1e-12);
        auto continuation = owner.evaluator.BeginPhysics(
            assets.motion, view.evaluation_seconds * 30);
        auto feedback = owner.coupling.ApplyPhysicsToPose(
            f.results.at(f.inputs[i].instance), continuation.BeforePhysics());
        CHECK(feedback);
        auto expected =
            owner.evaluator.ResumePhysics(continuation, feedback.value());
        CHECK(expected && f.pose(f.snapshots[i]) == expected.value());
      }
      publish(f, consumers);
      // Every tick must contain both prefixes, ONE shared Step, both suffixes.
      const std::vector<std::string> tick{
          "pre." + std::to_string(f.avatars[0]),
          "pre." + std::to_string(f.avatars[1]), "step",
          "post." + std::to_string(f.avatars[0]),
          "post." + std::to_string(f.avatars[1])};
      CHECK(f.calls.size() % tick.size() == 0);
      for (size_t i = 0; i < f.calls.size(); ++i)
        CHECK(f.calls[i] == tick[i % tick.size()]);
      f.calls.clear();
      const auto body = f.solver->bodyState(f.sharedBody);
      CHECK(body.transform.translation.x > 20 &&
            body.transform.translation.y < 10);
      CHECK(body.linearVelocity.y < 0);
    };
    auto display = [&](int milliseconds, int ticks) {
      const auto steps = f.steps;
      CHECK(host.Display(std::chrono::milliseconds(milliseconds)) == AR_OK);
      CHECK(f.lastAdvance.tick_count == uint32_t(ticks) &&
            f.steps == steps + ticks);
      if (f.snapshots[0])
        checkFinal();
    };
    display(1000, 0); // Starts paused, no initial snapshot.
    CHECK(!f.snapshots[0] && !f.snapshots[1]);
    host.Pause(false);
    display(5, 0);
    CHECK(std::abs(f.lastAdvance.pending_seconds - .005) < 1e-12);
    display(20, 3);
    display(100, 4); // 12 ticks due; limit 4, retain the other 8.
    CHECK(std::abs(f.lastAdvance.pending_seconds - 8. / 120) < 1e-12);
    const auto pausedPose = f.pose(f.snapshots[0]);
    const auto pausedBody = f.solver->bodyState(f.sharedBody);
    host.Pause(true);
    display(10000, 0);
    CHECK(f.pose(f.snapshots[0]) == pausedPose);
    CHECK(f.solver->bodyState(f.sharedBody).transform.translation.y ==
          pausedBody.transform.translation.y);
    CHECK(std::abs(f.lastAdvance.pending_seconds - 8. / 120) < 1e-12);
    host.Pause(false);
    display(0, 4);
    display(0, 4); // Resume drains backlog without resubmitting wall time.
    CHECK(f.steps == 15 && f.lastAdvance.pending_seconds < 1e-12);
    f.input.input_revision = 18;
    held.value = .5;
    held.source_seconds = 0;
    display(10, 1);

    auto retained = f.snapshots[0];
    CHECK(f.api.retain_snapshot(retained) == AR_OK);
    const auto retainedPose = f.pose(retained);
    std::vector<std::array<Pose, 2>> first;
    std::vector<usd_physics::core::BodyState> sharedFirst;
    // Reset discards the partial tick as well as paused time. Both avatars and
    // the non-avatar object restart together, using fresh world resources.
    for (generation = 2; generation <= 4; ++generation) {
      host.Reset(generation);
      f.calls.clear();
      CHECK(f.solver->bodyState(f.sharedBody).transform.translation.y == 10);
      CHECK(f.solver->bodyState(f.sharedBody).linearVelocity.x == .25);
      display(5000, 0);
      CHECK(f.lastAdvance.pending_seconds == 0 && !f.snapshots[0]);
      host.Pause(false);
      for (int frame = 0; frame < 8; ++frame) {
        display(25, 3);
        const std::array<Pose, 2> pose{f.pose(f.snapshots[0]),
                                       f.pose(f.snapshots[1])};
        const auto body = f.solver->bodyState(f.sharedBody);
        if (generation == 2) {
          first.push_back(pose);
          sharedFirst.push_back(body);
        } else {
          CHECK(pose == first[frame]);
          const auto &expected = sharedFirst[frame];
          CHECK(body.transform.translation.x ==
                expected.transform.translation.x);
          CHECK(body.transform.translation.y ==
                expected.transform.translation.y);
          CHECK(body.transform.translation.z ==
                expected.transform.translation.z);
          CHECK(body.linearVelocity.x == expected.linearVelocity.x);
          CHECK(body.linearVelocity.y == expected.linearVelocity.y);
          CHECK(body.linearVelocity.z == expected.linearVelocity.z);
        }
      }
      CHECK(f.steps == 24 && f.lastAdvance.pending_seconds < 1e-12);
      CHECK(f.pose(retained) == retainedPose);
    }

    // A failed suffix invalidates the group. The display consumer retains its
    // last valid output; a renderer retry never submits more elapsed time.
    f.badSnapshot = true;
    CHECK(host.Display(std::chrono::milliseconds(25)) == AR_PROVIDER_ERROR);
    CHECK(!f.snapshots[0] && !f.snapshots[1] && f.steps == 25);
    f.badSnapshot = false;
    CHECK(host.Display(std::chrono::milliseconds(0)) == AR_INVALID_STATE);
    CHECK(f.steps == 25 && f.pose(retained) == retainedPose);
    generation = 5;
    host.Reset(generation);
    f.calls.clear();
    host.Pause(false);
    display(25, 3);

    // Pre-Step retry uses zero elapsed: accepted time must not be submitted
    // twice. No failed attempt is delivered to the renderer.
    generation = 6;
    host.Reset(generation);
    host.Pause(false);
    f.failSubmit = true;
    CHECK(host.Display(std::chrono::milliseconds(25)) == AR_PROVIDER_ERROR);
    CHECK(!f.snapshots[0] && !f.snapshots[1] && f.steps == 0);
    f.failSubmit = false;
    f.calls.clear();
    display(0, 3);
    CHECK(f.input.frame_id == 0 && f.input.generation == 0 &&
          f.input.evaluation_seconds == 0);
    CHECK(f.pose(retained) == retainedPose);
    CHECK(f.api.release_snapshot(retained) == AR_OK);
  }
  for (auto &consumer : consumers)
    consumer->AfterProducerDestruction();
}
