project(Installer)

file(READ "${CMAKE_SOURCE_DIR}/resources/editor/version.txt" Version)

find_package (Python3 COMPONENTS Interpreter)
if (Python3_FOUND)
    if (WIN32)
        add_custom_target(Installer ALL
            COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --green "Generation installer:"
            COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --green "NAU_ROOT_DIR = ${NAU_ROOT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --green "CMAKE_TOOLCHAIN_FILE = ${CMAKE_TOOLCHAIN_FILE}"
            COMMAND ${Python3_EXECUTABLE} installer.py win64 "$<TARGET_FILE_DIR:NauEditor>" "${CMAKE_BINARY_DIR}/$<CONFIG>/Installer.exe" "${NAU_ROOT_DIR}" "$<CONFIG>" "${CMAKE_TOOLCHAIN_FILE}" "${Version}"  
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/installer
            VERBATIM)

    else()
        message(FATAL_ERROR "Installer not implemented for this platform")
    endif()
else()
    message(FATAL_ERROR "Python not found! Please check your system configuration")
endif()

add_dependencies(Installer NauEditor NauSceneEditor NauGuiEditor)