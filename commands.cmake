macro(cmake_path_ABSOLUTE_PATH VARIABLE_NAME)
    if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.20.0")
        cmake_path(ABSOLUTE_PATH ${VARIABLE_NAME})
    else()
        # polyfill for CMake < 3.20
        get_filename_component(${VARIABLE_NAME} ${${VARIABLE_NAME}} ABSOLUTE)
    endif()
endmacro()

function(jasc_to_gba_palette SOURCE OUTPUT)
    cmake_path_ABSOLUTE_PATH(SOURCE)

    cmake_policy(SET CMP0007 NEW)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    add_custom_command(
        OUTPUT
            "${OUTPUT}"
        COMMAND
            ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tools/jasc-to-gba-palette.py"
            "${SOURCE}"
            -o "${OUTPUT}"
        DEPENDS
            "${SOURCE}"
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tools/jasc-to-gba-palette.py"
        )
endfunction()

function(python_generate_file SCRIPT_NAME OUTPUT)
    cmake_policy(SET CMP0007 NEW)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    add_custom_command(
        OUTPUT
            "${OUTPUT}"
        COMMAND
            ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tools/${SCRIPT_NAME}"
            -o "${OUTPUT}"
            ${ARGN}     # pass any additional arguments
        DEPENDS
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tools/${SCRIPT_NAME}"
    )
endfunction()

macro(python_utility FUNC_NAME SCRIPT_NAME)
    function(${FUNC_NAME} SOURCE OUTPUT)
        cmake_path_ABSOLUTE_PATH(SOURCE)

        cmake_policy(SET CMP0007 NEW)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)

        # https://stackoverflow.com/a/31947751
        set(func_ARGN ARGN)

        add_custom_command(
            OUTPUT
                "${OUTPUT}"
            COMMAND
                ${Python3_EXECUTABLE}
                "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tools/${SCRIPT_NAME}"
                "${SOURCE}"
                -o "${OUTPUT}"
                ${${func_ARGN}}     # pass any additional arguments
            DEPENDS
                "${SOURCE}"
                "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tools/${SCRIPT_NAME}"
        )
    endfunction()
endmacro()

macro(python_utility_with_palette FUNC_NAME SCRIPT_NAME)
    function(${FUNC_NAME} SOURCE PALETTE OUTPUT)
        cmake_path_ABSOLUTE_PATH(SOURCE)
        cmake_path_ABSOLUTE_PATH(PALETTE)

        cmake_policy(SET CMP0007 NEW)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)

        # https://stackoverflow.com/a/31947751
        set(func_ARGN ARGN)

        add_custom_command(
                OUTPUT
                "${OUTPUT}"
                COMMAND
                ${Python3_EXECUTABLE}
                "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tools/${SCRIPT_NAME}"
                "${SOURCE}"
                --palette "${PALETTE}"
                -o "${OUTPUT}"
                ${${func_ARGN}}     # pass any additional arguments
                DEPENDS
                "${SOURCE}"
                "${PALETTE}"
                "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tools/${SCRIPT_NAME}"
        )
    endfunction()
endmacro()

python_utility_with_palette("image_compiler" "image-compiler.py")
python_utility_with_palette("sprite_compiler" "sprite-compiler.py")
python_utility_with_palette("texture_compiler" "texture-compiler.py")

python_utility("tiled_to_h" "tiled-to-h.py")
