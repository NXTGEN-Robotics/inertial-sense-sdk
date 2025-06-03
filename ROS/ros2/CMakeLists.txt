# FIXED: Use consistent C++ standard throughout
set(CMAKE_CXX_STANDARD 20)  # Changed from 17 to match later setting
cmake_minimum_required(VERSION 3.10.0)
project(inertial_sense_ros2)

# Find required packages
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclpy REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(ament_cmake_gtest REQUIRED)
find_package(Threads REQUIRED)

add_definitions(-DROS2)

# FIXED: More robust script path detection
find_file(BUILD_SCRIPT_PATH
    NAMES build_is_sdk.sh
    PATHS 
        ${CMAKE_CURRENT_SOURCE_DIR}/../inertial-sense-sdk/scripts
        ${CMAKE_CURRENT_SOURCE_DIR}/../../inertial-sense-sdk/scripts
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../inertial-sense-sdk/scripts
        ${CMAKE_CURRENT_SOURCE_DIR}/../scripts
        ${CMAKE_CURRENT_SOURCE_DIR}/../../scripts
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../scripts
    DOC "Path to build_is_sdk.sh script"
    NO_DEFAULT_PATH
)

if(NOT BUILD_SCRIPT_PATH)
    message(FATAL_ERROR "Could not find build_is_sdk.sh script. Please ensure inertial-sense-sdk is properly checked out.")
endif()

message(STATUS "Found build script at: ${BUILD_SCRIPT_PATH}")

# FIXED: Determine SDK source directory and build paths
get_filename_component(SDK_SOURCE_DIR "${BUILD_SCRIPT_PATH}/../.." ABSOLUTE)
# FIXED: Check multiple possible library locations
set(POSSIBLE_LIB_PATHS
    "${SDK_SOURCE_DIR}/build/libInertialSenseSDK.a"
    "${SDK_SOURCE_DIR}/libInertialSenseSDK.a"  
    "${CMAKE_BINARY_DIR}/libInertialSenseSDK.a"
)

# Find the actual library location after script execution
foreach(lib_path ${POSSIBLE_LIB_PATHS})
    if(EXISTS ${lib_path})
        set(IS_SDK_LIB_PATH ${lib_path})
        message(STATUS "Found existing library at: ${IS_SDK_LIB_PATH}")
        break()
    endif()
endforeach()

# If not found, use the default expected path
if(NOT IS_SDK_LIB_PATH)
    set(IS_SDK_LIB_PATH "${SDK_SOURCE_DIR}/build/libInertialSenseSDK.a")
endif()

message(STATUS "SDK source directory: ${SDK_SOURCE_DIR}")
message(STATUS "Expected library path: ${IS_SDK_LIB_PATH}")

add_custom_command(OUTPUT ${IS_SDK_LIB_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SDK_SOURCE_DIR}/build
    COMMAND ${CMAKE_COMMAND} -E env 
        PYTHON_VENV_PATH=""
        bash ${BUILD_SCRIPT_PATH}
    COMMAND ${CMAKE_COMMAND} -E echo "Verifying library build..."
    COMMAND test -f ${IS_SDK_LIB_PATH} || (echo "ERROR: libInertialSenseSDK.a not found at expected location ${IS_SDK_LIB_PATH}. Check build script output above." && exit 1)
    WORKING_DIRECTORY ${SDK_SOURCE_DIR}
    COMMENT "Building Inertial Sense SDK..."
    VERBATIM
    USES_TERMINAL
)

add_custom_target(build_InertialSenseSDK DEPENDS ${IS_SDK_LIB_PATH})

# FIXED: Verify library exists before setting properties
add_library(InertialSenseSDK STATIC IMPORTED GLOBAL)
add_dependencies(InertialSenseSDK build_InertialSenseSDK)
set_target_properties(InertialSenseSDK PROPERTIES 
    IMPORTED_LOCATION ${IS_SDK_LIB_PATH}
    INTERFACE_INCLUDE_DIRECTORIES "${SDK_SOURCE_DIR}/src;${CMAKE_CURRENT_SOURCE_DIR}/shared/include"
)

# Generate ROS2 interfaces
rosidl_generate_interfaces(${PROJECT_NAME}
    "msg/GTime.msg"
    "msg/SatInfo.msg"
    "msg/GPS.msg"
    "msg/GPSInfo.msg"
    "msg/PIMU.msg"
    "msg/RTKInfo.msg"
    "msg/RTKRel.msg"
    "msg/GlonassEphemeris.msg"
    "msg/GNSSEphemeris.msg"
    "msg/GNSSObservation.msg"
    "msg/GNSSObsVec.msg"
    "msg/INL2States.msg"
    "msg/DIDINS2.msg"
    "msg/DIDINS1.msg"
    "msg/DIDINS4.msg"
    "srv/FirmwareUpdate.srv"
    "srv/RefLLAUpdate.srv"
    DEPENDENCIES std_msgs geometry_msgs diagnostic_msgs builtin_interfaces
)

