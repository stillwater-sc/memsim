include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
)
FetchContent_MakeAvailable(Catch2)

# Unit tests
add_executable(memsim_tests
    unit/test_types.cpp
)

target_link_libraries(memsim_tests PRIVATE
    memsim
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(memsim_tests)
