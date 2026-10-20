#include <avatarStageRunner/PhysicsSession.h>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      std::cerr << __LINE__ << ": " << #x << '\n';                             \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)

struct World {
  ArRuntimeApi api{};
  ArPhysicsApi physics{};
  ArRuntime runtime{};
  ArPhysicsWorld world{};
  ArInstance avatar{};
  int steps = 0;
  bool failPre = false, failPost = false, failReset = false;
  static ArStatus AR_CALL pre(void *p, void *, const ArEvaluationContext *,
                              const ArStateWriter *) {
    return static_cast<World *>(p)->failPre ? AR_PROVIDER_ERROR : AR_OK;
  }
  static ArStatus AR_CALL post(void *p, void *, const ArEvaluationContext *,
                               const ArStateWriter *) {
    return static_cast<World *>(p)->failPost ? AR_PROVIDER_ERROR : AR_OK;
  }
  static ArStatus AR_CALL step(void *p, const ArPhysicsTick *,
                               const ArDiagnosticSink *) {
    ++static_cast<World *>(p)->steps;
    return AR_OK;
  }
  static ArStatus AR_CALL reset(void *p, const ArDiagnosticSink *) {
    auto &w = *static_cast<World *>(p);
    if (w.failReset)
      return AR_PROVIDER_ERROR;
    w.steps = 0;
    return AR_OK;
  }
  World() {
    CHECK(arGetApi(AR_ABI_VERSION, sizeof(api), &api) == AR_OK);
    CHECK(arGetPhysicsApi(AR_PHYSICS_API_VERSION, sizeof(physics), &physics) ==
          AR_OK);
    CHECK(api.create_runtime(&runtime) == AR_OK);
    const char *after[]{"pre"};
    for (bool suffix : {false, true}) {
      ArEvaluatorDesc d{AR_HEADER(ArEvaluatorDesc)};
      d.id = suffix ? "post" : "pre";
      d.provider_id = "test";
      d.provider_version = "1";
      d.phase = AR_PHASE_CONSTRAINTS;
      d.user_data = this;
      d.evaluate = suffix ? post : pre;
      if (suffix) {
        d.after = after;
        d.after_count = 1;
      }
      CHECK(api.register_evaluator(runtime, &d, nullptr) == AR_OK);
    }
    const char *selected[]{"post", "pre"};
    ArInstanceDesc d{AR_HEADER(ArInstanceDesc)};
    d.generation = 1;
    d.layout_id = "test";
    d.layout_version = 1;
    d.initial_state = {AR_HEADER(ArStateView)};
    d.evaluators = selected;
    d.evaluator_count = 2;
    CHECK(api.create_instance(runtime, &d, nullptr, &avatar) == AR_OK);
    ArPhysicsParticipant participant{avatar, "post"};
    ArPhysicsWorldDesc wd{AR_HEADER(ArPhysicsWorldDesc)};
    wd.id = "world";
    wd.participants = &participant;
    wd.participant_count = 1;
    wd.fixed_step_seconds = .125;
    wd.max_catch_up_ticks = 2;
    wd.user_data = this;
    wd.step = step;
    wd.reset = reset;
    CHECK(physics.create_world(runtime, &wd, nullptr, &world) == AR_OK);
  }
  ~World() { CHECK(api.destroy_runtime(runtime) == AR_OK); }
};

int main() {
  using Session = avatarStageRunner::PhysicsSession;
  Session::FrameClock::Clock::time_point now{};
  ArSnapshot retained{};
  ArRuntimeApi api{};
  {
    World w;
    api = w.api;
    bool refused = false;
    try {
      Session invalid(0, w.world);
    } catch (const std::invalid_argument &) {
      refused = true;
    }
    CHECK(refused);
    Session session(w.runtime, w.world, [&] { return now; });
    ArInputFrame frame{AR_HEADER(ArInputFrame)};
    ArPhysicsInput input{w.avatar, &frame};
    auto display = [&](int ms) {
      now += std::chrono::milliseconds(ms);
      return session.Display(&input, 1);
    };
    CHECK(session.IsPaused() && display(10000) == AR_OK && w.steps == 0);
    session.Play();
    now += std::chrono::milliseconds(125);
    session.Play(); // Repeated UI play must not discard active elapsed time.
    CHECK(display(0) == AR_OK && w.steps == 1);
    retained = session.Snapshots()[0];
    CHECK(api.retain_snapshot(retained) == AR_OK);
    CHECK(display(500) == AR_OK && w.steps == 3 &&
          session.LastAdvance().pending_seconds == .25);
    session.Pause();
    CHECK(display(10000) == AR_OK && w.steps == 3);
    session.Play();
    CHECK(display(0) == AR_OK && w.steps == 5);
    const auto latest = session.Snapshots()[0];
    w.failPost = true;
    CHECK(display(125) == AR_PROVIDER_ERROR && w.steps == 6);
    CHECK(session.Snapshots()[0] == latest);
    CHECK(session.RetryAdvance(&input, 1) == AR_INVALID_STATE && w.steps == 6);
    CHECK(session.Reset(1) == AR_INVALID_ARGUMENT && session.IsPaused());
    CHECK(session.Snapshots()[0] == latest);
    w.failReset = true;
    CHECK(session.Reset(2) == AR_PROVIDER_ERROR &&
          session.Snapshots()[0] == latest);
    w.failPost = w.failReset = false;
    CHECK(session.Reset(2) == AR_OK && session.Snapshots().empty());
    CHECK(display(10000) == AR_OK && w.steps == 0);
    session.Play();
    w.failPre = true;
    CHECK(display(125) == AR_PROVIDER_ERROR && w.steps == 0);
    w.failPre = false;
    now += std::chrono::seconds(10);
    CHECK(session.RetryAdvance(&input, 1) == AR_OK && w.steps == 1);
    CHECK(session.LastAdvance().pending_seconds == 0);
    CHECK(display(0) == AR_OK && w.steps == 1);
    ArStateView view{AR_HEADER(ArStateView)};
    CHECK(api.get_snapshot(session.Snapshots()[0], &view) == AR_OK &&
          view.generation == 2);
    CHECK(api.get_snapshot(retained, &view) == AR_OK && view.generation == 1 &&
          view.frame_id == 1);
  }
  ArStateView view{AR_HEADER(ArStateView)};
  CHECK(api.get_snapshot(retained, &view) == AR_OK && view.generation == 1);
  CHECK(api.release_snapshot(retained) == AR_OK);
}
