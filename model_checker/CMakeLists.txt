add_library(
  model_checker
  async.cc
  work_queue.cc
)

target_include_directories(
  model_checker
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)

add_executable(
  model_checker_test
  async_test.cc
  work_queue_test.cc
)
target_link_libraries(
  model_checker_test
  model_checker
  GTest::gtest_main
)

target_include_directories(
  model_checker_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)

include(GoogleTest)
gtest_discover_tests(model_checker_test)