include(FetchContent)

###################################################################################################
# - spdlog ----------------------------------------------------------------------------------------

set(RMM_MIN_VERSION_spdlog 1.7.0)

CPMFindPackage(
  NAME spdlog
  GITHUB_REPOSITORY gabime/spdlog
  VERSION ${RMM_MIN_VERSION_spdlog}
  GIT_SHALLOW TRUE
  OPTIONS
    # If there is no pre-installed spdlog we can use, we'll install our fetched copy
    # together with RMM
    "SPDLOG_INSTALL TRUE"
  )

###################################################################################################
# - thrust/cub ------------------------------------------------------------------------------------

set(RMM_MIN_VERSION_Thrust 1.9.0)

CPMFindPackage(
  NAME Thrust
  GITHUB_REPOSITORY NVIDIA/thrust
  GIT_TAG 1.10.0
  VERSION 1.10.0
  GIT_SHALLOW TRUE
  OPTIONS
    # If there is no pre-installed thrust we can use, we'll install our fetched copy
    # together with RMM
    "THRUST_INSTALL TRUE"
  )

thrust_create_target(rmm::Thrust FROM_OPTIONS)

###################################################################################################
# - boost stacktrace ------------------------------------------------------------------------------

set(RMM_MIN_VERSION_Boost 1.74.0)

CPMFindPackage(
  NAME Boost
  URL https://dl.bintray.com/boostorg/release/1.74.0/source/boost_1_74_0.tar.gz
  URL_HASH SHA256=afff36d392885120bcac079148c177d1f6f7730ec3d47233aa51b0afa4db94a5
  VERSION ${RMM_MIN_VERSION_Boost}
  DOWNLOAD_ONLY TRUE
  )

if (Boost_ADDED)
  add_library(Boost::boost IMPORTED INTERFACE)
  set_target_properties(Boost::boost PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${Boost_SOURCE_DIR}
    )
endif()

###################################################################################################
# - googletest ------------------------------------------------------------------------------------

if (BUILD_TESTS)
  CPMFindPackage(
    NAME GTest
    GITHUB_REPOSITORY google/googletest
    GIT_TAG release-1.10.0
    VERSION 1.10.0
    GIT_SHALLOW TRUE
    OPTIONS
      "INSTALL_GTEST OFF"
    # googletest >= 1.10.0 provides a cmake config file -- use it if it exists
    FIND_PACKAGE_ARGUMENTS "CONFIG"
    )

  if (GTest_ADDED)
    add_library(GTest::gtest ALIAS gtest)
    add_library(GTest::gmock ALIAS gmock)
    add_library(GTest::gtest_main ALIAS gtest_main)
    add_library(GTest::gmock_main ALIAS gmock_main)
  endif()
endif()

###################################################################################################
# - googlebenchmark -------------------------------------------------------------------------------

if (BUILD_BENCHMARKS)
  CPMFindPackage(
    NAME benchmark
    GITHUB_REPOSITORY google/benchmark
    VERSION 1.5.2
    GIT_SHALLOW TRUE
    OPTIONS
      "BENCHMARK_ENABLE_TESTING OFF"
      "BENCHMARK_ENABLE_INSTALL OFF"
      )
endif()
