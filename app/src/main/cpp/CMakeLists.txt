cmake_minimum_required(VERSION 3.22.1)
project(population_misfits LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_SOURCES
    character_skin.cpp
    customs_map.cpp
    battle_gameplay.cpp
)

if(ANDROID)
    find_package(OpenXR REQUIRED CONFIG)

    add_library(native_app_glue STATIC
        ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
    )
    target_include_directories(native_app_glue PUBLIC
        ${ANDROID_NDK}/sources/android/native_app_glue
    )

    # Export ANativeActivity_onCreate so Meta Quest NativeActivity launches cleanly
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

    add_library(population_misfits SHARED
        ${CORE_SOURCES}
        gles3_renderer.cpp
        quest_openxr_main.cpp
    )

    target_include_directories(population_misfits PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ANDROID_NDK}/sources/android/native_app_glue
    )

    target_link_libraries(population_misfits
        native_app_glue
        OpenXR::openxr_loader
        android
        EGL
        GLESv3
        log
    )
else()
    # Host Linux Test & Simulation Executable to verify all gameplay, customs, and character systems
    add_executable(population_misfits_tests
        ${CORE_SOURCES}
        host_test_runner.cpp
    )
    target_include_directories(population_misfits_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
