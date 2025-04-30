#include <rclcpp/rclcpp.hpp>
#include <tf2_ros/transform_broadcaster.h>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Matrix3x3.h>
#include <sensor_msgs/msg/camera_info.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <image_transport/camera_subscriber.hpp>
#include <image_transport/image_transport.hpp>

#ifdef cv_bridge_HPP
#include <cv_bridge/cv_bridge.hpp>
#else
#include <cv_bridge/cv_bridge.h>
#endif

#include <apriltag.h>
#include "apriltag_detection/tag_functions.hpp"
#include "apriltag_detection/pose_estimation.hpp"

#include <apriltag_msgs/msg/april_tag_detection.hpp>
#include <apriltag_msgs/msg/april_tag_detection_array.hpp>

#include <string>

class AprilTagNode : public rclcpp::Node {
public:
    AprilTagNode()
      : Node("apriltag_detector"),
        td(apriltag_detector_create()),
        tf_broadcaster(this)
    {

        RCLCPP_INFO(get_logger(), "April tag detector node init");

        // Declare parameters 
        declare_parameter("family", "36h11");
        declare_parameter("size", 0.05);
        declare_parameter("max_hamming", 0);
        declare_parameter("profile", false);
        declare_parameter("pose_estimation_method", "pnp");

        declare_parameter("detector.threads", 1);
        declare_parameter("detector.decimate", 2.0);
        declare_parameter("detector.blur", 0.0);
        declare_parameter("detector.refine", true);
        declare_parameter("detector.sharpening", 0.25);
        declare_parameter("detector.debug", false);

        // Assign parameters 
        tag_family_ = get_parameter("family").as_string();
        pose_estimation_method_ = get_parameter("pose_estimation_method").as_string();
        tag_size_ = get_parameter("size").as_double();
        max_hamming_ = get_parameter("max_hamming").as_int();
        profile_ = get_parameter("profile").as_bool();

        td->nthreads = get_parameter("detector.threads").as_int();
        td->quad_decimate = get_parameter("detector.decimate").as_double();
        td->quad_sigma = get_parameter("detector.blur").as_double();
        td->refine_edges = get_parameter("detector.refine").as_bool();
        td->decode_sharpening = get_parameter("detector.sharpening").as_double();
        td->debug = get_parameter("detector.debug").as_bool();

        // RCLCPP_INFO(get_logger(), "Tag family: %s", tag_family_.c_str());
        // RCLCPP_INFO(get_logger(), "Pose estimation method: %s", pose_estimation_method_.c_str());
        // RCLCPP_INFO(get_logger(), "Tag size: %f", tag_size_);
        // RCLCPP_INFO(get_logger(), "Max hamming: %d", max_hamming_);
        // RCLCPP_INFO(get_logger(), "Profile: %d", profile_);

        // Initialize tag family and detector
        if(tag_fun.count(tag_family_)) {
            tf = tag_fun.at(tag_family_).first();
            tf_destructor = tag_fun.at(tag_family_).second;
            apriltag_detector_add_family(td, tf);
        } else {
            throw std::runtime_error("Unsupported tag family: " + tag_family_);
        }
        // init method for estimating pose
        estimate_pose = pose_estimation_methods.at(pose_estimation_method_);

        // Create subscriptions
        sub_image = this->create_subscription<sensor_msgs::msg::Image>(
            "/bcr_bot/bottom_camera/image_raw", 20, std::bind(&AprilTagNode::callback_camera, this, std::placeholders::_1));

        sub_camera_info = this->create_subscription<sensor_msgs::msg::CameraInfo>(
            "/bcr_bot/bottom_camera/camera_info", 20, std::bind(&AprilTagNode::callback_camera_info, this, std::placeholders::_1));

        pub_detections = this->create_publisher<apriltag_msgs::msg::AprilTagDetectionArray>("april_tag_detections", rclcpp::QoS(10));
    }

    ~AprilTagNode() override {
        apriltag_detector_destroy(td);
        tf_destructor(tf);
    }

private:
    // april tag
    apriltag_family_t* tf;
    apriltag_detector_t* const td;
    std::function<void(apriltag_family_t*)> tf_destructor;
    pose_estimation_f estimate_pose = nullptr;

    // params
    std::string tag_family_;
    std::string pose_estimation_method_;
    double tag_size_;
    int max_hamming_;
    bool profile_;
    std::array<double, 4> intrinsics_;

