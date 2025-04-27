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

std::string GetLanguage() {
  if (!g_global_language.empty())
    return g_global_language.c_str();

  SDL_Locale* locale = SDL_GetPreferredLocales();
  std::string retval =
      (locale && locale->language) ? locale->language : std::string();
  SDL_free(locale);
  return retval;
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
const std::unordered_map<std::u8string, std::u8string> kHiraganaToRomajiMap = {
    {u8"あ", u8"a"},   {u8"い", u8"i"},  {u8"う", u8"u"},   {u8"え", u8"e"},
    {u8"お", u8"o"},   {u8"か", u8"ka"}, {u8"き", u8"ki"},  {u8"く", u8"ku"},
    {u8"け", u8"ke"},  {u8"こ", u8"ko"}, {u8"が", u8"ga"},  {u8"ぎ", u8"gi"},
    {u8"ぐ", u8"gu"},  {u8"げ", u8"ge"}, {u8"ご", u8"go"},  {u8"さ", u8"sa"},
    {u8"し", u8"shi"}, {u8"す", u8"su"}, {u8"せ", u8"se"},  {u8"そ", u8"so"},
    {u8"ざ", u8"za"},  {u8"じ", u8"ji"}, {u8"ず", u8"zu"},  {u8"ぜ", u8"ze"},
    {u8"ぞ", u8"zo"},  {u8"た", u8"ta"}, {u8"ち", u8"chi"}, {u8"つ", u8"tsu"},
    {u8"て", u8"te"},  {u8"と", u8"to"}, {u8"だ", u8"da"},  {u8"ぢ", u8"ji"},
    {u8"づ", u8"zu"},  {u8"で", u8"de"}, {u8"ど", u8"do"},  {u8"な", u8"na"},
    {u8"に", u8"ni"},  {u8"ぬ", u8"nu"}, {u8"ね", u8"ne"},  {u8"の", u8"no"},
    {u8"は", u8"ha"},  {u8"ひ", u8"hi"}, {u8"ふ", u8"fu"},  {u8"へ", u8"he"},
    {u8"ほ", u8"ho"},  {u8"ば", u8"ba"}, {u8"び", u8"bi"},  {u8"ぶ", u8"bu"},
    {u8"べ", u8"be"},  {u8"ぼ", u8"bo"}, {u8"ぱ", u8"pa"},  {u8"ぴ", u8"pi"},
    {u8"ぷ", u8"pu"},  {u8"ぺ", u8"pe"}, {u8"ぽ", u8"po"},  {u8"ま", u8"ma"},
    {u8"み", u8"mi"},  {u8"む", u8"mu"}, {u8"め", u8"me"},  {u8"も", u8"mo"},
    {u8"や", u8"ya"},  {u8"ゆ", u8"yu"}, {u8"よ", u8"yo"},  {u8"ら", u8"ra"},
    {u8"り", u8"ri"},  {u8"る", u8"ru"}, {u8"れ", u8"re"},  {u8"ろ", u8"ro"},
    {u8"わ", u8"wa"},  {u8"を", u8"wo"}, {u8"ん", u8"n"}};

const std::unordered_map<std::u8string, std::u8string>
    kHiraganaSpecialToRomajiMap = {
        {u8"きゃ", u8"kya"}, {u8"きゅ", u8"kyu"}, {u8"きょ", u8"kyo"},
        {u8"しゃ", u8"sha"}, {u8"しゅ", u8"shu"}, {u8"しょ", u8"sho"},
        {u8"ちゃ", u8"cha"}, {u8"ちゅ", u8"chu"}, {u8"ちょ", u8"cho"},
        {u8"にゃ", u8"nya"}, {u8"にゅ", u8"nyu"}, {u8"にょ", u8"nyo"},
        {u8"ひゃ", u8"hya"}, {u8"ひゅ", u8"hyu"}, {u8"ひょ", u8"hyo"},
        {u8"みゃ", u8"mya"}, {u8"みゅ", u8"myu"}, {u8"みょ", u8"myo"},
        {u8"りゃ", u8"rya"}, {u8"りゅ", u8"ryu"}, {u8"りょ", u8"ryo"},
        {u8"ぎゃ", u8"gya"}, {u8"ぎゅ", u8"gyu"}, {u8"ぎょ", u8"gyo"},
        {u8"じゃ", u8"ja"},  {u8"じゅ", u8"ju"},  {u8"じょ", u8"jo"},
        {u8"びゃ", u8"bya"}, {u8"びゅ", u8"byu"}, {u8"びょ", u8"byo"},
        {u8"ぴゃ", u8"pya"}, {u8"ぴゅ", u8"pyu"}, {u8"ぴょ", u8"pyo"}};

const std::unordered_map<std::u8string, std::u8string> kKatakanaToRomajiMap = {
    {u8"ア", u8"a"},   {u8"イ", u8"i"},  {u8"ウ", u8"u"},   {u8"エ", u8"e"},
    {u8"オ", u8"o"},   {u8"カ", u8"ka"}, {u8"キ", u8"ki"},  {u8"ク", u8"ku"},
    {u8"ケ", u8"ke"},  {u8"コ", u8"ko"}, {u8"ガ", u8"ga"},  {u8"ギ", u8"gi"},
    {u8"グ", u8"gu"},  {u8"ケ", u8"ge"}, {u8"ゴ", u8"go"},  {u8"サ", u8"sa"},
    {u8"シ", u8"shi"}, {u8"ス", u8"su"}, {u8"セ", u8"se"},  {u8"ソ", u8"so"},
    {u8"ザ", u8"za"},  {u8"ジ", u8"ji"}, {u8"ズ", u8"zu"},  {u8"ゼ", u8"ze"},
    {u8"ゾ", u8"zo"},  {u8"タ", u8"ta"}, {u8"チ", u8"chi"}, {u8"ツ", u8"tsu"},
    {u8"テ", u8"te"},  {u8"ト", u8"to"}, {u8"ダ", u8"da"},  {u8"ヂ", u8"ji"},
    {u8"ヅ", u8"zu"},  {u8"デ", u8"de"}, {u8"ド", u8"do"},  {u8"ナ", u8"na"},
    {u8"ニ", u8"ni"},  {u8"ヌ", u8"nu"}, {u8"ネ", u8"ne"},  {u8"ノ", u8"no"},
    {u8"ハ", u8"ha"},  {u8"ヒ", u8"hi"}, {u8"フ", u8"fu"},  {u8"ヘ", u8"he"},
    {u8"ホ", u8"ho"},  {u8"バ", u8"ba"}, {u8"ビ", u8"bi"},  {u8"ブ", u8"bu"},
    {u8"ベ", u8"be"},  {u8"ボ", u8"bo"}, {u8"パ", u8"pa"},  {u8"ピ", u8"pi"},
    {u8"プ", u8"pu"},  {u8"ペ", u8"pe"}, {u8"ポ", u8"bo"},  {u8"マ", u8"ma"},
    {u8"ミ", u8"mi"},  {u8"ム", u8"mu"}, {u8"メ", u8"me"},  {u8"モ", u8"mo"},
    {u8"ヤ", u8"ya"},  {u8"ユ", u8"yu"}, {u8"ヨ", u8"yo"},  {u8"ラ", u8"ra"},
    {u8"リ", u8"ri"},  {u8"ル", u8"ru"}, {u8"レ", u8"re"},  {u8"ロ", u8"ro"},
    {u8"ワ", u8"wa"},  {u8"ヲ", u8"wo"}, {u8"ン", u8"n"},
};

const std::unordered_map<std::u8string, std::u8string>
    kKatakanaSpecialToRomajiMap = {
        {u8"キャ", u8"kya"}, {u8"キュ", u8"kyu"}, {u8"キョ", u8"kyo"},
        {u8"シャ", u8"sha"}, {u8"シュ", u8"shu"}, {u8"ショ", u8"sho"},
        {u8"チャ", u8"cha"}, {u8"チュ", u8"chu"}, {u8"チョ", u8"cho"},
        {u8"ニャ", u8"nya"}, {u8"ニュ", u8"nyu"}, {u8"ニョ", u8"nyo"},
        {u8"ヒャ", u8"hya"}, {u8"ヒュ", u8"hyu"}, {u8"ヒョ", u8"hyo"},
        {u8"ミャ", u8"mya"}, {u8"ミュ", u8"myu"}, {u8"ミョ", u8"myo"},
        {u8"リャ", u8"rya"}, {u8"リュ", u8"ryu"}, {u8"リョ", u8"ryo"},
        {u8"ギャ", u8"gya"}, {u8"ギュ", u8"gyu"}, {u8"ギョ", u8"gyo"},
        {u8"ジャ", u8"ja"},  {u8"ジュ", u8"ju"},  {u8"ジョ", u8"jo"},
        {u8"ビャ", u8"bya"}, {u8"ビュ", u8"byu"}, {u8"ビョ", u8"byo"},
        {u8"ピャ", u8"pya"}, {u8"ピュ", u8"pyu"}, {u8"ピョ", u8"pyo"}};
}  // namespace

