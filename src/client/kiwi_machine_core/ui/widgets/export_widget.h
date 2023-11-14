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

#ifndef UI_WIDGETS_EXPORT_WIDGET_H_
#define UI_WIDGETS_EXPORT_WIDGET_H_

#include "ui/widgets/widget.h"

#include <kiwi_nes.h>

// A demo widget shows IMGui's demo.
class ExportWidget : public Widget {
 public:
  explicit ExportWidget(WindowBase* window_base);
  ~ExportWidget() override;

 public:
  void Start(int max, const kiwi::base::FilePath& export_path);
  void SetCurrent(const kiwi::base::FilePath& file);
  void Succeeded(const kiwi::base::FilePath& file);
  void Failed(const kiwi::base::FilePath& file);
  void Done();

 protected:
  // Widget:
  void Paint() override;

 private:
  kiwi::base::FilePath export_path_;
  std::vector<kiwi::base::FilePath> succeeded_files_;
  std::vector<kiwi::base::FilePath> failed_files_;
  int max_ = 1;
  int current_ = 0;
  std::string current_text_;
  bool is_started_ = false;
};

#endif  // UI_WIDGETS_EXPORT_WIDGET_H_