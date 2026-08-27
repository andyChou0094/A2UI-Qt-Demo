add_library(a2ui_services STATIC
    CalculationRecord.cpp
    CalculationRecord.h
    CalculationService.cpp
    CalculationService.h
)
target_include_directories(a2ui_services PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(a2ui_services PUBLIC
    a2ui_build_contract
    Qt5::Core
    Qt5::Network
)
