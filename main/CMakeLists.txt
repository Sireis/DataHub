set(sources "main.c")
set(include_directories ".")
set(required_components bluetooth_keyboard_sender
                        physical_keyboard)

idf_component_register(SRCS         "${sources}"
                       INCLUDE_DIRS "${include_directories}"
                       REQUIRES     "${required_components}"
                      )

add_custom_target(
    build_info_generator ALL
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_LIST_DIR}/generate_build_info.cmake"
)