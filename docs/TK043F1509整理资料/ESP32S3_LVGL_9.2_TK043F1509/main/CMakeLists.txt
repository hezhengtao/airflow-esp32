set(UI_DIR ./ui)
file(GLOB_RECURSE UI_SRCS ${UI_DIR}/*.c)

idf_component_register(SRCS "main.c" "myui/myui.c"
                    ${UI_SRCS}
                    INCLUDE_DIRS ""
                    "myui"
                    ${UI_DIR})
