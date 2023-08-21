// Copyright (C) 2023 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#ifndef BASE_MESSAGE_PUMP_TYPE_H_
#define BASE_MESSAGE_PUMP_TYPE_H_

namespace kiwi::base {

enum class MessagePumpType {
  // This type of pump only supports tasks and timers.
  DEFAULT,

  // This type of pump also supports native UI events (e.g., Windows
  // messages).
  UI,

  // This type of pump also supports asynchronous IO.
  IO,
};

}  // namespace kiwi::base

#endif  // BASE_MESSAGE_PUMP_TYPE_H_