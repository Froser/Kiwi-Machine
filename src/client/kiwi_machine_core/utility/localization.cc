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

#include "utility/localization.h"

#include <SDL.h>
#include <kiwi_nes.h>
#include <map>
#include <memory>

#include "preset_roms/preset_roms.h"
#include "resources/string_resources.h"
#include "utility/zip_reader.h"

namespace {
constexpr char kVisibleChars[] =
    "!\"#$%&'()*+,-./"
    "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
    "abcdefghijklmnopqrstuvwxyz{|}~";
static_assert(sizeof(kVisibleChars) == 95);
std::string g_global_language;

using GlyphRangePtr = std::unique_ptr<ImVector<ImWchar>>;
std::map<SupportedLanguage, GlyphRangePtr> g_glyph_ranges;

const char* GetROMLocalizedTitle(SupportedLanguage language,
                                 const preset_roms::PresetROM& rom) {
  auto local_name_iter = rom.i18n_names.find(ToLanguageCode(language));
  if (local_name_iter != rom.i18n_names.end()) {
    return local_name_iter->second.c_str();
  }

  return rom.name;
}

const char* GetROMLocalizedCollateStringHint(
    SupportedLanguage language,
    const preset_roms::PresetROM& rom) {
  // Comparison order:
  // Hints, then ROM's localized name.
  auto local_name_iter =
      rom.i18n_names.find(std::string(ToLanguageCode(language)) + "-hint");
  if (local_name_iter != rom.i18n_names.end()) {
    return local_name_iter->second.c_str();
  }

  return GetROMLocalizedTitle(language, rom);
}

const std::string& GetLocalizedString(SupportedLanguage language, int id) {
  const string_resources::StringMap& string_map =
      string_resources::GetGlobalStringMap();

  auto id_iter = string_map.find(id);
  SDL_assert(id_iter != string_map.end());

  const auto& i18n_strings = id_iter->second;
  const char* app_language = ToLanguageCode(language);
  auto lang_iter = i18n_strings.find(app_language);
  if (lang_iter == i18n_strings.end()) {
    lang_iter = i18n_strings.find("default");
    SDL_assert(lang_iter != i18n_strings.end());
  }

  return lang_iter->second;
}

void BuildGlyphRanges(SupportedLanguage language,
                      ImVector<ImWchar>& out_ranges) {
  ImFontGlyphRangesBuilder ranges_builder;
  ranges_builder.AddText(kVisibleChars);
  for (int i = 0; i < string_resources::END_OF_STRINGS; ++i) {
    ranges_builder.AddText(GetLocalizedString(language, i).c_str());
  }

  const auto& packages = preset_roms::GetPresetOrTestRomsPackages();
  for (const auto& package : packages) {
    for (size_t i = 0; i < package->GetRomsCount(); ++i) {
      auto& rom = package->GetRomsByIndex(i);
      std::string title = package->GetTitleForLanguage(language);
      ranges_builder.AddText(title.c_str());
      ranges_builder.AddText(GetROMLocalizedTitle(language, rom));
      for (const auto& alter : rom.alternates) {
        ranges_builder.AddText(GetROMLocalizedTitle(language, alter));
      }
    }
  }
  ranges_builder.BuildRanges(&out_ranges);
}

}  // namespace

LocalizedStringUpdater::LocalizedStringUpdater() = default;
LocalizedStringUpdater::~LocalizedStringUpdater() = default;

const char* ToLanguageCode(SupportedLanguage language) {
  switch (language) {
    case SupportedLanguage::kEnglish:
      return "en";
#if !KIWI_WASM
    case SupportedLanguage::kSimplifiedChinese:
      return "zh";
    case SupportedLanguage::kJapanese:
      return "ja";
#endif
    default:
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Wrong language type %d",
                  static_cast<int>(language));
      SDL_assert(false);
      return "en";
  }
}

void SetLanguage(const char* language) {
  if (language)
    g_global_language = language;
  else
    g_global_language.clear();
}

void SetLanguage(SupportedLanguage language) {
  switch (language) {
    case SupportedLanguage::kEnglish:
      SetLanguage("en");
      break;
#if !DISABLE_CHINESE_FONT
    case SupportedLanguage::kSimplifiedChinese:
      SetLanguage("zh");
      break;
#endif
#if !DISABLE_JAPANESE_FONT
    case SupportedLanguage::kJapanese:
      SetLanguage("ja");
      break;
#endif
    default:
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Wrong language type %d",
                  static_cast<int>(language));
      SDL_assert(false);
      break;
  }
}

SupportedLanguage GetCurrentSupportedLanguage() {
#if !DISABLE_CHINESE_FONT
  if (kiwi::base::StartsWith(GetLanguage(), "zh-") ||
      kiwi::base::CompareCaseInsensitiveASCII(GetLanguage(), "zh") == 0)
    return SupportedLanguage::kSimplifiedChinese;
#endif
#if !DISABLE_JAPANESE_FONT
  if (kiwi::base::StartsWith(GetLanguage(), "ja-") ||
      kiwi::base::CompareCaseInsensitiveASCII(GetLanguage(), "ja") == 0)
    return SupportedLanguage::kJapanese;
#endif

  return SupportedLanguage::kEnglish;
}

