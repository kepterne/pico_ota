cmake_minimum_required(VERSION 3.13)

set(ENV{PICO_BOARD} pico_w)

set(ENV{VERSION_MAJOR} 1)
set(ENV{VERSION_MINOR} 0)
set(ENV{PROJECT} pico_ota)

#include(pico_sdk_import.cmake)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Define project
project($ENV{PROJECT})
pico_sdk_init()
add_definitions( -DVERSION_MAJOR=$ENV{VERSION_MAJOR} )
add_definitions( -DVERSION_MINOR=$ENV{VERSION_MINOR} )

add_executable($ENV{PROJECT} main.c common.c analog_reader.c lcd_display.c wifi.c md5.c)

#
target_include_directories($ENV{PROJECT} PRIVATE
	${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries($ENV{PROJECT} hardware_adc)
target_link_libraries($ENV{PROJECT} pico_stdlib)
target_link_libraries($ENV{PROJECT} pico_cyw43_arch_lwip_threadsafe_background)
target_link_libraries($ENV{PROJECT} pico_multicore)
target_link_libraries($ENV{PROJECT} hardware_dma)
target_link_libraries($ENV{PROJECT} hardware_i2c)

pico_enable_stdio_usb($ENV{PROJECT} 1)
pico_enable_stdio_uart($ENV{PROJECT} 1)

pico_add_extra_outputs($ENV{PROJECT})

add_custom_command(TARGET $ENV{PROJECT}
	POST_BUILD
	COMMAND ../stamp.sh $ENV{PROJECT} $ENV{VERSION_MAJOR} $ENV{VERSION_MINOR}
)