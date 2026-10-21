#pragma once
#include <array>
#include <avatarStageRunner/PhysicsSession.h>

// Test host composition only. FrameClock measures display time; avatar-runtime
// alone owns the world's accumulator, barrier, tick IDs and input holding.
class StageRunnerCheck {
  using FrameClock = usd_stage_runner::runtime::FrameClock;
  Fixture &fixture_;
  FrameClock::Clock::time_point now_{};
  avatarStageRunner::PhysicsSession session_;
  std::vector<ArPhysicsInput> inputs_;

  ArStatus Deliver(ArStatus status) {
    fixture_.release();
    fixture_.lastAdvance = session_.LastAdvance();
    if (status == AR_OK) {
      CHECK(session_.Snapshots().size() == inputs_.size());
      for (size_t i = 0; i < inputs_.size(); ++i) {
        fixture_.snapshots[i] = session_.Snapshots()[i];
        if (fixture_.snapshots[i])
          CHECK(fixture_.api.retain_snapshot(fixture_.snapshots[i]) == AR_OK);
      }
    }
    return status;
  }

public:
  explicit StageRunnerCheck(Fixture &fixture)
      : fixture_(fixture),
        session_(fixture.runtime, fixture.worldDescriptor(),
                 [this] { return now_; }),
        inputs_(fixture.inputs, fixture.inputs + 2) {}
  void Unload(ArInstance instance) {
    CHECK(session_.RemoveParticipant(instance) == AR_OK);
    // Membership first; then stop all external references to the removed body
    // set before the exclusive host driver can issue another request/reset.
    CHECK(fixture_.resources.at(instance)->Close());
    fixture_.resources.erase(instance);
    fixture_.handles.erase(instance);
    fixture_.shapes.erase(instance);
    fixture_.pending.erase(instance);
    fixture_.results.erase(instance);
    fixture_.unloaded.insert(instance);
    inputs_.erase(std::remove_if(inputs_.begin(), inputs_.end(),
        [instance](const auto& input) { return input.instance == instance; }), inputs_.end());
    CHECK(fixture_.api.destroy_instance(fixture_.runtime, instance) == AR_OK);
  }
  void Close() {
    CHECK(session_.Close() == AR_OK && !session_.IsOpen());
  }
  void Pause(bool paused) {
    if (paused)
      session_.Pause();
    else
      session_.Play();
  }
  ArStatus Display(std::chrono::milliseconds elapsed) {
    now_ += elapsed;
    ArDiagnosticSink sink{&fixture_, Fixture::diagnostic};
    return Deliver(session_.Display(inputs_.data(), uint32_t(inputs_.size()), &sink));
  }
  ArStatus Retry() {
    ArDiagnosticSink sink{&fixture_, Fixture::diagnostic};
    return Deliver(session_.RetryAdvance(inputs_.data(), uint32_t(inputs_.size()), &sink));
  }
  ArSnapshot LastSnapshot() const {
    return session_.Snapshots().empty() ? 0 : session_.Snapshots()[0];
  }
  void Reset(uint64_t generation) {
    CHECK(session_.Reset(generation) == AR_OK);
    fixture_.release();
    CHECK(session_.IsPaused() && session_.Snapshots().empty());
  }
};

void stageWorldJolt(Assets &assets) {
  // Whole-stage discovery/factory lifetime is independent of the explicitly
  // configured two-avatar scheduler fixture below. No edits reach the importer.
  auto stage = UsdStage::CreateInMemory();
  stage->GetRootLayer()->TransferContent(assets.stage->Flatten());
  auto settings = UsdPhysicsScene::Define(stage, SdfPath("/HostPhysics"));
  settings.CreateGravityDirectionAttr().Set(GfVec3f(0, -1, 0));
  settings.CreateGravityMagnitudeAttr().Set(2.0f);
  double previous = 0;
  for (int replay = 0; replay < 2; ++replay) {
    int factoryCalls = 0;
    avatarPhysics::StageWorld owned(stage, [&](const auto& scene) {
      CHECK(scene && scene->path == "/HostPhysics" && scene->gravity.y == -2);
      ++factoryCalls;
      return usd_physics::jolt::createWorld({}, scene->gravity);
    }, [](const auto&, auto& body) {
      body.collisionFilter = {1,0}; body.damping = {0,0};
    });
    CHECK(factoryCalls == 1 && owned.Resources()->Bodies().size() == assets.bodies.size());
    const auto scene = usd_physics::usd::readPhysicsScene(stage);
    const auto body = std::find_if(scene.bodies.begin(), scene.bodies.end(), [](const auto& b) {
      return b.motionType == usd_physics::core::MotionType::dynamicBody;
    });
    CHECK(body != scene.bodies.end());
    auto* world = owned.World();
    const auto handle = owned.Resources()->Bodies().at(body->path);
    const double initial = world->bodyState(handle).transform.translation.y;
    for (int tick = 0; tick < 120; ++tick)
      world->step(std::chrono::duration<double>(1.0 / 120));
    const double final = world->bodyState(handle).transform.translation.y;
    CHECK(std::isfinite(final) && final < initial - .5 && final > initial - 1.5);
    if (replay) CHECK(final == previous);
    previous = final;
    CHECK(owned.Close() && !owned.World() && !owned.Resources());
  }
}