const char* GetLanguage() {
  if (!g_global_language.empty())
    return g_global_language.c_str();

  SDL_Locale* locale = SDL_GetPreferredLocales();
  return locale->language;
}

const char* GetROMLocalizedTitle(const preset_roms::PresetROM& rom) {
  return GetROMLocalizedTitle(GetCurrentSupportedLanguage(), rom);
}

const char* GetROMLocalizedCollateStringHint(
    const preset_roms::PresetROM& rom) {
  return GetROMLocalizedCollateStringHint(GetCurrentSupportedLanguage(), rom);
}

const std::string& GetLocalizedString(int id) {
  return GetLocalizedString(GetCurrentSupportedLanguage(), id);
}

const ImVector<ImWchar>& GetGlyphRanges(SupportedLanguage language) {
  auto iter = g_glyph_ranges.find(language);
  if (iter == g_glyph_ranges.end()) {
    GlyphRangePtr range = std::make_unique<ImVector<ImWchar>>();
    BuildGlyphRanges(language, *range);
    g_glyph_ranges[language] = std::move(range);
  }

  SDL_assert(g_glyph_ranges[language]);
  return *g_glyph_ranges[language];
}

namespace language_conversion {
namespace {
const std::unordered_map<std::string, std::string> kHiraganaToRomajiMap = {
    {"あ", "a"},  {"い", "i"},   {"う", "u"},   {"え", "e"},  {"お", "o"},
    {"か", "ka"}, {"き", "ki"},  {"く", "ku"},  {"け", "ke"}, {"こ", "ko"},
    {"が", "ga"}, {"ぎ", "gi"},  {"ぐ", "gu"},  {"げ", "ge"}, {"ご", "go"},
    {"さ", "sa"}, {"し", "shi"}, {"す", "su"},  {"せ", "se"}, {"そ", "so"},
    {"ざ", "za"}, {"じ", "ji"},  {"ず", "zu"},  {"ぜ", "ze"}, {"ぞ", "zo"},
    {"た", "ta"}, {"ち", "chi"}, {"つ", "tsu"}, {"て", "te"}, {"と", "to"},
    {"だ", "da"}, {"ぢ", "ji"},  {"づ", "zu"},  {"で", "de"}, {"ど", "do"},
    {"な", "na"}, {"に", "ni"},  {"ぬ", "nu"},  {"ね", "ne"}, {"の", "no"},
    {"は", "ha"}, {"ひ", "hi"},  {"ふ", "fu"},  {"へ", "he"}, {"ほ", "ho"},
    {"ば", "ba"}, {"び", "bi"},  {"ぶ", "bu"},  {"べ", "be"}, {"ぼ", "bo"},
    {"ぱ", "pa"}, {"ぴ", "pi"},  {"ぷ", "pu"},  {"ぺ", "pe"}, {"ぽ", "po"},
    {"ま", "ma"}, {"み", "mi"},  {"む", "mu"},  {"め", "me"}, {"も", "mo"},
    {"や", "ya"}, {"ゆ", "yu"},  {"よ", "yo"},  {"ら", "ra"}, {"り", "ri"},
    {"る", "ru"}, {"れ", "re"},  {"ろ", "ro"},  {"わ", "wa"}, {"を", "wo"},
    {"ん", "n"}};

const std::unordered_map<std::string, std::string> kHiraganaSpecialToRomajiMap =
    {{u8"きゃ", "kya"}, {u8"きゅ", "kyu"}, {u8"きょ", "kyo"}, {u8"しゃ", "sha"},
     {u8"しゅ", "shu"}, {u8"しょ", "sho"}, {u8"ちゃ", "cha"}, {u8"ちゅ", "chu"},
     {u8"ちょ", "cho"}, {u8"にゃ", "nya"}, {u8"にゅ", "nyu"}, {u8"にょ", "nyo"},
     {u8"ひゃ", "hya"}, {u8"ひゅ", "hyu"}, {u8"ひょ", "hyo"}, {u8"みゃ", "mya"},
     {u8"みゅ", "myu"}, {u8"みょ", "myo"}, {u8"りゃ", "rya"}, {u8"りゅ", "ryu"},
     {u8"りょ", "ryo"}, {u8"ぎゃ", "gya"}, {u8"ぎゅ", "gyu"}, {u8"ぎょ", "gyo"},
     {u8"じゃ", "ja"},  {u8"じゅ", "ju"},  {u8"じょ", "jo"},  {u8"びゃ", "bya"},
     {u8"びゅ", "byu"}, {u8"びょ", "byo"}, {u8"ぴゃ", "pya"}, {u8"ぴゅ", "pyu"},
     {u8"ぴょ", "pyo"}};

const std::unordered_map<std::string, std::string> kKatakanaToRomajiMap = {
    {"ア", "a"},  {"イ", "i"},   {"ウ", "u"},   {"エ", "e"},  {"オ", "o"},
    {"カ", "ka"}, {"キ", "ki"},  {"ク", "ku"},  {"ケ", "ke"}, {"コ", "ko"},
    {"ガ", "ga"}, {"ギ", "gi"},  {"グ", "gu"},  {"ケ", "ge"}, {"ゴ", "go"},
    {"サ", "sa"}, {"シ", "shi"}, {"ス", "su"},  {"セ", "se"}, {"ソ", "so"},
    {"ザ", "za"}, {"ジ", "ji"},  {"ズ", "zu"},  {"ゼ", "ze"}, {"ゾ", "zo"},
    {"タ", "ta"}, {"チ", "chi"}, {"ツ", "tsu"}, {"テ", "te"}, {"ト", "to"},
    {"ダ", "da"}, {"ヂ", "ji"},  {"ヅ", "zu"},  {"デ", "de"}, {"ド", "do"},
    {"ナ", "na"}, {"ニ", "ni"},  {"ヌ", "nu"},  {"ネ", "ne"}, {"ノ", "no"},
    {"ハ", "ha"}, {"ヒ", "hi"},  {"フ", "fu"},  {"ヘ", "he"}, {"ホ", "ho"},
    {"バ", "ba"}, {"ビ", "bi"},  {"ブ", "bu"},  {"ベ", "be"}, {"ボ", "bo"},
    {"パ", "pa"}, {"ピ", "pi"},  {"プ", "pu"},  {"ペ", "pe"}, {"ポ", "bo"},
    {"マ", "ma"}, {"ミ", "mi"},  {"ム", "mu"},  {"メ", "me"}, {"モ", "mo"},
    {"ヤ", "ya"}, {"ユ", "yu"},  {"ヨ", "yo"},  {"ラ", "ra"}, {"リ", "ri"},
    {"ル", "ru"}, {"レ", "re"},  {"ロ", "ro"},  {"ワ", "wa"}, {"ヲ", "wo"},
    {"ン", "n"},
};

const std::unordered_map<std::string, std::string> kKatakanaSpecialToRomajiMap =
    {{u8"キャ", "kya"}, {u8"キュ", "kyu"}, {u8"キョ", "kyo"}, {u8"シャ", "sha"},
     {u8"シュ", "shu"}, {u8"ショ", "sho"}, {u8"チャ", "cha"}, {u8"チュ", "chu"},
     {u8"チョ", "cho"}, {u8"ニャ", "nya"}, {u8"ニュ", "nyu"}, {u8"ニョ", "nyo"},
     {u8"ヒャ", "hya"}, {u8"ヒュ", "hyu"}, {u8"ヒョ", "hyo"}, {u8"ミャ", "mya"},
     {u8"ミュ", "myu"}, {u8"ミョ", "myo"}, {u8"リャ", "rya"}, {u8"リュ", "ryu"},
     {u8"リョ", "ryo"}, {u8"ギャ", "gya"}, {u8"ギュ", "gyu"}, {u8"ギョ", "gyo"},
     {u8"ジャ", "ja"},  {u8"ジュ", "ju"},  {u8"ジョ", "jo"},  {u8"ビャ", "bya"},
     {u8"ビュ", "byu"}, {u8"ビョ", "byo"}, {u8"ピャ", "pya"}, {u8"ピュ", "pyu"},
     {u8"ピョ", "pyo"}};
}  // namespace

std::string KanaToRomaji(const std::string& kana) {
  std::string r = kana;
  for (const auto& i : kHiraganaSpecialToRomajiMap) {
    size_t pos = std::string::npos;
    while ((pos = r.find(i.first)) != std::string::npos) {
      r.replace(pos, i.first.size(), i.second);
    }
  }

  for (const auto& i : kKatakanaSpecialToRomajiMap) {
    size_t pos = std::string::npos;
    while ((pos = r.find(i.first)) != std::string::npos) {
      r.replace(pos, i.first.size(), i.second);
    }
  }

  for (const auto& i : kHiraganaToRomajiMap) {
    size_t pos = std::string::npos;
    while ((pos = r.find(i.first)) != std::string::npos) {
      r.replace(pos, i.first.size(), i.second);
    }
  }

  for (const auto& i : kKatakanaToRomajiMap) {
    size_t pos = std::string::npos;
    while ((pos = r.find(i.first)) != std::string::npos) {
      r.replace(pos, i.first.size(), i.second);
    }
  }

  size_t pos = std::string::npos;
  while ((pos = r.find(u8"っ")) != std::string::npos) {
    if (pos < r.size() - 1)
      r.replace(pos, sizeof(u8"っ") - 1,
                std::string(r[pos + sizeof(u8"っ") - 1], 1));
    else
      r.replace(pos, 1, u8"xtsu");
  }

  return r;
}

}  // namespace language_conversion