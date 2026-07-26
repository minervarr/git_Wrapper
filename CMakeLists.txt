cmake_minimum_required(VERSION 3.22.1)
project(git_wrapper CXX C)

# Optional x86-64 psABI microarch level (v2/v3/v4), "native", or any literal
# -march value GCC/Clang accepts directly (e.g. znver4 for AMD Zen 4 tuning).
# Empty (default) leaves the compiler's generic baseline untouched — this
# binary is subprocess/IO-bound (spawns git, does no hot-loop compute), so
# there's no real performance case for this; it exists purely so
# scripts/build.sh --share can produce per-microarch binaries alongside a
# portable one, matching the other native projects' build scripts. A v4/zen4
# binary will SIGILL on a CPU that lacks those extensions, and GCC only
# recognizes -march=x86-64-v2/v3/v4 from GCC 11+ / Clang 12+.
set(GIT_WRAPPER_ARCH_LEVEL "" CACHE STRING "x86-64 target to compile for: v2/v3/v4, native, or a specific -march name. Empty = compiler default (portable).")
if(GIT_WRAPPER_ARCH_LEVEL AND NOT MSVC)
    if(GIT_WRAPPER_ARCH_LEVEL MATCHES "^v[234]$")
        set(GIT_WRAPPER_ARCH_FLAG "-march=x86-64-${GIT_WRAPPER_ARCH_LEVEL}")
    else()
        set(GIT_WRAPPER_ARCH_FLAG "-march=${GIT_WRAPPER_ARCH_LEVEL}")
    endif()
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("${GIT_WRAPPER_ARCH_FLAG}" GIT_WRAPPER_ARCH_FLAG_SUPPORTED)
    if(NOT GIT_WRAPPER_ARCH_FLAG_SUPPORTED)
        message(FATAL_ERROR "Compiler does not support ${GIT_WRAPPER_ARCH_FLAG} "
                             "(x86-64-v2/v3/v4 need GCC 11+ or Clang 12+). "
                             "Leave GIT_WRAPPER_ARCH_LEVEL unset to build portably.")
    endif()
    string(APPEND CMAKE_CXX_FLAGS " ${GIT_WRAPPER_ARCH_FLAG}")
    string(APPEND CMAKE_C_FLAGS " ${GIT_WRAPPER_ARCH_FLAG}")
elseif(GIT_WRAPPER_ARCH_LEVEL AND MSVC)
    message(WARNING "GIT_WRAPPER_ARCH_LEVEL is a GCC/Clang psABI flag and has "
                     "no MSVC equivalent for v4 — ignored.")
endif()

# Single source of truth for the version: manifest.json.
file(READ "${CMAKE_SOURCE_DIR}/manifest.json" GIT_WRAPPER_MANIFEST)
string(JSON GIT_WRAPPER_VERSION GET "${GIT_WRAPPER_MANIFEST}" version)
configure_file(src/version.h.in ${CMAKE_BINARY_DIR}/generated/version.h @ONLY)

add_executable(git_wrapper
    src/main.cc
    src/util.cc
    src/sanitize.cc
    src/repo.cc
    src/commands.cc
)

if(WIN32)
    target_sources(git_wrapper PRIVATE src/platform/windows/process_win.cc)
else()
    target_sources(git_wrapper PRIVATE src/platform/linux/process_posix.cc)
endif()

target_include_directories(git_wrapper PRIVATE src ${CMAKE_BINARY_DIR}/generated)
target_compile_features(git_wrapper PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(git_wrapper PRIVATE /utf-8)
endif()

# Drop the exe at the build root so it's easy to find / add to PATH.
set_target_properties(git_wrapper PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
