if(NOT DEFINED ENGINE_SIM_BINARY_DIR OR NOT DEFINED ENGINE_SIM_UNITY_IOS_ARCHIVE)
    message(FATAL_ERROR "The iOS archive build paths are required")
endif()

set(configuration "$ENV{CONFIGURATION}")
set(platform "$ENV{EFFECTIVE_PLATFORM_NAME}")
if(configuration STREQUAL "")
    message(FATAL_ERROR "Xcode did not provide CONFIGURATION")
endif()

set(output_dir "${configuration}${platform}")
set(archives
    "${ENGINE_SIM_BINARY_DIR}/${output_dir}/libengine_sim_unity_ios_bridge.a"
    "${ENGINE_SIM_BINARY_DIR}/${output_dir}/libengine-sim.a"
    "${ENGINE_SIM_BINARY_DIR}/${output_dir}/libengine-sim-script-interpreter.a"
    "${ENGINE_SIM_BINARY_DIR}/dependencies/submodules/simple-2d-constraint-solver/${output_dir}/libsimple-2d-constraint-solver.a"
    "${ENGINE_SIM_BINARY_DIR}/dependencies/submodules/csv-io/${output_dir}/libcsv-io.a"
    "${ENGINE_SIM_BINARY_DIR}/dependencies/submodules/piranha/${output_dir}/libpiranha.a")

foreach(archive IN LISTS archives)
    if(NOT EXISTS "${archive}")
        message(FATAL_ERROR "Required iOS archive does not exist: ${archive}")
    endif()
endforeach()

execute_process(
    COMMAND /usr/bin/libtool -static -o "${ENGINE_SIM_UNITY_IOS_ARCHIVE}" ${archives}
    COMMAND_ERROR_IS_FATAL ANY)
