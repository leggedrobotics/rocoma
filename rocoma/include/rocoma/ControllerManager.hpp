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
 * @author  Christian Gehring, Dario Bellicoso, Gabriel Hottiger
 * @date    Oct, 2014
 */

#pragma once

// roco
#include <roco/controllers/controllers.hpp>

// any_worker
#include <any_worker/WorkerManager.hpp>

// Signal logger
#include <signal_logger/signal_logger.hpp>

// STL
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <future>

namespace rocoma {
//! Signal logger options
struct LoggerOptions {
  //! Constructor
  LoggerOptions() = default;

  //! Copy constructor
  LoggerOptions(const LoggerOptions& other) :
      enable{other.enable.load()},
      updateOnStart{other.updateOnStart.load()},
      fileTypes{other.fileTypes}
  {
  }

  //! Copy assignement
  LoggerOptions& operator=(const LoggerOptions& other) {
    enable.store(other.enable.load());
    updateOnStart.store(other.updateOnStart.load());
    fileTypes = other.fileTypes;
    return *this;
  }

  //! Rocoma should handle the logger (start, stop, update)
  std::atomic<bool> enable{true};
  //! If true, the logger start is delayed until the logger data is saved and the logger was updated
  std::atomic<bool> updateOnStart{true};
  //! Log file types
  signal_logger::LogFileTypeSet fileTypes{signal_logger::LogFileType::BINARY};
};

//! Options struct to initialize manager
struct ControllerManagerOptions {
  //! Default Constructor
  ControllerManagerOptions():
    timeStep(0.01),
    isRealRobot(false),
    loggerOptions()
  {

  }

  //! Constructor
  ControllerManagerOptions(const double timeStep,
                           const bool isRealRobot,
                           const LoggerOptions loggerOptions = LoggerOptions()):
    timeStep(timeStep),
    isRealRobot(isRealRobot),
    loggerOptions(loggerOptions)
  {

  }

  //! Copy constructor
  ControllerManagerOptions(const ControllerManagerOptions& other) :
      timeStep{other.timeStep.load()},
      isRealRobot{other.isRealRobot.load()},
      loggerOptions{other.loggerOptions}
  {
  }

  //! Copy assignement
  ControllerManagerOptions& operator=(const ControllerManagerOptions& other) {
    timeStep.store(other.timeStep.load());
    isRealRobot.store(other.isRealRobot.load());
    loggerOptions = other.loggerOptions;
    return *this;
  }

  //! Controller update rate
  std::atomic<double> timeStep;
  //! Simulation flag
  std::atomic<bool> isRealRobot;
  //! Signal logger options
  LoggerOptions loggerOptions;
};

//! Implementation of a controllermanager for adater interfaces
class ControllerManager
{
 public:
  //! Enumeration for switch controller feedback
  enum class SwitchResponse : int {
    NOTFOUND  = -2,
    ERROR     = -1,
    NA        =  0,
    RUNNING   =  1,
    SWITCHING =  2
  };

  //! Enumeration indicating the controller manager state
  enum class State : int {
    FAILURE   = -2,
    EMERGENCY = -1,
    NA        = 0,
    OK        =  1
  };

  //! Enumeration indicating the emergency stop type
  enum class EmergencyStopType : int {
    FAILPROOF   = -1,
    EMERGENCY   =  0
  };

 protected:
  //! Set of controller pointers (normal and emergency controller)
  struct ControllerSetPtr {
    ControllerSetPtr(roco::ControllerAdapterInterface * controller,
                     roco::EmergencyControllerAdapterInterface * emgcyController):
                       controller_(controller),
                       emgcyController_(emgcyController),
                       controllerName_(controller?controller->getControllerName():"none"),
                       emgcyControllerName_(emgcyController?emgcyController->getControllerName():"none")
    {
    }

    roco::ControllerAdapterInterface * controller_;
    roco::EmergencyControllerAdapterInterface * emgcyController_;
    std::string controllerName_;
    std::string emgcyControllerName_;
  };

