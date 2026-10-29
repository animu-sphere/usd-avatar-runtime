function(avatar_require_vrm_spring_solver)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_LIBRARIES vrmRig::vrmRig)
    if(CMAKE_BUILD_TYPE)
        set(CMAKE_TRY_COMPILE_CONFIGURATION "${CMAKE_BUILD_TYPE}")
    else()
        set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
    endif()
    unset(AR_VRM_SPRING_SOLVER CACHE)
    check_cxx_source_compiles([[
        #include <vrmRig/SpringBoneSolver.h>
        #if !defined(VRMRIG_SPRING_BONE_API) || VRMRIG_SPRING_BONE_API < 1
        #error Missing installed spring solver
        #endif
        int main() {
            vrmRig::SpringBoneSolver solver;
            vrmRig::SpringBoneRig rig;
            vrmRig::SpringBoneResult result;
            solver.Configure(rig);
            auto independent = solver;
            independent.Reset({}, &result);
            independent.Step(0.01f, {}, &result);
        }
    ]] AR_VRM_SPRING_SOLVER)
    if(NOT AR_VRM_SPRING_SOLVER)
        message(FATAL_ERROR "VRM spring evaluator requires complete installed vrmRig SpringBoneSolver (VRMRIG_SPRING_BONE_API >= 1), independent value copies and linked symbols.")
    endif()
endfunction()

function(avatar_require_vrm_spring)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_LIBRARIES vrmRig::vrmRig vrmSchema::vrmSchema)
    if(CMAKE_BUILD_TYPE)
        set(CMAKE_TRY_COMPILE_CONFIGURATION "${CMAKE_BUILD_TYPE}")
    else()
        set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
    endif()
    unset(AR_VRM_SPRING_OWNER CACHE)
    check_cxx_source_compiles([[
        #include <vrmRig/SpringBoneSolver.h>
        #include <vrmSchema/vrmSpringBoneAPI.h>
        #if !defined(VRMRIG_SPRING_BONE_API) || VRMRIG_SPRING_BONE_API < 1
        #error Missing installed spring solver
        #endif
        int main() {
            vrmRig::SpringBoneSolver solver;
            vrmRig::SpringBoneRig rig;
            vrmRig::SpringBoneResult result;
            solver.Configure(rig);
            solver.Reset({}, &result);
            solver.Step(0.01f, {}, &result);
            pxr::UsdVrmSpringBoneAPI api;
            api.GetVrmRootJointsAttr();
        }
    ]] AR_VRM_SPRING_OWNER)
    if(NOT AR_VRM_SPRING_OWNER)
        message(FATAL_ERROR "VRM spring USD binding requires complete installed vrmRig SpringBoneSolver (VRMRIG_SPRING_BONE_API >= 1) and contract-v2 vrmSchema rootJoints headers and linked symbols. Existing expression-only builds do not require these APIs.")
    endif()
endfunction()
