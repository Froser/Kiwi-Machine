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

#include "ui/widgets/game_library_view.h"
#include "build/build_config.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "build/kiwi_defines.h"
#include "preset_roms/preset_roms.h"
#include "resources/image_resources.h"
#include "resources/string_resources.h"
#include "ui/application.h"
#include "ui/main_window.h"
#include "ui/widgets/card_widget.h"
#include "ui/widgets/flex_items_widget.h"
#include "ui/widgets/side_menu.h"
#include "utility/algorithm.h"
#include "utility/localization.h"
#include "utility/math.h"
#include "utility/zip_reader.h"

namespace {

constexpr int kSideMenuAnimationMs = 180;
constexpr std::array<kiwi::emu::Emulator::Type, 2> kEmulationSystems = {
    kiwi::emu::Emulator::Type::kNES,
    kiwi::emu::Emulator::Type::kGBA,
};

size_t GetSystemIndex(kiwi::emu::Emulator::Type system) {
  return system == kiwi::emu::Emulator::Type::kGBA ? 1 : 0;
}

float EaseOutQuadratic(float progress) {
  progress = std::clamp(progress, 0.f, 1.f);
  return 1.f - (1.f - progress) * (1.f - progress);
}

bool HasHDVersion(const preset_roms::PresetROM* rom) {
#if BUILDFLAG(ENABLE_HD_TEXTURE)
  return rom->hd_texture_toggle_available || rom->hd_edition_available;
#else
  return false;
#endif
}

bool HasSwitchableHDTexture(const preset_roms::PresetROM* rom) {
#if BUILDFLAG(ENABLE_HD_TEXTURE)
  return rom->hd_texture_toggle_available;
#else
  return false;
#endif
}

bool PackageMatchesSystem(const preset_roms::Package* package,
                          kiwi::emu::Emulator::Type system) {
  return (package->GetType() == preset_roms::PackageType::kGBA) ==
         (system == kiwi::emu::Emulator::Type::kGBA);
}

bool MatchesCurrentLanguage(const preset_roms::PresetROM* rom) {
  const SupportedLanguage current_language = GetCurrentSupportedLanguage();
  if (current_language == SupportedLanguage::kEnglish &&
      rom->region == preset_roms::Region::kUSA) {
    return true;
  }

#if !DISABLE_JAPANESE_FONT
  if (current_language == SupportedLanguage::kJapanese &&
      rom->region == preset_roms::Region::kJapan) {
    return true;
  }
#endif

#if !DISABLE_CHINESE_FONT
  if (current_language == SupportedLanguage::kSimplifiedChinese &&
      (rom->region == preset_roms::Region::kCN ||
       rom->region == preset_roms::Region::kJapan)) {
    return true;
  }
#endif

  return false;
}

preset_roms::PresetROM* FindPreferredROM(
    const std::vector<preset_roms::PresetROM*>& roms) {
  for (preset_roms::PresetROM* rom : roms) {
    if (HasHDVersion(rom) && MatchesCurrentLanguage(rom)) {
      return rom;
    }
  }
  for (preset_roms::PresetROM* rom : roms) {
    if (HasHDVersion(rom)) {
      return rom;
    }
  }
  for (preset_roms::PresetROM* rom : roms) {
    if (MatchesCurrentLanguage(rom)) {
      return rom;
    }
  }
  return roms.front();
}

class SideMenuTitleStringUpdater : public LocalizedStringUpdater {
 public:
  explicit SideMenuTitleStringUpdater(preset_roms::Package* package)
      : package_(package) {}
  ~SideMenuTitleStringUpdater() override = default;

 protected:
  std::string GetLocalizedString() override {
    SDL_assert(package_);
    return package_->GetTitleForLanguage(GetCurrentSupportedLanguage());
  }

  std::string GetCollateStringHint() override { return GetLocalizedString(); }

  bool IsTitleMatchedFilter(const std::string& filter,
                            int& similarity) override {
    SDL_assert(false);
    return false;
  }

 private:
  preset_roms::Package* package_;
};

class StringUpdater : public LocalizedStringUpdater {
 public:
  explicit StringUpdater(int string_id) : string_id_(string_id) {}
  ~StringUpdater() override = default;

 protected:
  std::string GetLocalizedString() override {
    return ::GetLocalizedString(string_id_);
  }