void stageRunner(Assets &assets) {
  std::unique_ptr<MmdToonCheck> consumers[2];
  {
    Fixture f(assets, true, {}, false);
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
    const auto lastDisplay = host.LastSnapshot();
    const auto lastDisplayPose = f.pose(lastDisplay);
    f.badSnapshot = true;
    CHECK(host.Display(std::chrono::milliseconds(25)) == AR_PROVIDER_ERROR);
    CHECK(!f.snapshots[0] && !f.snapshots[1] && f.steps == 25);
    CHECK(host.LastSnapshot() == lastDisplay &&
          f.pose(lastDisplay) == lastDisplayPose);
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
    CHECK(host.Retry() == AR_OK);
    CHECK(f.lastAdvance.tick_count == 3 && f.steps == 3);
    checkFinal();
    CHECK(f.input.frame_id == 0 && f.input.generation == 0 &&
          f.input.evaluation_seconds == 0);
    CHECK(f.pose(retained) == retainedPose);
    // Close the runtime registration before releasing instances and external
    // body resources. Retained CPU/Toon output needs none of these resources.
    const auto steps = f.steps, resets = f.resets;
    const auto calls = f.calls;
    host.Close();
    host.Close();
    CHECK(host.Display(std::chrono::milliseconds(10000)) == AR_INVALID_STATE);
    CHECK(f.steps == steps && f.resets == resets && f.calls == calls);
    for (auto avatar : f.avatars)
      CHECK(f.api.destroy_instance(f.runtime, avatar) == AR_OK);
    for (auto& [avatar, owned] : f.resources) CHECK(owned->Close());
    f.resources.clear();
    f.solver.reset();
    f.handles.clear();
    CHECK(f.pose(retained) == retainedPose);
    CHECK(f.api.release_snapshot(retained) == AR_OK);
  }
  for (auto &consumer : consumers)
    consumer->AfterProducerDestruction();
}

