### CppReactTest
add_executable(
    CppReactTest
    main.cpp
    EventStreamTest.cpp
    ObserverTest.cpp
    OperationsTest.cpp
    SignalTest.cpp
)

target_link_libraries(CppReactTest CppReact cppreact::doctest)

target_compile_options(CppReactTest PRIVATE ${CPPREACT_WARNING_OPTION})
