cmake_minimum_required(VERSION 3.16)

project(RedAnchor VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 DBus Network Concurrent Multimedia WebEngineQuick ShaderTools)

qt_standard_project_setup(REQUIRES 6.10)

# Content of downloaded design from Figma to Qt or similar
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()

qt_add_executable(appRedAnchor
    src/main.cpp
)

set(QML_SOURCES
    qml/Main.qml
    qml/Theme.qml
    qml/Keyboard.qml
    qml/components/Icon.qml
    qml/components/AppBackground.qml
    qml/components/FocusRing.qml
    qml/components/GlassPanel.qml
    qml/components/Badge.qml
    qml/components/ProgressBar.qml
    qml/components/ButtonGlyph.qml
    qml/components/ConsoleButton.qml
    qml/components/ConsoleListItem.qml
    qml/components/ConsoleTextField.qml
    qml/components/DualSense.qml
    qml/components/HintBar.qml
    qml/components/InstructionStep.qml
    qml/components/OnScreenKeyboard.qml
    qml/components/SetupPage.qml
    qml/components/SignalBars.qml
    qml/components/Spinner.qml
    qml/components/StepIndicator.qml
    qml/components/UsbCable.qml
    qml/components/Avatar.qml
    qml/components/EmptyState.qml
    qml/components/ToggleRow.qml
    qml/components/InfoRow.qml
    qml/components/ActionMenu.qml
    qml/components/NetworkPanel.qml
    qml/components/ModelSync.js
    qml/components/Toast.qml
    qml/components/GlyphText.qml
    qml/components/ComponentRow.qml
    qml/components/GameCover.qml
    qml/components/RoundedClip.qml
    qml/components/Compat.js
    qml/components/GameLaunchOverlay.qml
    qml/components/SystemOverlay.qml
    qml/components/Format.js
    qml/setup/ControllerScreen.qml
    qml/setup/IntroScreen.qml
    qml/setup/LanguageScreen.qml
    qml/setup/NetworkScreen.qml
    qml/setup/UpdatesScreen.qml
    qml/setup/AccountScreen.qml
    qml/setup/WelcomeScreen.qml
    qml/home/HomeScreen.qml
    qml/home/UserSelectScreen.qml
    qml/home/NewControllerScreen.qml
    qml/home/GamesView.qml
    qml/home/LibraryView.qml
    qml/home/StoreView.qml
    qml/home/StorePanel.qml
    qml/home/WebLoginWindow.qml
    qml/home/SettingsView.qml
    qml/home/settings/NetworkSettings.qml
    qml/home/settings/BluetoothSettings.qml
    qml/home/settings/UsersSettings.qml
    qml/home/settings/UpdatesSettings.qml
    qml/home/settings/StorageSettings.qml
    qml/home/settings/GamepadSettings.qml
    qml/home/settings/AboutSettings.qml
)

