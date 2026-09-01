cmake_minimum_required(VERSION 3.22)
project(webclip VERSION 1.7.0 LANGUAGES CXX)

option(USE_LEGACY_TOOLCHAIN "Use legacy toolchain (GCC on Linux, MSVC cl.exe on Windows)" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Svg Widgets)

if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

if(COMMAND qt_standard_project_setup)
    qt_standard_project_setup(REQUIRES 6.5)
endif()

add_library(webclip_core STATIC
    src/clipboard/clipboard.hpp
    src/clipboard/clipboard.cpp
    src/net/http_client.hpp
    src/net/http_client.cpp
    src/net/sse_parser.hpp
    src/net/sse_parser.cpp
    src/sync/sync_manager.hpp
    src/sync/sync_manager.cpp
    src/util/json.hpp
    src/util/cli.hpp
)

target_include_directories(webclip_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CURL_INCLUDE_DIRS}
)

target_link_libraries(webclip_core PUBLIC
    Threads::Threads
    ${CURL_LIBRARIES}
)

if(WIN32)
    target_link_libraries(webclip_core PUBLIC
        ws2_32
        user32
        psapi
        gdi32
        gdiplus
        ole32
    )
endif()

set(WINDOWS_RESOURCE "")
if(WIN32)
    set(WINDOWS_RESOURCE src/gui/resources/webclip.rc)
endif()

add_executable(webclip src/gui/main_gui.cpp ${WINDOWS_RESOURCE})