  //! Convenience typedef for Controller
  using ControllerPtr = std::unique_ptr<roco::ControllerAdapterInterface>;
  using EmgcyControllerPtr = std::unique_ptr<roco::EmergencyControllerAdapterInterface>;
  using FailproofControllerPtr = std::unique_ptr<roco::FailproofControllerAdapterInterface>;

 public:
  /**
   * @brief Constructor
   */
  ControllerManager();

  /**
   * @brief Constructor
   * @param timestep    controller timestep
   * @param isRealRobot simulation flag
   */
  ControllerManager(double timestep,
                    bool isRealRobot,
                    const LoggerOptions & loggerOptions = LoggerOptions());

  /**
   * @brief Constructor
   * @param options Configuration Options of the manager
   */
  ControllerManager(const ControllerManagerOptions & options);

  //! Destructor
  virtual ~ControllerManager() = default;

  /**
   * @brief Initializes the controller manager
   * @param options Configuration Options of the manager
   */
  void init(const ControllerManagerOptions & options);

  /**
   * @brief Add a controller pair to the manager
   * @param controller             Pointer to the controller (unique ptr -> ownership transfer)
   * @param emergencyController    Pointer to the emergency controller (unique ptr -> ownership transfer)
   * @return true, if controllers were created and added successfully
   */
  bool addControllerPair(ControllerPtr&& controller,
                         EmgcyControllerPtr&& emergencyController);
  /**
   * @brief Add a controller pair to the manager
   * @param controller              Pointer to the controller (unique ptr -> ownership transfer)
   * @param emergencyControllerName Name of the emergency controller
   * @return true, if controllers were created and added successfully
   */
  bool addControllerPairWithExistingEmergencyController(ControllerPtr&& controller,
                                                        const std::string & emgcyControllerName);

  /**
   * @brief Sets the failproof controller
   * @param controller             Pointer to the failproof controller (unique ptr -> ownership transfer)
   * @return true, if controllers was created and added successfully
   */
  bool setFailproofController(FailproofControllerPtr&& controller);

  /**
   * @brief Advance the currently active controller
   * @return true, if controller was advanced successfully
   */
  bool updateController();

  /**
   * @brief Prestop and stop controller
   * @return true, if both stopping procedures where successful
   */
  bool emergencyStop();

  /**
   * @brief Clears the emergency stop, controller switches are now allowed
   */
  virtual void clearEmergencyStop();

  /**
   * @brief Returns the emergency stop, controller switches are now allowed
   */
  bool hasClearedEmergencyStop() { return !emergencyStopMustBeCleared_ || hasClearedEmergencyStop_; }

  /**
   * @brief Sets whether emergency stop must be cleared
   */
  void emergencyStopMustBeCleared(bool emergencyStopMustBeCleared) { emergencyStopMustBeCleared_ = emergencyStopMustBeCleared; }

  /**
   * @brief Tries to switch to a desired controller
   * @param controllerName    Name of the desired controller
   * @return result of the switching operation
   */
  SwitchResponse switchController(const std::string & controllerName);

  /**
   * @brief Tries to switch to a desired controller
   * @param controllerName    Name of the desired controller
   * @param response_promise  Reference to a promise in which the result will be stored in (Lifetime of response_promise must be taken care of)
   */
  void switchController(const std::string & controllerName,
                        std::promise<SwitchResponse> & response_promise);

  /**
   * @brief Get a vector of all available controller names
   * @return vector of the available controller names
   */
  std::vector<std::string> getAvailableControllerNames();

  /**
   * @brief Get the current controller name
   * @return name of the currently active controller
   */
  std::string getActiveControllerName();

  /**
   * @brief Get the current state of the manager
   * @return state of the manager
   */
  State getControllerManagerState();

  /**
   * @brief Cleanup all controllers
   * @return true, if successful emergency stop and all controllers are cleaned up
   */
  virtual bool cleanup();

  /**
   * @brief Add a shared module to the controller manager
    * @param sharedModule The shared module to add
    * @return true iff successful
    */
  bool addSharedModule(roco::SharedModulePtr&& sharedModule);

