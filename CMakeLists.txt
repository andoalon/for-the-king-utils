cmake_minimum_required(VERSION 3.15)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/submodules/build-utils/cmake")

include(static-msvc-runtime)

set_old_msvc_runtime_policy()
project(for_the_king_utils CXX)
set_static_msvc_runtime()

include(set-globals)
include(set-link-time-optimization)

include(conan-run)
set(CMAKE_CONFIGURATION_TYPES Debug Release)

conan_add_remote(
	NAME bincrafters
	URL https://bincrafters.jfrog.io/artifactory/api/conan/public-conan
)

conan_run(
	EXTRA_SETTINGS
		"compiler.cppstd=17"
)

find_package(Protobuf REQUIRED COMPONENTS libprotobuf protoc)
include(protobuf-generate) # Found in find_package(Protobuf ...)

add_library(for_the_king_utils_proto OBJECT
    src/proto/attacks.proto
)
target_compile_features(for_the_king_utils_proto
    PUBLIC
        cxx_std_17
)
target_link_libraries(for_the_king_utils_proto
    PUBLIC
        protobuf::libprotobuf
)
protobuf_generate(TARGET for_the_king_utils_proto)

target_include_directories(for_the_king_utils_proto
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR} # For protobuf's generated .cc to be able to find their .h
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/src/proto
)

find_package(SFML REQUIRED COMPONENTS window graphics)
find_package(ImGui-SFML REQUIRED)

add_executable(for_the_king_utils WIN32
    src/main.cc
)
set_directory_properties(PROPERTIES
    VS_STARTUP_PROJECT for_the_king_utils
)

target_link_libraries(for_the_king_utils
    PRIVATE
        for_the_king_utils_proto

        SFML::window
        SFML::graphics
        ImGui-SFML::ImGui-SFML
)

target_compile_features(for_the_king_utils
    PRIVATE
        cxx_std_17
)

include(common-flags)
target_common_flags(for_the_king_utils PRIVATE)

option(ANDONI_WARNINGS_AS_ERRORS "Treat compiler warnings as errors (-Werror etc.)" ON)
if (ANDONI_WARNINGS_AS_ERRORS)
    include(warnings-as-errors)
    target_warnings_as_errors(for_the_king_utils PRIVATE)
endif()
