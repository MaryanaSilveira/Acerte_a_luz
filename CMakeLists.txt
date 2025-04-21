cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")
include(pico_sdk_import.cmake)

# Define o nome do projeto como Acerte_a_luz
project(Acerte_a_luz C CXX ASM)
pico_sdk_init()

add_executable(${PROJECT_NAME}
    Acerte_a_luz.c # Código principal em C
    lib/ssd1306.c) # Biblioteca para o display OLED

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/Acerte_a_luz.pio)

target_link_libraries(${PROJECT_NAME}
        pico_stdlib
        hardware_i2c
        hardware_adc
        hardware_pio
        )

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

pico_add_extra_outputs(${PROJECT_NAME})

