cmake_minimum_required(VERSION 3.21)

project(uos-ai-libre VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Quick QuickControls2 Svg Network DBus)
qt_standard_project_setup()

# Debian/Fedora expect position-independent executables. Ask CMake to verify
# that the linker supports PIE and to add -pie when the target requests it.
include(CheckPIESupported)
check_pie_supported()

# Treeland's native screenshot protocol is an optional Wayland client
# dependency. The client uses QtWaylandClient's shm buffer helper; X11 builds
# simply keep QScreen::grabWindow().
find_package(Qt6 QUIET COMPONENTS WaylandClient)
set(UOS_AI_LIBRE_TREELAND_CAPTURE OFF)
if(TARGET Qt6::WaylandClient AND TARGET Qt6::WaylandClientPrivate)
    set(UOS_AI_LIBRE_TREELAND_CAPTURE ON)
endif()
find_package(X11 QUIET)
set(UOS_AI_LIBRE_X11_SHORTCUT OFF)
if(X11_FOUND AND TARGET X11::X11)
    set(UOS_AI_LIBRE_X11_SHORTCUT ON)
endif()

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

# UOS AI's right-side dock entry is a dde-shell DAppletDock (plugin id
# org.deepin.ds.dock.aibar), not a StatusNotifierItem or a tray-loader plugin.
# Build the same kind of applet and reuse its dock id so dockOrder 26 puts it
# between the tray (25) and Show Desktop (30).
option(UOS_AI_LIBRE_DDE_SHELL_PLUGIN
    "Build the native dde-shell dock applet (org.deepin.ds.dock.aibar)" ON)
set(UOS_AI_LIBRE_DDE_SHELL_AVAILABLE OFF)
if(UOS_AI_LIBRE_DDE_SHELL_PLUGIN)
    find_package(DDEShell QUIET)
    find_package(DDEShellDock QUIET)
    if(DDEShell_FOUND AND DDEShellDock_FOUND)
        set(UOS_AI_LIBRE_DDE_SHELL_AVAILABLE ON)
        message(STATUS "DDE shell applet: org.deepin.ds.dock.aibar")
    else()
        message(STATUS "DDE shell applet: disabled (libdde-shell-dev/libdde-shell-dock-dev not found)")
    endif()
endif()

set(AISH_CXXBRIDGE_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/aish-cxxbridge"
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
        "Run ../vendor/aish-cxxbridge/build.sh first.")