  std::string GetCollateStringHint() override {
    return ::GetLocalizedString(string_id_);
  }

  bool IsTitleMatchedFilter(const std::string& filter,
                            int& similarity) override {
    SDL_assert(false);
    return false;
  }

 private:
  int string_id_;
};

void AppendROMFilterStrings(const preset_roms::PresetROM& rom,
                            preset_roms::ROMEdition edition,
                            std::vector<std::string>* filter_strings) {
  filter_strings->push_back(GetROMLocalizedTitle(rom, edition));
  filter_strings->push_back(rom.name);
  filter_strings->push_back(
      language_conversion::KanaToRomaji(GetROMLocalizedCollateStringHint(rom)));
}

class ROMTitleUpdater : public LocalizedStringUpdater {
 public:
  ROMTitleUpdater(
      const preset_roms::PresetROM& preset_rom,
      preset_roms::ROMEdition edition,
      std::vector<const preset_roms::PresetROM*> filter_aliases = {})
      : preset_rom_(preset_rom),
        edition_(edition),
        filter_aliases_(std::move(filter_aliases)) {}
  ~ROMTitleUpdater() override = default;

 protected:
  std::string GetLocalizedString() override {
    return GetROMLocalizedTitle(preset_rom_, edition_);
  }

  std::string GetCollateStringHint() override {
    std::string title = GetROMLocalizedCollateStringHint(preset_rom_);
#if BUILDFLAG(ENABLE_HD_TEXTURE)
    if (edition_ == preset_roms::ROMEdition::kHD)
      title.append(" HD");
#endif
    return title;
  }

  std::vector<std::string> GetFilterStrings() override {
    std::vector<std::string> filter_strings;
    AppendROMFilterStrings(preset_rom_, edition_, &filter_strings);
    for (const preset_roms::PresetROM* alias : filter_aliases_) {
      if (alias != &preset_rom_) {
        AppendROMFilterStrings(*alias, preset_roms::ROMEdition::kOriginal,
                               &filter_strings);
      }
    }
    return filter_strings;
  }

  bool IsTitleMatchedFilter(const std::string& filter,
                            int& similarity) override {
    for (const std::string& candidate : GetFilterStrings()) {
      if (HasString(candidate, filter)) {
        similarity = static_cast<int>(candidate.size()) -
                     static_cast<int>(filter.size());
        return true;
      }
    }
    return false;
  }

 private:
  const preset_roms::PresetROM& preset_rom_;
  preset_roms::ROMEdition edition_;
  std::vector<const preset_roms::PresetROM*> filter_aliases_;
};

}  // namespace

GameLibraryView::GameLibraryView(MainWindow* main_window,
                                 kiwi::emu::Emulator::Type emulation_system,
                                 PreferredSelections preferred_selections,
                                 Callbacks callbacks)
    : Widget(main_window),
      main_window_(main_window),
      emulation_system_(emulation_system),
      callbacks_(std::move(callbacks)) {
  SDL_assert(main_window_);
  SDL_assert(callbacks_.load_rom);
  SDL_assert(callbacks_.switch_emulation_system);
  SDL_assert(callbacks_.show_settings);
  SDL_assert(callbacks_.show_about);
  SDL_assert(callbacks_.layout_changed);
  const size_t nes_index = GetSystemIndex(kiwi::emu::Emulator::Type::kNES);
  const size_t gba_index = GetSystemIndex(kiwi::emu::Emulator::Type::kGBA);
  pending_preferred_indices_[nes_index] = preferred_selections.nes.index;
  pending_preferred_indices_[gba_index] = preferred_selections.gba.index;
  preferred_package_ids_[nes_index] =
      std::move(preferred_selections.nes.package_id);
  preferred_package_ids_[gba_index] =
      std::move(preferred_selections.gba.package_id);
  Initialize();
}

GameLibraryView::~GameLibraryView() {
  lifetime_token_.reset();
}

