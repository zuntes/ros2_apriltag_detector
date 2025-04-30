cmake_minimum_required(VERSION 3.5)

project(apriltag_detection)

set(CMAKE_CXX_STANDARD 14)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Werror -Wall -Wextra -Wpedantic)
  add_link_options("-Wl,-z,relro,-z,now,-z,defs")
endif()

option(ASAN "use AddressSanitizer to detect memory issues" OFF)

if(ASAN)
    set(ASAN_FLAGS "\
        -fsanitize=address \
        -fsanitize=bool \
        -fsanitize=bounds \
        -fsanitize=enum \
        -fsanitize=float-cast-overflow \
        -fsanitize=float-divide-by-zero \
        -fsanitize=nonnull-attribute \
        -fsanitize=returns-nonnull-attribute \
        -fsanitize=signed-integer-overflow \
        -fsanitize=undefined \
        -fsanitize=vla-bound \
        -fno-sanitize=alignment \
        -fsanitize=leak \
        -fsanitize=object-size \
    ")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ASAN_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ASAN_FLAGS}")
endif()

# Explicitly set OpenCV_DIR 
set(OpenCV_DIR "/lib/x86_64-linux-gnu/cmake/opencv4")
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${OpenCV_DIR})

find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(apriltag_msgs REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(image_transport REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(Eigen3 REQUIRED NO_MODULE)
find_package(Threads REQUIRED)
find_package(OpenCV 4.5 REQUIRED COMPONENTS core calib3d imgcodecs imgproc)
find_package(apriltag 3.2 REQUIRED)

if(cv_bridge_VERSION VERSION_GREATER_EQUAL 3.3.0)
    add_compile_definitions(cv_bridge_HPP)
endif()

include_directories(
  include
  ${OpenCV_INCLUDE_DIRS}
)

# database of tag functions
add_library(tags STATIC
    src/tag_functions.cpp
)
target_link_libraries(tags
    apriltag::apriltag
)
set_property(TARGET tags PROPERTY
    POSITION_INDEPENDENT_CODE ON
)

# conversion functions as template specialisation
add_library(conversion STATIC
    src/conversion.cpp
)
target_link_libraries(conversion
    apriltag::apriltag
    Eigen3::Eigen
    ${OpenCV_LIBRARIES}
)
ament_target_dependencies(conversion
    geometry_msgs
    tf2
)
set_property(TARGET conversion PROPERTY
    POSITION_INDEPENDENT_CODE ON
)

# pose estimation methods
add_library(pose_estimation STATIC
    src/pose_estimation.cpp
)
target_link_libraries(pose_estimation
    apriltag::apriltag
    Eigen3::Eigen
    ${OpenCV_LIBRARIES}
    conversion
)
ament_target_dependencies(pose_estimation
    geometry_msgs
    tf2
)
set_property(TARGET pose_estimation PROPERTY
    POSITION_INDEPENDENT_CODE ON
)

# Install node
add_executable(apriltag_detector
    src/apriltag_detector.cpp
)

ament_target_dependencies(apriltag_detector
    rclcpp
    rclcpp_components
    sensor_msgs
    apriltag_msgs
    tf2_ros
    image_transport
    cv_bridge
)

target_link_directories(apriltag_detector BEFORE PRIVATE /lib/x86_64-linux-gnu)

target_link_libraries(apriltag_detector
    apriltag::apriltag
    tags
    pose_estimation
    ${OpenCV_LIBRARIES}
    /lib/x86_64-linux-gnu/libopencv_imgcodecs.so.4.5d
    /lib/x86_64-linux-gnu/libopencv_imgproc.so.4.5d
    /lib/x86_64-linux-gnu/libopencv_core.so.4.5d
)

ament_environment_hooks(${ament_cmake_package_templates_ENVIRONMENT_HOOK_LIBRARY_PATH})

install(TARGETS apriltag_detector
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(
  DIRECTORY include/
  DESTINATION include
)

install(DIRECTORY params
    DESTINATION share/${PROJECT_NAME}
)

install(DIRECTORY launch
    DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  set(ament_cmake_clang_format_CONFIG_FILE "${CMAKE_SOURCE_DIR}/.clang-format")
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-new-dtags")

ament_package()