#pragma once
#include "avatarRuntime/FixedTickSession.h"
#include "avatarVrmUsd/HumanoidBinding.h"
#include "avatarVrm/ExpressionAdapter.h"
#include "avatarMotionUsd/ReadStageClip.h"

namespace avatarVrmSession {
struct ClipExpressionInput {
    std::string channel;    // verbatim channel name in the clip
    std::string expression; // verbatim owner name; explicit semantic mapping
};
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
    // Sampled from the clip at each tick time instead of coming from the host. Read
    // them with MotionStageReadOptions. Each expression and the gaze has one source:
    // the clip or the host. A mapped input the clip does not author refuses construction.
    std::vector<ClipExpressionInput> clipExpressions;
    bool clipGaze = false;
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
    // for the call. A channel absent from this request contributes nothing. With clip
    // inputs selected, the snapshot's input revision is its frame ID instead.
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
