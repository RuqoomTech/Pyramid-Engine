add_executable(PublicApiLinkage PublicApiLinkage.cpp)
target_link_libraries(PublicApiLinkage PRIVATE Pyramid::Engine)
set_target_properties(PublicApiLinkage PROPERTIES FOLDER "Tests/API")
add_test(NAME API.PublicApiLinkage COMMAND $<TARGET_FILE:PublicApiLinkage>)
