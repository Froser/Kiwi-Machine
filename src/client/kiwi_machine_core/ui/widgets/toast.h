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

#ifndef UI_WIDGETS_TOAST_H_
#define UI_WIDGETS_TOAST_H_

#include <string>

#include "base/time/time.h"
#include "ui/widgets/widget.h"
#include "utility/timer.h"

// A demo widget shows IMGui's demo.
class Toast : public Widget {
 public:
  static void ShowToast(
      WindowBase* window_base,
      const std::string& message,
      kiwi::base::TimeDelta duration = kiwi::base::Milliseconds(1000));
  ~Toast() override;

 protected:
  explicit Toast(WindowBase* window_base,
                 const std::string& message,
                 kiwi::base::TimeDelta duration);

 protected:
  // Widget:
  void Paint() override;

 private:
  Timer elapsed_timer_;
  bool first_paint_ = true;
  std::string message_;
  kiwi::base::TimeDelta duration_;
};

#endif  // UI_WIDGETS_TOAST_H_