std::string KanaToRomaji(const std::string& kana) {
  std::u8string r(kana.begin(), kana.end());
  for (const auto& i : kHiraganaSpecialToRomajiMap) {
    size_t pos = std::u8string::npos;
    while ((pos = r.find(i.first)) != std::u8string::npos) {
      r.replace(pos, i.first.size(), i.second);
    }
  }

  for (const auto& i : kKatakanaSpecialToRomajiMap) {
    size_t pos = std::u8string::npos;
    while ((pos = r.find(i.first)) != std::u8string::npos) {
      r.replace(pos, i.first.size(), i.second);
    }
  }

  for (const auto& i : kHiraganaToRomajiMap) {
    size_t pos = std::u8string::npos;
    while ((pos = r.find(i.first)) != std::u8string::npos) {
      r.replace(pos, i.first.size(), i.second);
    }
  }

  for (const auto& i : kKatakanaToRomajiMap) {
    size_t pos = std::u8string::npos;
    while ((pos = r.find(i.first)) != std::u8string::npos) {
      r.replace(pos, i.first.size(), i.second);
    }
  }

  size_t pos = std::u8string::npos;
  while ((pos = r.find(u8"っ")) != std::u8string::npos) {
    if (pos < r.size() - 1)
      r.replace(pos, sizeof(u8"っ") - 1,
                std::u8string(r[pos + sizeof(u8"っ") - 1], 1));
    else
      r.replace(pos, 1, u8"xtsu");
  }

  return std::string(r.begin(), r.end());
}

}  // namespace language_conversion