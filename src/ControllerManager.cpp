/*
 * Copyright (c) 2014, Christian Gehring
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Autonomous Systems Lab, ETH Zurich nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL  Christian Gehring BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/
/*!
 * @file    ControllerManager.hpp
 * @author  Christian Gehring
 * @date    Oct, 2014
 */
#include "locomotion_controller/ControllerManager.hpp"
#include "locomotion_controller/ControllerRos.hpp"


namespace locomotion_controller {

ControllerManager::ControllerManager() :
    timeStep_(0.0),
    isInitializingTask_(false),
    controllers_(),
    availableControllers_(nullptr),
    isRealRobot_(false)
{

}

ControllerManager::~ControllerManager()
{
}

void ControllerManager::setupControllers(double dt, robotModel::State& state, robotModel::Command& command)  {
  timeStep_ = dt;

  /* Create no task, which is active until estimator converged*/
  auto controller = new ControllerRos<robot_controller::RocoFreeze>(state, command);
  controller->setControllerManager(this);
  //controller->setIsCheckingState(false);
  addController(controller);
  availableControllers_ = &controllers_.back();

  if (!availableControllers_->initializeController(timeStep_)) {
    ROS_FATAL("Could not initialized NoTask!");
  }



  add_locomotion_controllers(this, state, command);

}

void ControllerManager::addController(ControllerPtr controller)  {
  controllers_.push_back(controller);
//  controller = &controllers_.back();


  ROS_INFO("Added Task %s.", controller->getName().c_str());
  if (!controller->createController(timeStep_)) {
    std::string error = "Could not add controller " +  controller->getName() + "!";
    throw std::runtime_error(error);
  }
}




void ControllerManager::updateController() {
  availableControllers_->advanceController(timeStep_);
}

bool ControllerManager::emergencyStop() {
 availableControllers_->stopController();
 return true;
}

bool ControllerManager::switchControllerAfterEmergencyStop() {
  this->switchToEmergencyTask();
 return true;
}

void ControllerManager::switchToEmergencyTask() {
  for (auto& controller : controllers_) {
    if (controller.getName() == "No Task") {
      availableControllers_ = &controller;
      availableControllers_->initializeController(timeStep_);
      return;
    }
  }
  throw std::runtime_error("No Task not found!");
}


bool ControllerManager::switchController(locomotion_controller_msgs::SwitchController::Request  &req,
                                         locomotion_controller_msgs::SwitchController::Response &res)
{

  //--- Check if controller is already active
  if (req.name == availableControllers_->getName()) {
    res.status = res.STATUS_RUNNING;
    ROS_INFO("Controller is already running!");
    return true;
  }

  for (auto& controller : controllers_) {
    if (req.name == controller.getName()) {
      ControllerPtr initController = &controller;

//      isInitializingTask_ = true;
      initController->initializeController(timeStep_);
      if (initController->isInitialized()) {
        res.status = res.STATUS_SWITCHED;
        ROS_INFO("Switched to controller %s", initController->getName().c_str());
        availableControllers_ = initController;
      }
      else {
        // switch to no task
        switchToEmergencyTask();
        res.status = res.STATUS_ERROR;
        ROS_INFO("Could not switched to controller %s", initController->getName().c_str());
      }
      return true;
    }
  }
  res.status = res.STATUS_NOTFOUND;
  ROS_INFO("Controller %s not found!", req.name.c_str());
  return true;
}

bool ControllerManager::getAvailableControllers(locomotion_controller_msgs::GetAvailableControllers::Request &req,
                                                locomotion_controller_msgs::GetAvailableControllers::Response &res)
{

  for (auto& controller : controllers_) {
    res.availableControllers.push_back(controller.getName());
  }

  return true;
}

bool ControllerManager::isRealRobot() const {
	return isRealRobot_;
}
void ControllerManager::setIsRealRobot(bool isRealRobot) {
	isRealRobot_ = isRealRobot;
}

} /* namespace locomotion_controller */
