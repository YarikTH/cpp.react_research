### CppReactTest
add_executable(CppReactTest
	src/algorithm_tests.cpp
	src/common_tests.cpp
	src/event_tests.cpp
	src/observer_test.cpp
	src/state_tests.cpp
	src/transaction_tests.cpp)

target_link_libraries(CppReactTest CppReact gtest gtest_main)
