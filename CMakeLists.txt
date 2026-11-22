# sh2o: PS1 GTE and libgte semantics for the Sega Saturn SH-2.
# Copyright (c) 2026 sh2o Contributors. MIT License.
cmake_minimum_required(VERSION 3.21)
project(sh2o VERSION 0.1.0 DESCRIPTION "PS1 GTE maths for the Saturn SH-2" LANGUAGES C)

# Tests and examples are on for a top-level build and off when another
# project adds sh2o with add_subdirectory.
option(SH2O_BUILD_TESTS "Build and register the host test suites" ${PROJECT_IS_TOP_LEVEL})
option(SH2O_BUILD_EXAMPLES "Build the host examples" ${PROJECT_IS_TOP_LEVEL})
option(SH2O_FORCE_PORTABLE "Use the portable C path also on the SH-2" OFF)
option(SH2O_SANITIZE "Host build with AddressSanitizer and UBSan" OFF)
option(SH2O_STRICT "Stop the build on a compiler warning" OFF)
option(SH2O_ZAM "Host build with the experimental SH2ZAM delegate (src/sh2o_zam.c); not installed" OFF)
option(SH2O_COUNT "Build with the call counters of sh2o/count.h" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(sh2o_warnings "")
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  set(sh2o_warnings -Wall -Wextra -Wconversion -Wshadow)
  if(SH2O_STRICT)
    list(APPEND sh2o_warnings -Werror -Wpedantic)
  endif()
endif()

if(SH2O_SANITIZE)
  if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "SH2O_SANITIZE is for host builds only")
  endif()
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# The library is every src/*.c file. The core needs no other library: SH2ZAM
# is not a dependency of sh2o. src/sh2o_zam.c is an empty unit unless
# SH2O_ZAM is ON (sh2o/zam_experimental.h).
file(GLOB sh2o_sources CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.c)
list(SORT sh2o_sources)
add_library(sh2o STATIC ${sh2o_sources})
add_library(sh2o::sh2o ALIAS sh2o)
target_include_directories(sh2o PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(sh2o PUBLIC c_std_11)
target_compile_options(sh2o PRIVATE ${sh2o_warnings})
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(sh2o PRIVATE -ffunction-sections -fdata-sections)
endif()
# PUBLIC: the inline kernels in a consumer take the path of the library.
if(SH2O_FORCE_PORTABLE)
  target_compile_definitions(sh2o PUBLIC SH2O_FORCE_PORTABLE=1)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "sh2")
  # A cross build for the SH-2 must take the native path.
  target_compile_definitions(sh2o PUBLIC SH2O_REQUIRE_SH2=1)
endif()
# The experimental delegate, host only: SH2ZAM's portable C path from the
# pinned submodule. PRIVATE, so the mutant programs and consumers never get it.
if(SH2O_ZAM)
  set(SH2Z_FORCE_PORTABLE ON CACHE BOOL "" FORCE)
  add_subdirectory(external/SH2ZAM)
  target_compile_definitions(sh2o PRIVATE SH2O_ZAM=1)
  target_link_libraries(sh2o PRIVATE $<BUILD_INTERFACE:SH2ZAM::sh2zam>)
endif()
# The call counters (sh2o/count.h, src/sh2o_count_internal.h). PRIVATE: the count points are in the library.
if(SH2O_COUNT)
  target_compile_definitions(sh2o PRIVATE SH2O_COUNT=1)
endif()

# One host executable and one CTest entry per source file.
function(sh2o_add_host_test name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE sh2o::sh2o)
  target_compile_options(${name} PRIVATE ${sh2o_warnings})
  add_test(NAME ${name} COMMAND ${name})
endfunction()

# A test that reads private inputs through environment variables
# (docs/EXACTNESS.md). SOURCE <file>: a C test, given the source folder as
# argv[1] (tests/private_input.h). COMMAND <cmd>...: any other check, for
# example a Python script. Exit code 77 shows the test as skipped.
function(sh2o_add_private_test name)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "SOURCE" "COMMAND")
  if(arg_SOURCE)
    add_executable(${name} ${arg_SOURCE})
    target_compile_options(${name} PRIVATE ${sh2o_warnings})
    add_test(NAME ${name} COMMAND ${name} ${PROJECT_SOURCE_DIR})
  else()
    add_test(NAME ${name} COMMAND ${arg_COMMAND})
  endif()
  set_tests_properties(${name} PROPERTIES SKIP_RETURN_CODE 77 LABELS private)
