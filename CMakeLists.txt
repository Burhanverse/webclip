cmake_minimum_required(VERSION 3.16)
project(webclip VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Quick Qml QuickControls2 Svg Widgets)

if(COMMAND qt_policy)
    qt_policy(SET QTP0004 OLD)
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/models
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/controllers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/effects
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/theme
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/util
)

qt_add_qml_module(webclip
    URI WebClip
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    NO_RESOURCE_TARGET_PATH
    QML_FILES
        src/gui/qml/Main.qml
        src/gui/qml/components/MD3Icon.qml
        src/gui/qml/components/MD3Button.qml
        src/gui/qml/components/MD3IconButton.qml
        src/gui/qml/components/MD3Card.qml
        src/gui/qml/components/MD3Badge.qml
        src/gui/qml/components/MD3TextField.qml
        src/gui/qml/components/MD3Switch.qml
        src/gui/qml/components/MD3ColorPickerDialog.qml
        src/gui/qml/components/MD3SettingsDialog.qml
        src/gui/qml/components/MD3SmoothScroll.qml
        src/gui/qml/pages/ClipsPage.qml
    RESOURCES
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
    SOURCES
        src/gui/theme/md3_theme.hpp
        src/gui/theme/md3_theme.cpp
        src/gui/models/clipboard_history_model.hpp
        src/gui/models/clipboard_history_model.cpp
        src/gui/controllers/webclip_controller.hpp
        src/gui/controllers/webclip_controller.cpp
        src/gui/effects/thanos_effect_item.hpp
        src/gui/effects/thanos_effect_item.cpp
        src/gui/util/icon_image_provider.hpp
        src/gui/util/icon_image_provider.cpp
        src/gui/util/i18n.hpp
        src/gui/util/i18n.cpp
        src/gui/util/tray_icon_manager.hpp
        src/gui/util/tray_icon_manager.cpp
        src/gui/util/style_core_font.hpp
        src/gui/util/style_core_font.cpp
        src/gui/util/md3_kinetic_scroller.hpp
        src/gui/util/md3_kinetic_scroller.cpp
)

target_link_libraries(webclip PRIVATE
    webclip_core
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
    Qt6::Svg
    Qt6::Widgets
)

if(MSVC)
    target_compile_options(webclip_core PRIVATE /W4 /O2 /Oi /Ot /Oy /Gy /GF /permissive-)
    target_compile_options(webclip PRIVATE /W4 /O2 /Oi /Ot /Oy /Gy /GF /permissive-)
    target_link_options(webclip PRIVATE /OPT:REF /OPT:ICF)
else()
    target_compile_options(webclip_core PRIVATE -Wall -Wextra -Wpedantic -O3)
    target_compile_options(webclip PRIVATE -Wall -Wextra -Wpedantic -O3)
endif()

install(TARGETS webclip RUNTIME DESTINATION bin)