# Все QML-файлы кладём в корень модуля, чтобы типы были видны друг другу
# без дополнительных import'ов независимо от подкаталога.
foreach(qml_file IN LISTS QML_SOURCES)
    get_filename_component(qml_name ${qml_file} NAME)
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_name})
endforeach()
set_source_files_properties(qml/Theme.qml qml/Keyboard.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(appRedAnchor
    URI RedAnchor
    QML_FILES ${QML_SOURCES}
    SOURCES
        src/dbustypes.h src/dbustypes.cpp
        src/gamepadmanager.h src/gamepadmanager.cpp
        src/virtualpad.h src/virtualpad.cpp
        src/systemcontrol.h src/systemcontrol.cpp
        src/bluetoothmanager.h src/bluetoothmanager.cpp
        src/networkbackend.h src/networkbackend.cpp
        src/accountmanager.h src/accountmanager.cpp
        src/systeminfo.h src/systeminfo.cpp
        src/componentsmanager.h src/componentsmanager.cpp
        src/gamelibrary.h src/gamelibrary.cpp
        src/gameart.h src/gameart.cpp
        src/storemanager.h src/storemanager.cpp
        src/gamelauncher.h src/gamelauncher.cpp
)

include(GNUInstallDirs)

# Ключ API SteamGridDB для обложек игр (пустой — обложки не загружаются)
set(REDANCHOR_STEAMGRIDDB_KEY "7d37689e46a4217b65ba817ffa295d7a" CACHE STRING "Ключ API SteamGridDB")

# Помощник сопряжения DualSense (запускается от root через pkexec)
add_executable(redanchor-pair-controller src/pairhelper/main.cpp)
set(PAIR_HELPER_PATH "${CMAKE_INSTALL_FULL_LIBEXECDIR}/redanchor-pair-controller")

# Помощник установки компонентов (apt, от root через pkexec); для запуска из каталога
# сборки копируем его рядом с исполняемым файлом
set(COMPONENTS_HELPER_PATH "${CMAKE_INSTALL_FULL_LIBEXECDIR}/redanchor-components")
file(COPY data/redanchor-components DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
     FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
configure_file(data/org.redanchor.shell.policy.in org.redanchor.shell.policy @ONLY)

# Шейдеры компилируются в .qsb при сборке и кладутся рядом с QML-файлами модуля
qt_add_shaders(appRedAnchor "redanchor_shaders"
    PREFIX "/qt/qml/RedAnchor"
    FILES shaders/aurora.frag
)

target_include_directories(appRedAnchor PRIVATE src)
target_compile_definitions(appRedAnchor PRIVATE
    PROJECT_VERSION_STRING="${PROJECT_VERSION}"
    REDANCHOR_PAIR_HELPER="${PAIR_HELPER_PATH}"
    REDANCHOR_COMPONENTS_HELPER="${COMPONENTS_HELPER_PATH}"
    REDANCHOR_STEAMGRIDDB_KEY="${REDANCHOR_STEAMGRIDDB_KEY}"
)
add_dependencies(appRedAnchor redanchor-pair-controller)

set_target_properties(appRedAnchor PROPERTIES
    OUTPUT_NAME redanchor-shell
    WIN32_EXECUTABLE TRUE
)
# Qt из установщика (~/Qt/<версия>) не лежит в системных каталогах: без этого установленный
# redanchor-shell не находит библиотеки Qt и сессия с экрана входа сразу завершается.
# (make install по умолчанию убирает путь к библиотекам, который есть у файла в каталоге сборки.)
if(QT6_INSTALL_PREFIX AND NOT QT6_INSTALL_PREFIX MATCHES "^/usr/?$")
    set_target_properties(appRedAnchor PROPERTIES INSTALL_RPATH "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_LIBS}")
endif()

target_link_libraries(appRedAnchor
    PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::DBus Qt6::Network Qt6::Concurrent Qt6::Multimedia Qt6::WebEngineQuick
)

install(TARGETS appRedAnchor
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(TARGETS redanchor-pair-controller RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
install(PROGRAMS data/redanchor-components DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
# polkit и udev читают правила только из системных каталогов, поэтому путь не зависит от префикса
set(REDANCHOR_POLKIT_ACTIONS_DIR "/usr/share/polkit-1/actions" CACHE PATH "Каталог действий polkit")
set(REDANCHOR_UDEV_RULES_DIR "/usr/lib/udev/rules.d" CACHE PATH "Каталог правил udev")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.redanchor.shell.policy DESTINATION ${REDANCHOR_POLKIT_ACTIONS_DIR})
install(FILES data/70-redanchor-dualsense.rules DESTINATION ${REDANCHOR_UDEV_RULES_DIR})
install(PROGRAMS session/redanchor-session DESTINATION ${CMAKE_INSTALL_BINDIR})
# Экран входа (GDM, SDDM) ищет сессии в /usr/share/wayland-sessions — путь не зависит от префикса,
# а в файле сессии — полные пути: PATH у экрана входа может не содержать /usr/local/bin
set(REDANCHOR_SESSIONS_DIR "/usr/share/wayland-sessions" CACHE PATH "Каталог сессий экрана входа")
set(SESSION_BIN "${CMAKE_INSTALL_FULL_BINDIR}/redanchor-session")
configure_file(session/redanchor.desktop.in redanchor.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/redanchor.desktop DESTINATION ${REDANCHOR_SESSIONS_DIR})
