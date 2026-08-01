cmake_minimum_required(VERSION 3.25)

project(
        sixchange
        VERSION 0.1.0
        LANGUAGES CXX
)

option(
        SIXCHANGE_ENABLE_SANITIZERS
        "Enable ASan and UBSan"
        OFF
)

option(
        SIXCHANGE_ENABLE_NATIVE_OPTIMIZATIONS
        "Optimize Release builds for the CPU compiling Sixchange"
        OFF
)

option(
        SIXCHANGE_ENABLE_IPO
        "Enable IPO/LTO in Release builds"
        OFF
)

option(
        SIXCHANGE_WARNINGS_AS_ERRORS
        "Treat Sixchange compiler warnings as errors"
        OFF
)

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR
            "Unsupported compiler!"
    )
endif()

if(SIXCHANGE_ENABLE_IPO)
    include(CheckIPOSupported)

    check_ipo_supported(
            RESULT SIXCHANGE_IPO_SUPPORTED
            OUTPUT SIXCHANGE_IPO_ERROR
            LANGUAGES CXX
    )

    if(NOT SIXCHANGE_IPO_SUPPORTED)
        message(FATAL_ERROR
                "IPO/LTO was requested but is not supported: "
                "${SIXCHANGE_IPO_ERROR}"
        )
    endif()

    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()


add_library(sixchange_build_options INTERFACE)

if(SIXCHANGE_ENABLE_SANITIZERS)
    target_compile_options(sixchange_build_options
            INTERFACE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
    )

    target_link_options(sixchange_build_options
            INTERFACE
            -fsanitize=address,undefined
    )
endif()

if(SIXCHANGE_ENABLE_NATIVE_OPTIMIZATIONS)
    target_compile_options(sixchange_build_options
            INTERFACE
            $<$<CONFIG:Release>:-march=native>
            $<$<CONFIG:Release>:-mtune=native>
    )
endif()

function(sixchange_enable_warnings target)
    target_compile_options(${target}
            PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
    )

    if(SIXCHANGE_WARNINGS_AS_ERRORS)
        set_property(
                TARGET ${target}
                PROPERTY COMPILE_WARNING_AS_ERROR ON
        )
    endif()
endfunction()

find_package(Threads REQUIRED)

add_library(sixchange_engine
        src/engine/OrderBook.cpp
        src/engine/MatchingEngine.cpp
        src/engine/Sequencer.cpp
        src/protocol/TextCodec.cpp
        src/gateway/OrderGateway.cpp
        src/logging/Log.cpp
        src/logging/AsyncFileLogger.cpp
)

target_compile_features(sixchange_engine
        PUBLIC
        cxx_std_23
)

set_target_properties(sixchange_engine PROPERTIES
        CXX_EXTENSIONS OFF
)

target_include_directories(sixchange_engine
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(sixchange_engine
        PUBLIC
        sixchange_build_options
        Threads::Threads
)

sixchange_enable_warnings(sixchange_engine)

add_executable(sixchange_cli
        apps/sixchange_cli.cpp
)

target_compile_features(sixchange_cli
        PRIVATE
        cxx_std_23
)

set_target_properties(sixchange_cli PROPERTIES
        CXX_EXTENSIONS OFF
        OUTPUT_NAME sixchange
)

target_link_libraries(sixchange_cli
        PRIVATE
        sixchange_engine
)

sixchange_enable_warnings(sixchange_cli)

include(CTest)

if(BUILD_TESTING)
    include(FetchContent)

    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    FetchContent_MakeAvailable(googletest)

    add_executable(sixchange_tests
            tests/engine/OrderPoolTests.cpp
            tests/engine/PriceLevelTests.cpp
            tests/engine/OrderBookTests.cpp
            tests/engine/MatchingEngineTests.cpp
            tests/protocol/TextCodecTests.cpp
            tests/gateway/OrderGatewayTests.cpp
            tests/integration/OrderFlowTests.cpp
            tests/engine/FixedOrderMapTests.cpp
    )

    target_include_directories(sixchange_tests
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )

    target_compile_features(sixchange_tests
            PRIVATE
            cxx_std_23
    )

    set_target_properties(sixchange_tests PROPERTIES
            CXX_EXTENSIONS OFF
    )

    target_link_libraries(sixchange_tests
            PRIVATE
            sixchange_engine
            GTest::gtest_main
    )

    sixchange_enable_warnings(sixchange_tests)

    include(GoogleTest)
    gtest_discover_tests(sixchange_tests)
endif()