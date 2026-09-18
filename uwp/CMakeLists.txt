cmake_minimum_required(VERSION 3.24)

set(CMAKE_SYSTEM_NAME WindowsStore)
set(CMAKE_SYSTEM_VERSION 10.0.19041.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(PaperBoat-UWP LANGUAGES CXX)
set_directory_properties(PROPERTIES VS_STARTUP_PROJECT ${PROJECT_NAME})

set(PAPERBOAT_BUILD_DIR "" CACHE PATH "Paper Boat native build directory")
set(PAPERBOAT_PACKAGE_VERSION "1.0.1.0" CACHE STRING "Four-part MSIX package version")
if(NOT PAPERBOAT_BUILD_DIR)
    message(FATAL_ERROR "PAPERBOAT_BUILD_DIR must point to the native Paper Boat build")
endif()
if(NOT PAPERBOAT_PACKAGE_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "PAPERBOAT_PACKAGE_VERSION must be a four-part MSIX version")
endif()

set(PAPERBOAT_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/Package.appxmanifest")
configure_file(Package.appxmanifest "${PAPERBOAT_MANIFEST}" @ONLY)

add_executable(${PROJECT_NAME} WIN32 main.cpp)

set(NATIVE_DIR "${PAPERBOAT_BUILD_DIR}")
set(UWP_DEPS "${PAPERBOAT_BUILD_DIR}/_deps/uwplibs-src/x64")

set(BIN_DEPS
    "${NATIVE_DIR}/PaperBoat.dll"
    "${PAPERBOAT_BUILD_DIR}/paperboat.o2r"
    "${UWP_DEPS}/bin/dxil.dll"
    "${UWP_DEPS}/bin/glfw3.dll"
    "${UWP_DEPS}/bin/libgallium_wgl.dll"
    "${UWP_DEPS}/bin/libuwp.dll"
    "${UWP_DEPS}/bin/opengl32.dll"
    "${UWP_DEPS}/bin/SDL2.dll"
    "${UWP_DEPS}/bin/z-1.dll"
    "${PAPERBOAT_MANIFEST}"
    INSTALL.txt
)
set_source_files_properties(${BIN_DEPS} PROPERTIES
    VS_COPY_TO_OUT_DIR Always
    VS_DEPLOYMENT_CONTENT TRUE
    VS_DEPLOYMENT_LOCATION "."
)

set(PKG_ASSETS
    Assets/LockScreenLogo.scale-200.png
    Assets/Square44x44Logo.targetsize-24_altform-unplated.png
    Assets/Square44x44Logo.scale-200.png
    Assets/Square150x150Logo.scale-200.png
    Assets/StoreLogo.png
    Assets/Wide310x150Logo.scale-200.png
)
set_source_files_properties(${PKG_ASSETS} PROPERTIES
    VS_DEPLOYMENT_CONTENT TRUE
    VS_DEPLOYMENT_LOCATION "Assets"
)

target_sources(${PROJECT_NAME} PRIVATE ${BIN_DEPS} ${PKG_ASSETS})
target_include_directories(${PROJECT_NAME} PRIVATE "${UWP_DEPS}/include")
target_link_libraries(${PROJECT_NAME} PRIVATE
    "${UWP_DEPS}/lib/libuwp.lib"
    "${UWP_DEPS}/lib/SDL2.lib"
    OneCore.lib
    WindowsApp.lib
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION 10.0.19041.0
    VS_GLOBAL_AppxPackage true
    VS_GLOBAL_AppxBundle Always
    VS_GLOBAL_AppxBundlePlatforms x64
    VS_GLOBAL_UapAppxPackageBuildMode SideloadOnly
)
