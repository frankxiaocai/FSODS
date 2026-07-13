cmake_minimum_required(VERSION 3.19)
project(FSODS LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network SerialBus Concurrent)

qt_standard_project_setup()

# ==============================
#  海康 SDK 路径
# ==============================
set(HIK_SDK_DIR ${CMAKE_SOURCE_DIR}/ThirdParty/HIKSDK)
# ==============================
#  高光谱 SDK 路径
# ==============================
set(LUMO_SDK_DIR ${CMAKE_SOURCE_DIR}/ThirdParty/Lumo_Sensor_SDK)
# ==============================
#  高光谱 胡杏算法 依赖onnxruntime 路径
# ==============================
set(ONNXRUNTIME_ROOT ${CMAKE_SOURCE_DIR}/ThirdParty/onnxruntime-win-x64-1.10.0)
# OpenCV配置
# ==============================

# ==============================

qt_add_executable(FSODS
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    Devices/hikcamera.cpp Devices/hikcamera.h Devices/hyperspectralcamera.cpp Devices/hyperspectralcamera.h Devices/larmanmodbustcp.cpp Devices/larmanmodbustcp.h Devices/plccontroller.cpp Devices/plccontroller.h
    CoreTools/fileio.cpp CoreTools/fileio.h CoreTools/logger.cpp CoreTools/logger.h CoreTools/mystruct.h CoreTools/RamanPlasticRecognizer.cpp CoreTools/RamanPlasticRecognizer.h
    CoreTools/HSIPlasticRecognizer/HSIProcessor.cpp CoreTools/HSIPlasticRecognizer/HSIProcessor.h CoreTools/HSIPlasticRecognizer/ModelWrapper.cpp CoreTools/HSIPlasticRecognizer/ModelWrapper.h
    devicemanager.cpp devicemanager.h
)

# ==============================
#  头文件路径
# ==============================
target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${HIK_SDK_DIR}/Includes
    ${LUMO_SDK_DIR}/include
    ${ONNXRUNTIME_ROOT}/include
)

# ==============================
#  链接库路径
# ==============================
target_link_directories(${PROJECT_NAME}
    PRIVATE
    ${HIK_SDK_DIR}/Libraries/win64
    ${LUMO_SDK_DIR}/lib
    ${ONNXRUNTIME_ROOT}/lib
)

target_link_libraries(FSODS
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
        Qt::SerialBus
        Qt::Concurrent
        MvCameraControl
        SpecSensor
        onnxruntime
)

install(TARGETS FSODS
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET FSODS
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
