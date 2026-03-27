add_executable(${PROJECT_NAME}
    main.cpp
    kicad.cpp
    kicad.hpp
)
target_link_libraries(${PROJECT_NAME}
    libzippp::libzippp
    nlohmann_json::nlohmann_json
)

# install
install(TARGETS ${PROJECT_NAME})
