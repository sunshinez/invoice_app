cmake_minimum_required(VERSION 3.16)
project(InvoiceManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Concurrent)
find_package(PkgConfig REQUIRED)

# Find pugixml for XML parsing
pkg_check_modules(PUGIXML REQUIRED pugixml)

add_executable(invoice_manager
    src/main.cpp
    src/domain/invoice.h
    src/services/pdf_text_extractor.cpp
    src/services/pdf_text_extractor.h
    src/services/async_pdf_processor.cpp
    src/services/async_pdf_processor.h
    src/data/invoice_database.cpp
    src/data/invoice_database.h
    src/ui/project_selection_dialog.cpp
    src/ui/project_selection_dialog.h
    src/ui/manual_entry_dialog.cpp
    src/ui/manual_entry_dialog.h
    src/ui/invoice_manager_window.cpp
    src/ui/invoice_manager_window.h
    src/ui/sidebar_widget.cpp
    src/ui/sidebar_widget.h
    src/ui/invoice_list_widget.cpp
    src/ui/invoice_list_widget.h
    src/ui/invoice_detail_widget.cpp
    src/ui/invoice_detail_widget.h
)

target_link_libraries(invoice_manager PRIVATE
    Qt6::Widgets
    Qt6::Sql
    Qt6::Concurrent
    ${PUGIXML_LIBRARIES}
)

target_include_directories(invoice_manager PRIVATE ${PUGIXML_INCLUDE_DIRS})
target_link_directories(invoice_manager PRIVATE ${PUGIXML_LIBRARY_DIRS})
target_compile_options(invoice_manager PRIVATE ${PUGIXML_CFLAGS_OTHER})
target_link_options(invoice_manager PRIVATE ${PUGIXML_LDFLAGS_OTHER})

# Optional: Enable high DPI scaling
set_target_properties(invoice_manager PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
