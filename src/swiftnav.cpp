/**
 *      _____
 *     /  _  \
 *    / _/ \  \
 *   / / \_/   \
 *  /  \_/  _   \  ___  _    ___   ___   ____   ____   ___   _____  _   _
 *  \  / \_/ \  / /  _\| |  | __| / _ \ | ++ \ | ++ \ / _ \ |_   _|| | | |
 *   \ \_/ \_/ /  | |  | |  | ++ | |_| || ++ / | ++_/| |_| |  | |  | +-+ |
 *    \  \_/  /   | |_ | |_ | ++ |  _  || |\ \ | |   |  _  |  | |  | +-+ |
 *     \_____/    \___/|___||___||_| |_||_| \_\|_|   |_| |_|  |_|  |_| |_|
 *             ROBOTICS
 *`
 *  File: swiftnav.cpp
 *  Desc: Wrapper for to-fix and from-fix functionality of libswiftnav
 *  Auth: Ryan Gariepy
 *
 *  Copyright (c) 2013, Clearpath Robotics, Inc. 
 *  All Rights Reserved
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Clearpath Robotics, Inc. nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CLEARPATH ROBOTICS, INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * Please send comments, questions, or patches to skynet@clearpathrobotics.com 
 *
 */

#include "enu/swiftnav.h"

#include "ros/ros.h"
#include "sensor_msgs/NavSatFix.h"
#include "nav_msgs/Odometry.h"

#include <boost/bind.hpp>

extern "C" {
  // The backend of this node is an included C library called libswiftnav.
  #include "coord_system.h"
}

#define TO_RADIANS (M_PI/180)
#define TO_DEGREES (180/M_PI)

void llh_to_ecef(sensor_msgs::NavSatFix llh, double ecef[3]) {
  // The datum point is stored as an ECEF, for mathematical reasons.
  // We convert it here, using the appropriate function from
  // libswiftnav.
  double llh_array[3] = { llh.latitude * TO_RADIANS,
                          llh.longitude * TO_RADIANS,
                          llh.altitude };
  wgsllh2ecef(llh_array, ecef);
}

nav_msgs::Odometry llh_to_enu(sensor_msgs::NavSatFixConstPtr fix_ptr,
                              double ecef_datum[3],
                              const std::string& output_tf_frame,
                              double invalid_covariance_value)
{

  // Prepare the appropriate input vector to convert the input latlon
  // to an ECEF triplet.
  double llh[3] = { fix_ptr->latitude * TO_RADIANS,
                    fix_ptr->longitude * TO_RADIANS,
                    fix_ptr->altitude };
  double ecef[3];
  wgsllh2ecef(llh, ecef);
  
  // ECEF triplet is converted to north-east-down (NED), by combining it
  // with the ECEF-formatted datum point.
  double ned[3];
  wgsecef2ned_d(ecef, ecef_datum, ned);

  nav_msgs::Odometry odom_msg;
  odom_msg.header.stamp = fix_ptr->header.stamp;
  odom_msg.header.frame_id = output_tf_frame; // Name of output tf frame
  odom_msg.child_frame_id = fix_ptr->header.frame_id; // Antenna location
  odom_msg.pose.pose.position.x = ned[1];
  odom_msg.pose.pose.position.y = ned[0];
  odom_msg.pose.pose.position.z = -ned[2];

  // We only need to populate the diagonals of the covariance matrix; the
  // rest initialize to zero automatically, which is correct as the
  // dimensions of the state are independent.
  odom_msg.pose.covariance[0] = fix_ptr->position_covariance[0];
  odom_msg.pose.covariance[7] = fix_ptr->position_covariance[4];
  odom_msg.pose.covariance[14] = fix_ptr->position_covariance[8];
  
  // Do not use orientation dimensions from GPS.
  // (-1 is an invalid covariance and standard ROS practice to set as invalid.)
  odom_msg.pose.covariance[21] = invalid_covariance_value;
  odom_msg.pose.covariance[28] = invalid_covariance_value;
  odom_msg.pose.covariance[35] = invalid_covariance_value;

  return odom_msg;

}