endfunction()

if(NOT CMAKE_CROSSCOMPILING AND (SH2O_BUILD_TESTS OR SH2O_BUILD_EXAMPLES))
  enable_testing()
endif()

if(SH2O_BUILD_TESTS AND NOT CMAKE_CROSSCOMPILING)
  # The vector models and some checks are Python scripts.
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  # tests/test_<family>.c becomes the CTest entry <family>. The family zam
  # (the delegate's suite) is built only when SH2O_ZAM is ON, the family count
  # (the call counters) only when SH2O_COUNT is ON.
  file(GLOB sh2o_test_sources CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/tests/test_*.c)
  list(SORT sh2o_test_sources)
  foreach(source IN LISTS sh2o_test_sources)
    get_filename_component(stem ${source} NAME_WE)
    string(REGEX REPLACE "^test_" "" family ${stem})
    if(family STREQUAL "zam" AND NOT SH2O_ZAM)
      continue()
    endif()
    if(family STREQUAL "count" AND NOT SH2O_COUNT)
      continue()
    endif()
    sh2o_add_host_test(${family} ${source})
    # A family suite that a consumer can also compile (tests/suite/) is part of its test.
    if(EXISTS ${PROJECT_SOURCE_DIR}/tests/suite/sh2o_${family}_test_suite.c)
      target_sources(${family} PRIVATE ${PROJECT_SOURCE_DIR}/tests/suite/sh2o_${family}_test_suite.c)
      target_include_directories(${family} PRIVATE ${PROJECT_SOURCE_DIR}/tests/suite)
      list(APPEND sh2o_suite_objects --objects "$<TARGET_OBJECTS:${family}>")
    endif()
  endforeach()
  # The bench verdict helper is support code in benchmarks/: only the test bench builds it.
  target_sources(bench PRIVATE ${PROJECT_SOURCE_DIR}/benchmarks/sh2o_bench.c)
  target_include_directories(bench PRIVATE ${PROJECT_SOURCE_DIR}/benchmarks)

  # The k bulk vectors: Python makes them again and compares each line of the
  # dump of test_k, then the six pinned digests.
  add_test(NAME k_bulk COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/k_bulk_check.py $<TARGET_FILE:k>)

  # The Python GTE model: hand cases, and the case set of the gte suite.
  add_test(NAME gte_model COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/gte_model_check.py)

  # The GTE budget script: the formula, the refusals and one run on synthetic inputs.
  add_test(NAME gte_budget COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/gte_budget_check.py
    --scratch ${PROJECT_BINARY_DIR}/gte_budget)

  # ratan2's table: the formula of scripts/psyq_tables.py against the literal in src/sh2o_psyq.c.
  add_test(NAME psyq_table COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/psyq_table_check.py)

  # The SH2ZAM fence: the tree, this file and the built library against the
  # delegate's isolation rules, and the plants that each rule must catch.
  add_test(NAME zam_isolation COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/zam_isolation_check.py
    ${PROJECT_SOURCE_DIR} --zam ${SH2O_ZAM}
    --defs "$<TARGET_PROPERTY:sh2o,COMPILE_DEFINITIONS>"
    --libs "$<TARGET_PROPERTY:sh2o,LINK_LIBRARIES>"
    --lib $<TARGET_FILE:sh2o>
    --objects "$<TARGET_OBJECTS:sh2o>"
    --scratch ${PROJECT_BINARY_DIR}/zam_isolation)

  # The global names of the library and of each family program with a suite:
  # sh2o_* or sh2oi_* only (main in tests/test_*.c), and the plants it must catch.
  add_test(NAME global_names COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/global_names_check.py
    --cc ${CMAKE_C_COMPILER} --scratch ${PROJECT_BINARY_DIR}/global_names
    --objects "$<TARGET_OBJECTS:sh2o>" ${sh2o_suite_objects})

  # No run of 6 lines of our sources equals a run of the SH2ZAM pin; skipped
  # when external/SH2ZAM is not checked out.
  add_test(NAME zam_copy COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/zam_copy_check.py
    ${PROJECT_SOURCE_DIR})
  set_tests_properties(zam_copy PROPERTIES SKIP_RETURN_CODE 77)

  # The same check against the psyz pin (test-only); skipped when external/psyz
  # is not checked out, unless SH2O_REQUIRE_PSYZ is 1 (CI). The plant: with
  # SH2O_REQUIRE_PSYZ=1 an empty source dir must give the required line.
  add_test(NAME psyz_copy COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/zam_copy_check.py
    ${PROJECT_SOURCE_DIR} external/psyz)
  set_tests_properties(psyz_copy PROPERTIES SKIP_RETURN_CODE 77)
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/psyz_require_plant)
  add_test(NAME psyz_require_plant COMMAND ${CMAKE_COMMAND} -E env SH2O_REQUIRE_PSYZ=1
    ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/zam_copy_check.py
    ${PROJECT_BINARY_DIR}/psyz_require_plant external/psyz)
  set_tests_properties(psyz_require_plant PROPERTIES
    PASS_REGULAR_EXPRESSION "psyz_copy: required \\(SH2O_REQUIRE_PSYZ=1\\)")

  # ratan2 against the psyz checkout (test-only, read in place): the table, and
  # the sweep of 17,834,154 pairs with its mutant (label proof). Both skipped
  # when external/psyz is not checked out, unless SH2O_REQUIRE_PSYZ is 1.
  add_test(NAME psyz_table COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/psyz_table_check.py)
  set_tests_properties(psyz_table PROPERTIES SKIP_RETURN_CODE 77)
  add_test(NAME psyz_ratan2_sweep COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/psyz_ratan2_sweep.py
    ${CMAKE_C_COMPILER} ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR}/psyz_ratan2_sweep)
  set_tests_properties(psyz_ratan2_sweep PROPERTIES LABELS proof TIMEOUT 120 SKIP_RETURN_CODE 77)

  # The psyz libgte test (test_libgte.c and ztest.c, read in place) linked with
  # the shim tests/psyz/sh2o_libgte_shim.c and src/: each case's class must be
  # the one of the case list in docs/PSYQ.md. Skipped when external/psyz is not
  # checked out, unless SH2O_REQUIRE_PSYZ is 1. The plant: with
  # SH2O_REQUIRE_PSYZ=1 an empty psyz dir must give the required line.
  add_test(NAME psyz_libgte COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/psyz_libgte_check.py
    ${CMAKE_C_COMPILER} ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR}/psyz_libgte)
  set_tests_properties(psyz_libgte PROPERTIES SKIP_RETURN_CODE 77)
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/psyz_libgte_plant)
  add_test(NAME psyz_libgte_require_plant COMMAND ${CMAKE_COMMAND} -E env SH2O_REQUIRE_PSYZ=1
    ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/psyz_libgte_check.py ${CMAKE_C_COMPILER}
    ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR}/psyz_libgte_plant_work ${PROJECT_BINARY_DIR}/psyz_libgte_plant)
  set_tests_properties(psyz_libgte_require_plant PROPERTIES
    PASS_REGULAR_EXPRESSION "psyz_libgte: required \\(SH2O_REQUIRE_PSYZ=1\\)")

  # The psyz software GTE (its GTE sources, read in place) against the GTE
  # machine (test_gte --driver) on the ps1-tests cases, every differing
  # register named, and psyz on the ps1-tests sample; the private test adds the
  # recorded PS1 pairs. Skipped when external/psyz is not checked out, unless
  # SH2O_REQUIRE_PSYZ is 1. The plant: with SH2O_REQUIRE_PSYZ=1 an empty psyz
  # dir must give the required line.
  add_test(NAME psyz_gte COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/psyz_gte_compare.py
    ${CMAKE_C_COMPILER} ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR}/psyz_gte $<TARGET_FILE:gte>)
  set_tests_properties(psyz_gte PROPERTIES SKIP_RETURN_CODE 77)
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/psyz_gte_plant)
  add_test(NAME psyz_gte_require_plant COMMAND ${CMAKE_COMMAND} -E env SH2O_REQUIRE_PSYZ=1
    ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/psyz_gte_compare.py ${CMAKE_C_COMPILER}
    ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR}/psyz_gte_plant_work $<TARGET_FILE:gte>
    ${PROJECT_BINARY_DIR}/psyz_gte_plant)
  set_tests_properties(psyz_gte_require_plant PROPERTIES
    PASS_REGULAR_EXPRESSION "psyz_gte: required \\(SH2O_REQUIRE_PSYZ=1\\)")
  sh2o_add_private_test(psyz_gte_recordings COMMAND ${Python3_EXECUTABLE}
    ${PROJECT_SOURCE_DIR}/tests/psyz_gte_compare.py --recordings ${CMAKE_C_COMPILER} ${PROJECT_SOURCE_DIR}
    ${PROJECT_BINARY_DIR}/psyz_gte_recordings $<TARGET_FILE:gte>)

  # The PsyQ LIB/LNK reader and linker on a two-module LIB built in memory (offline),
  # and its plant: a wrong VALUE in one patch must make the check fail.
  add_test(NAME psyq_lnk COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/psyq_lnk_check.py)
  add_test(NAME psyq_lnk_plant COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/psyq_lnk_check.py --plant)
  set_tests_properties(psyq_lnk_plant PROPERTIES WILL_FAIL TRUE)

  # The 4.7 routines against the game's objects by one digest (tests/psyq47_digests.py):
  # game re-derives tests/psyq47/game_digests.txt from the private objects; archive
  # compares the 4.7 routines with that file (SH2O_PSYQ47_ARCHIVE, else skipped); plants
  # checks the digest rule offline.
  sh2o_add_private_test(psyq47_game_digests COMMAND ${Python3_EXECUTABLE}
    ${PROJECT_SOURCE_DIR}/tests/psyq47_digests.py game)
  add_test(NAME psyq47_digests COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/psyq47_digests.py archive)
  set_tests_properties(psyq47_digests PROPERTIES SKIP_RETURN_CODE 77 LABELS psyq47)
  add_test(NAME psyq47_digest_plants COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/psyq47_digests.py plants)

  # The break plant: a copy of the psyq suite where SetFogNearFar's h is 0 (a
  # zero divisor); test_psyq --plant-break passes only when the seeded cases
  # call the break hook.
  file(READ ${PROJECT_SOURCE_DIR}/tests/suite/sh2o_psyq_test_suite.c sh2o_plant_text)
  set(sh2o_plant_old "        c->a[2]=(int32_t)(sh2o_psyq_lcg(&x)&0x3FFu);\n")
  string(FIND "${sh2o_plant_text}" "${sh2o_plant_old}" sh2o_plant_first)
  string(FIND "${sh2o_plant_text}" "${sh2o_plant_old}" sh2o_plant_last REVERSE)
  if(sh2o_plant_first EQUAL -1 OR NOT sh2o_plant_first EQUAL sh2o_plant_last)
    message(FATAL_ERROR "psyq_plant_break: the SetFogNearFar draw line is not in the psyq suite exactly once")
  endif()
  string(REPLACE "${sh2o_plant_old}" "${sh2o_plant_old}        c->a[2]=0;\n" sh2o_plant_text
    "${sh2o_plant_text}")
  file(WRITE ${PROJECT_BINARY_DIR}/psyq_plant_break_suite/new.c "${sh2o_plant_text}")
  configure_file(${PROJECT_BINARY_DIR}/psyq_plant_break_suite/new.c
    ${PROJECT_BINARY_DIR}/psyq_plant_break_suite/sh2o_psyq_test_suite.c COPYONLY)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/tests/suite/sh2o_psyq_test_suite.c)
  add_executable(psyq_plant_break ${PROJECT_SOURCE_DIR}/tests/test_psyq.c
    ${PROJECT_BINARY_DIR}/psyq_plant_break_suite/sh2o_psyq_test_suite.c)
  target_link_libraries(psyq_plant_break PRIVATE sh2o::sh2o)
  target_include_directories(psyq_plant_break PRIVATE ${PROJECT_SOURCE_DIR}/tests/suite)
  add_test(NAME psyq_plant_break COMMAND psyq_plant_break --plant-break)

  # The mutants: tests/mutant.py copies src/ and include/ into the build tree
  # and plants one bug there. Each mutant program is the copied src/*.c with
  # tests/test_<family>.c and its suite; each test passes only when an oracle
  # vector differs. The UNR mutants (family gte_unr, program test_gte) are not
  # in gte_sources; tests/unr_check.py mutant runs the vectors, the recip check
  # and the sweep, and passes when one of them catches the mutant. The
  # fixed-word entry mutants (family gte_entry, program test_gte) run
  # test_gte --mutant-entry: it passes when the entry check finds a difference.
  file(GLOB_RECURSE sh2o_headers CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/include/*.h ${PROJECT_SOURCE_DIR}/src/*.h)
  set(sh2o_mutants_k shift_11 shift_13 dropped_sign_extension swapped_index lost_carry)
  set(sh2o_mutants_gte shift_11 shift_13 clamp_off_by_one unr_index_off_by_one swapped_flag_bit dropped_sxy_push
    dropped_sz_push dropped_colour_push dpct_reads_rgbc dpct_fifo_not_advanced)
  set(sh2o_mutants_gte_unr clz32_bound_off_by_one unr_recip_rounding unr_div_clamp_bound)
  set(sh2o_mutants_gte_entry nrtps_sf0 nrtpt_lm1)
  # The psyq mutants: one changed body each; test_psyq --mutant passes when the psyq suite returns non-zero.
  set(sh2o_mutants_psyq back_colour_shift rot_matrix_swapped transpose_dropped_store rot_trans_no_sf pop_matrix_bit
    fog_no_break_hook ratan_index_off_by_one init_geom_screen apply_matrix_swapped apply_matrix_lv_sign
    comp_matrix_sign push_matrix_top colour_matrix_base far_colour_shift geom_offset_shift geom_screen_plus_one
    light_matrix_base trans_matrix_swapped trans_matrix_store)
  # The list mutants: one changed line or block of src/sh2o_list.c; an oracle vector of test_list must differ.
  set(sh2o_mutants_list item_dropped ir_not_sign_extended mac_wrong_register rows_swapped vz0_not_loaded)
  foreach(family k gte gte_unr gte_entry psyq list)
    string(REGEX REPLACE "_(unr|entry)$" "" program ${family})
    foreach(mutant IN LISTS sh2o_mutants_${family})
      set(mutant_dir ${PROJECT_BINARY_DIR}/mutants/${family}/${mutant})
      set(mutant_sources "")
      foreach(source IN LISTS sh2o_sources)
        get_filename_component(name ${source} NAME)
        list(APPEND mutant_sources ${mutant_dir}/src/${name})
      endforeach()
      add_custom_command(OUTPUT ${mutant_sources}
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/mutant.py ${family} ${mutant}
          ${PROJECT_SOURCE_DIR} ${mutant_dir}
        DEPENDS ${PROJECT_SOURCE_DIR}/tests/mutant.py ${sh2o_sources} ${sh2o_headers}
        COMMENT "${family} mutant ${mutant}")
      add_executable(${family}_mutant_${mutant} ${mutant_sources} ${PROJECT_SOURCE_DIR}/tests/test_${program}.c
        ${PROJECT_SOURCE_DIR}/tests/suite/sh2o_${program}_test_suite.c)
      # The copied include/ only (no link to sh2o::sh2o). No warning flags: a
      # mutant can leave a variable unused.
      target_include_directories(${family}_mutant_${mutant} BEFORE PRIVATE ${mutant_dir}/include
        ${PROJECT_SOURCE_DIR}/tests/suite)
      target_compile_definitions(${family}_mutant_${mutant} PRIVATE
        $<TARGET_PROPERTY:sh2o,INTERFACE_COMPILE_DEFINITIONS>)
      if(family STREQUAL "gte_unr")
        add_test(NAME gte_unr_mutant_${mutant} COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/unr_check.py
          mutant ${mutant} $<TARGET_FILE:gte_unr_mutant_${mutant}>)
      elseif(family STREQUAL "gte_entry")
        add_test(NAME gte_entry_mutant_${mutant} COMMAND gte_entry_mutant_${mutant} --mutant-entry ${mutant})
      else()
        add_test(NAME ${family}_mutant_${mutant} COMMAND ${family}_mutant_${mutant} --mutant ${mutant})
      endif()
    endforeach()
  endforeach()

  # The delegate's suite (test zam) and its mutants, SH2O_ZAM ON only. The
  # test zam gets the suite's defines and SH2ZAM. Each mutant program is
  # src/*.c, the suite and tests/test_zam.c, built with SH2O_MUTANT=k (the
  # planted bug of src/sh2o_zam.c); its test passes only when the suite's rc
  # equals the pin of mutant k (tests/test_zam.c).
  if(SH2O_ZAM)
    target_compile_definitions(zam PRIVATE SH2O_ZAM=1)
    target_link_libraries(zam PRIVATE SH2ZAM::sh2zam)
    set(sh2o_mutants_zam rows_shift_3 gate_2_49 tr_dropped transpose_twice fog_no_wrap_gate_dropped)
    set(mutant_k 0)
    foreach(mutant IN LISTS sh2o_mutants_zam)
      math(EXPR mutant_k "${mutant_k} + 1")
      add_executable(zam_mutant_${mutant} ${sh2o_sources} ${PROJECT_SOURCE_DIR}/tests/test_zam.c
        ${PROJECT_SOURCE_DIR}/tests/suite/sh2o_zam_test_suite.c)
      # No warning flags: a mutant can leave a variable unused.
      target_include_directories(zam_mutant_${mutant} PRIVATE ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/tests/suite)
      target_compile_definitions(zam_mutant_${mutant} PRIVATE SH2O_ZAM=1 SH2O_MUTANT=${mutant_k}
        $<TARGET_PROPERTY:sh2o,INTERFACE_COMPILE_DEFINITIONS>)
      target_link_libraries(zam_mutant_${mutant} PRIVATE SH2ZAM::sh2zam)
      add_test(NAME zam_mutant_${mutant} COMMAND zam_mutant_${mutant})
    endforeach()
    # The delegate header from C++ (sh2o/zam_experimental.h's extern "C"), strict warnings.
    enable_language(CXX)
    add_executable(zam_cxx ${PROJECT_SOURCE_DIR}/tests/zam_cxx.cpp)
    target_link_libraries(zam_cxx PRIVATE sh2o)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
      target_compile_options(zam_cxx PRIVATE -Wall -Wextra -Wconversion -Wshadow -pedantic -Werror)
    endif()
    add_test(NAME zam_cxx COMMAND zam_cxx)
  endif()

  # The call counters' plant, SH2O_COUNT ON only: src/*.c and tests/test_count.c built with SH2O_COUNT_PLANT=1
  # (src/sh2o_count_internal.h drops the list items); its test passes only when test_count gives its pinned rc.
  if(SH2O_COUNT)
    add_executable(count_plant ${sh2o_sources} ${PROJECT_SOURCE_DIR}/tests/test_count.c)
    target_include_directories(count_plant PRIVATE ${PROJECT_SOURCE_DIR}/include)
    target_compile_definitions(count_plant PRIVATE SH2O_COUNT=1 SH2O_COUNT_PLANT=1
      $<TARGET_PROPERTY:sh2o,INTERFACE_COMPILE_DEFINITIONS>)
    add_test(NAME count_plant COMMAND count_plant)
  endif()

  # The GTE machine (test_gte --driver) against the Python model over the
  # edge, LCG and ps1-tests cases, and the pinned count of differing cases of
  # each gte mutant.
  set(sh2o_gte_programs $<TARGET_FILE:gte>)
  foreach(mutant IN LISTS sh2o_mutants_gte)
    list(APPEND sh2o_gte_programs $<TARGET_FILE:gte_mutant_${mutant}>)
  endforeach()
  add_test(NAME gte_sources
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/gte_sources_check.py ${sh2o_gte_programs})

  # The UNR divide: the reciprocal of each SZ against the model, and the sweep
  # of all 3,221,192,704 (h, SZ) pairs of the divide (label proof: make proofs
  # and make proof-tests run it, make test does not).
  add_test(NAME gte_unr COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/unr_check.py recip $<TARGET_FILE:gte>)
  add_test(NAME gte_unr_sweep
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/unr_check.py sweep $<TARGET_FILE:gte>)
  set_tests_properties(gte_unr_sweep PROPERTIES LABELS proof TIMEOUT 60)

  # The check of the private input variables. Its twin sets one variable to a
  # path in the repository and must print that error.
  sh2o_add_private_test(private_inputs SOURCE ${PROJECT_SOURCE_DIR}/tests/private_inputs.c)
  add_test(NAME private_inputs_in_tree COMMAND private_inputs ${PROJECT_SOURCE_DIR})
  set_tests_properties(private_inputs_in_tree PROPERTIES LABELS private
    PASS_REGULAR_EXPRESSION "error: SH2O_PS1_GTE_RECORDINGS=[^\n]* is in the repository"
    ENVIRONMENT "SH2O_PS1_GTE_RECORDINGS=${PROJECT_SOURCE_DIR}/tests")
  # The same check as a script (the COMMAND form), and a test that runs it
  # through CTest: Skipped with no variable, Skipped with an absent path,
  # Failed with a path in the repository.
  sh2o_add_private_test(private_inputs_script
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/private_inputs.py ${PROJECT_SOURCE_DIR})
  add_test(NAME private_inputs_script_ctest
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/private_inputs.py --ctest-self-test
      ${CMAKE_CTEST_COMMAND} ${PROJECT_BINARY_DIR} private_inputs_script ${PROJECT_SOURCE_DIR})
  set_tests_properties(private_inputs_script_ctest PROPERTIES LABELS private)

  # The same comparison with the recorded PS1 GTE command pairs of
  # SH2O_PS1_GTE_RECORDINGS added. Its twin sets the variable to a path in
  # the repository and must print that error.
  sh2o_add_private_test(gte_recordings
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/gte_sources_check.py --recordings ${sh2o_gte_programs})
  add_test(NAME gte_recordings_in_tree
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/gte_sources_check.py --recordings ${sh2o_gte_programs})
  set_tests_properties(gte_recordings_in_tree PROPERTIES LABELS private
    PASS_REGULAR_EXPRESSION "error: SH2O_PS1_GTE_RECORDINGS=[^\n]* is in the repository"
    ENVIRONMENT "SH2O_PS1_GTE_RECORDINGS=${PROJECT_SOURCE_DIR}/tests")

  # The PsyQ libgte bodies against their own object bytes, run by
  # scripts/psyq_model.py from the private build inputs (tests/psyq_game_check.py:
  # identity, table, the seeded bodies and the psyq mutants, the suite digest,
  # the ratan2 pairs). Each twin sets one variable to a path in the repository
  # and must print that error.
  set(sh2o_psyq_programs $<TARGET_FILE:psyq>)
  foreach(mutant IN LISTS sh2o_mutants_psyq)
    list(APPEND sh2o_psyq_programs $<TARGET_FILE:psyq_mutant_${mutant}>)
  endforeach()
  set(sh2o_args_psyq_identity identity)
  set(sh2o_args_psyq_game_table table)
  set(sh2o_args_psyq_bodies bodies ${sh2o_psyq_programs})
  set(sh2o_args_psyq_model_suite suite)
  set(sh2o_args_psyq_ratan2_pairs ratan2 $<TARGET_FILE:psyq>)
  foreach(name psyq_identity psyq_game_table psyq_bodies psyq_model_suite psyq_ratan2_pairs)
    sh2o_add_private_test(${name}
      COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/psyq_game_check.py ${sh2o_args_${name}})
    add_test(NAME ${name}_in_tree
      COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/psyq_game_check.py ${sh2o_args_${name}})
    set_tests_properties(${name}_in_tree PROPERTIES LABELS private
      PASS_REGULAR_EXPRESSION "error: SH2O_PS1_LIBGTE_OBJECTS=[^\n]* is in the repository"
      ENVIRONMENT "SH2O_PS1_LIBGTE_OBJECTS=${PROJECT_SOURCE_DIR}/tests")
  endforeach()

  # The same checks on the 4.7 bodies linked from the archive (tests/psyq47_model_check.py,
  # SH2O_PSYQ47_ARCHIVE, else skipped; the plant: SH2O_REQUIRE_PSYQ47=1 and an absent archive
  # must give the required line). same: the game machine against the 4.7 one (private inputs
  # too), with its in-repository twin.
  set(sh2o_args_psyq47_bodies ${sh2o_psyq_programs})
  set(sh2o_args_psyq47_ratan2 $<TARGET_FILE:psyq>)
  foreach(name table bodies suite ratan2)
    add_test(NAME psyq47_model_${name} COMMAND ${Python3_EXECUTABLE}
      ${PROJECT_SOURCE_DIR}/tests/psyq47_model_check.py ${name} ${sh2o_args_psyq47_${name}})
    set_tests_properties(psyq47_model_${name} PROPERTIES SKIP_RETURN_CODE 77 LABELS psyq47)
  endforeach()
  add_test(NAME psyq47_require_plant COMMAND ${CMAKE_COMMAND} -E env SH2O_REQUIRE_PSYQ47=1
    SH2O_PSYQ47_ARCHIVE=${PROJECT_BINARY_DIR}/psyq47_absent.zip
    ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/psyq47_model_check.py table)
  set_tests_properties(psyq47_require_plant PROPERTIES
    PASS_REGULAR_EXPRESSION "psyq47_model: required \\(SH2O_REQUIRE_PSYQ47=1\\)")
  sh2o_add_private_test(psyq47_same
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/psyq47_model_check.py same)
  add_test(NAME psyq47_same_in_tree
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/psyq47_model_check.py same)
  set_tests_properties(psyq47_same_in_tree PROPERTIES LABELS private
    PASS_REGULAR_EXPRESSION "error: SH2O_PS1_LIBGTE_OBJECTS=[^\n]* is in the repository"
    ENVIRONMENT "SH2O_PS1_LIBGTE_OBJECTS=${PROJECT_SOURCE_DIR}/tests")
endif()

if(SH2O_BUILD_EXAMPLES AND NOT CMAKE_CROSSCOMPILING)
  file(GLOB sh2o_example_sources CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/examples/*.c)
  list(SORT sh2o_example_sources)
  foreach(source IN LISTS sh2o_example_sources)
    get_filename_component(stem ${source} NAME_WE)
    sh2o_add_host_test(example_${stem} ${source})
  endforeach()
endif()

set(sh2o_cmake_dir ${CMAKE_INSTALL_LIBDIR}/cmake/sh2o)
install(TARGETS sh2o EXPORT sh2o-targets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
# sh2o/zam_experimental.h is for the SH2O_ZAM build only.
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PATTERN zam_experimental.h EXCLUDE)
install(EXPORT sh2o-targets NAMESPACE sh2o:: FILE sh2oTargets.cmake DESTINATION ${sh2o_cmake_dir})
configure_package_config_file(cmake/sh2oConfig.cmake.in
  ${PROJECT_BINARY_DIR}/sh2oConfig.cmake INSTALL_DESTINATION ${sh2o_cmake_dir})
write_basic_package_version_file(${PROJECT_BINARY_DIR}/sh2oConfigVersion.cmake
  COMPATIBILITY SameMinorVersion)
install(FILES ${PROJECT_BINARY_DIR}/sh2oConfig.cmake ${PROJECT_BINARY_DIR}/sh2oConfigVersion.cmake
  DESTINATION ${sh2o_cmake_dir})
install(FILES LICENSE NOTICE.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/sh2o)
