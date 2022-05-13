set(sources "webFlash.c")
set(include_directories "include")
set(required_components web_base
                        esp_https_ota
                        )

idf_component_register(SRCS           "${sources}"
                       INCLUDE_DIRS   "${include_directories}"
                       REQUIRES       "${required_components}"
                       EMBED_TXTFILES "webpage/flash.html"
                                      "certificates/otacacert.pem"
                                      "certificates/otaprvtkey.pem"
                      )