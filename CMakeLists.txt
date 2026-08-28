cmake_minimum_required(VERSION 3.19)

set(CMAKE_OSX_ARCHITECTURES arm64)
set(ONLY_ACTIVE_ARCH        NO)

include(CMakeDependentOption)

include_directories(/opt/homebrew/include)
include_directories(/opt/homebrew/lib)

set(BISON_EXECUTABLE /opt/homebrew/Cellar/bison/3.8.2/bin/bison)

option(DTV "Enable video output" OFF)
option(PIRANHA_ENABLED "Enable scripting input" ON)
option(ENGINE_SIM_BUILD_APP "Build the graphical engine-sim application" ON)
option(ENGINE_SIM_BUILD_TESTS "Build engine-sim tests" ON)
option(ENGINE_SIM_BUILD_UNITY_PLUGIN "Build the headless Unity native plugin" ON)
option(ENGINE_SIM_BUILD_UNITY_IOS "Build the headless Unity iOS static library" OFF)
option(ENGINE_SIM_BUILD_BENCHMARKS "Build performance microbenchmarks" OFF)
cmake_dependent_option(DISCORD_ENABLED "Enable Discord Rich Presence" ON WIN32 OFF)

# Default to compiling as Release in single-configuration generators
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

if (DTV)
    add_compile_definitions(ATG_ENGINE_SIM_VIDEO_CAPTURE)
endif (DTV)

if (PIRANHA_ENABLED)
    add_compile_definitions(ATG_ENGINE_SIM_PIRANHA_ENABLED)
endif (PIRANHA_ENABLED)

if (DISCORD_ENABLED)
    add_compile_definitions(ATG_ENGINE_SIM_DISCORD_ENABLED)
endif (DISCORD_ENABLED)

# Enable group projects in folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "cmake")

project(engine-sim)

set(CMAKE_CXX_STANDARD 17)

# Keep Release builds optimized across all generators. In particular, the
# Xcode/iOS generator otherwise leaves GCC_OPTIMIZATION_LEVEL at 0 in this
# project even when --config Release is selected.
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG" CACHE STRING "Release C flags" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG" CACHE STRING "Release C++ flags" FORCE)

