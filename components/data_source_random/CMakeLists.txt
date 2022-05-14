set(sources "dataSourceRandom.c")
set(include_directories "include")
set(required_components data_interface
			esp_timer)

idf_component_register(SRCS           "${sources}"
                       INCLUDE_DIRS   "${include_directories}"
                       REQUIRES       "${required_components}"
                      )