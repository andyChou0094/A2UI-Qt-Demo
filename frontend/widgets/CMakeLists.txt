add_library(a2ui_widgets STATIC
    DemoWidgets.cpp
    DemoWidgets.h
)
target_include_directories(a2ui_widgets PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(a2ui_widgets PUBLIC
    a2ui_build_contract
    a2ui_services
    Qt5::Core
    Qt5::Widgets
)
