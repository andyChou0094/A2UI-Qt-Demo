add_executable(a2ui_test_build_contract
    frontend/test_build_contract.cpp
)
target_link_libraries(a2ui_test_build_contract
    PRIVATE
        a2ui_build_contract
        Qt5::Core
        Qt5::Test
)
add_test(NAME frontend_build_contract COMMAND a2ui_test_build_contract)

add_executable(a2ui_test_widget_registry
    frontend/test_widget_registry.cpp
)
target_link_libraries(a2ui_test_widget_registry
    PRIVATE
        a2ui_build_contract
        a2ui_composition
        Qt5::Core
        Qt5::Test
        Qt5::Widgets
)
add_test(NAME frontend_widget_registry COMMAND a2ui_test_widget_registry)

add_executable(a2ui_test_surface_validator
    frontend/test_surface_validator.cpp
    frontend/TestSourceRoot.h
)
target_compile_definitions(a2ui_test_surface_validator
    PRIVATE A2UI_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
target_link_libraries(a2ui_test_surface_validator
    PRIVATE
        a2ui_build_contract
        a2ui_composition
        Qt5::Core
        Qt5::Test
)
add_test(NAME frontend_surface_validator COMMAND a2ui_test_surface_validator)

add_executable(a2ui_test_surface_renderer
    frontend/test_surface_renderer.cpp
    frontend/TestSourceRoot.h
)
target_compile_definitions(a2ui_test_surface_renderer
    PRIVATE A2UI_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
target_link_libraries(a2ui_test_surface_renderer
    PRIVATE
        a2ui_build_contract
        a2ui_composition
        Qt5::Core
        Qt5::Test
        Qt5::Widgets
)
add_test(NAME frontend_surface_renderer COMMAND a2ui_test_surface_renderer)

add_executable(a2ui_test_surface_batch_rendering
    frontend/test_surface_batch_rendering.cpp
)
target_compile_definitions(a2ui_test_surface_batch_rendering PRIVATE
    A2UI_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    A2UI_PYTHON_EXECUTABLE="${A2UI_PYTHON_EXECUTABLE}"
)
target_link_libraries(a2ui_test_surface_batch_rendering
    PRIVATE
        a2ui_app_shell
        a2ui_build_contract
        a2ui_composition
        Qt5::Core
        Qt5::Test
        Qt5::Widgets
)
add_test(NAME frontend_surface_batch_rendering COMMAND a2ui_test_surface_batch_rendering)

add_executable(a2ui_test_host_shell
    frontend/test_host_shell.cpp
)
target_link_libraries(a2ui_test_host_shell
    PRIVATE
        a2ui_app_shell
        a2ui_build_contract
        a2ui_composition
        Qt5::Core
        Qt5::Test
        Qt5::Widgets
)
add_test(NAME frontend_host_shell COMMAND a2ui_test_host_shell)

add_executable(a2ui_test_reconciliation_state
    frontend/test_reconciliation_state.cpp
    frontend/TestSourceRoot.h
)
target_compile_definitions(a2ui_test_reconciliation_state
    PRIVATE A2UI_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
target_link_libraries(a2ui_test_reconciliation_state
    PRIVATE
        a2ui_build_contract
        a2ui_composition
        a2ui_widgets
        Qt5::Core
        Qt5::Test
        Qt5::Widgets
)
add_test(NAME frontend_reconciliation_state COMMAND a2ui_test_reconciliation_state)

add_executable(a2ui_test_composition_client
    frontend/test_composition_client.cpp
)
target_link_libraries(a2ui_test_composition_client
    PRIVATE
        a2ui_build_contract
        a2ui_composition
        Qt5::Core
        Qt5::Network
        Qt5::Test
        Qt5::Widgets
)
add_test(NAME frontend_composition_client COMMAND a2ui_test_composition_client)

add_executable(a2ui_test_surface_document_client
    frontend/test_surface_document_client.cpp
)
target_link_libraries(a2ui_test_surface_document_client
    PRIVATE
        a2ui_build_contract
        a2ui_composition
        Qt5::Core
        Qt5::Network
        Qt5::Test
)
add_test(NAME frontend_surface_document_client COMMAND a2ui_test_surface_document_client)

add_executable(a2ui_test_demo_widgets
    frontend/test_demo_widgets.cpp
)
target_link_libraries(a2ui_test_demo_widgets
    PRIVATE
        a2ui_build_contract
        a2ui_widgets
        Qt5::Core
        Qt5::Test
        Qt5::Widgets
)
add_test(NAME frontend_demo_widgets COMMAND a2ui_test_demo_widgets)

add_executable(a2ui_test_calculator
    frontend/test_calculator.cpp
)
target_link_libraries(a2ui_test_calculator
    PRIVATE
        a2ui_build_contract
        a2ui_widgets
        Qt5::Core
        Qt5::Test
        Qt5::Widgets
)
add_test(NAME frontend_calculator COMMAND a2ui_test_calculator)

add_executable(a2ui_test_calculation_service
    frontend/test_calculation_service.cpp
)
target_link_libraries(a2ui_test_calculation_service
    PRIVATE
        a2ui_build_contract
        a2ui_services
        Qt5::Core
        Qt5::Network
        Qt5::Test
)
add_test(NAME frontend_calculation_service COMMAND a2ui_test_calculation_service)

add_test(
    NAME backend_unit
    COMMAND "${A2UI_PYTHON_EXECUTABLE}" -m unittest discover
            -s backend/tests/unit -p test_*.py
)
set_tests_properties(backend_unit PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_test(
    NAME backend_integration
    COMMAND "${A2UI_PYTHON_EXECUTABLE}" -m unittest discover
            -s backend/tests/integration -p test_*.py
)
set_tests_properties(backend_integration PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
