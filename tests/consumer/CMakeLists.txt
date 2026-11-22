# A project that uses an installed sh2o through find_package (make install-check).
# Copyright (c) 2026 sh2o Contributors. MIT License.
cmake_minimum_required(VERSION 3.21)
project(sh2o_consumer LANGUAGES C CXX)
find_package(sh2o 0.1 CONFIG REQUIRED)
add_executable(sh2o_consumer ${CMAKE_CURRENT_LIST_DIR}/../../examples/fixed_basics.c)
target_link_libraries(sh2o_consumer PRIVATE sh2o::sh2o)
# The inline kernels of sh2o/k.h, compiled with the strict warnings of a consumer.
add_executable(sh2o_k_consumer ${CMAKE_CURRENT_LIST_DIR}/k_consumer.c)
target_link_libraries(sh2o_k_consumer PRIVATE sh2o::sh2o)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(sh2o_k_consumer PRIVATE -Wall -Wextra -Wconversion -Wshadow -pedantic -Werror)
endif()
# The GTE API of sh2o/gte.h, compiled with the strict warnings of a consumer.
add_executable(sh2o_gte_consumer ${CMAKE_CURRENT_LIST_DIR}/gte_consumer.c)
target_link_libraries(sh2o_gte_consumer PRIVATE sh2o::sh2o)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(sh2o_gte_consumer PRIVATE -Wall -Wextra -Wconversion -Wshadow -pedantic -Werror)
endif()
# The PSY-Q bodies of sh2o/psyq.h, compiled with the strict warnings of a consumer.
add_executable(sh2o_psyq_consumer ${CMAKE_CURRENT_LIST_DIR}/psyq_consumer.c)
target_link_libraries(sh2o_psyq_consumer PRIVATE sh2o::sh2o)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(sh2o_psyq_consumer PRIVATE -Wall -Wextra -Wconversion -Wshadow -pedantic -Werror)
endif()
# The MVMVA list of sh2o/list.h, compiled with the strict warnings of a consumer.
add_executable(sh2o_list_consumer ${CMAKE_CURRENT_LIST_DIR}/list_consumer.c)
target_link_libraries(sh2o_list_consumer PRIVATE sh2o::sh2o)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(sh2o_list_consumer PRIVATE -Wall -Wextra -Wconversion -Wshadow -pedantic -Werror)
endif()
# Every installed header from C++, compiled with the strict warnings of a consumer.
add_executable(sh2o_cxx_consumer ${CMAKE_CURRENT_LIST_DIR}/sh2o_cxx_consumer.cpp)
target_link_libraries(sh2o_cxx_consumer PRIVATE sh2o::sh2o)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(sh2o_cxx_consumer PRIVATE -Wall -Wextra -Wconversion -Wshadow -pedantic -Werror)
endif()
