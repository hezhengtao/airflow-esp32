SET(SOURCES screens/ui_Screen1.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    images/ui_img_dc1_png.c
    images/ui_img_dc2_png.c
    images/ui_img_dc3_png.c
    images/ui_img_mv1_png.c
    images/ui_img_mv2_png.c
    images/ui_img_kw1_png.c
    images/ui_img_km2_png.c
    images/ui_img_km3_png.c
    images/ui_img_km4_png.c
    images/ui_img_kuan1_png.c
    images/ui_img_kuan2_png.c
    fonts/ui_font_Be20.c
    fonts/ui_font_Be30.c)

add_library(ui ${SOURCES})
