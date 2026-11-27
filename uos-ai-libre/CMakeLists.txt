cmake_minimum_required(VERSION 3.21)

project(uos-ai-libre VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Quick QuickControls2 Svg Network DBus)
qt_standard_project_setup()

# Layer-shell surfaces have no client-side move request. On Wayland we use the
# relative-pointer protocol for drag deltas; it is optional so X11-only and
# minimal build environments still compile.
find_package(PkgConfig QUIET)
set(UOS_AI_LIBRE_WAYLAND_DRAG OFF)
if(PkgConfig_FOUND)
    pkg_check_modules(WAYLAND_CLIENT QUIET wayland-client)
    pkg_check_modules(WAYLAND_PROTOCOLS QUIET wayland-protocols)
endif()
find_program(WAYLAND_SCANNER_EXECUTABLE wayland-scanner)
if(WAYLAND_CLIENT_FOUND AND WAYLAND_PROTOCOLS_FOUND
   AND WAYLAND_SCANNER_EXECUTABLE)
    pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
    set(RELATIVE_POINTER_XML
        "${WAYLAND_PROTOCOLS_DIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml")
    if(EXISTS "${RELATIVE_POINTER_XML}")
        set(UOS_AI_LIBRE_WAYLAND_DRAG ON)
        set(RELATIVE_POINTER_HEADER
            "${CMAKE_CURRENT_BINARY_DIR}/relative-pointer-unstable-v1-client-protocol.h")
        set(RELATIVE_POINTER_CODE
            "${CMAKE_CURRENT_BINARY_DIR}/relative-pointer-unstable-v1-protocol.c")
        add_custom_command(
            OUTPUT "${RELATIVE_POINTER_HEADER}" "${RELATIVE_POINTER_CODE}"
            COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" client-header
                    "${RELATIVE_POINTER_XML}" "${RELATIVE_POINTER_HEADER}"
            COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" private-code
                    "${RELATIVE_POINTER_XML}" "${RELATIVE_POINTER_CODE}"
            DEPENDS "${RELATIVE_POINTER_XML}"
            VERBATIM)
        set(RELATIVE_POINTER_SOURCES
            "${RELATIVE_POINTER_HEADER}" "${RELATIVE_POINTER_CODE}")
    endif()
endif()

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
    src/dde/dde_adaptors.cpp
    src/dde/dde_adaptors.h
    src/dde/dde_payload.cpp
    src/dde/dde_payload.h
    src/dde/dde_prompt.cpp
    src/dde/dde_prompt.h
    src/dde/dde_service.cpp
    src/dde/dde_service.h
    src/iconprovider.cpp
    src/iconprovider.h
    src/markdownmodel.cpp
    src/markdownmodel.h
    src/quickaiwindow.cpp
    src/quickaiwindow.h
    src/rhibackend-selector.cpp
    src/rhibackend-selector.h
    src/singleinstance.cpp
    src/singleinstance.h
    src/windowhost.cpp
    src/windowhost.h
    src/waylanddrag.cpp
    src/waylanddrag.h
    ${RELATIVE_POINTER_SOURCES}
)

target_include_directories(uos-ai-libre PRIVATE src)
target_compile_options(uos-ai-libre PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(uos-ai-libre PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Quick Qt6::QuickControls2 Qt6::Svg Qt6::Network Qt6::DBus
    ${CMAKE_DL_LIBS}
    aish_cxxbridge)
if(UOS_AI_LIBRE_WAYLAND_DRAG)
    target_include_directories(uos-ai-libre PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}" ${WAYLAND_CLIENT_INCLUDE_DIRS})
    target_compile_definitions(uos-ai-libre PRIVATE
        UOS_AI_LIBRE_HAVE_WAYLAND_DRAG=1)
    target_link_libraries(uos-ai-libre PRIVATE ${WAYLAND_CLIENT_LIBRARIES})
endif()

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

set(UOS_AI_LIBRE_EXEC "${CMAKE_INSTALL_FULL_BINDIR}/uos-ai-libre")
configure_file(resources/dbus/com.deepin.copilot.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/com.deepin.copilot.service" @ONLY)
configure_file(resources/dbus/com.iflytek.aiassistant.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/com.iflytek.aiassistant.service" @ONLY)
configure_file(resources/autostart/uos-ai-libre-service.desktop.in
    "${CMAKE_CURRENT_BINARY_DIR}/uos-ai-libre-service.desktop" @ONLY)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/com.deepin.copilot.service"
    "${CMAKE_CURRENT_BINARY_DIR}/com.iflytek.aiassistant.service"
    DESTINATION share/dbus-1/services)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/uos-ai-libre-service.desktop"
    DESTINATION etc/xdg/autostart)
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

    add_executable(dde-payload-test
        tests/dde_payload_test.cpp
        src/dde/dde_payload.cpp
        src/dde/dde_payload.h
        src/dde/dde_prompt.cpp
        src/dde/dde_prompt.h
    )
    target_include_directories(dde-payload-test PRIVATE src)
    target_compile_options(dde-payload-test PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(dde-payload-test PRIVATE Qt6::Core Qt6::Gui)
    add_test(NAME dde-payload-test COMMAND dde-payload-test)
    set_tests_properties(dde-payload-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(dde-service-test
        tests/dde_service_test.cpp
        src/dde/dde_adaptors.cpp
        src/dde/dde_adaptors.h
        src/dde/dde_payload.cpp
        src/dde/dde_payload.h
        src/dde/dde_prompt.cpp
        src/dde/dde_prompt.h
        src/dde/dde_service.cpp
        src/dde/dde_service.h
    )
    target_include_directories(dde-service-test PRIVATE src)
    target_compile_options(dde-service-test PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(dde-service-test PRIVATE
        Qt6::Core Qt6::Gui Qt6::DBus)
    find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session)
    if(DBUS_RUN_SESSION_EXECUTABLE)
        add_test(NAME dde-service-test
            COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} --
                    $<TARGET_FILE:dde-service-test>)
    else()
        add_test(NAME dde-service-test COMMAND dde-service-test)
    endif()
    set_tests_properties(dde-service-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
