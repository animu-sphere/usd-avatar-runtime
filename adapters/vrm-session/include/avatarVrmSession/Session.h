#pragma once
#include "avatarRuntime/FixedTickSession.h"
#include "avatarVrmUsd/HumanoidBinding.h"
#include "avatarVrm/ExpressionAdapter.h"
#include "avatarMotionUsd/ReadStageClip.h"

namespace avatarVrmSession {
struct SessionConfig {
    avatarVrmUsd::HumanoidBindingConfig avatar;
    avatarRuntime::FixedTickConfig clock;
    bool secondaryMotion = true;
    // Explicit, like secondaryMotion: a selected capability the avatar does not
    // author refuses construction. Expression-type LookAt needs expressions too.
    // The layout version must cover the selection: expressions add outputs.
    bool expressions = false;
    std::vector<avatarVrm::ExpressionInputBinding> expressionInputs;
    bool lookAt = false;
    avatarVrm::InputIdentity gaze;
};
// Single-avatar native clip host. Input stages are snapshotted at construction;
// no stage, renderer or physics backend is retained. Runtime owns the clock.
// To change bindings or enable/disable secondary motion, close and construct a
// new session and consumer binding. Reset/seek retain the same instance/layout.
class Session {
public:
    Session(const pxr::UsdStagePtr& avatar, const openstrata::motion::MotionStageRead& clip,
        SessionConfig config);
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    void Play();
    void Pause();
    bool IsPaused() const;
    ArStatus Advance(double elapsedSeconds, const ArDiagnosticSink* = nullptr);
    // Selected observations and input revision are copied from the frame and held
    // across catch-up; identity and time come from the runtime. Arrays are borrowed
    // for the call. A channel absent from this request contributes nothing.
    ArStatus Advance(double elapsedSeconds, const ArInputFrame& observations,
        const ArDiagnosticSink* = nullptr);
    ArStatus Reset(double startSeconds); // pauses; increasing generation; next Advance seeds
    void Close(); // idempotent; unload runtime before provider code
    const avatarRuntime::FixedTickStats& Stats() const;
    const avatarUsd::SkeletonBinding& Binding() const;
    const ArRuntimeApi& Api() const;
    ArSnapshot Snapshot() const; // borrowed; retain before advance/reset/close
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace avatarVrmSession
