cmake_minimum_required(VERSION 3.21)

# ---------------------------------------------------------------------------
# Abyssograph add-on SDK --- samples.
#
# The only dependency is ../include; nothing links against the application.
# To add another DLL, create a folder for it and add its name to the loop below.
#
# Build for x64. Abyssograph is a 64-bit application and does not load x86 DLLs.
# ---------------------------------------------------------------------------
project(AbyssoSamples VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Write every configuration into bin/ instead of a folder per configuration.
foreach(suffix "" _DEBUG _RELEASE _RELWITHDEBINFO _MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY${suffix} "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endforeach()

foreach(sample MinimalAddon JuliaAddon)
    add_library(${sample} SHARED ${sample}/${sample}.cpp)
    target_include_directories(${sample} PRIVATE ../include)

    if (MSVC)
        # The sources contain non-ASCII string literals, so /utf-8 is required.
        target_compile_options(${sample} PRIVATE /utf-8)
    endif()

    # Link the CRT statically so the DLL loads without a redistributable.
    set_property(TARGET ${sample} PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endforeach()
