/**
* This file is part of ORB-SLAM2.
*
* Copyright (C) 2014-2016 Raúl Mur-Artal <raulmur at unizar dot es> (University of Zaragoza)
* For more information see <https://github.com/raulmur/ORB_SLAM2>
*
* ORB-SLAM2 is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* ORB-SLAM2 is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with ORB-SLAM2. If not, see <http://www.gnu.org/licenses/>.
*/


#include<iostream>
#include<algorithm>
#include<fstream>
#include<chrono>

#include<ros/ros.h>
#include <cv_bridge/cv_bridge.h>
#include <tf/transform_datatypes.h>
#include <tf/transform_listener.h>
#include <tf/transform_broadcaster.h>
#include <geometry_msgs/PoseWithCovarianceStamped.h>

#include<opencv2/core/core.hpp>

#include"../../../include/System.h"

using namespace std;

ros::Publisher pose_pub_;

class ImageGrabber
{
public:
    ImageGrabber(ORB_SLAM2::System* pSLAM):mpSLAM(pSLAM){}

    void GrabImage(const sensor_msgs::ImageConstPtr& msg);

    ORB_SLAM2::System* mpSLAM;
};

int main(int argc, char **argv)
{
    ros::init(argc, argv, "Mono");
    ros::start();

    if(argc != 3)
    {
        cerr << endl << "Usage: rosrun ORB_SLAM2 Mono path_to_vocabulary path_to_settings" << endl;        
        ros::shutdown();
        return 1;
    }    

    // Create SLAM system. It initializes all system threads and gets ready to process frames.
    ORB_SLAM2::System SLAM(argv[1],argv[2],ORB_SLAM2::System::MONOCULAR,true);

    ImageGrabber igb(&SLAM);

    ros::NodeHandle nodeHandler;
    ros::Subscriber sub = nodeHandler.subscribe("/camera/image_raw", 1, &ImageGrabber::GrabImage,&igb);

    pose_pub_ = nodeHandler.advertise<geometry_msgs::PoseWithCovarianceStamped>("camera_pose", 1);
    ros::spin();

    // Stop all threads
    SLAM.Shutdown();

    // Save camera trajectory
    SLAM.SaveKeyFrameTrajectoryTUM("KeyFrameTrajectory.txt");

    ros::shutdown();

    return 0;
}

void ImageGrabber::GrabImage(const sensor_msgs::ImageConstPtr& msg)
{
    // Copy the ros image message to cv::Mat.
    cv_bridge::CvImageConstPtr cv_ptr;
    try
    {
        cv_ptr = cv_bridge::toCvShare(msg);
    }
    catch (cv_bridge::Exception& e)
    {
        ROS_ERROR("cv_bridge exception: %s", e.what());
        return;
    }

    cv::Mat pose = mpSLAM->TrackMonocular(cv_ptr->image,cv_ptr->header.stamp.toSec());

    if (pose.empty())
        return;

    // transform into right handed camera frame
    tf::Matrix3x3 rh_cameraPose(  - pose.at<float>(0,0),   pose.at<float>(0,1),   pose.at<float>(0,2),
                                  - pose.at<float>(1,0),   pose.at<float>(1,1),   pose.at<float>(1,2),
                                    pose.at<float>(2,0), - pose.at<float>(2,1), - pose.at<float>(2,2));

    tf::Vector3 rh_cameraTranslation( pose.at<float>(0,3),pose.at<float>(1,3), - pose.at<float>(2,3) );

    //rotate 270deg about z and 270deg about x
    tf::Matrix3x3 rotation270degZX( 0, 0, 1,
                                   -1, 0, 0,
                                    0,-1, 0);

    //publish right handed, x forward, y right, z down (NED)
    //static tf::TransformBroadcaster br;
    //tf::Transform transformCoordSystem = tf::Transform(rotation270degZX,tf::Vector3(0.0, 0.0, 0.0));
    //br.sendTransform(tf::StampedTransform(transformCoordSystem, ros::Time::now(), "camera_link", "camera_pose"));
//
    //tf::Transform transformCamera = tf::Transform(rh_cameraPose,rh_cameraTranslation);
    //br.sendTransform(tf::StampedTransform(transformCamera, ros::Time::now(), "camera_pose", "pose"));

    geometry_msgs::PoseWithCovarianceStamped camera_pose;
    camera_pose.header.stamp = ros::Time().now();
    camera_pose.header.frame_id = 'map';
    camera_pose.pose.pose.position.x = pose.at<float>(0,3);
    camera_pose.pose.pose.position.y = pose.at<float>(1,3);
    camera_pose.pose.pose.position.z = pose.at<float>(2,3);
    tf::Quaternion q;
    rh_cameraPose.getRotation(q);
    camera_pose.pose.pose.orientation.x = q.getX();
    camera_pose.pose.pose.orientation.y = q.getY();
    camera_pose.pose.pose.orientation.z = q.getZ();
    camera_pose.pose.pose.orientation.w = q.getW();

    pose_pub_.publish(camera_pose);
}


