set(sources "webVisualizer.c")
set(include_directories "include")
set(required_components web_base
			data_interface)

idf_component_register(SRCS           "${sources}"
                       INCLUDE_DIRS   "${include_directories}"
                       REQUIRES       "${required_components}"
                       EMBED_TXTFILES "webpage/charts.html"
                      )