void GameLibraryView::Initialize() {
  std::unique_ptr<CardWidget> contents_card_widget =
      std::make_unique<CardWidget>(main_window_);
  contents_card_widget_ = contents_card_widget.get();
  AddWidget(std::move(contents_card_widget));

  const auto& packages = preset_roms::GetPresetOrTestRomsPackages();
  size_t package_index = 0;
  for (preset_roms::Package* package : packages) {
    std::unique_ptr<FlexItemsWidget> items_widget =
        std::make_unique<FlexItemsWidget>(main_window_);
    items_widgets_.push_back(items_widget.get());
    items_widget->set_back_callback(kiwi::base::BindRepeating(
        &GameLibraryView::ChangeFocus, kiwi::base::Unretained(this),
        Focus::kSideMenu));
    SDL_assert(package->GetRomsCount() > 0);
    contents_card_widget_->AddWidget(std::move(items_widget));

    std::unique_ptr<FlexItemsWidget> gba_items_widget =
        std::make_unique<FlexItemsWidget>(main_window_);
    gba_items_widget->SetEmulationSystem(kiwi::emu::Emulator::Type::kGBA);
    gba_items_widget->SetEmptyMessage(
        package->IsMainPackage() ? string_resources::IDR_GBA_EMPTY_LIBRARY
                                 : string_resources::IDR_GBA_EMPTY_SPECIALS);
    gba_items_widget->set_back_callback(kiwi::base::BindRepeating(
        &GameLibraryView::ChangeFocus, kiwi::base::Unretained(this),
        Focus::kSideMenu));
    gba_items_widgets_.push_back(gba_items_widget.get());
    contents_card_widget_->AddWidget(std::move(gba_items_widget));

    if (IsPackageIndexReady(package)) {
      FlexItemsWidget* target =
          package->GetType() == preset_roms::PackageType::kGBA
              ? gba_items_widgets_.back()
              : items_widgets_.back();
      PopulatePackageItems(package, target);
    }
    ++package_index;
  }

  std::unique_ptr<SideMenu> side_menu =
      std::make_unique<SideMenu>(main_window_);
  if (packages.empty()) {
    side_menu->set_auto_trigger_first_item(false);
  }
  side_menu->set_activate(true);

  package_index = 0;
  for (preset_roms::Package* package : packages) {
    FlexItemsWidget* items_widget = items_widgets_[package_index];
    FlexItemsWidget* gba_items_widget = gba_items_widgets_[package_index];
    if (package->GetType() == preset_roms::PackageType::kGBA) {
      gba_flex_items_map_[static_cast<int>(package_index)] = gba_items_widget;
    } else {
      flex_items_map_[static_cast<int>(package_index)] = items_widget;
    }

    SideMenu::MenuCallbacks menu_callbacks;
    menu_callbacks.trigger_callback = kiwi::base::BindRepeating(
        [](GameLibraryView* view, preset_roms::Package* package,
           FlexItemsWidget* nes_items_widget, FlexItemsWidget* gba_items_widget,
           int menu_index) {
          if (PackageMatchesSystem(package, view->emulation_system_)) {
            view->RememberSideMenuSelection(menu_index);
            view->SwitchToWidgetForSideMenu(menu_index);
            FlexItemsWidget* target =
                package->GetType() == preset_roms::PackageType::kGBA
                    ? gba_items_widget
                    : nes_items_widget;
            view->EnsurePackageIndex(package, target);
          }
        },
        kiwi::base::Unretained(this), kiwi::base::Unretained(package),
        kiwi::base::Unretained(items_widget),
        kiwi::base::Unretained(gba_items_widget));
    menu_callbacks.enter_callback = kiwi::base::BindRepeating(
        &GameLibraryView::ChangeFocus, kiwi::base::Unretained(this),
        Focus::kContents);
    menu_callbacks.is_visible = kiwi::base::BindRepeating(
        [](GameLibraryView* view, preset_roms::Package* package) {
          return PackageMatchesSystem(package, view->emulation_system_);
        },
        kiwi::base::Unretained(this), kiwi::base::Unretained(package));
    side_menu->AddMenu(std::make_unique<SideMenuTitleStringUpdater>(package),
                       package->GetSideMenuImage(),
                       package->GetSideMenuHighlightImage(),
                       std::move(menu_callbacks));
    ++package_index;
  }

  SideMenu::MenuCallbacks settings_callbacks;
  settings_callbacks.trigger_callback = kiwi::base::BindRepeating(
      [](GameLibraryView* view, int) { view->callbacks_.show_settings.Run(); },
      kiwi::base::Unretained(this));
  side_menu->AddMenu(
      std::make_unique<StringUpdater>(string_resources::IDR_SIDE_MENU_SETTINGS),
      image_resources::ImageID::kMenuSettings,
      image_resources::ImageID::kMenuSettingsHighlight,
      std::move(settings_callbacks));

  SideMenu::MenuCallbacks about_callbacks;
  about_callbacks.trigger_callback = kiwi::base::BindRepeating(
      [](GameLibraryView* view, int) { view->callbacks_.show_about.Run(); },
      kiwi::base::Unretained(this));
  side_menu->AddMenu(
      std::make_unique<StringUpdater>(string_resources::IDR_SIDE_MENU_ABOUT),
      image_resources::ImageID::kMenuAbout,
      image_resources::ImageID::kMenuAboutHighlight,
      std::move(about_callbacks));

#if !BUILDFLAG(IS_MOBILE) && !BUILDFLAG(IS_SWITCH)
  SideMenu::MenuCallbacks quit_callbacks;
  quit_callbacks.trigger_callback = kiwi::base::BindRepeating(
      [](GameLibraryView* view, int) { view->callbacks_.quit.Run(); },
      kiwi::base::Unretained(this));
  side_menu->AddMenu(
      std::make_unique<StringUpdater>(string_resources::IDR_SIDE_MENU_QUIT),
      image_resources::ImageID::kMenuQuit,
      image_resources::ImageID::kMenuQuitHighlight, std::move(quit_callbacks));
#endif

  SideMenu::SystemToggleCallbacks system_toggle_callbacks;
  system_toggle_callbacks.cycle_callback = kiwi::base::BindRepeating(
      &GameLibraryView::CycleEmulationSystem, kiwi::base::Unretained(this));
  system_toggle_callbacks.is_gba_selected = kiwi::base::BindRepeating(
      &GameLibraryView::IsEmulationSystem, kiwi::base::Unretained(this),
      kiwi::emu::Emulator::Type::kGBA);
  side_menu->SetSystemToggle(std::make_unique<StringUpdater>(
                                 string_resources::IDR_SIDE_MENU_SYSTEM_NES),
                             std::make_unique<StringUpdater>(
                                 string_resources::IDR_SIDE_MENU_SYSTEM_GBA),
                             image_resources::ImageID::kMenuNes,
                             image_resources::ImageID::kMenuGba,
                             std::move(system_toggle_callbacks));

#if BUILDFLAG(IS_SWITCH)
  constexpr int kSearchStringId = string_resources::IDR_SIDE_MENU_SEARCH_SWITCH;
  constexpr SDL_GameControllerButton kSearchControllerHotkey =
      SDL_CONTROLLER_BUTTON_X;
#else
  constexpr int kSearchStringId = string_resources::IDR_SIDE_MENU_SEARCH;
  constexpr SDL_GameControllerButton kSearchControllerHotkey =
      SDL_CONTROLLER_BUTTON_INVALID;
#endif
  SideMenu::ButtonCallbacks search_callbacks;
  search_callbacks.trigger_callback = kiwi::base::BindRepeating(
      &GameLibraryView::ShowFilter, kiwi::base::Unretained(this));
  side_menu->AddButton(std::make_unique<StringUpdater>(kSearchStringId),
                       image_resources::ImageID::kMenuSearch,
                       std::move(search_callbacks), SDLK_f,
                       kSearchControllerHotkey);

  side_menu->Layout();
  side_menu_ = side_menu.get();
  AddWidget(std::move(side_menu));
  SetEmulationSystem(emulation_system_);
}

