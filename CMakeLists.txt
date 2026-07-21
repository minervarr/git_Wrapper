cmake_minimum_required(VERSION 3.22.1)
project(git_wrapper CXX C)

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
