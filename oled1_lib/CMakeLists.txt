
add_library(oled1_lib
    ssd1306.c
    gfx.c
    oled1_init.c
)


target_link_libraries(oled1_lib pico_stdlib hardware_spi)


target_include_directories(oled1_lib PUBLIC
    .
)