int GameLibraryView::GetContentLeft() {
  return contents_card_widget_ ? contents_card_widget_->bounds().x : 0;
}

std::optional<GameLibraryView::SystemSelection>
GameLibraryView::GetCurrentSelection(kiwi::emu::Emulator::Type system) {
  const size_t system_index = GetSystemIndex(system);
  const int menu_index = selected_menu_indices_[system_index];
  const auto& items_map = GetFlexItemsMap(system);
  const auto item = items_map.find(menu_index);
  const auto& packages = preset_roms::GetPresetOrTestRomsPackages();
  if (item == items_map.end() || item->second->empty() || menu_index < 0 ||
      menu_index >= static_cast<int>(packages.size())) {
    return std::nullopt;
  }
  return SystemSelection{
      .index = static_cast<int>(item->second->current_index()),
      .package_id = packages[menu_index]->GetPersistentId(),
  };
}

void GameLibraryView::ChangeFocus(Focus focus) {
  SDL_assert(side_menu_);
  switch (focus) {
    case Focus::kContents:
      if (contents_card_widget_->HasWidgets()) {
        side_menu_->set_activate(false);
        SwitchToSideMenuByCurrentFlexItemWidget();
        for (FlexItemsWidget* items_widget : items_widgets_) {
          items_widget->SetActivate(true);
        }
        for (FlexItemsWidget* items_widget : gba_items_widgets_) {
          items_widget->SetActivate(true);
        }
      }
      break;
    case Focus::kSideMenu:
      if (contents_card_widget_->HasWidgets()) {
        side_menu_->set_activate(true);
        for (FlexItemsWidget* items_widget : items_widgets_) {
          items_widget->SetActivate(false);
        }
        for (FlexItemsWidget* items_widget : gba_items_widgets_) {
          items_widget->SetActivate(false);
        }
      }
      break;
  }
  Layout();
}

