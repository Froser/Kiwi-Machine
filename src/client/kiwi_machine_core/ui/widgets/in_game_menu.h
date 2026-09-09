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

#ifndef UI_WIDGETS_IN_GAME_MENU_H_
#define UI_WIDGETS_IN_GAME_MENU_H_

#include <kiwi_nes.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>

#include "models/nes_runtime.h"
#include "ui/widgets/widget.h"
#include "utility/fonts.h"
#include "utility/timer.h"

class MainWindow;
class InGameMenu : public Widget {
 public:
  enum class MenuItem {
    kContinue,
    kLoadAutoSave,
    kLoadState,
    kSaveState,
    kOptions,
    kResetGame,
    kToGameSelection,

    kMax,
  };

  enum class SettingsItem {
    kVolume,
    kWindowMode,
    kJoyP1,
    kSwapABP1,
    kJoyP2,
    kSwapABP2,
    kLanguage,

    kMax,
  };

  struct StateSlot {
    int value = 0;
  };

  struct AutoSaveTimestamp {
    int value = 0;
  };

  using MenuItemPayload =
      std::variant<std::monostate, StateSlot, AutoSaveTimestamp>;
  struct MenuCommand {
    MenuItem item;
    MenuItemPayload payload;
  };

  using MenuItemCallback =
      kiwi::base::RepeatingCallback<void(const MenuCommand&)>;
  using SettingsItemValue = std::variant<bool, float>;
  using SettingsItemCallback =
      kiwi::base::RepeatingCallback<void(SettingsItem, SettingsItemValue)>;

  explicit InGameMenu(MainWindow* main_window,
                      NESRuntimeID runtime_id,
                      MenuItemCallback menu_callback,
                      SettingsItemCallback settings_callback);
  ~InGameMenu() override;

  void Close();
  void Show();
  void RefreshStatePreview();
  void SetMenuItemVisible(MenuItem item, bool visible);

 protected:
  // Widget:
  void Paint() override;
  bool OnKeyPressed(SDL_KeyboardEvent* event) override;
  bool OnMouseMove(SDL_MouseMotionEvent* event) override;
  bool OnMouseWheel(SDL_MouseWheelEvent* event) override;
  bool OnMousePressed(SDL_MouseButtonEvent* event) override;
  bool OnMouseReleased(SDL_MouseButtonEvent* event) override;
  bool OnControllerButtonPressed(SDL_ControllerButtonEvent* event) override;
  bool OnControllerAxisMotionEvent(SDL_ControllerAxisEvent* event) override;
  bool OnTouchFingerDown(SDL_TouchFingerEvent* event) override;
  bool OnTouchFingerUp(SDL_TouchFingerEvent* event) override;
  bool OnTouchFingerMove(SDL_TouchFingerEvent* event) override;
  void OnWindowPreRender() override;
  void OnWindowPostRender() override;

 private:
  static constexpr size_t kMenuItemCount = static_cast<size_t>(MenuItem::kMax);
  static constexpr size_t kSettingsItemCount =
      static_cast<size_t>(SettingsItem::kMax);

  enum class Page {
    kMainMenu,
    kStateBrowser,
    kSettings,
    kConfirmation,
  };

  enum class LayoutMode {
    kTwoPane,
    kSinglePane,
  };

  enum class NavigationAction {
    kUp,
    kDown,
    kLeft,
    kRight,
    kActivate,
    kBack,
  };

  enum class InputModality {
    kKeyboard,
    kController,
    kMouse,
    kTouch,
  };

  enum class FocusArea {
    kMenu,
    kDetail,
    kConfirmation,
  };

  enum class StepDirection {
    kPrevious,
    kNext,
  };

  struct FocusState {
    MenuItem menu_item = MenuItem::kContinue;
    SettingsItem settings_item = SettingsItem::kVolume;
    FocusArea area = FocusArea::kMenu;
    bool confirm_action = false;
  };