  /**
   * @brief Checks if a shared module with this name was already added
   * @param moduleName The name of the shared module
   * @return true if a module with this name was already added
   */
  bool hasSharedModule(const std::string & moduleName);

  //  /**
  //   * @brief Check timing and perform emergency stop on violation
  //   */
  //  bool checkTimingWorker(const any_worker::WorkerEvent& event);
 protected:
  /**
   * @brief Try to create controller
   * @param controller  Const reference to unique_ptr to the controller
   * @return true, if controller was created and added successfully
   */
  bool createController(const ControllerPtr & controller);

  /**
   * @brief Worker callback stopping the previous controller and notify emergency stop
   * @param event        Worker event
   * @param controller   Pointer to the controller that was active when the emergency stop occured
   * @param stopType     Type of the emergency stop
   * @return true, if controller was stopped successfully
   */
  bool emergencyStopControllerWorker(const any_worker::WorkerEvent& event,
                                     roco::ControllerAdapterInterface * controller,
                                     EmergencyStopType stopType);

  /**
   * @brief notify others of the emergency stop (default: do nothing)
   * @param type     Type of the emergency stop
   */
  virtual void notifyEmergencyStop(EmergencyStopType type) { }

  /**
   * @brief notify others of the emergency stop (default: do nothing)
   * @param type     Type of the emergency stop
   */
  virtual void notifyControllerChanged(const std::string & newControllerName) { }

  /**
   * @brief notify others of a controller state change (default: do nothing)
   * @param state     New state of the emergency stop
   */
  virtual void notifyControllerManagerStateChanged(State state) { }

  /**
   * @brief Worker callback switching the controller
   * @param event        Worker event
   * @param oldController   Pointer to the controller that is currently active
   * @param newController   Pointer to the controller that shall be switched to
   * @return true, if controller switching was successful
   */
  bool switchControllerWorker(const any_worker::WorkerEvent& event,
                              roco::ControllerAdapterInterface * oldController,
                              roco::ControllerAdapterInterface * newController,
                              std::promise<SwitchResponse> & response_promise);

 protected:
  //! Conditional variables for measuring execution time
  //  std::atomic_bool updating_;
  //  std::condition_variable timerStart_;
  //  std::condition_variable timerStop_;
  //  std::atomic<double> minimalRealtimeFactor_;

  //! True, iff initialized
  std::atomic<bool> isInitialized_;

  //! Options
  ControllerManagerOptions options_;

  //! Flag indicating if emergency stop was cleared
  std::atomic<bool> emergencyStopMustBeCleared_;
  std::atomic<bool> hasClearedEmergencyStop_;

  //! Current controller state
  std::atomic<State> activeControllerState_;

  //! Stopping controllers
  any_worker::WorkerManager workerManager_;

  //! Unordered map of all available controllers (owned by the manager)
  std::unordered_map< std::string, ControllerPtr > controllers_;
  std::unordered_map< std::string, EmgcyControllerPtr > emergencyControllers_;
  std::unordered_map< std::string, roco::SharedModulePtr > sharedModules_;

  //! Controller Pairs
  std::unordered_map< std::string, ControllerSetPtr > controllerPairs_;
  ControllerSetPtr activeControllerPair_;

  //! Emergency stop controller
  FailproofControllerPtr failproofController_;

  //! Controller Mutex
  std::mutex controllerMutex_;
  //! Emergency Controller Mutex
  std::mutex emergencyControllerMutex_;
  //! Failproof Controller Mutex
  std::mutex failproofControllerMutex_;
  //! Mutex protecting emergency stop function call
  std::mutex emergencyStopMutex_;
  //! Mutex protecting update Controller function call
  std::mutex updateControllerMutex_;
  //! Mutex protecting switch Controller function call
  std::mutex switchControllerMutex_;
  //! Mutex protecting worker manager
  std::mutex workerManagerMutex_;
  //! Mutex protecting active controller
  std::mutex activeControllerMutex_;

};

} /* namespace rocoma */