void GameLibraryView::SetEmulationSystem(kiwi::emu::Emulator::Type system) {
  if (emulation_system_ != system)
    RememberCurrentSideMenuSelection();
  emulation_system_ = system;
  side_menu_->invalidate();

  const auto& items_map = GetCurrentFlexItemsMap();
  const size_t system_index = GetSystemIndex(system);
  int menu_index = selected_menu_indices_[system_index];
  if (!items_map.contains(menu_index) && !items_map.empty()) {
    menu_index =
        FindMenuIndexForPackage(system, preferred_package_ids_[system_index]);
  }
  if (!items_map.contains(menu_index) && !items_map.empty()) {
    if (!preferred_package_ids_[system_index].empty())
      pending_preferred_indices_[system_index].reset();
    menu_index = GetDefaultMenuIndex(system);
  }
  selected_menu_indices_[system_index] = menu_index;
  side_menu_->SetIndex(menu_index);
  SwitchToWidgetForSideMenu(menu_index);

  const auto item = items_map.find(menu_index);
  const auto& packages = preset_roms::GetPresetOrTestRomsPackages();
  if (item != items_map.end() &&
      menu_index < static_cast<int>(packages.size())) {
    EnsurePackageIndex(packages[menu_index], item->second);
    ApplyPendingPreferredIndex(system, item->second);
  }
}

void GameLibraryView::ShowFilter() {
  const auto& items_map = GetCurrentFlexItemsMap();
  const auto target = items_map.find(side_menu_->current_index());
  if (target != items_map.end()) {
    ChangeFocus(Focus::kContents);
    target->second->ShowFilterWidget();
  }
}

void GameLibraryView::Layout(bool animate) {
#if BUILDFLAG(IS_MOBILE)
  const SDL_Rect client_bounds = window()->GetSafeAreaClientBounds();
#else
  const SDL_Rect window_client_bounds = window()->GetClientBounds();
  const SDL_Rect client_bounds = {0, 0, window_client_bounds.w,
                                  window_client_bounds.h};
#endif
  const int left_width = side_menu_->GetSuggestedCollapsedWidth();
  const int right_width = client_bounds.w - left_width;
  const int extended_width =
      side_menu_->GetSuggestedExtendedWidth(client_bounds.w);

  side_menu_target_width_ =
      side_menu_->expanded() ? extended_width : left_width;
  if (animate) {
    side_menu_timer_.Reset();
    side_menu_original_width_ = side_menu_->bounds().w;
    if (side_menu_original_width_ <= 0) {
      side_menu_original_width_ =
          side_menu_->expanded() ? left_width : extended_width;
    }
  } else {
    side_menu_original_width_ = side_menu_target_width_;
    side_menu_->set_bounds(SDL_Rect{client_bounds.x, client_bounds.y,
                                    side_menu_target_width_, client_bounds.h});
    side_menu_->invalidate();
  }

  contents_card_widget_->set_bounds(SDL_Rect{client_bounds.x + left_width,
                                             client_bounds.y, right_width,
                                             client_bounds.h});
}