  struct FrameText {
    std::array<std::string, kMenuItemCount> menu_items;
    std::array<std::string, kSettingsItemCount> setting_labels;
    std::array<std::string, kSettingsItemCount> setting_values;
    std::string header;
    std::string subtitle;
    std::string state_title;
    std::string state_metadata;
    std::string state_position;
    std::string state_action;
    std::string confirmation_title;
    std::string confirmation_message;
    std::string cancel;
    std::string confirm;
  };

  struct FrameLayout {
    LayoutMode mode = LayoutMode::kSinglePane;
    Page visible_page = Page::kMainMenu;
    PreferredFontSize font_size = PreferredFontSize::k1x;
    float font_height = 0.f;
    float row_height = 0.f;
    float padding = 0.f;
    float menu_content_height = 0.f;
    float settings_content_height = 0.f;

    SDL_Rect safe_area = {};
    SDL_Rect panel = {};
    SDL_Rect header = {};
    SDL_Rect header_title = {};
    SDL_Rect header_subtitle = {};
    SDL_Rect back_button = {};
    SDL_Rect close_button = {};
    SDL_Rect content = {};
    SDL_Rect navigation = {};
    SDL_Rect detail = {};

    std::array<SDL_Rect, kMenuItemCount> menu_items = {};
    std::array<SDL_Rect, kSettingsItemCount> setting_items = {};
    std::array<SDL_Rect, kSettingsItemCount> setting_previous = {};
    std::array<SDL_Rect, kSettingsItemCount> setting_values = {};
    std::array<SDL_Rect, kSettingsItemCount> setting_next = {};

    SDL_Rect state_preview = {};
    SDL_Rect state_title = {};
    SDL_Rect state_metadata = {};
    SDL_Rect state_previous = {};
    SDL_Rect state_position = {};
    SDL_Rect state_next = {};
    SDL_Rect state_action = {};

    SDL_Rect contextual_action = {};
    SDL_Rect confirmation_body = {};
    SDL_Rect confirmation_cancel = {};
    SDL_Rect confirmation_accept = {};
  };

  enum class HitTargetType {
    kNone,
    kBack,
    kClose,
    kMenuItem,
    kStatePrevious,
    kStateNext,
    kStateAction,
    kContextualAction,
    kSettingItem,
    kSettingPrevious,
    kSettingValue,
    kSettingNext,
    kConfirmationCancel,
    kConfirmationAccept,
  };

  struct HitTarget {
    HitTargetType type = HitTargetType::kNone;
    int index = -1;

    bool operator==(const HitTarget& other) const {
      return type == other.type && index == other.index;
    }
  };

  struct StatePreview {
    enum class Status {
      kEmpty,
      kLoading,
      kReady,
    };

    Status status = Status::kEmpty;
    bool has_thumbnail = false;
    int timestamp = 0;
  };

  FrameText BuildFrameText() const;
  FrameLayout CalculateFrameLayout(const FrameText& text);
  void LayoutMenu(const FrameText& text, FrameLayout& layout);
  void LayoutStateBrowser(FrameLayout& layout);
  void LayoutSettings(const FrameText& text, FrameLayout& layout);
  void LayoutConfirmation(FrameLayout& layout);

  void DrawFrame(const FrameText& text, const FrameLayout& layout);
  void DrawBackground(const FrameLayout& layout);
  void DrawHeader(const FrameText& text, const FrameLayout& layout);
  void DrawMenu(const FrameText& text, const FrameLayout& layout);
  void DrawStateBrowser(const FrameText& text, const FrameLayout& layout);
  void DrawSettings(const FrameText& text, const FrameLayout& layout);
  void DrawConfirmation(const FrameText& text, const FrameLayout& layout);
  void DrawContextualAction(const FrameText& text, const FrameLayout& layout);

