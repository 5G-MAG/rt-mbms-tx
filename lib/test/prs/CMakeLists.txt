#
# Positioning reference signal tests. The golden vectors come from the Python reference of rt-mbms-examples/positioning-assistance
# (tools/gen_prs_golden.py), which was itself checked against srsRAN's Gold sequence and the TS 36.211 text.
#
set(CTEST_LABELS "lib;phy;prs")

add_executable(prs_test prs_test.c)
target_link_libraries(prs_test srsran_phy srsran_common ${CMAKE_THREAD_LIBS_INIT})
add_test(prs_test prs_test ${CMAKE_CURRENT_SOURCE_DIR}/prs_golden.txt)
