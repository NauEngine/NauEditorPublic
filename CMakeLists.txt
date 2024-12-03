cmake_minimum_required(VERSION 3.13)

project(NauEditor)


if(POLICY CMP0079)
  cmake_policy(SET CMP0079 NEW)
endif()
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif() 


# Compiler config
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD ON)
set(QT_VERSION 6)
set(CMAKE_CXX_STANDARD 20)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)


# Global config
if(NOT DEFINED NauEditorFolder)
  set(NauEditorFolder "NauEditor")
endif()

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/output CACHE PATH "" FORCE)
  message(STATUS "Use default install folder: ${CMAKE_INSTALL_PREFIX}")
else()
  message(STATUS "Defined install folder: ${CMAKE_INSTALL_PREFIX}")
endif()

# In multi-configuration IDEs, only allow Debug and Release
if(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES Debug Release)
  set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "Build Configurations" FORCE)
endif()

if (DEFINED CMAKE_BUILD_TYPE)
  message(STATUS "Build configuration: ${CMAKE_BUILD_TYPE}")
else()
  if (DEFINED CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Use multi-configuration. Allowed configurations at build time: ${CMAKE_CONFIGURATION_TYPES}")
  else()
    set(CMAKE_BUILD_TYPE "Debug")
    message(STATUS "Use default build configuration: ${CMAKE_BUILD_TYPE}")
  endif()
endif()

if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  message(FATAL_ERROR "Toolchain not selected! Please define -DCMAKE_TOOLCHAIN_FILE")
else()
  message(STATUS "Toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
endif()

if (NOT DEFINED NAU_ROOT_DIR)
  set(NAU_ROOT_DIR "$ENV{NAU_ENGINE_SOURCE_DIR}")
endif()
cmake_path(SET NAU_ROOT_DIR NORMALIZE "${NAU_ROOT_DIR}")
if (EXISTS "${NAU_ROOT_DIR}")
  message(STATUS "Found Nau Engine SDK: ${NAU_ROOT_DIR}")
else()
  message(FATAL_ERROR "Unable to find SDK!")
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)


# Nau options
option(NAU_CORE_TESTS "Build core tests projects" OFF)
# Shaders are assembled in samples, so we temporarily include them in the assembly.
# TODO: Remove them from the assembly when shader generation is transferred to engine modules
option(NAU_CORE_TOOLS "Build core tools projects" ON)
option(NAU_CORE_SAMPLES "Build core samples projects" ON)
option(NAU_RTTI "Enable rtti support" OFF)
option(NAU_EXCEPTIONS "Enable exception support" OFF)

option(BUILD_SHARED_LIBS "Build shared libs" OFF)

set(CMAKE_MODULE_PATH
  "${NAU_ROOT_DIR}/cmake"
  "${CMAKE_SOURCE_DIR}/cmake/defaults"
  "${CMAKE_SOURCE_DIR}/cmake/macros"
)


include(defaults/UsdPackages)
include(NauCommon)
include(NauModule)
include(NauGenFunctions)
include(platformSpec/NauPlatformSetup)
include(Packages)

add_subdirectory("${NAU_ROOT_DIR}" "NauEngine")

add_subdirectory(editor)
add_subdirectory(installer)
add_subdirectory(translations)
