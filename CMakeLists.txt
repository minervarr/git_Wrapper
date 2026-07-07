cmake_minimum_required(VERSION 3.22.1)
project(git_wrapper CXX C)

add_executable(git_wrapper main.cc)

target_compile_features(git_wrapper PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(git_wrapper PRIVATE /utf-8)
endif()

# Drop the exe at the build root so it's easy to find / add to PATH.
set_target_properties(git_wrapper PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
