#pragma once
#include <avatarRuntime/api.h>
#include <pxr/usd/usd/stage.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace avatarMmdSession {
struct Stats {
    uint64_t steps = 0, generation = 0;
    std::size_t bodies = 0;
    bool paused = true;
    std::size_t translationAxes = 0, rotationAxes = 0;
};
struct BodyState {
    std::string path;
    std::array<double, 3> translation;
    std::array<double, 4> rotation; // x, y, z, w
    std::array<double, 3> velocity;
};
struct DampingNormalization {
    std::string attributePath;
    float original, coefficient;
};

// Optional single-avatar PMX/VMD application session. MMD owns controls and
// coupling; the runtime owns fixed ticks; Jolt supplies the backend. Use one
// exclusive calling thread. No Python, Qt or renderer dependency.
class Session {
public:
    Session(pxr::UsdStageRefPtr stage, const std::string& pmx, const std::string& vmd,
        bool translationSprings = false, bool rotationSprings = false, bool publishUsd = true);
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    void Play();
    void Pause();
    Stats Advance(double elapsedSeconds);
    void Reset(); // pauses and rebuilds frame-zero bodies with increasing generation
    void Close(); // unregister world before destroying solver resources; idempotent
    Stats GetStats() const;
    std::vector<BodyState> BodyStates() const;
    // Owned source-policy normalization records from the latest successful
    // world build; reread on Reset. Endpoint selection itself is not a warning.
    const std::vector<DampingNormalization>& DampingNormalizations() const;
    // Source joint paths omitted by the MMD owner's inert-joint policy.
    // Owned records from the latest successful construction/Reset.
    const std::vector<std::string>& InertJoints() const;
    const std::vector<std::string>& UnappliedMetadata() const;
    const ArStateView& Baseline() const;
    const ArRuntimeApi& Api() const;
    // Borrowed until the next successful advance/reset/close or destruction.
    // Retain independently before carrying a snapshot across those operations.
    ArSnapshot Snapshot() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace avatarMmdSession