endif()
if(NOT EXISTS "${AISH_CXXBRIDGE_INCLUDE}/aish-cxxbridge.h")
    message(FATAL_ERROR
        "Missing generated cxx header:\n  ${AISH_CXXBRIDGE_INCLUDE}/aish-cxxbridge.h\n"
        "Run ../vendor/aish-cxxbridge/build.sh first.")
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
    src/backend/screenshot/screenshotbackend.h
    src/backend/screenshot/screenshotservice.cpp
    src/backend/screenshot/screenshotservice.h
    src/backend/screenshot/treelandscreenshotbackend.cpp
    src/backend/screenshot/treelandscreenshotbackend.h
    src/backend/screenshot/x11screenshotbackend.cpp
    src/backend/screenshot/x11screenshotbackend.h
    src/backend/shortcuts/ddeshortcutbackend.cpp
    src/backend/shortcuts/ddeshortcutbackend.h
    src/backend/shortcuts/shortcutbackend.h
    src/backend/shortcuts/shortcutservice.cpp
    src/backend/shortcuts/shortcutservice.h
    src/backend/shortcuts/x11shortcutbackend.cpp
    src/backend/shortcuts/x11shortcutbackend.h
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
    src/followalong/followalongcontroller.cpp
    src/followalong/followalongcontroller.h
    src/followalong/followalongeventbus.cpp
    src/followalong/followalongeventbus.h
    src/followalong/followalongeventsource.cpp
    src/followalong/followalongeventsource.h
    src/followalong/followalongservice.cpp
    src/followalong/followalongservice.h
    src/followalong/followalongtypes.h
    src/followalong/followalongview.cpp
    src/followalong/followalongview.h
    src/followalong/followalongxwayland.cpp
    src/followalong/followalongxwayland.h
    src/followalong/x11followalongeventsource.cpp
    src/followalong/x11followalongeventsource.h
    src/followalong/x11followalongsurface.cpp
    src/followalong/x11followalongsurface.h
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
if(UOS_AI_LIBRE_TREELAND_CAPTURE)
    qt6_generate_wayland_protocol_client_sources(uos-ai-libre
        FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/protocols/treeland-capture-unstable-v1.xml")
    target_compile_definitions(uos-ai-libre PRIVATE
        UOS_AI_LIBRE_HAVE_TREELAND_CAPTURE=1)
    target_link_libraries(uos-ai-libre PRIVATE
        Qt6::WaylandClient Qt6::WaylandClientPrivate)

    # FollowAlong uses two Wayland clients for input:
    #  - wlr-data-control: global primary-selection monitoring
    #  - treeland-active-notify: global left-button press/release activity
    # Presentation uses wlr-layer-shell through dde-shell's DLayerShellWindow
    # QML binding, so no private role protocol is generated here.
    target_sources(uos-ai-libre PRIVATE
        src/followalong/treelandfollowalongeventsource.cpp
        src/followalong/treelandfollowalongeventsource.h
        src/followalong/treelandfollowalongsurface.cpp
        src/followalong/treelandfollowalongsurface.h
    )
    qt6_generate_wayland_protocol_client_sources(uos-ai-libre
        FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/protocols/treeland-active-notify-unstable-v1.xml"
            "${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-data-control-unstable-v1.xml")
    target_include_directories(uos-ai-libre PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}" ${WAYLAND_CLIENT_INCLUDE_DIRS})
    target_compile_definitions(uos-ai-libre PRIVATE
        UOS_AI_LIBRE_FOLLOWALONG_WAYLAND=1)
    target_link_libraries(uos-ai-libre PRIVATE
        Qt6::WaylandClientPrivate ${WAYLAND_CLIENT_LIBRARIES})
endif()
if(UOS_AI_LIBRE_X11_SHORTCUT)
    target_compile_definitions(uos-ai-libre PRIVATE
        UOS_AI_LIBRE_HAVE_X11_SHORTCUT=1)
    target_link_libraries(uos-ai-libre PRIVATE X11::X11)
    if(X11_XTest_FOUND)
        target_compile_definitions(uos-ai-libre PRIVATE
            UOS_AI_LIBRE_HAVE_X11_RECORD=1)
        target_link_libraries(uos-ai-libre PRIVATE ${X11_XTest_LIB})
    endif()
endif()

qt_add_resources(uos-ai-libre "app_assets"
    PREFIX "/"
    BASE "resources"
    FILES
        ${ICON_FILES}
        resources/images/UosAiLibre.svg
)

qt_add_qml_module(uos-ai-libre
    URI UosAiLibre
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES ${QML_SOURCES}
)

set_target_properties(uos-ai-libre PROPERTIES
    OUTPUT_NAME "uos-ai-libre"
    POSITION_INDEPENDENT_CODE ON
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
# XDG system autostart is /etc/xdg/autostart, not ${prefix}/etc. Keep this
# absolute so a /usr-prefixed package and a /usr/local dev install both work.
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/uos-ai-libre-service.desktop"
    DESTINATION /etc/xdg/autostart)
install(FILES resources/images/UosAiLibre.svg
    DESTINATION share/icons/hicolor/scalable/apps
    RENAME uos-ai-libre.svg)

if(UOS_AI_LIBRE_DDE_SHELL_AVAILABLE)
    add_library(uos-ai-libre-aibar SHARED
        src/dde/aibar/aibar.cpp
        src/dde/aibar/aibar.h
    )
    target_link_libraries(uos-ai-libre-aibar PRIVATE
        Dde::Shell Dde::ShellDock Qt6::DBus)
    target_compile_definitions(uos-ai-libre-aibar PRIVATE QT_PLUGIN)
    ds_install_package(
        PACKAGE org.deepin.ds.dock.aibar
        TARGET uos-ai-libre-aibar
        PACKAGE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/dde/aibar/package")
endif()

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

    add_executable(followalong-contract-test
        tests/followalong_contract_test.cpp
        src/followalong/followalongcontroller.cpp
        src/followalong/followalongcontroller.h
        src/followalong/followalongeventbus.cpp
        src/followalong/followalongeventbus.h
        src/followalong/followalongtypes.h
        src/followalong/followalongview.cpp
        src/followalong/followalongview.h
        src/followalong/x11followalongsurface.cpp
        src/followalong/x11followalongsurface.h
    )
    target_include_directories(followalong-contract-test PRIVATE
        src src/followalong)
    target_compile_options(followalong-contract-test PRIVATE
        -Wall -Wextra -Wpedantic)
    target_link_libraries(followalong-contract-test PRIVATE
        Qt6::Core Qt6::Gui Qt6::Quick)
    if(X11_FOUND AND TARGET X11::X11)
        target_link_libraries(followalong-contract-test PRIVATE X11::X11)
        target_compile_definitions(followalong-contract-test PRIVATE
            UOS_AI_LIBRE_HAVE_X11_SHORTCUT=1)
    endif()
    add_test(NAME followalong-contract-test
             COMMAND followalong-contract-test)
    set_tests_properties(followalong-contract-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

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
