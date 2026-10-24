# Require both installed owner headers and the static library implementation.
function(avatar_require_mmd_morph_resolution)
    include(CheckCXXSourceCompiles)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_REQUIRED_LIBRARIES mmdControl::mmdControl)
    if(CMAKE_BUILD_TYPE)
        set(CMAKE_TRY_COMPILE_CONFIGURATION "${CMAKE_BUILD_TYPE}")
    elseif(NOT CMAKE_TRY_COMPILE_CONFIGURATION)
        set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
    endif()
    unset(AR_MMD_OWNER_MORPH_RESOLUTION CACHE)
    check_cxx_source_compiles([[
        #include <mmdControl/Evaluator.h>
        int main() {
            auto evaluator = mmd::control::Evaluator::Prepare(mmd::CanonicalDocument{});
            return int(evaluator.value().ResolveMorphChannels({}).size());
        }
    ]] AR_MMD_OWNER_MORPH_RESOLUTION)
    if(NOT AR_MMD_OWNER_MORPH_RESOLUTION)
        message(FATAL_ERROR "The MMD adapter requires installed mmdControl Evaluator::ResolveMorphChannels headers and linked symbols; rebuild/install a compatible MMD owner package")
    endif()
endfunction()
