cmake_minimum_required(VERSION 3.20)

project(project0904 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# This CMake file is for CLion indexing/navigation. Keil uVision remains the
# authoritative build and flashing flow for the NUC140VE3AN target.

set(CMSIS_SOURCES
    CMSIS/CM0/CoreSupport/core_cm0.c
    CMSIS/CM0/DeviceSupport/Nuvoton/NUC1xx/system_NUC1xx.c
)

set(DRIVER_SOURCES
    Dependencies/Src/Driver/DrvADC.c
    Dependencies/Src/Driver/DrvCAN.c
    Dependencies/Src/Driver/DrvEBI.c
    Dependencies/Src/Driver/DrvFMC.c
    Dependencies/Src/Driver/DrvGPIO.c
    Dependencies/Src/Driver/DrvI2C.c
    Dependencies/Src/Driver/DrvI2S.c
    Dependencies/Src/Driver/DrvPDMA.c
    Dependencies/Src/Driver/DrvPS2.c
    Dependencies/Src/Driver/DrvPWM.c
    Dependencies/Src/Driver/DrvRTC.c
    Dependencies/Src/Driver/DrvSPI.c
    Dependencies/Src/Driver/DrvSYS.c
    Dependencies/Src/Driver/DrvTIMER.c
    Dependencies/Src/Driver/DrvUART.c
    Dependencies/Src/Driver/retarget.c
)

set(LB002_SOURCES
    Dependencies/Src/NUC1xx-LB_002/Ascii_Table.C
    Dependencies/Src/NUC1xx-LB_002/DrvSDCard.c
    Dependencies/Src/NUC1xx-LB_002/EEPROM_24LC64.c
    Dependencies/Src/NUC1xx-LB_002/NAU7802.c
    Dependencies/Src/NUC1xx-LB_002/Seven_Segment.c
)

set(BOARD_SUPPORT_SOURCES
        BoardSupport/AD9850_func.c
        BoardSupport/adc_sample.c
        BoardSupport/LCD_Driver_Modified.c
        BoardSupport/Scan_6Key.c
)

set(APP_SOURCES
    COSTASProject/app_state.c
        COSTASProject/costas_loop.c
        COSTASProject/main_source.c
        COSTASProject/UIFUNC0728.c
)

set(PROJECT_HEADERS
        BoardSupport/AD9850_func.h
        BoardSupport/adc_sample.h
        BoardSupport/LCD_Driver_Modified.h
        BoardSupport/Scan_6Key.h
    COSTASProject/app_state.h
    COSTASProject/costas_loop.h
        COSTASProject/UIFUNC0728.h
)

add_executable(project0904_index
    ${CMSIS_SOURCES}
    ${DRIVER_SOURCES}
    ${LB002_SOURCES}
    ${BOARD_SUPPORT_SOURCES}
    ${APP_SOURCES}
    ${PROJECT_HEADERS}
)

target_include_directories(project0904_index PRIVATE
    .
    CMSIS/CM0/CoreSupport
    CMSIS/CM0/DeviceSupport/Nuvoton/NUC1xx
    Dependencies/Include
    Dependencies/Include/Driver
    Dependencies/Include/NUC1xx-LB_002
    Dependencies/Include/System
    Dependencies/Src/Driver
    Dependencies/Src/NUC1xx-LB_002
    BoardSupport
    COSTASProject
)

foreach(KEIL_INCLUDE_DIR
    "D:/Keil_v5/ARM/RV31/Inc"
    "D:/Keil_v5/ARM/CMSIS/Include"
    "D:/Keil_v5/ARM/Inc/Nuvoton/NUC1xx"
)
    if(EXISTS "${KEIL_INCLUDE_DIR}")
        target_include_directories(project0904_index SYSTEM PRIVATE "${KEIL_INCLUDE_DIR}")
    endif()
endforeach()

target_compile_definitions(project0904_index PRIVATE
    NUC140VE3AN
)

source_group("CMSIS" FILES ${CMSIS_SOURCES})
source_group("Drivers" FILES ${DRIVER_SOURCES})
source_group("NUC1xx-LB_002" FILES ${LB002_SOURCES})
source_group("BoardSupport" FILES ${BOARD_SUPPORT_SOURCES})
source_group("COSTASProject" FILES ${APP_SOURCES} ${PROJECT_HEADERS})