# Export package information
ament_export_include_directories(shared/include)
ament_export_libraries(inertial_sense_ros2)
ament_export_dependencies(rclcpp sensor_msgs geometry_msgs std_msgs diagnostic_msgs)

# We don't need the example projects
set(IGNORE_EXAMPLE_PROJECTS 1)

# FIXED: Use target-based includes instead of global include_directories
# Only include essential directories globally, use target_include_directories for the rest
include_directories(
    shared/include
    ${YAML_CPP_INCLUDE_DIR}
)

# FIXED: Find ROS2 installation path more reliably
find_path(ROS2_INCLUDE_PATH
    NAMES sensor_msgs
    PATHS 
        /opt/ros/jazzy/include
        /opt/ros/humble/include
        /opt/ros/galactic/include
        /opt/ros/foxy/include
        $ENV{ROS_ROOT}/../include
    DOC "ROS2 include directory"
)

if(ROS2_INCLUDE_PATH)
    include_directories(${ROS2_INCLUDE_PATH})
    message(STATUS "Found ROS2 includes at: ${ROS2_INCLUDE_PATH}")
endif()

# Build configuration
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fms-extensions -Wl,--no-as-needed -DPLATFORM_IS_LINUX")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fms-extensions -Wl,--no-as-needed -DPLATFORM_IS_LINUX")

# Create main executable
add_executable(${PROJECT_NAME}_node
    src/inertial_sense_ros2.cpp
    shared/src/inertial_sense_node.cpp
    shared/src/ParamHelper.cpp
    shared/src/RtkRover.cpp
    shared/src/RtkBase.cpp
)

# FIXED: Set target properties after target is created
set_target_properties(${PROJECT_NAME}_node PROPERTIES
    C_STANDARD 11
    CXX_STANDARD 20
    POSITION_INDEPENDENT_CODE ON
)

# FIXED: Add target-specific include directories
target_include_directories(${PROJECT_NAME}_node PRIVATE
    ${SDK_SOURCE_DIR}/src
    ${SDK_SOURCE_DIR}/src/libusb/libusb
    ${SDK_SOURCE_DIR}/src/libusb/linux
    ${SDK_SOURCE_DIR}/src/yaml-cpp
    ${CMAKE_BINARY_DIR}/rosidl_generator_cpp/
)

# Link libraries and dependencies
rosidl_get_typesupport_target(cpp_typesupport_target "${PROJECT_NAME}" "rosidl_typesupport_cpp")
target_link_libraries(${PROJECT_NAME}_node "${cpp_typesupport_target}")
ament_target_dependencies(${PROJECT_NAME}_node rclcpp std_msgs geometry_msgs nav_msgs diagnostic_msgs sensor_msgs std_srvs)
target_link_libraries(${PROJECT_NAME}_node InertialSenseSDK ${YAML_CPP_LIBRARIES} Threads::Threads)

# FIXED: Conditional test building
if(BUILD_TESTING)
    ament_add_gtest(test_${PROJECT_NAME}
        src/inertial_sense_ros2.cpp
        shared/src/ParamHelper.cpp
        shared/src/RtkRover.cpp
        shared/src/RtkBase.cpp
        test/test_basic_unit_tests.cpp
        test/test_client_reconnect.cpp
        test/test_communications.cpp
    )

    # FIXED: Set target properties for test executable
    if(TARGET test_${PROJECT_NAME})
        set_target_properties(test_${PROJECT_NAME} PROPERTIES
            C_STANDARD 11
            CXX_STANDARD 20
            POSITION_INDEPENDENT_CODE ON
        )

        target_include_directories(test_${PROJECT_NAME} PRIVATE
            ${SDK_SOURCE_DIR}/src
            ${SDK_SOURCE_DIR}/src/libusb/libusb
            ${SDK_SOURCE_DIR}/src/libusb/linux
            ${SDK_SOURCE_DIR}/src/yaml-cpp
            ${CMAKE_BINARY_DIR}/rosidl_generator_cpp/
        )

        rosidl_get_typesupport_target(cpp_typesupport_target "${PROJECT_NAME}" "rosidl_typesupport_cpp")
        target_link_libraries(test_${PROJECT_NAME} "${cpp_typesupport_target}")
        ament_target_dependencies(test_${PROJECT_NAME} rclcpp std_msgs geometry_msgs nav_msgs diagnostic_msgs sensor_msgs std_srvs)
        target_link_libraries(test_${PROJECT_NAME} InertialSenseSDK ${YAML_CPP_LIBRARIES} Threads::Threads)

        install(TARGETS test_${PROJECT_NAME}
                DESTINATION lib/${PROJECT_NAME})
    endif()
endif()

ament_package()