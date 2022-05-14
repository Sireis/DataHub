set(sources "data_interface.c")
set(include_directories "include")
set(required_components)

idf_component_register(SRCS         "${sources}"
                       INCLUDE_DIRS "${include_directories}"
                       REQUIRES     "${required_components}"
                      )
