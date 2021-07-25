### CppReactTest
add_executable(
    CppReactTest
    EventStreamTest.cpp
    ObserverTest.cpp
    OperationsTest.cpp
    SignalTest.cpp
)

target_link_libraries(CppReactTest CppReact gtest gtest_main cppreact::doctest)
