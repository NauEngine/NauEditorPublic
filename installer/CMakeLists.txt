project(Installer)

file(READ "${CMAKE_SOURCE_DIR}/resources/editor/version.txt" Version)

find_package (Python3 COMPONENTS Interpreter)
if (Python3_FOUND)
    if (WIN32)
        message(NOTICE "NAU_ROOT_DIR ${NAU_ROOT_DIR}")
        add_custom_target(Installer ALL
            COMMAND ${Python3_EXECUTABLE} installer.py win64 "$<TARGET_FILE_DIR:NauEditor>" "${CMAKE_BINARY_DIR}/$<CONFIG>/Installer.exe" "${NAU_ROOT_DIR}" "$<CONFIG>" "${Version}" "${CMAKE_TOOLCHAIN_FILE}" 
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/installer
            VERBATIM)

    else()
        message(FATAL_ERROR "Installer not implemented for this platform")
    endif()
else()
    message(FATAL_ERROR "Python not found! Please check your system configuration")
endif()

add_dependencies(Installer NauEditor NauSceneEditor NauGuiEditor)