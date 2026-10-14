set(TESTS_ROOT_DIR "${BHUMAN_PREFIX}/Src/Apps/Tests")
set(TESTS_OUTPUT_DIR "${OUTPUT_PREFIX}/Build/${PLATFORM}/Tests/$<CONFIG>")

file(GLOB_RECURSE TESTS_SOURCES CONFIGURE_DEPENDS
    "${TESTS_ROOT_DIR}/*.cpp" "${TESTS_ROOT_DIR}/*.h")

if(MACOS)
  list(REMOVE_ITEM TESTS_SOURCES "${TESTS_ROOT_DIR}/Test.cpp")
  list(APPEND TESTS_SOURCES "${TESTS_ROOT_DIR}/Test.mm")
endif()

file(GLOB MAZE_SOURCES CONFIGURE_DEPENDS
    "${BHUMAN_PREFIX}/Src/Tools/Maze/*.cpp" "${BHUMAN_PREFIX}/Src/Tools/Maze/*.h")

add_executable(Tests MACOSX_BUNDLE ${TESTS_SOURCES} ${MAZE_SOURCES})

set_property(TARGET Tests PROPERTY RUNTIME_OUTPUT_DIRECTORY "${TESTS_OUTPUT_DIR}")
set_property(TARGET Tests PROPERTY FOLDER Apps)
set_property(TARGET Tests PROPERTY MACOSX_BUNDLE_INFO_PLIST "${TESTS_ROOT_DIR}/Info.plist")
set_property(TARGET Tests PROPERTY XCODE_GENERATE_SCHEME ON)
set_property(TARGET Tests PROPERTY XCODE_PRODUCT_TYPE "com.apple.product-type.bundle.unit-test")

target_include_directories(Tests PRIVATE "${TESTS_ROOT_DIR}" "${BHUMAN_PREFIX}/Src")

if(MACOS)
  target_link_libraries(Tests PRIVATE ${APP_KIT_FRAMEWORK})
endif()

target_link_libraries(Tests PRIVATE Framework)
target_link_libraries(Tests PRIVATE Math)
target_link_libraries(Tests PRIVATE Platform)
target_link_libraries(Tests PRIVATE Streaming)
target_link_libraries(Tests PRIVATE GTest::GTest)
if(TARGET apriltag::apriltag)
  target_link_libraries(Tests PRIVATE apriltag::apriltag)
endif()

target_compile_definitions(Tests PRIVATE GTEST_DONT_DEFINE_FAIL GTEST_DONT_DEFINE_TEST GTEST_HAS_TR1_TUPLE=0)

target_link_libraries(Tests PRIVATE Flags::DebugInDevelop)

source_group(TREE "${TESTS_ROOT_DIR}" FILES ${TESTS_SOURCES})
source_group(TREE "${BHUMAN_PREFIX}/Src/Tools/Maze" PREFIX Maze FILES ${MAZE_SOURCES})
