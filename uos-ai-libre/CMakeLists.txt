cmake_minimum_required(VERSION 3.21)

project(uos-ai-libre VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Quick QuickControls2 Svg Network)
qt_standard_project_setup()

set(AISH_CXXBRIDGE_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../aish-cxxbridge"
    CACHE PATH "Directory containing the aish-cxxbridge build")
set(AISH_CXXBRIDGE_LIB
    "${AISH_CXXBRIDGE_ROOT}/target/release/libaish_cxxbridge.a"
    CACHE FILEPATH "Path to the Rust static library")
set(AISH_CXXBRIDGE_INCLUDE
    "${AISH_CXXBRIDGE_ROOT}/generated"
    CACHE PATH "Directory containing the generated cxx header")

if(NOT EXISTS "${AISH_CXXBRIDGE_LIB}")
    message(FATAL_ERROR
        "Missing aish-cxxbridge static library:\n  ${AISH_CXXBRIDGE_LIB}\n"
        "Run ../aish-cxxbridge/build.sh first.")
endif()
if(NOT EXISTS "${AISH_CXXBRIDGE_INCLUDE}/aish-cxxbridge.h")
    message(FATAL_ERROR
        "Missing generated cxx header:\n  ${AISH_CXXBRIDGE_INCLUDE}/aish-cxxbridge.h\n"
        "Run ../aish-cxxbridge/build.sh first.")
endif()

add_library(aish_cxxbridge STATIC IMPORTED GLOBAL)
set_target_properties(aish_cxxbridge PROPERTIES
    IMPORTED_LOCATION "${AISH_CXXBRIDGE_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${AISH_CXXBRIDGE_INCLUDE}")
set_property(TARGET aish_cxxbridge APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES "-lpthread;-ldl;-lm;-lrt;-lutil")

set_source_files_properties(qml/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE)

file(GLOB_RECURSE QML_SOURCES CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" qml/*.qml)
file(GLOB ICON_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/*.svg")

# qt_add_qml_module runs qmlcachegen at build time. The resulting bytecode and
# AOT-compiled functions are linked into the executable, so the first launch
# does not parse or compile QML from the source tree. Keep cachegen enabled
# even if a caller has changed the default for other targets in this build.
set_source_files_properties(${QML_SOURCES} PROPERTIES
    QT_QML_SKIP_CACHEGEN FALSE)

qt_add_executable(uos-ai-libre
    src/main.cpp
    src/aishbridge.cpp
    src/aishbridge.h
    src/appcontroller.cpp
    src/appcontroller.h
    src/chatmodel.cpp
    src/chatmodel.h
    src/iconprovider.cpp
    src/iconprovider.h
    src/markdownmodel.cpp
    src/markdownmodel.h
    src/rhibackend-selector.cpp
    src/rhibackend-selector.h
    src/singleinstance.cpp
    src/singleinstance.h
    src/windowhost.cpp
    src/windowhost.h
)

target_include_directories(uos-ai-libre PRIVATE src)
target_compile_options(uos-ai-libre PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(uos-ai-libre PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Quick Qt6::QuickControls2 Qt6::Svg Qt6::Network
    ${CMAKE_DL_LIBS}
    aish_cxxbridge)

qt_add_resources(uos-ai-libre "app_assets"
    PREFIX "/"
    BASE "resources"
    FILES
        ${ICON_FILES}
        resources/images/logo.png
)

qt_add_qml_module(uos-ai-libre
    URI UosAiLibre
    VERSION 1.0
    QML_FILES ${QML_SOURCES}
)

set_target_properties(uos-ai-libre PROPERTIES
    OUTPUT_NAME "uos-ai-libre"
    MACOSX_BUNDLE OFF
    WIN32_EXECUTABLE OFF)

install(TARGETS uos-ai-libre RUNTIME DESTINATION bin)
install(FILES resources/uos-ai-libre.desktop DESTINATION share/applications)
install(FILES resources/images/logo.png
    DESTINATION share/icons/hicolor/256x256/apps
    RENAME uos-ai-libre.png)

option(UOS_AI_LIBRE_BUILD_TESTS "Build the Markdown parser tests" ON)
if(UOS_AI_LIBRE_BUILD_TESTS)
    enable_testing()
    add_executable(markdown-test
        tests/markdown_test.cpp
        src/markdownmodel.cpp
        src/markdownmodel.h
    )
    target_include_directories(markdown-test PRIVATE src)
    target_compile_options(markdown-test PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(markdown-test PRIVATE Qt6::Core)
    add_test(NAME markdown-test COMMAND markdown-test)

    add_executable(chatmodel-test
        tests/chatmodel_test.cpp
        src/chatmodel.cpp
        src/chatmodel.h
    )
    target_include_directories(chatmodel-test PRIVATE src)
    target_compile_options(chatmodel-test PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(chatmodel-test PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME chatmodel-test COMMAND chatmodel-test)
endif()