void GameLibraryView::Paint() {
#if BUILDFLAG(IS_MOBILE)
  const SDL_Rect safe_bounds = window()->GetSafeAreaClientBounds();
  const int collapsed_width = side_menu_->GetSuggestedCollapsedWidth();
  const SDL_Rect expected_contents_bounds = {
      safe_bounds.x + collapsed_width, safe_bounds.y,
      safe_bounds.w - collapsed_width, safe_bounds.h};
  const SDL_Rect current_contents_bounds = contents_card_widget_->bounds();
  const SDL_Rect current_side_menu_bounds = side_menu_->bounds();
  if (!SDL_RectEquals(&expected_contents_bounds, &current_contents_bounds) ||
      current_side_menu_bounds.x != safe_bounds.x ||
      current_side_menu_bounds.y != safe_bounds.y ||
      current_side_menu_bounds.h != safe_bounds.h) {
    Layout(false);
    callbacks_.layout_changed.Run();
  }
  const SDL_Rect layout_bounds = safe_bounds;
#else
  const SDL_Rect client_bounds = window()->GetClientBounds();
  const SDL_Rect layout_bounds = {0, 0, client_bounds.w, client_bounds.h};
#endif

  const int desired_side_menu_width =
      side_menu_->expanded()
          ? side_menu_->GetSuggestedExtendedWidth(layout_bounds.w)
          : side_menu_->GetSuggestedCollapsedWidth();
  if (desired_side_menu_width != side_menu_target_width_) {
    side_menu_original_width_ = side_menu_->bounds().w;
    side_menu_target_width_ = desired_side_menu_width;
    side_menu_timer_.Reset();
  }

  float percentage = side_menu_timer_.ElapsedInMilliseconds() /
                     static_cast<float>(kSideMenuAnimationMs);
  percentage = EaseOutQuadratic(percentage);
  const int side_menu_width =
      Lerp(side_menu_original_width_, side_menu_target_width_, percentage);
  const SDL_Rect target_bounds = {layout_bounds.x, layout_bounds.y,
                                  side_menu_width, layout_bounds.h};
  const SDL_Rect current_bounds = side_menu_->bounds();
  if (!SDL_RectEquals(&target_bounds, &current_bounds)) {
    side_menu_->set_bounds(target_bounds);
    side_menu_->invalidate();
  }
}

bool GameLibraryView::IsWindowless() {
  return true;
}

void GameLibraryView::LoadROM(preset_roms::PresetROM& rom,
                              preset_roms::ROMEdition edition,
                              bool load_from_finger_gesture) {
  callbacks_.load_rom.Run(rom, edition, load_from_finger_gesture);
}

void GameLibraryView::CycleEmulationSystem(int direction) {
  const auto current = std::find(kEmulationSystems.begin(),
                                 kEmulationSystems.end(), emulation_system_);
  if (current == kEmulationSystems.end() || direction == 0)
    return;

  const int system_count = static_cast<int>(kEmulationSystems.size());
  const int current_index =
      static_cast<int>(std::distance(kEmulationSystems.begin(), current));
  const int next_index =
      (current_index + direction % system_count + system_count) % system_count;
  SwitchEmulationSystem(kEmulationSystems[next_index]);
}

void GameLibraryView::SwitchEmulationSystem(kiwi::emu::Emulator::Type system) {
  callbacks_.switch_emulation_system.Run(system);
}

bool GameLibraryView::IsEmulationSystem(
    kiwi::emu::Emulator::Type system) const {
  return emulation_system_ == system;
}

