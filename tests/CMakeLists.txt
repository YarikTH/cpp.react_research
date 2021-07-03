### CppReactTest
add_executable(
    CppReactTest
    src/EventStreamTest.cpp
    src/MoveTest.cpp
    src/ObserverTest.cpp
    src/OperationsTest.cpp
    src/SignalTest.cpp
)

target_link_libraries(CppReactTest CppReact gtest gtest_main)
