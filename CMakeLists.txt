cmake_minimum_required(VERSION 3.25)

project(
        sixchange
        VERSION 0.1.0
        LANGUAGES CXX
)

add_library(sixchange_engine
        src/engine/OrderBook.cpp
        src/engine/MatchingEngine.cpp
        src/engine/Sequencer.cpp
)

target_compile_features(sixchange_engine
        PUBLIC
        cxx_std_23
)

target_include_directories(sixchange_engine
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(sixchange_engine
        PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        >
        $<$<CXX_COMPILER_ID:MSVC>:
        /W4
        >
)

include(CTest)

if(BUILD_TESTING)
    include(FetchContent)

    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    # Relevant primarily on Windows.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)

    add_executable(sixchange_tests
            tests/OrderPoolTests.cpp
            tests/PriceLevelTests.cpp
            tests/OrderBookTests.cpp
            tests/MatchingEngineTests.cpp
    )

    target_link_libraries(sixchange_tests
            PRIVATE
            sixchange_engine
            GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(sixchange_tests)
endif()

target_compile_options(sixchange_engine PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
)

target_link_options(sixchange_engine PRIVATE
        -fsanitize=address,undefined
)

target_compile_options(sixchange_tests PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
)

target_link_options(sixchange_tests PRIVATE
        -fsanitize=address,undefined
)