    std::mutex mutex;
    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr sub_image;
    rclcpp::Subscription<sensor_msgs::msg::CameraInfo>::SharedPtr sub_camera_info;
    rclcpp::Publisher<apriltag_msgs::msg::AprilTagDetectionArray>::SharedPtr pub_detections;
    
    tf2_ros::TransformBroadcaster tf_broadcaster;

    void callback_camera(const sensor_msgs::msg::Image::ConstSharedPtr& msg_img) 
    {
        if (intrinsics_[0] == 0) {
            RCLCPP_WARN(get_logger(), "Camera info not received yet.");
            return;
        }

        // Convert to 8bit monochrome image
        const cv::Mat img_uint8 = cv_bridge::toCvShare(msg_img, "mono8")->image;
        image_u8_t im{img_uint8.cols, img_uint8.rows, img_uint8.cols, img_uint8.data};

        // Detect tags
        mutex.lock();
        zarray_t* detections = apriltag_detector_detect(td, &im);
        mutex.unlock();

        if(profile_)
            timeprofile_display(td->tp);
        
        // Create arpril tag msg
        apriltag_msgs::msg::AprilTagDetectionArray msg_detections;
        msg_detections.header = msg_img->header;

        // Create tf
        std::vector<geometry_msgs::msg::TransformStamped> tfs;

        for(int i = 0; i < zarray_size(detections); i++) 
        {
            apriltag_detection_t* det;
            zarray_get(detections, i, &det);

            RCLCPP_DEBUG(get_logger(),
                        "detection %3d: id (%2dx%2d)-%-4d, hamming %d, margin %8.3f\n",
                        i, det->family->nbits, det->family->h, det->id,
                        det->hamming, det->decision_margin);

            // reject detections with more corrected bits than allowed
            if(det->hamming > max_hamming_) { continue; }

            // detection
            apriltag_msgs::msg::AprilTagDetection msg_detection;
            msg_detection.family = std::string(det->family->name);
            msg_detection.id = det->id;
            msg_detection.hamming = det->hamming;
            msg_detection.decision_margin = det->decision_margin;
            msg_detection.centre.x = det->c[0];
            msg_detection.centre.y = det->c[1];
            std::memcpy(msg_detection.corners.data(), det->p, sizeof(double) * 8);
            std::memcpy(msg_detection.homography.data(), det->H->data, sizeof(double) * 9);
            msg_detections.detections.push_back(msg_detection);

            // 3D orientation and position
            geometry_msgs::msg::TransformStamped tf;
            tf.header = msg_img->header;
            tf.child_frame_id = std::string(det->family->name) + ":" + std::to_string(det->id);
            if(estimate_pose != nullptr) {
                tf.transform = estimate_pose(det, intrinsics_, tag_size_);

                tf2::Quaternion q(
                    tf.transform.rotation.x,
                    tf.transform.rotation.y,
                    tf.transform.rotation.z,
                    tf.transform.rotation.w
                );
                
                double roll, pitch, yaw;
                tf2::Matrix3x3(q).getRPY(roll, pitch, yaw);

                // Convert radians to degrees
                double roll_deg = roll * 180.0 / M_PI;
                double pitch_deg = pitch * 180.0 / M_PI;
                double yaw_deg = yaw * 180.0 / M_PI;

                RCLCPP_INFO(get_logger(), 
                "Transform for tag %d: Translation: [%f, %f, %f], Rotation: [Roll: %f, Pitch: %f, Yaw: %f] degrees", 
                det->id, 
                tf.transform.translation.x, tf.transform.translation.y, tf.transform.translation.z, 
                roll_deg, pitch_deg, yaw_deg);

            }

            tfs.push_back(tf);
        }

        pub_detections->publish(msg_detections);
        tf_broadcaster.sendTransform(tfs);
        apriltag_detections_destroy(detections);
    }

    void callback_camera_info(const sensor_msgs::msg::CameraInfo::ConstSharedPtr& msg_ci) {
        intrinsics_ = {msg_ci->p[0], msg_ci->p[5], msg_ci->p[2], msg_ci->p[6]};
        // RCLCPP_INFO(get_logger(), "Camera intrinsics: fx=%f, fy=%f, cx=%f, cy=%f",
        //             intrinsics_[0], intrinsics_[1], intrinsics_[2], intrinsics_[3]);
    }

};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<AprilTagNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}