  bool HandleInputEvent(SDL_KeyboardEvent* keyboard,
                        SDL_ControllerButtonEvent* controller);
  bool HandleNavigationAction(NavigationAction action, InputModality modality);
  bool HandleControllerAxis(SDL_ControllerAxisEvent* event);
  void ActivateMenuItem(MenuItem item);
  void ExecuteMenuItem(MenuItem item);
  void ExecuteStateAction();
  void ExecuteConfirmation();
  void OpenPage(Page page);
  void ReturnToMenu();
  void MoveMenuSelection(int delta);
  void MoveSettingsSelection(int delta);
  void MoveMenuItemTo(MenuItem item);
  void StepState(StepDirection direction);
  void StepSetting(SettingsItem item, StepDirection direction);
  void SetVolumeFromPoint(float x, const SDL_Rect& bounds);

  HitTarget HitTest(const FrameLayout& layout, float x, float y) const;
  void UpdatePointerFocus(const HitTarget& target, InputModality modality);
  void ActivateHitTarget(const HitTarget& target, float x);
  void ClampMenuScroll(const FrameLayout& layout);
  void ClampSettingsScroll(const FrameLayout& layout);
  void ScrollMenuSelectionIntoView();
  void ScrollSettingsSelectionIntoView();
  void SetFirstSelection();

  void RequestAutoSavedStateCount();
  static void DispatchAutoSavedStateCount(std::weak_ptr<int> weak_lifetime,
                                          InGameMenu* menu,
                                          uint64_t request_id,
                                          int count);
  static void DispatchStateResult(std::weak_ptr<int> weak_lifetime,
                                  InGameMenu* menu,
                                  uint64_t request_id,
                                  const NESRuntime::Data::StateResult& result);
  void OnGotAutoSavedStateCount(uint64_t request_id, int count);
  void OnGotState(uint64_t request_id,
                  const NESRuntime::Data::StateResult& result);
  void CancelStatePreviewRequest();

  bool IsMenuItemVisible(MenuItem item) const;
  bool IsStateMenuItem(MenuItem item) const;
  bool IsStandaloneSettingsMenu() const;
  bool CanStepState(StepDirection direction) const;
  bool CanStepSetting(SettingsItem item, StepDirection direction) const;
  Page GetPreviewPage() const;
  std::string GetSettingValue(SettingsItem item) const;

  MainWindow* main_window_ = nullptr;
  NESRuntime::Data* runtime_data_ = nullptr;
  MenuItemCallback menu_callback_;
  SettingsItemCallback settings_callback_;

  Page page_ = Page::kMainMenu;
  Page page_before_confirmation_ = Page::kMainMenu;
  FocusState focus_;
  InputModality input_modality_ = InputModality::kKeyboard;
  MenuItem confirmation_item_ = MenuItem::kResetGame;
  std::array<bool, kMenuItemCount> menu_item_visible_ = {};

  int which_state_ = 0;
  int which_autosave_state_slot_ = 0;
  int current_auto_states_count_ = 0;

  FrameLayout last_layout_;
  bool has_layout_ = false;
  float menu_scroll_offset_ = 0.f;
  float settings_scroll_offset_ = 0.f;
  HitTarget hovered_target_;
  HitTarget pressed_target_;
  bool mouse_pressed_ = false;

  bool touch_active_ = false;
  SDL_FingerID active_touch_id_ = 0;
  ImVec2 touch_down_position_;
  ImVec2 last_touch_position_;
  bool touch_moved_ = false;

  std::array<int, 2> controller_axis_direction_ = {};
  std::array<Timer, 2> controller_axis_repeat_timer_;

  std::shared_ptr<int> lifetime_token_ = std::make_shared<int>(0);
  uint64_t state_preview_request_id_ = 0;
  uint64_t auto_save_count_request_id_ = 0;
  StatePreview state_preview_;
  SDL_Texture* snapshot_ = nullptr;
};

#endif  // UI_WIDGETS_IN_GAME_MENU_H_
