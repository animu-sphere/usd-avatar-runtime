# Typed reading ships in 0.5.4. Also verify that the installed headers/archive
# provide the required skeleton/clip APIs.
function(avatar_require_motion_usd_reading)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_LIBRARIES motionUsd::motionUsd)
    if(CMAKE_BUILD_TYPE)
        set(CMAKE_TRY_COMPILE_CONFIGURATION "${CMAKE_BUILD_TYPE}")
    elseif(NOT CMAKE_TRY_COMPILE_CONFIGURATION)
        set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
    endif()
    unset(AR_MOTION_USD_OWNER_READING CACHE)
    check_cxx_source_compiles([[
        #include <motionUsd/SkeletonReader.h>
        int main() {
            namespace motion = openstrata::motion;
            motion::MotionSkeletonRead skeleton;
            motion::MotionStageRead clip;
            motion::SkeletonReadDiagnostic diagnostic;
            const pxr::UsdStagePtr stage;
            const pxr::SdfPath path("/Skeleton");
            motion::ReadMotionSkeleton(stage, path, motion::SkeletonReadRole::Generic, &skeleton, &diagnostic);
            motion::ReadCanonicalMotionStage(stage, path, &clip, &diagnostic);
            return skeleton.skeleton.GetSize() + (clip.descriptor.has_value() ? 1 : 0)
                + (clip.sourceRest.has_value() ? 1 : 0);
        }
    ]] AR_MOTION_USD_OWNER_READING)
    if(NOT AR_MOTION_USD_OWNER_READING)
        message(FATAL_ERROR "USD bindings require installed motionUsd typed skeleton/clip reading APIs: ReadMotionSkeleton and MotionStageRead descriptor/sourceRest. Use a complete motionUsd 0.5.4 or compatible later install with motionUsd/SkeletonReader.h and its linked symbols.")
    endif()
endfunction()

function(avatar_require_motion_usd_inputs)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_LIBRARIES motionUsd::motionUsd)
    if(CMAKE_BUILD_TYPE)
        set(CMAKE_TRY_COMPILE_CONFIGURATION "${CMAKE_BUILD_TYPE}")
    elseif(NOT CMAKE_TRY_COMPILE_CONFIGURATION)
        set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
    endif()
    unset(AR_MOTION_USD_OWNER_INPUTS CACHE)
    check_cxx_source_compiles([[
        #include <motionUsd/SkeletonReader.h>
        int main() {
            namespace motion = openstrata::motion;
            motion::MotionStageRead clip;
            motion::SkeletonReadDiagnostic diagnostic;
            motion::MotionStageReadOptions inputs;
            inputs.channels.push_back({"/Input.name", "/Input.weight", "vrm:"});
            inputs.lookAtTargetAttributePath = "/Input.target";
            const pxr::UsdStagePtr stage;
            motion::ReadCanonicalMotionStage(stage, pxr::SdfPath("/Skeleton"), inputs, &clip, &diagnostic);
            return (clip.descriptor.has_value() ? 1 : 0) + (clip.sourceRest.has_value() ? 1 : 0);
        }
    ]] AR_MOTION_USD_OWNER_INPUTS)
    if(NOT AR_MOTION_USD_OWNER_INPUTS)
        message(FATAL_ERROR "Motion USD binding requires installed motionUsd owner-selected input APIs, including the ReadCanonicalMotionStage options overload. Use a complete motionUsd 0.5.4 or compatible later install.")
    endif()
endfunction()
