include(CheckCXXSourceCompiles)

set(A2UI_SUPPORTED_GNU_VERSIONS "7.3.0;9.3.0" CACHE STRING
    "Exact GNU compiler versions accepted by the compatibility matrix")
set(A2UI_PRECOMPILED_PLUGIN "" CACHE FILEPATH
    "External precompiled plugin (intentionally unsupported by this demo)")

function(a2ui_enforce_build_policy)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR
            "A2UIQtDemo supports GNU GCC 7.3.0 and 9.3.0 only; "
            "found ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    endif()

    if(NOT CMAKE_CXX_COMPILER_VERSION IN_LIST A2UI_SUPPORTED_GNU_VERSIONS)
        message(FATAL_ERROR
            "Unsupported GCC ${CMAKE_CXX_COMPILER_VERSION}; expected one of "
            "${A2UI_SUPPORTED_GNU_VERSIONS}")
    endif()

    if(NOT Qt5Core_VERSION_STRING STREQUAL "5.12.8")
        message(FATAL_ERROR
            "Unsupported Qt ${Qt5Core_VERSION_STRING}; Qt 5.12.8 is required")
    endif()

    if(A2UI_PRECOMPILED_PLUGIN)
        message(FATAL_ERROR
            "External precompiled plugins are rejected because their Qt, GCC "
            "and libstdc++ ABI cannot be proven compatible. Build adapters "
            "from source in this project instead.")
    endif()

    set(CMAKE_REQUIRED_FLAGS "-std=c++14")
    check_cxx_source_compiles(
        "#if __cplusplus != 201402L\n#error C++14 required\n#endif\nint main(){return 0;}"
        A2UI_EXACT_CXX14_MODE)
    if(NOT A2UI_EXACT_CXX14_MODE)
        message(FATAL_ERROR "The compiler did not enter exact ISO C++14 mode")
    endif()

    check_cxx_source_compiles(
        "#include <string>\n#ifndef _GLIBCXX_USE_CXX11_ABI\n#error ABI unknown\n#endif\nstatic_assert(_GLIBCXX_USE_CXX11_ABI == 1, \"new ABI required\");\nint main(){return 0;}"
        A2UI_GLIBCXX_NEW_ABI)
    if(NOT A2UI_GLIBCXX_NEW_ABI)
        message(FATAL_ERROR
            "A2UIQtDemo requires _GLIBCXX_USE_CXX11_ABI=1 for all C++ targets")
    endif()

    find_program(A2UI_PYTHON_EXECUTABLE NAMES python3 python)
    if(NOT A2UI_PYTHON_EXECUTABLE)
        message(FATAL_ERROR "Python is required to run the source policy check")
    endif()

    execute_process(
        COMMAND "${A2UI_PYTHON_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/scripts/check_source_policy.py"
                "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE A2UI_POLICY_RESULT)
    if(NOT A2UI_POLICY_RESULT EQUAL 0)
        message(FATAL_ERROR "Source policy check failed")
    endif()

    add_custom_target(a2ui_source_policy_check ALL
        COMMAND "${A2UI_PYTHON_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/scripts/check_source_policy.py"
                "${CMAKE_SOURCE_DIR}"
        COMMENT "Checking Qt 5.12/C++14 dependency policy"
        VERBATIM)
endfunction()
