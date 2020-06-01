if (MSVC)
    cmake_minimum_required(VERSION 3.13)
else()
    cmake_minimum_required(VERSION 3.10)
endif()

project(for_the_king_utils CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_IPO "Enable Iterprocedural Optimization, aka Link Time Optimization (LTO)" OFF)
option(FAIL_IF_IPO_NOT_SUPPORTED "If ENABLE_IPO is true, and it is not supported, consider it a failure" ON)

if(ENABLE_IPO)
	include(CheckIPOSupported)
	check_ipo_supported(RESULT ipo_supported OUTPUT check_ipo_supported_output LANGUAGES CXX)
	
	if(ipo_supported)
		set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
	else()
		if (FAIL_IF_IPO_NOT_SUPPORTED)
			message(FATAL_ERROR "IPO is not supported: ${check_ipo_supported_output}")
		else()
			message(WARNING "IPO is not supported: ${check_ipo_supported_output}")
		endif()
	endif()
endif()

# Download automatically, you can also just copy the conan.cmake file
if (NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
	message(STATUS
	  "Downloading conan.cmake from https://github.com/conan-io/cmake-conan"
	)
	file(DOWNLOAD "https://github.com/conan-io/cmake-conan/raw/v0.15/conan.cmake"
	   "${CMAKE_BINARY_DIR}/conan.cmake"
	)
endif()

include(${CMAKE_BINARY_DIR}/conan.cmake)

conan_add_remote(
	NAME bincrafters
	URL https://api.bintray.com/conan/bincrafters/public-conan
)

conan_cmake_run(
	CONANFILE ./conanfile.txt
	BASIC_SETUP
	CMAKE_TARGETS # individual targets to link to
	BUILD missing
)

if (CONAN_PROTOC_INSTALLER_ROOT_DEBUG)
    list(APPEND CMAKE_MODULE_PATH ${CONAN_PROTOC_INSTALLER_ROOT_DEBUG}/cmake)
else()
    list(APPEND CMAKE_MODULE_PATH ${CONAN_PROTOC_INSTALLER_ROOT}/cmake)
endif()

include(protoc-config)

add_library(for_the_king_utils_proto OBJECT
    src/proto/attacks.proto
)
target_compile_features(for_the_king_utils_proto
    PUBLIC
        cxx_std_17
)
target_link_libraries(for_the_king_utils_proto
    PUBLIC
        CONAN_PKG::protobuf
)
protobuf_generate(TARGET for_the_king_utils_proto)

if (MSVC)
    target_link_options(for_the_king_utils_proto
        PUBLIC
            /ignore:4099 # PDB not found
    )
endif()

target_include_directories(for_the_king_utils_proto
    PRIVATE
        ${CMAKE_BINARY_DIR} # For protobuf's generated .cc to be able to find their .h
    PUBLIC
        ${CMAKE_BINARY_DIR}/src/proto
)

if (MSVC)
    add_compile_options(/W4)

    if (MSVC_VERSION GREATER_EQUAL 1910) # Visual Studio 2017
        add_compile_options(/permissive- /diagnostics:caret)
    endif()
endif()

add_executable(for_the_king_utils
    src/main.cc

    $<$<CONFIG:Debug>:src/imgui_demo.cpp>
)

target_link_libraries(for_the_king_utils
    PRIVATE
        for_the_king_utils_proto

        CONAN_PKG::imgui-sfml
        CONAN_PKG::sfml
)

target_compile_features(for_the_king_utils
    PRIVATE
        cxx_std_17
)

target_compile_definitions(for_the_king_utils
    PRIVATE
        $<$<CONFIG:Debug>:ANDONI_DEBUG>
)

if (MSVC AND CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    target_link_options(for_the_king_utils_proto
        PUBLIC
            /time
    )
endif()
