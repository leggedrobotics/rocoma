/*
 * LocomotionController.hpp
 *
 *  Created on: Oct 30, 2014
 *      Author: gech
 */

#ifndef LOCOMOTIONCONTROLLER_HPP_
#define LOCOMOTIONCONTROLLER_HPP_

#include <ros/ros.h>
#include <roscpp_nodewrap/NodeImpl.h>
#include <roscpp_nodewrap/Nodelet.h>

#include <starleth_msgs/RobotState.h>
#include <sensor_msgs/Joy.h>
#include <starleth_msgs/SeActuatorCommands.h>


#include "locomotion_controller/Model.hpp"
#include "locomotion_controller/ControllerManager.hpp"



#include <kindr/rotations/RotationEigen.hpp>
#include <kindr/rotations/RotationDiffEigen.hpp>
#include <kindr/phys_quant/PhysicalQuantitiesEigen.hpp>


#include <memory>



namespace locomotion_controller {

class LocomotionController : public nodewrap::NodeImpl
{
 public:
  typedef kindr::rotations::eigen_impl::RotationQuaternionPD RotationQuaternion;
  typedef kindr::rotations::eigen_impl::EulerAnglesZyxPD EulerAnglesZyx;
  typedef kindr::rotations::eigen_impl::LocalAngularVelocityPD LocalAngularVelocity;
  typedef kindr::rotations::eigen_impl::AngleAxisPD AngleAxis;
  typedef kindr::phys_quant::eigen_impl::Position3D Position;
  typedef kindr::phys_quant::eigen_impl::Velocity3D LinearVelocity;
  typedef kindr::phys_quant::eigen_impl::VectorTypeless3D Vector;
 public:
  LocomotionController();
  virtual ~LocomotionController();

  void init();
  bool run();


 protected:
  void publish();
  void robotStateCallback(const starleth_msgs::RobotState::ConstPtr& msg);
  void joystickCallback(const sensor_msgs::Joy::ConstPtr& msg);


 private:
  ros::Subscriber robotStateSubscriber_;
  ros::Subscriber joystickSubscriber_;
  ros::Publisher jointCommandsPublisher_;
  ros::ServiceServer switchControllerService_;

  starleth_msgs::SeActuatorCommandsPtr jointCommands_;
  double timeStep_;
  double time_;
  model::Model model_;
  ControllerManager controllerManager_;



};

} /* namespace locomotion_controller */

#endif /* LOCOMOTIONCONTROLLER_HPP_ */
