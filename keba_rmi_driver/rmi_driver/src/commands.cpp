/*
 * Copyright (c) 2017, Doug Smith
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 *  Created on: Aug 3, 2017
 *      Author: Doug Smith
 */

#include "rmi_driver/commands.h"
#include <robot_movement_interface/Command.h>
#include <ros/ros.h>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <vector>
#include "rmi_driver/util.h"

namespace rmi_driver
{
// Begin Command::

const robot_movement_interface::Command& CommandHandler::getSampleCommand() const
{
  return sample_command_;
}

std::string Command::paramsToString(const std::vector<float>& floatVec, int precision)
{
  if (floatVec.empty())
    return "";

  std::ostringstream oss;

  std::for_each(floatVec.begin(), floatVec.end() - 1, [&](const float& fval) {
    auto str_val = util::floatToStringNoTrailing(fval, precision);
    oss << str_val << " ";

  });

  oss << util::floatToStringNoTrailing(floatVec.back(), precision);

  auto out_str = oss.str();

  return out_str;
}

std::string Command::toString(bool append_newline) const
{
  std::ostringstream oss;

  for (auto&& cmd : full_command_)
  {
    oss << cmd.first;
    if (cmd.second.length() > 0)
      oss << " : " << cmd.second;
    oss << ";";
  }
  if (append_newline)
    oss << "\n";

  std::string ret = oss.str();
  return ret;
}

bool Command::checkResponse(std::string& response) const
{
  if (response.compare("error") != 0)
    return true;
  else
    return false;
}

void Command::makeCommand(CommandType type, std::string command, std::string params, bool erase_params)
{
  type_ = type;
  if (erase_params)
    full_command_.clear();

  if (full_command_.size() > 0)
    full_command_[0] = std::make_pair(command, params);  //@todo check this
  else
    full_command_.emplace_back(command, params);
}

void Command::addParam(std::string param, std::string param_vals)
{
  if (full_command_.empty())
    full_command_[1] = std::make_pair(param, param_vals);
  else
    full_command_.emplace_back(param, param_vals);
}

// Eclipse has a fit every time I try to call resize(int) or construct the vector with a size,
// even though it compiles fine, so I have no way to guarantee that the vector isn't empty.
// So, I need to check at() and return a string, not a reference to one.
std::string Command::getCommand() const
{
  try
  {
    return full_command_.at(0).first;
  }
  catch (const std::out_of_range& oor)
  {
    return "";
  }
}

Command::CommandType Command::getType() const
{
  return type_;
}

void Command::setType(CommandType type)
{
  type_ = type;
}

int Command::getCommandId() const
{
  return command_id_;
}

void Command::setCommandId(int commandId)
{
  command_id_ = commandId;
}

// Begin CommandHandler::

CommandHandler::CommandHandler(const robot_movement_interface::Command& sample_command, CommandHandlerFunc f)
  : sample_command_(sample_command), process_func_(std::move(f))
{
}

bool CommandHandler::operator==(const robot_movement_interface::Command& cmd_msg)
{
  // Check strings for usage and equality
  // Check vectors for usage and length
  if (util::usedAndNotEqual(sample_command_.command_type, cmd_msg.command_type))
    return false;

  if (util::usedAndNotEqual(sample_command_.pose_reference, cmd_msg.pose_reference))
    return false;

  if (util::usedAndNotEqual(sample_command_.pose_type, cmd_msg.pose_type))
    return false;

  if (util::usedAndNotEqual(sample_command_.pose, cmd_msg.pose))
    return false;

  if (util::usedAndNotEqual(sample_command_.velocity_type, cmd_msg.velocity_type))
    return false;

  return true;  // If it got this far it's a match
}

std::ostream& CommandHandler::dump(std::ostream& o) const
{
  o << "CommandHandler " << getName() << " criteria: " << std::endl;

  if (sample_command_.command_type.length() > 0)
    o << "command_type:" << sample_command_.command_type << std::endl;

  if (sample_command_.pose_reference.length() > 0)
    o << "pose_reference:" << sample_command_.pose_reference << std::endl;
  if (sample_command_.pose_type.length() > 0)
    o << "pose_type:" << sample_command_.pose_type << std::endl;

  if (sample_command_.velocity_type.length() > 0)
    o << "velocity_type:" << sample_command_.velocity_type << std::endl;

  if (sample_command_.velocity.size() > 0)
    o << "velocity (size):" << sample_command_.velocity.size() << std::endl;

  if (sample_command_.pose.size() > 0)
    o << "pose (size):" << sample_command_.pose.size() << std::endl;

  return o;
}

const CommandHandler* CommandRegister::findHandler(const robot_movement_interface::Command& msg_cmd)
{
  auto foundItem = std::find_if(this->handlers().begin(), this->handlers().end(),
                                [&](const std::unique_ptr<CommandHandler>& p) { return *p.get() == msg_cmd; });

  if (foundItem != std::end(this->handlers()))
  {
    return foundItem->get();
  }
  else
  {
    return nullptr;
  }
}

CommandPtr CommandHandler::processMsg(const robot_movement_interface::Command& cmd_msg) const
{
  if (!process_func_)
  {
    ROS_ERROR_STREAM("Base CommandHandler::processMsg was called but the process function was not set!");
    return false;
  }

  // proceess_func_ will return a shared_ptr<Command>
  auto ret = process_func_(cmd_msg);

  return ret;
}

}  // namespace rmi_driver
