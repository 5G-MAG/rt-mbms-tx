#
# Copyright 2013-2021 Software Radio Systems Limited
#
# This file is part of srsRAN
#
# srsRAN is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# srsRAN is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# A copy of the GNU Affero General Public License can be found in
# the LICENSE file in the top-level directory of this distribution
# and at http://www.gnu.org/licenses/.
#

add_subdirectory(common)
add_subdirectory(phy)
add_subdirectory(stack)


# Link libstdc++ and libgcc
if(BUILD_STATIC)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
endif(BUILD_STATIC)


if (RPATH)
  SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif (RPATH)

add_library(enb_cfg_parser STATIC parser.cc enb_cfg_parser.cc)
target_link_libraries(enb_cfg_parser srsran_common ${LIBCONFIGPP_LIBRARIES})

add_executable(srsenb main.cc enb.cc control_server.cc metrics_stdout.cc metrics_csv.cc metrics_json.cc)

set(SRSENB_SOURCES srsenb_phy srsenb_stack srsenb_common srsenb_s1ap srsenb_m3ap srsenb_upper srsenb_mac srsenb_rrc srslog system)
set(SRSRAN_SOURCES srsran_common srsran_mac srsran_phy srsran_gtpu srsran_rlc srsran_pdcp srsran_radio rrc_asn1 s1ap_asn1 m3ap_asn1 enb_cfg_parser srslog support system)

# rrc_nr_asn1/ngap_nr_asn1 are pure ASN.1 codec libraries (only depend on
# asn1_utils/srsran_common, verified via lib/src/asn1/CMakeLists.txt) that
# enb_cfg_parser.cc needs for its general config parsing regardless of
# whether the 5G-NR stack itself is linked - unlike srsgnb_stack below, they
# carry no risky transitive dependency, so they stay linked unconditionally.
set(SRSRAN_SOURCES ${SRSRAN_SOURCES} rrc_nr_asn1 ngap_nr_asn1)

# 5G-NR/gNB STACK support (srsgnb_*) is unconditionally linked upstream, but
# this fork is LTE-only (FeMBMS/5G Terrestrial Broadcast, TS 36.xxx, no 5G
# NR) and srsgnb_stack unconditionally depends on srsue_upper, which is not
# built without ENABLE_SRSUE=ON. Rather than build srsue's NR/RRC code too (a
# large, unrelated detour - it has real staleness bugs of its own, unrelated
# to FeMBMS, from never having been compiled in this fork before), the gNB
# stack libraries are simply not linked into this executable. No source
# files were removed; flip this one variable (and fix srsue's NR build) to
# bring it back.
set(SRSENB_ENABLE_5GNR FALSE)
if(SRSENB_ENABLE_5GNR)
  set(SRSENB_SOURCES ${SRSENB_SOURCES} srsgnb_stack srsgnb_ngap srsgnb_upper srsgnb_mac srsgnb_rrc)
  target_compile_definitions(srsenb PRIVATE SRSENB_HAS_5GNR_STACK)
endif()

target_link_libraries(srsenb  ${SRSENB_SOURCES}
                              ${SRSRAN_SOURCES}
                              ${CMAKE_THREAD_LIBS_INIT}
                              ${Boost_LIBRARIES}
                              ${SEC_LIBRARIES}
                              ${LIBCONFIGPP_LIBRARIES}
                              ${SCTP_LIBRARIES})

if (RPATH)
  set_target_properties(srsenb PROPERTIES INSTALL_RPATH ".")
endif (RPATH)

########################################################################
# Option to run command after build (useful for remote builds)
########################################################################
if (NOT ${BUILDENB_CMD} STREQUAL "")
  message(STATUS "Added custom post-build-ENB command: ${BUILDENB_CMD}")
  add_custom_command(TARGET srsenb POST_BUILD COMMAND ${BUILDENB_CMD})
else(NOT ${BUILDENB_CMD} STREQUAL "")
  message(STATUS "No post-build-ENB command defined")
endif (NOT ${BUILDENB_CMD} STREQUAL "")

install(TARGETS srsenb DESTINATION ${RUNTIME_DIR})