include(CheckIPOSupported)
check_ipo_supported(RESULT ENGINE_SIM_IPO_SUPPORTED OUTPUT ENGINE_SIM_IPO_ERROR)
if (ENGINE_SIM_IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
else ()
    message(WARNING "Release LTO is unavailable: ${ENGINE_SIM_IPO_ERROR}")
endif ()

# ========================================================
# GTEST

if (ENGINE_SIM_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL
        https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    set_property(TARGET gmock PROPERTY FOLDER "gtest")
    set_property(TARGET gmock_main PROPERTY FOLDER "gtest")
    set_property(TARGET gtest PROPERTY FOLDER "gtest")
    set_property(TARGET gtest_main PROPERTY FOLDER "gtest")
endif ()

# ========================================================

# Define dependency targets before linking the core libraries. Otherwise CMake
# treats their names as raw -l flags when configuring a headless-only build.
add_subdirectory(dependencies)

add_library(engine-sim STATIC
    # Source files
    src/audio_buffer.cpp
    src/camshaft.cpp
    src/crankshaft.cpp
    src/combustion_chamber.cpp
    src/connecting_rod.cpp
    src/convolution_filter.cpp
    src/cylinder_bank.cpp
    src/cylinder_head.cpp
    src/delay_filter.cpp
    src/derivative_filter.cpp
    src/direct_throttle_linkage.cpp
    src/dynamometer.cpp
    src/engine.cpp
    src/exhaust_system.cpp
    src/feedback_comb_filter.cpp
    src/filter.cpp
    src/fuel.cpp
    src/function.cpp
    src/gas_system.cpp
    src/gaussian_filter.cpp
    src/governor.cpp
    src/ignition_module.cpp
    src/impulse_response.cpp
    src/intake.cpp
    src/jitter_filter.cpp
    src/leveling_filter.cpp
    src/low_pass_filter.cpp
    src/part.cpp
    src/piston.cpp
    src/simulator.cpp
    src/standard_valvetrain.cpp
    src/starter_motor.cpp
    src/synthesizer.cpp
    src/throttle.cpp
    src/transmission.cpp
    src/utilities.cpp
    src/valvetrain.cpp
    src/vehicle.cpp
    src/vehicle_drag_constraint.cpp
    src/vtec_valvetrain.cpp

    # Include files
    include/audio_buffer.h
    include/application_settings.h
    include/camshaft.h
    include/crankshaft.h
    include/combustion_chamber.h
    include/connecting_rod.h
    include/convolution_filter.h
    include/cylinder_bank.h
    include/cylinder_head.h
    include/delay_filter.h
    include/derivative_filter.h
    include/direct_throttle_linkage.h
    include/dynamometer.h
    include/engine.h
    include/exhaust_system.h
    include/feedback_comb_filter.h
    include/filter.h
    include/fuel.h
    include/function.h
    include/gas_system.h
    include/gaussian_filter.h
    include/governor.h
    include/ignition_module.h
    include/impulse_response.h
    include/intake.h
    include/jitter_filter.h
    include/leveling_filter.h
    include/low_pass_filter.h
    include/part.h
    include/piston.h
    include/simulator.h
    include/standard_valvetrain.h
    include/starter_motor.h
    include/synthesizer.h
    include/throttle.h
    include/transmission.h
    include/units.h
    include/utilities.h
    include/valvetrain.h
    include/vehicle.h
    include/vehicle_drag_constraint.h
    include/vtec_valvetrain.h
)

target_link_libraries(engine-sim
    simple-2d-constraint-solver
    csv-io)

if (APPLE)
    target_link_libraries(engine-sim "-framework Accelerate")
endif ()

target_include_directories(engine-sim
    PUBLIC dependencies/submodules)

if (ENGINE_SIM_BUILD_BENCHMARKS)
    add_executable(engine-sim-convolution-benchmark
        tools/benchmark_convolution.cpp)
    target_include_directories(engine-sim-convolution-benchmark PRIVATE include)
    target_link_libraries(engine-sim-convolution-benchmark engine-sim)
endif ()

if (PIRANHA_ENABLED)
    add_library(engine-sim-script-interpreter STATIC
        # Source files
        scripting/src/channel_types.cpp
        scripting/src/compiler.cpp
        scripting/src/engine_context.cpp
        scripting/src/language_rules.cpp

        # Include files
        scripting/include/actions.h
        scripting/include/camshaft_node.h
        scripting/include/channel_types.h
        scripting/include/compiler.h
        scripting/include/connecting_rod_node.h
        scripting/include/crankshaft_node.h
        scripting/include/cylinder_bank_node.h
        scripting/include/cylinder_head_node.h
        scripting/include/engine_context.h
        scripting/include/engine_node.h
        scripting/include/engine_sim.h
        scripting/include/exhaust_system_node.h
        scripting/include/intake_node.h
        scripting/include/function_node.h
        scripting/include/ignition_module_node.h
        scripting/include/ignition_wire_node.h
        scripting/include/impulse_response_node.h
        scripting/include/intake_node.h
        scripting/include/language_rules.h
        scripting/include/node.h
        scripting/include/object_reference_node.h
        scripting/include/object_reference_node_output.h
        scripting/include/piranha.h
        scripting/include/piston_node.h
        scripting/include/rod_journal_node.h
        scripting/include/standard_valvetrain_node.h
        scripting/include/transmission_node.h
        scripting/include/valvetrain_node.h
        scripting/include/vtec_valvetrain_node.h
		scripting/include/vehicle_node.h
    )

    target_include_directories(engine-sim-script-interpreter
        PUBLIC dependencies/submodules)

    target_link_libraries(engine-sim-script-interpreter
        csv-io
        piranha)
endif (PIRANHA_ENABLED)

if (ENGINE_SIM_BUILD_APP)
add_executable(engine-sim-app WIN32
    # Source files
    src/main.cpp
    src/engine_sim_application.cpp
    src/geometry_generator.cpp
    src/simulation_object.cpp
    src/piston_object.cpp
    src/connecting_rod_object.cpp
    src/ui_element.cpp
    src/ui_manager.cpp
    src/cylinder_pressure_gauge.cpp
    src/ui_math.cpp
    src/gauge.cpp
    src/crankshaft_object.cpp
    src/cylinder_bank_object.cpp
    src/cylinder_head_object.cpp
    src/ui_button.cpp
    src/ui_utilities.cpp
    src/combustion_chamber_object.cpp
    src/oscilloscope.cpp
    src/shaders.cpp
    src/engine_view.cpp
    src/right_gauge_cluster.cpp
    src/cylinder_temperature_gauge.cpp
    src/labeled_gauge.cpp
    src/throttle_display.cpp
    src/afr_cluster.cpp
    src/fuel_cluster.cpp
    src/oscilloscope_cluster.cpp
    src/performance_cluster.cpp
    src/firing_order_display.cpp
    src/load_simulation_cluster.cpp
    src/mixer_cluster.cpp
    src/info_cluster.cpp

    # Include files
    include/engine_sim_application.h
    include/geometry_generator.h
    include/simulation_object.h
    include/piston_object.h
    include/connecting_rod_object.h
    include/ui_element.h
    include/ui_manager.h
    include/cylinder_pressure_gauge.h
    include/ui_math.h
    include/crankshaft_object.h
    include/cylinder_bank_object.h
    include/cylinder_head_object.h
    include/ui_button.h
    include/ui_utilities.h
    include/combustion_chamber_object.h
    include/oscilloscope.h
    include/shaders.h
    include/engine_view.h
    include/right_gauge_cluster.h
    include/cylinder_temperature_gauge.h
    include/labeled_gauge.h
    include/throttle_display.h
    include/afr_cluster.h
    include/fuel_cluster.h
    include/oscilloscope_cluster.h
    include/performance_cluster.h
    include/firing_order_display.h
    include/load_simulation_cluster.h
    include/mixer_cluster.h
    include/info_cluster.h
    include/units.h
)

if (DTV)
    target_link_libraries(engine-sim-app
        direct-to-video)
endif (DTV)

if (PIRANHA_ENABLED)
    target_link_libraries(engine-sim-app
        engine-sim-script-interpreter)
endif (PIRANHA_ENABLED)

target_include_directories(engine-sim-app
    PUBLIC dependencies/submodules)

if (DISCORD_ENABLED)
    target_link_libraries(engine-sim-app
        discord
        discord-rpc
    )
endif (DISCORD_ENABLED)

target_link_libraries(engine-sim-app
    engine-sim
    delta-basic
)
endif ()

if (ENGINE_SIM_BUILD_UNITY_PLUGIN)
    if (NOT PIRANHA_ENABLED)
        message(FATAL_ERROR "ENGINE_SIM_BUILD_UNITY_PLUGIN requires PIRANHA_ENABLED")
    endif ()

    add_library(engine-sim-unity SHARED
        include/engine_sim_unity.h
        src/engine_sim_unity.cpp)
    target_include_directories(engine-sim-unity PUBLIC include)
    target_link_libraries(engine-sim-unity
        engine-sim
        engine-sim-script-interpreter)
    set_target_properties(engine-sim-unity PROPERTIES
        OUTPUT_NAME "engine_sim_unity"
        MACOSX_RPATH ON
        POSITION_INDEPENDENT_CODE ON)
endif ()

if (ENGINE_SIM_BUILD_UNITY_IOS)
    if (NOT APPLE)
        message(FATAL_ERROR "ENGINE_SIM_BUILD_UNITY_IOS is only supported with an Apple toolchain")
    endif ()
    if (NOT PIRANHA_ENABLED)
        message(FATAL_ERROR "ENGINE_SIM_BUILD_UNITY_IOS requires PIRANHA_ENABLED")
    endif ()

    # A static library does not absorb the static libraries it links against.
    # Build the bridge separately, then combine the complete native dependency
    # graph into the single archive Unity expects in Assets/Plugins/iOS.
    add_library(engine-sim-unity-ios-bridge STATIC
        include/engine_sim_unity.h
        src/engine_sim_unity.cpp)
    target_include_directories(engine-sim-unity-ios-bridge PUBLIC include)
    target_link_libraries(engine-sim-unity-ios-bridge
        engine-sim
        engine-sim-script-interpreter)
    set_target_properties(engine-sim-unity-ios-bridge PROPERTIES
        OUTPUT_NAME "engine_sim_unity_ios_bridge"
        POSITION_INDEPENDENT_CODE ON)

    set(ENGINE_SIM_UNITY_IOS_ARCHIVE
        "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}engine_sim_unity_ios${CMAKE_STATIC_LIBRARY_SUFFIX}")
    add_custom_command(
        OUTPUT "${ENGINE_SIM_UNITY_IOS_ARCHIVE}"
        # With the Xcode generator, TARGET_FILE paths contain the build-time
        # EFFECTIVE_PLATFORM_NAME variable. CMake 4 escapes that variable in
        # custom build phases, leaving a literal ${EFFECTIVE_PLATFORM_NAME} in
        # the path. Resolve the Xcode environment in a CMake script instead.
        COMMAND "${CMAKE_COMMAND}"
            -DENGINE_SIM_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
            -DENGINE_SIM_UNITY_IOS_ARCHIVE=${ENGINE_SIM_UNITY_IOS_ARCHIVE}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/create_unity_ios_archive.cmake"
        DEPENDS
            engine-sim-unity-ios-bridge
            engine-sim
            engine-sim-script-interpreter
            simple-2d-constraint-solver
            csv-io
            piranha
        COMMENT "Creating self-contained Unity iOS static library"
        VERBATIM)
    add_custom_target(engine-sim-unity-ios
        DEPENDS "${ENGINE_SIM_UNITY_IOS_ARCHIVE}")
endif ()

# GTEST

if (ENGINE_SIM_BUILD_TESTS)
enable_testing()

add_executable(engine-sim-test
    # Source files
    test/convolution_filter_tests.cpp
    test/gas_system_tests.cpp
    test/function_test.cpp
    test/synthesizer_tests.cpp
)

target_link_libraries(engine-sim-test
    gtest_main
    engine-sim
)

include(GoogleTest)
gtest_discover_tests(engine-sim-test)
endif ()