if(WIN32)
    set_target_properties(webclip PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_include_directories(webclip PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/ui/basic
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/ui/md3
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/ui/chrome
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/ui/dialogs
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/ui/gallery
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/ui/clips
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/models
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/controllers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/clips
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/effects
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/theme
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/util
)

qt_add_resources(webclip "webclip_resources"
    PREFIX "/qt/qml"
    FILES
        src/gui/resources/fonts/GoogleSansFlexRegular.ttf
        src/gui/resources/fonts/GoogleSansFlexMedium.ttf
        src/gui/resources/fonts/GoogleSansItalic.ttf
        src/gui/resources/fonts/GoogleSansMediumItalic.ttf
        src/gui/resources/fonts/Vazirmatn-UI-NL-Regular.ttf
        src/gui/resources/fonts/Vazirmatn-UI-NL-SemiBold.ttf
        src/gui/resources/fonts/Twemoji.ttf
        src/gui/resources/langs/en.json
        src/gui/resources/icons/webclip.svg
        src/gui/resources/icons/webclip.ico
        src/gui/resources/icons/android.svg
        src/gui/resources/icons/attach_file.svg
        src/gui/resources/icons/check.svg
        src/gui/resources/icons/clear_all.svg
        src/gui/resources/icons/clips.svg
        src/gui/resources/icons/close.svg
        src/gui/resources/icons/content_copy.svg
        src/gui/resources/icons/content_paste.svg
        src/gui/resources/icons/copy.svg
        src/gui/resources/icons/dark_mode.svg
        src/gui/resources/icons/delete.svg
        src/gui/resources/icons/download.svg
        src/gui/resources/icons/edit.svg
        src/gui/resources/icons/image.svg
        src/gui/resources/icons/laptop.svg
        src/gui/resources/icons/light_mode.svg
        src/gui/resources/icons/link.svg
        src/gui/resources/icons/link_off.svg
        src/gui/resources/icons/moon.svg
        src/gui/resources/icons/palette.svg
        src/gui/resources/icons/paste.svg
        src/gui/resources/icons/phone.svg
        src/gui/resources/icons/send.svg
        src/gui/resources/icons/settings.svg
        src/gui/resources/icons/smartphone.svg
        src/gui/resources/icons/sun.svg
        src/gui/resources/icons/sync.svg
        src/gui/resources/icons/view_agenda.svg
)

target_sources(webclip PRIVATE
    src/gui/theme/md3_theme.hpp
    src/gui/theme/md3_theme.cpp
    src/gui/models/clipboard_history_model.hpp
    src/gui/models/clipboard_history_model.cpp
    src/gui/controllers/webclip_controller.hpp
    src/gui/controllers/webclip_controller.cpp
    src/gui/clips/bubble_corners.hpp
    src/gui/clips/bubble_corners.cpp
    src/gui/clips/clip_text_layout.hpp
    src/gui/clips/clip_text_layout.cpp
    src/gui/clips/clip_element.hpp
    src/gui/clips/clip_element.cpp
    src/gui/util/i18n.hpp
    src/gui/util/i18n.cpp
    src/gui/util/tray_icon_manager.hpp
    src/gui/util/tray_icon_manager.cpp
    src/gui/util/style_core_font.hpp
    src/gui/util/style_core_font.cpp
    src/gui/ui/basic/rpl_lite.hpp
    src/gui/ui/basic/animation.hpp
    src/gui/ui/basic/animation.cpp
    src/gui/ui/basic/painter_helpers.hpp
    src/gui/ui/basic/rp_widget.hpp
    src/gui/ui/basic/rp_widget.cpp
    src/gui/ui/basic/ripple_animation.hpp
    src/gui/ui/basic/ripple_animation.cpp
    src/gui/ui/basic/ripple_button.hpp
    src/gui/ui/basic/ripple_button.cpp
    src/gui/ui/md3/icon_loader.hpp
    src/gui/ui/md3/icon_loader.cpp
    src/gui/ui/md3/md3_switch.hpp
    src/gui/ui/md3/md3_switch.cpp
    src/gui/ui/md3/md3_button.hpp
    src/gui/ui/md3/md3_button.cpp
    src/gui/ui/md3/md3_icon_button.hpp
    src/gui/ui/md3/md3_icon_button.cpp
    src/gui/ui/md3/md3_badge.hpp
    src/gui/ui/md3/md3_badge.cpp
    src/gui/ui/md3/md3_text_field.hpp
    src/gui/ui/md3/md3_text_field.cpp
    src/gui/ui/md3/md3_card.hpp
    src/gui/ui/md3/md3_card.cpp
    src/gui/ui/md3/md3_slider.hpp
    src/gui/ui/md3/md3_slider.cpp
    src/gui/ui/chrome/header_bar.hpp
    src/gui/ui/chrome/header_bar.cpp
    src/gui/ui/chrome/input_dock.hpp
    src/gui/ui/chrome/input_dock.cpp
    src/gui/ui/chrome/toast_widget.hpp
    src/gui/ui/chrome/toast_widget.cpp
    src/gui/ui/chrome/popup_menu.hpp
    src/gui/ui/chrome/popup_menu.cpp
    src/gui/ui/dialogs/image_preview_modal.hpp
    src/gui/ui/dialogs/image_preview_modal.cpp
    src/gui/ui/dialogs/color_picker_dialog.hpp
    src/gui/ui/dialogs/color_picker_dialog.cpp
    src/gui/ui/dialogs/settings_dialog.hpp
    src/gui/ui/dialogs/settings_dialog.cpp
    src/gui/ui/clips/clip_widget.hpp
    src/gui/ui/clips/clip_widget.cpp
    src/gui/ui/main_window.hpp
    src/gui/ui/main_window.cpp
)

target_link_libraries(webclip PRIVATE
    webclip_core
    Qt6::Core
    Qt6::Gui
    Qt6::Svg
    Qt6::Widgets
)

if(MSVC)
    target_compile_definitions(webclip_core PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(webclip PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    target_compile_options(webclip_core PRIVATE /W4 /O2 /Oi /Ot /Oy /Gy /GF /permissive-)
    target_compile_options(webclip PRIVATE /W4 /O2 /Oi /Ot /Oy /Gy /GF /permissive-)
    target_link_options(webclip PRIVATE /OPT:REF /OPT:ICF)
    if(NOT USE_LEGACY_TOOLCHAIN AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_link_options(webclip PRIVATE "-fuse-ld=lld-link")
    endif()
else()
    target_compile_options(webclip_core PRIVATE -Wall -Wextra -Wpedantic -O3)
    target_compile_options(webclip PRIVATE -Wall -Wextra -Wpedantic -O3)
    if(NOT USE_LEGACY_TOOLCHAIN AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_link_options(webclip PRIVATE "-fuse-ld=lld")
    endif()
endif()

include(GNUInstallDirs)

install(TARGETS webclip
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(UNIX AND NOT APPLE)
    install(FILES packaging/io.github.burhanverse.webclip.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
    install(FILES packaging/io.github.burhanverse.webclip.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
    )
    install(FILES src/gui/resources/icons/webclip.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
    )
    install(FILES src/gui/resources/icons/webclip.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps
    )
endif()

# CPack packaging configuration
set(CPACK_PACKAGE_NAME "webclip")
set(CPACK_PACKAGE_VENDOR "Burhanverse")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Real-Time Gboard Clipboard Sync")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
include(CPack)

