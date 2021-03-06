cmake_minimum_required(VERSION 2.8)

cpu_setup("test_lockit")

add_library(test_lockit_lib
            coma.cc
            cpu.cc
            field.cc
            read.cc
            template.cc
            util.cc)

function(test_lockit_add_executable exe)
    cpu_add_executable(${exe} ${ARGN})
    cpu_target_link_libraries(${exe} test_lockit_lib)
    cpu_target_link_common_libraries(${exe})
endfunction()

function(test_lockit_add_test exe)
  test_lockit_add_executable(${exe} ${exe}.cc)
  cpu_target_link_libraries(${exe} gtest gtest_main)
  if(NOT ARGV1)
    cpu_add_test(${exe})
  endif()
endfunction()

test_lockit_add_executable(niina niina.cpp)

cpu_add_runner(nidub.sh)
cpu_add_runner(rendaS9.sh)
cpu_add_runner(rendaGS9.sh)
cpu_add_runner(rendaGS9a.sh)

test_lockit_add_test(field_test)
