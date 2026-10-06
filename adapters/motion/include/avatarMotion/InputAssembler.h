#pragma once
#include "avatarMotion/MotionPoseInputBridge.h"

namespace avatarMotion {
// Source-compatible names for the original host input helper. New hosts use
// MotionPoseInputBridge to describe its selected-motion/runtime boundary.
using InputAssemblerConfig = MotionPoseInputBridgeConfig;
using InputAssembler = MotionPoseInputBridge;
} // namespace avatarMotion