void stageRunnerUnload(Assets &assets, bool constrained = false) {
  // Add a standard joint only in the in-memory session layer. The installed
  // reader discovers it on every initial build/reset alongside imported bodies.
  UsdEditContext edit(assets.stage, assets.stage->GetSessionLayer());
  SdfPath constrainedBody;
  double anchorY = 0;
  if (constrained) {
    const auto scene = usd_physics::usd::readPhysicsScene(assets.stage);
    const auto body = std::find_if(scene.bodies.begin(), scene.bodies.end(),
        [](const auto& b) { return b.motionType == usd_physics::core::MotionType::dynamicBody; });
    CHECK(body != scene.bodies.end());
    constrainedBody = SdfPath(body->path);
    anchorY = body->transform.translation.y;
    auto joint = UsdPhysicsJoint::Define(assets.stage, SdfPath("/Host/worldJoint"));
    joint.CreateBody1Rel().SetTargets({constrainedBody});
    joint.CreateLocalPos0Attr().Set(GfVec3f(body->transform.translation.x,
        body->transform.translation.y, body->transform.translation.z));
    auto limit = UsdPhysicsLimitAPI::Apply(joint.GetPrim(), TfToken("transY"));
    limit.CreateLowAttr().Set(-.25f); limit.CreateHighAttr().Set(.25f);
  }
  std::unique_ptr<MmdToonCheck> consumers[2];
  {
    Fixture f(assets, true, {}, false, constrained);
    StageRunnerCheck host(f);
    host.Pause(false);
    CHECK(host.Display(std::chrono::milliseconds(25)) == AR_OK);
    publish(f, consumers);
    host.Reset(2);
    host.Pause(false);
    CHECK(host.Display(std::chrono::milliseconds(25)) == AR_OK);
    publish(f, consumers);
    const auto removed = f.avatars[1], survivor = f.avatars[0];
    const auto retained = f.snapshots[0];
    CHECK(f.api.retain_snapshot(retained) == AR_OK);
    const auto retainedPose = f.pose(retained);
    const auto bodyBefore = f.solver->bodyState(f.sharedBody);
    const auto survivorBefore = f.solver->bodyState(f.handles.at(survivor).begin()->second);
    const auto calls = f.calls;
    auto* solver = f.solver.get();
    host.Unload(removed);
    CHECK(f.resources.size() == 1 && !f.resources.count(removed));
    CHECK(f.solver.get() == solver && f.steps == 3 && f.resets == 1 && f.calls == calls);
    CHECK(f.handles.size() == 1 && f.handles.at(survivor).size() == 3);
    CHECK(f.solver->bodyState(f.sharedBody).transform.translation.y == bodyBefore.transform.translation.y);
    CHECK(f.solver->bodyState(f.handles.at(survivor).begin()->second).transform.translation.y == survivorBefore.transform.translation.y);
    f.calls.clear();
    CHECK(host.Display(std::chrono::milliseconds(25)) == AR_OK && f.steps == 6);
    const std::vector<std::string> tick{"pre." + std::to_string(survivor), "step", "post." + std::to_string(survivor)};
    CHECK(f.calls.size() == 9);
    for (size_t i = 0; i < f.calls.size(); ++i) CHECK(f.calls[i] == tick[i % 3]);
    ArStateView view{AR_HEADER(ArStateView)};
    CHECK(f.api.get_snapshot(f.snapshots[0], &view) == AR_OK && view.instance == survivor && view.frame_id == 6 && view.generation == 2);
    const auto owner = assets.config();
    auto continuation = owner.evaluator.BeginPhysics(assets.motion, view.evaluation_seconds * 30);
    auto feedback = owner.coupling.ApplyPhysicsToPose(f.results.at(survivor), continuation.BeforePhysics());
    CHECK(feedback);
    auto expected = owner.evaluator.ResumePhysics(continuation, feedback.value());
    CHECK(expected && f.pose(f.snapshots[0]) == expected.value());
    CHECK(f.solver->bodyState(f.sharedBody).transform.translation.y < bodyBefore.transform.translation.y);
    consumers[1]->Apply(f.api, f.snapshots[0]);
    if (constrained) {
      // Run long enough to hit the joint bound, past the free-fall result.
      for (int i = 0; i < 40; ++i)
        CHECK(host.Display(std::chrono::milliseconds(25)) == AR_OK);
      const auto body = f.solver->bodyState(f.handles.at(survivor).at(constrainedBody));
      CHECK(std::isfinite(body.transform.translation.y));
      CHECK(std::abs(body.transform.translation.y - anchorY) <= .27);
      CHECK(body.transform.translation.y < anchorY - .1);
      consumers[1]->Apply(f.api, f.snapshots[0]);
    }
    CHECK(f.pose(retained) == retainedPose);
    // Group reset must not resurrect the unloaded avatar or stale body maps.
    host.Reset(3);
    CHECK(f.handles.size() == 1 && !f.handles.count(removed));
    host.Pause(false);
    CHECK(host.Display(std::chrono::milliseconds(25)) == AR_OK && f.steps == 3);
    consumers[1]->Apply(f.api, f.snapshots[0]);
    host.Close();
    CHECK(f.api.destroy_instance(f.runtime, survivor) == AR_OK);
    for (auto& [avatar, owned] : f.resources) CHECK(owned->Close());
    f.resources.clear();
    f.solver.reset(); f.handles.clear(); f.shapes.clear();
    CHECK(f.pose(retained) == retainedPose);
    CHECK(f.api.release_snapshot(retained) == AR_OK);
  }
  for (auto& consumer : consumers) consumer->AfterProducerDestruction();
  if (constrained) CHECK(assets.stage->RemovePrim(SdfPath("/Host")));
}
