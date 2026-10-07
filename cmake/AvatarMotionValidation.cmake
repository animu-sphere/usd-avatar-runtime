# Owner validation ships in 0.5.4. Verify installed headers and linked symbols
# as well as the package version to catch incomplete owner installations.
function(avatar_require_motion_validation)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_LIBRARIES motionRetarget::motionRetarget)
    # Match the host's CRT/iterator mode when probing static owner libraries.
    if(CMAKE_BUILD_TYPE)
        set(CMAKE_TRY_COMPILE_CONFIGURATION "${CMAKE_BUILD_TYPE}")
    elseif(NOT CMAKE_TRY_COMPILE_CONFIGURATION)
        set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
    endif()
    unset(AR_MOTION_OWNER_VALIDATION CACHE)
    check_cxx_source_compiles([[
        #include <motionRetarget/Validation.h>
        int main() {
            namespace motion = openstrata::motion;
            auto clip = motion::ValidateMotionClip(motion::MotionClip{});
            auto rig = motion::ValidateRetargetConfiguration(
                motion::SkeletonDescriptor{}, motion::RetargetMap{}, motion::SourceRestPose{});
            return clip.IsValid() && rig.IsValid() ? 0 : 1;
        }
    ]] AR_MOTION_OWNER_VALIDATION)
    if(NOT AR_MOTION_OWNER_VALIDATION)
        message(FATAL_ERROR "The motion adapter requires installed motionCore and motionRetarget validation APIs. Use a complete motion owner 0.5.4 or compatible later install with motionCore/Validation.h, motionRetarget/Validation.h and their linked symbols.")
    endif()
endfunction()
