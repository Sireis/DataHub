set(sources "webBase.c")
set(include_directories "include")
set(required_components esp_netif
                        esp_https_server)

idf_component_register(SRCS           "${sources}"
                       INCLUDE_DIRS   "${include_directories}"
                       REQUIRES       "${required_components}"
                       EMBED_TXTFILES "certificates/cacert.pem"
                                      "certificates/prvtkey.pem"
                                      "webpage/index.html"
                                      "webpage/styles.css"
                                      "webpage/favicon.png"
                      )