void GameLibraryView::PopulatePackageItems(preset_roms::Package* package,
                                           FlexItemsWidget* items_widget) {
  SDL_assert(package);
  SDL_assert(items_widget);
  SDL_assert(items_widget->empty());
  for (size_t i = 0; i < package->GetRomsCount(); ++i) {
    std::vector<preset_roms::PresetROM*> roms;
    preset_roms::PresetROM& rom = package->GetRomsByIndex(i);
    roms.push_back(&rom);
    for (preset_roms::PresetROM& alternative_rom : rom.alternates) {
      roms.push_back(&alternative_rom);
    }

    preset_roms::PresetROM* priority_rom = FindPreferredROM(roms);
    std::vector<preset_roms::PresetROM*> ordered_roms = {priority_rom};
    for (preset_roms::PresetROM* candidate : roms) {
      if (candidate != priority_rom) {
        ordered_roms.push_back(candidate);
      }
    }

    std::vector<const preset_roms::PresetROM*> filter_aliases;
    filter_aliases.reserve(roms.size());
    for (preset_roms::PresetROM* candidate : roms) {
      filter_aliases.push_back(candidate);
    }

#if BUILDFLAG(ENABLE_HD_TEXTURE)
    for (preset_roms::PresetROM* candidate : ordered_roms) {
      if (!candidate->hd_edition_available) {
        continue;
      }
      items_widget->AddItem(
          std::make_unique<ROMTitleUpdater>(
              *candidate, preset_roms::ROMEdition::kHD, filter_aliases),
          candidate->boxart_width, candidate->boxart_height, true,
          kiwi::base::BindRepeating(&LoadPresetROMBoxArt, *candidate),
          kiwi::base::BindRepeating(
              &GameLibraryView::LoadROM, kiwi::base::Unretained(this),
              std::ref(*candidate), preset_roms::ROMEdition::kHD));
    }
#endif

    preset_roms::PresetROM* default_rom = ordered_roms.front();
    const size_t main_item_index = items_widget->AddItem(
        std::make_unique<ROMTitleUpdater>(*default_rom,
                                          preset_roms::ROMEdition::kOriginal),
        default_rom->boxart_width, default_rom->boxart_height,
        HasSwitchableHDTexture(default_rom),
        kiwi::base::BindRepeating(&LoadPresetROMBoxArt, *default_rom),
        kiwi::base::BindRepeating(
            &GameLibraryView::LoadROM, kiwi::base::Unretained(this),
            std::ref(*default_rom), preset_roms::ROMEdition::kOriginal));

    for (size_t rom_index = 1; rom_index < ordered_roms.size(); ++rom_index) {
      preset_roms::PresetROM* candidate = ordered_roms[rom_index];
      items_widget->AddSubItem(
          main_item_index,
          std::make_unique<ROMTitleUpdater>(*candidate,
                                            preset_roms::ROMEdition::kOriginal),
          candidate->boxart_width, candidate->boxart_height,
          HasSwitchableHDTexture(candidate),
          kiwi::base::BindRepeating(&LoadPresetROMBoxArt, *candidate),
          kiwi::base::BindRepeating(
              &GameLibraryView::LoadROM, kiwi::base::Unretained(this),
              std::ref(*candidate), preset_roms::ROMEdition::kOriginal));
    }
  }
}

void GameLibraryView::EnsurePackageIndex(preset_roms::Package* package,
                                         FlexItemsWidget* items_widget) {
  if (IsPackageIndexReady(package)) {
    if (items_widget->empty()) {
      PopulatePackageItems(package, items_widget);
    }
  } else if (indexing_packages_.insert(package).second) {
    items_widget->SetIndexingProgress(0, package->GetRomsCount());
    std::weak_ptr<int> weak_lifetime = lifetime_token_;
    Application::Get()->InitializePackageIndex(
        package,
        kiwi::base::BindRepeating(
            [](std::weak_ptr<int> weak_lifetime, GameLibraryView* view,
               FlexItemsWidget* items_widget, size_t completed, size_t total) {
              if (!weak_lifetime.expired()) {
                view->OnPackageIndexProgress(items_widget, completed, total);
              }
            },
            weak_lifetime, kiwi::base::Unretained(this),
            kiwi::base::Unretained(items_widget)),
        kiwi::base::BindOnce(
            [](std::weak_ptr<int> weak_lifetime, GameLibraryView* view,
               preset_roms::Package* package, FlexItemsWidget* items_widget,
               bool success) {
              if (!weak_lifetime.expired()) {
                view->OnPackageIndexReady(package, items_widget, success);
              }
            },
            weak_lifetime, kiwi::base::Unretained(this),
            kiwi::base::Unretained(package),
            kiwi::base::Unretained(items_widget)));
  }
}

void GameLibraryView::OnPackageIndexProgress(FlexItemsWidget* items_widget,
                                             size_t completed,
                                             size_t total) {
  items_widget->SetIndexingProgress(completed, total);
}

void GameLibraryView::OnPackageIndexReady(preset_roms::Package* package,
                                          FlexItemsWidget* items_widget,
                                          bool success) {
  indexing_packages_.erase(package);
  if (success && items_widget->empty()) {
    PopulatePackageItems(package, items_widget);
    const kiwi::emu::Emulator::Type system =
        package->GetType() == preset_roms::PackageType::kGBA
            ? kiwi::emu::Emulator::Type::kGBA
            : kiwi::emu::Emulator::Type::kNES;
    if (items_widget == GetSelectedItemsWidget(system))
      ApplyPendingPreferredIndex(system, items_widget);
  }
  items_widget->FinishIndexing(success);
}

