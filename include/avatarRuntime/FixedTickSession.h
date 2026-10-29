#pragma once
#include "avatarRuntime/api.h"
#include <memory>

namespace avatarRuntime {
struct FixedTickConfig {
    double startSeconds = 0;
    double fixedStepSeconds = 0;
    uint32_t maxCatchUpTicks = 0;
};
struct FixedTickStats {
    uint32_t tickCount = 0; // completed in the last accepted request, excluding the seed
    uint64_t tick = 0;
    double simulationSeconds = 0, pendingSeconds = 0;
    bool initialized = false;
};

// Experimental native runtime surface; no new C table, phase or backend.
// Binds one fresh/reset instance exclusively. Runtime/provider code outlives
// this object. Calls and borrowed output use one exclusive calling thread.
// A rigid-body participant must instead use its shared-world clock/barrier.
class FixedTickSession {
public:
    AR_EXPORT FixedTickSession(ArRuntime, ArInstance, FixedTickConfig);
    AR_EXPORT ~FixedTickSession();
    FixedTickSession(const FixedTickSession&) = delete;
    FixedTickSession& operator=(const FixedTickSession&) = delete;
    // Observations/revision are held across catch-up; identity/time come from
    // runtime. Validates before accepting elapsed. A first request seeds at
    // startSeconds, even paused, before any due tick. No display interpolation.
    // Accepted request IDs strictly increase, also on evaluator failure. Retry
    // with a new ID and zero elapsed; completed ticks and failed-tick backlog
    // survive. Paused requests discard elapsed and do not process backlog.
    AR_EXPORT ArStatus Advance(uint64_t requestId, double elapsedSeconds, bool paused,
        const ArInputFrame&, const ArDiagnosticSink* = nullptr);
    // Recreates provider state, clears clock/output and declares a new seed.
    // Generation must increase. Failure preserves the previous state/output.
    AR_EXPORT ArStatus Reset(uint64_t generation, double startSeconds);
    // Releases membership/output, requiring reset before direct instance reuse.
    // Does not destroy the caller-owned instance. Idempotent.
    AR_EXPORT ArStatus Close();
    AR_EXPORT const FixedTickStats& Stats() const;
    // Borrowed until next committed tick/seed, successful reset or close.
    // Consumer retries read/retain this handle without calling Advance.
    AR_EXPORT ArSnapshot Snapshot() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace avatarRuntime
