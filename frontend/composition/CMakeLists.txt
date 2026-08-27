add_library(a2ui_composition STATIC
    CompositionClient.cpp
    CompositionClient.h
    SurfaceDocumentClient.cpp
    SurfaceDocumentClient.h
    DemoWidgetRegistration.cpp
    DemoWidgetRegistration.h
    SurfaceRenderer.cpp
    SurfaceRenderer.h
    SurfaceValidator.cpp
    SurfaceValidator.h
    WidgetRegistry.cpp
    WidgetRegistry.h
)
target_include_directories(a2ui_composition PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(a2ui_composition PUBLIC
    a2ui_build_contract
    a2ui_services
    a2ui_widgets
    Qt5::Core
    Qt5::Widgets
    Qt5::Network
)
