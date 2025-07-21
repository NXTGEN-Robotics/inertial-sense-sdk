#include "inertial_sense_ros.h"
#ifdef ROS2
using namespace rclcpp;
#endif

int main(int argc, char**argv)
{
    rclcpp::init(argc, argv);
    
    try {
        auto node = std::make_shared<rclcpp::Node>("inertial_sense_node");
        
        YAML::Node yamlNode = YAML::Node(YAML::NodeType::Undefined);
        bool use_yaml_file = false;
        
        // Check if we have a YAML file argument (not ROS args)
        if (argc > 1) {
            std::string potential_yaml_path = argv[1];
            
            // Only treat as YAML file if it doesn't start with "--" (ROS args)
            if (!potential_yaml_path.empty() && 
                potential_yaml_path[0] != '-' && 
                potential_yaml_path.find(".yaml") != std::string::npos) {
                
                RCLCPP_INFO(node->get_logger(), "Loading YAML config file: %s", potential_yaml_path.c_str());
                
                try {
                    yamlNode = YAML::LoadFile(potential_yaml_path);
                    use_yaml_file = true;
                    RCLCPP_INFO(node->get_logger(), "Successfully loaded YAML configuration from file");
                } catch (const YAML::BadFile &bf) {
                    RCLCPP_WARN(node->get_logger(), "Failed to load %s: %s. Using ROS parameters instead.", 
                               potential_yaml_path.c_str(), bf.what());
                } catch (const std::exception& e) {
                    RCLCPP_WARN(node->get_logger(), "Exception loading YAML: %s. Using ROS parameters instead.", e.what());
                }
            } else {
                RCLCPP_INFO(node->get_logger(), "Launched with ROS args, using ROS parameter server for configuration");
            }
        } else {
            RCLCPP_INFO(node->get_logger(), "No arguments provided, using ROS parameter server for configuration");
        }
        
        // Create InertialSense instance
        // When using launch files, pass undefined YAML node so it uses ROS parameters
        auto inertial_sense = std::make_unique<InertialSenseROS>(node, yamlNode, false);
        
        // Initialize with error checking
        if (!inertial_sense->initialize()) {
            RCLCPP_ERROR(node->get_logger(), "Failed to initialize InertialSense driver");
            return -1;
        }
        
        RCLCPP_INFO(node->get_logger(), "InertialSense driver initialized successfully");
        
        // Use single-threaded executor
        rclcpp::executors::SingleThreadedExecutor executor;
        executor.add_node(node);
        
        // Main loop
        auto last_update = std::chrono::steady_clock::now();
        constexpr auto update_period = std::chrono::milliseconds(10);
        
        while (rclcpp::ok()) {
            auto now = std::chrono::steady_clock::now();
            
            try {
                executor.spin_some(std::chrono::milliseconds(1));
                
                if (now - last_update >= update_period) {
                    inertial_sense->update();
                    last_update = now;
                }
                
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                
            } catch (const std::exception& e) {
                RCLCPP_ERROR(node->get_logger(), "Exception in main loop: %s", e.what());
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (!rclcpp::ok()) break;
            }
        }
        
        RCLCPP_INFO(node->get_logger(), "Shutting down InertialSense driver...");
        inertial_sense->terminate();
        
    } catch (const std::exception& e) {
        RCLCPP_FATAL(rclcpp::get_logger("main"), "Fatal exception: %s", e.what());
        rclcpp::shutdown();
        return -1;
    }
    
    rclcpp::shutdown();
    RCLCPP_INFO(rclcpp::get_logger("main"), "InertialSense node shutdown complete");
    return 0;
}