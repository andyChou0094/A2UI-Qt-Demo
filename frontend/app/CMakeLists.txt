add_library(a2ui_app_shell STATIC
    HostShell.cpp
    HostShell.h
    Theme.cpp
    Theme.h
    theme.qrc
)
target_include_directories(a2ui_app_shell PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(a2ui_app_shell
    PUBLIC
        a2ui_build_contract
        a2ui_composition
        a2ui_services
        a2ui_widgets
        Qt5::Core
        Qt5::Widgets
        Qt5::Network
)

add_executable(a2ui_qt_demo
    main.cpp
)

target_link_libraries(a2ui_qt_demo PRIVATE a2ui_app_shell)

set_target_properties(a2ui_qt_demo PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
