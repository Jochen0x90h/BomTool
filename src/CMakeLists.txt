add_executable(${PROJECT_NAME}
    main.cpp
    kicad.cpp
    kicad.hpp
)
target_link_libraries(${PROJECT_NAME}
    libzippp::libzippp
)

# install
install(TARGETS ${PROJECT_NAME})
