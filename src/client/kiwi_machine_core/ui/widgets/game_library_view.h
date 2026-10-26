// Copyright (C) 2026 Yisi Yu
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

#ifndef UI_WIDGETS_GAME_LIBRARY_VIEW_H_
#define UI_WIDGETS_GAME_LIBRARY_VIEW_H_

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "emulator/emulator.h"
#include "ui/widgets/widget.h"
#include "utility/timer.h"

class CardWidget;
class FlexItemsWidget;
class MainWindow;
class SideMenu;

namespace preset_roms {
enum class ROMEdition;
struct Package;
struct PresetROM;
}  // namespace preset_roms

class GameLibraryView : public Widget {
 public:
  enum class Focus {
    kSideMenu,
    kContents,
  };

  using LoadROMCallback = kiwi::base::RepeatingCallback<
      void(preset_roms::PresetROM&, preset_roms::ROMEdition, bool)>;

  struct Callbacks {
    LoadROMCallback load_rom;
    kiwi::base::RepeatingCallback<void(kiwi::emu::Emulator::Type)>
        switch_emulation_system;
    kiwi::base::RepeatingClosure show_settings;
    kiwi::base::RepeatingClosure show_about;
    kiwi::base::RepeatingClosure quit;
    kiwi::base::RepeatingClosure layout_changed;
  };

  struct SystemSelection {
    int index = 0;
    std::string package_id;
  };

  struct PreferredSelections {
    SystemSelection nes;
    SystemSelection gba;
  };

  GameLibraryView(MainWindow* main_window,
                  kiwi::emu::Emulator::Type emulation_system,
                  PreferredSelections preferred_selections,
                  Callbacks callbacks);
  ~GameLibraryView() override;

  int GetContentLeft();
  std::optional<SystemSelection> GetCurrentSelection(
      kiwi::emu::Emulator::Type system);
  void ChangeFocus(Focus focus);
  void SetEmulationSystem(kiwi::emu::Emulator::Type system);
  void ShowFilter();
  void Layout(bool animate = true);

 protected:
  void Paint() override;
  bool IsWindowless() override;

 private:
  void Initialize();
  void LoadROM(preset_roms::PresetROM& rom,
               preset_roms::ROMEdition edition,
               bool load_from_finger_gesture);
  void CycleEmulationSystem(int direction);
  void SwitchEmulationSystem(kiwi::emu::Emulator::Type system);
  bool IsEmulationSystem(kiwi::emu::Emulator::Type system) const;
  void PopulatePackageItems(preset_roms::Package* package,
                            FlexItemsWidget* items_widget);
  void EnsurePackageIndex(preset_roms::Package* package,
                          FlexItemsWidget* items_widget);
  void OnPackageIndexProgress(FlexItemsWidget* items_widget,
                              size_t completed,
                              size_t total);
  void OnPackageIndexReady(preset_roms::Package* package,
                           FlexItemsWidget* items_widget,
                           bool success);
  FlexItemsWidget* GetSelectedItemsWidget(
      kiwi::emu::Emulator::Type system) const;
  void ApplyPendingPreferredIndex(kiwi::emu::Emulator::Type system,
                                  FlexItemsWidget* items_widget);
  const std::map<int, FlexItemsWidget*>& GetFlexItemsMap(
      kiwi::emu::Emulator::Type system) const;
  const std::map<int, FlexItemsWidget*>& GetCurrentFlexItemsMap() const;
  int GetDefaultMenuIndex(kiwi::emu::Emulator::Type system) const;
  int FindMenuIndexForPackage(kiwi::emu::Emulator::Type system,
                              const std::string& package_id) const;
  void RememberCurrentSideMenuSelection();
  void RememberSideMenuSelection(int menu_index);
  void SwitchToWidgetForSideMenu(int menu_index);
  void SwitchToSideMenuByCurrentFlexItemWidget();

  MainWindow* main_window_ = nullptr;
  kiwi::emu::Emulator::Type emulation_system_;
  Callbacks callbacks_;
  std::vector<FlexItemsWidget*> items_widgets_;
  std::vector<FlexItemsWidget*> gba_items_widgets_;
  std::set<preset_roms::Package*> indexing_packages_;
  std::map<int, FlexItemsWidget*> flex_items_map_;
  std::map<int, FlexItemsWidget*> gba_flex_items_map_;
  std::array<int, 2> selected_menu_indices_ = {-1, -1};
  std::array<std::optional<int>, 2> pending_preferred_indices_;
  std::array<std::string, 2> preferred_package_ids_;
  SideMenu* side_menu_ = nullptr;
  CardWidget* contents_card_widget_ = nullptr;
  Timer side_menu_timer_;
  int side_menu_target_width_ = 0;
  int side_menu_original_width_ = 0;
  std::shared_ptr<int> lifetime_token_ = std::make_shared<int>(0);
};

#endif  // UI_WIDGETS_GAME_LIBRARY_VIEW_H_