FlexItemsWidget* GameLibraryView::GetSelectedItemsWidget(
    kiwi::emu::Emulator::Type system) const {
  const auto& items_map = GetFlexItemsMap(system);
  const auto item =
      items_map.find(selected_menu_indices_[GetSystemIndex(system)]);
  return item == items_map.end() ? nullptr : item->second;
}

void GameLibraryView::ApplyPendingPreferredIndex(
    kiwi::emu::Emulator::Type system,
    FlexItemsWidget* items_widget) {
  std::optional<int>& pending_index =
      pending_preferred_indices_[GetSystemIndex(system)];
  if (!pending_index || items_widget->empty())
    return;

  const int index =
      std::clamp(*pending_index, 0, static_cast<int>(items_widget->size() - 1));
  items_widget->SetIndex(index);
  pending_index.reset();
}

const std::map<int, FlexItemsWidget*>& GameLibraryView::GetFlexItemsMap(
    kiwi::emu::Emulator::Type system) const {
  return system == kiwi::emu::Emulator::Type::kGBA ? gba_flex_items_map_
                                                   : flex_items_map_;
}

const std::map<int, FlexItemsWidget*>& GameLibraryView::GetCurrentFlexItemsMap()
    const {
  return GetFlexItemsMap(emulation_system_);
}

int GameLibraryView::GetDefaultMenuIndex(
    kiwi::emu::Emulator::Type system) const {
  const auto& items_map = GetFlexItemsMap(system);
  const auto& packages = preset_roms::GetPresetOrTestRomsPackages();
  for (const auto& [menu_index, items_widget] : items_map) {
    static_cast<void>(items_widget);
    if (menu_index < static_cast<int>(packages.size()) &&
        packages[menu_index]->IsMainPackage()) {
      return menu_index;
    }
  }
  return items_map.empty() ? -1 : items_map.begin()->first;
}

int GameLibraryView::FindMenuIndexForPackage(
    kiwi::emu::Emulator::Type system,
    const std::string& package_id) const {
  if (package_id.empty())
    return -1;

  const auto& items_map = GetFlexItemsMap(system);
  const auto& packages = preset_roms::GetPresetOrTestRomsPackages();
  for (const auto& [menu_index, items_widget] : items_map) {
    static_cast<void>(items_widget);
    if (menu_index >= 0 && menu_index < static_cast<int>(packages.size()) &&
        packages[menu_index]->GetPersistentId() == package_id) {
      return menu_index;
    }
  }
  return -1;
}

void GameLibraryView::RememberCurrentSideMenuSelection() {
  Widget* current_widget = contents_card_widget_->GetCurrentWidget();
  if (!current_widget)
    return;
  for (const auto& [menu_index, widget] : GetCurrentFlexItemsMap()) {
    if (widget == current_widget) {
      RememberSideMenuSelection(menu_index);
      return;
    }
  }
}

void GameLibraryView::RememberSideMenuSelection(int menu_index) {
  if (!GetCurrentFlexItemsMap().contains(menu_index))
    return;
  const size_t system_index = GetSystemIndex(emulation_system_);
  selected_menu_indices_[system_index] = menu_index;
}

void GameLibraryView::SwitchToWidgetForSideMenu(int menu_index) {
  const auto& items_map = GetCurrentFlexItemsMap();
  const auto target = items_map.find(menu_index);
  if (target != items_map.end()) {
    const bool succeeded =
        contents_card_widget_->SetCurrentWidget(target->second);
    SDL_assert(succeeded);
  }
}

void GameLibraryView::SwitchToSideMenuByCurrentFlexItemWidget() {
  Widget* current_widget = contents_card_widget_->GetCurrentWidget();
  SDL_assert(current_widget);
  const auto& items_map = GetCurrentFlexItemsMap();
  int target_index = -1;
  for (auto [side_menu_index, widget] : items_map) {
    if (widget == current_widget) {
      target_index = side_menu_index;
      break;
    }
  }
  if (target_index >= 0) {
    RememberSideMenuSelection(target_index);
    side_menu_->SetIndex(target_index);
  } else if (!items_map.empty()) {
    side_menu_->SetIndex(items_map.begin()->first);
    RememberSideMenuSelection(items_map.begin()->first);
    const bool succeeded =
        contents_card_widget_->SetCurrentWidget(items_map.begin()->second);
    SDL_assert(succeeded);
  }
}
