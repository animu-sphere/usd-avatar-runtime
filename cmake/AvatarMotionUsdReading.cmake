# Reading is additive to the local owner package; the version alone does not
# prove that an installed archive exports the strict skeleton/clip APIs.
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
            motion::SkeletonStageRead skeleton;
            motion::MotionStageRead clip;
            motion::SkeletonReadDiagnostic diagnostic;
            const pxr::UsdStagePtr stage;
            const pxr::SdfPath path("/Skeleton");
            motion::ReadSkeleton(stage, path, &skeleton, &diagnostic);
            motion::ReadCanonicalMotionStage(stage, path, &clip, &diagnostic);
        }
    ]] AR_MOTION_USD_OWNER_READING)
    if(NOT AR_MOTION_USD_OWNER_READING)
        message(FATAL_ERROR "USD bindings require installed motionUsd strict skeleton/clip reading APIs. Rebuild/install motionUsd with motionUsd/SkeletonReader.h and its linked symbols; older packages with the same version are insufficient.")
    endif()
endfunction()
