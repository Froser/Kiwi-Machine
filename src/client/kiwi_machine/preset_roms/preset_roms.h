
#ifndef PRESET_ROMS_PRESET_ROMS_H_
#define PRESET_ROMS_PRESET_ROMS_H_
#include <kiwi_nes.h>

namespace preset_roms {
struct PresetROM {
  const char* name;
  const kiwi::nes::Byte* data;
  bool compressed;
  size_t raw_size;
  size_t compressed_size;
  const kiwi::nes::Byte* cover;
  size_t cover_size;
};

#define PRESET_ROM(name) \
  name::ROM_NAME, name::ROM_DATA, name::ROM_COMPRESSED, \
  name::ROM_SIZE, name::ROM_COMPRESSED_SIZE, \
  name::ROM_COVER, name::ROM_COVER_SIZE
  
#define EXTERN_ROM(name) \
namespace name { \
  extern const char ROM_NAME[]; \
  extern const kiwi::nes::Byte ROM_DATA[]; \
  extern bool ROM_COMPRESSED; \
  extern size_t ROM_SIZE; \
  extern size_t ROM_COMPRESSED_SIZE; \
  extern const kiwi::nes::Byte ROM_COVER[]; \
  extern size_t ROM_COVER_SIZE; \
}

EXTERN_ROM(arkanoid_usa)
EXTERN_ROM(balloon_fight_usa)
EXTERN_ROM(battlecity_japan)
EXTERN_ROM(bomber_man_ii_japan)
EXTERN_ROM(castlevania_usa)
EXTERN_ROM(castlevania_ii__simons_quest_usa)
EXTERN_ROM(chackn_pop_japan)
EXTERN_ROM(chip_n_dale__rescue_rangers_usa)
EXTERN_ROM(chip_n_dale__rescue_rangers_2_usa)
EXTERN_ROM(circus_charlie_japan)
EXTERN_ROM(clu_clu_land_world)
EXTERN_ROM(contra_u)
EXTERN_ROM(crystalis_usa)
EXTERN_ROM(donkey_kong_usa_gamecube_edition)
EXTERN_ROM(dr_mario_japan_usa)
EXTERN_ROM(dragon_warrior_usa_rev_a)
EXTERN_ROM(dragon_warrior_2_u)
EXTERN_ROM(excitebike_japan_usa)
EXTERN_ROM(field_combat_japan)
EXTERN_ROM(final_fantasy_usa)
EXTERN_ROM(flipull__an_exciting_cube_game_japan_en)
EXTERN_ROM(ghostsn_goblins_usa)
EXTERN_ROM(goonies_japan)
EXTERN_ROM(gradius_usa)
EXTERN_ROM(ice_climber_japan)
EXTERN_ROM(kirbys_adventure_usa_rev_a)
EXTERN_ROM(kuniokun_no_nekketsu_soccer_league_japan)
EXTERN_ROM(legend_of_zelda_the_usa_rev_a)
EXTERN_ROM(lode_runner_usa)
EXTERN_ROM(lunar_pool_usa)
EXTERN_ROM(mappy_japan)
EXTERN_ROM(mario_bros_usa_gamecube_edition)
EXTERN_ROM(metroid_usa)
EXTERN_ROM(milons_secret_castle_usa)
EXTERN_ROM(mitsume_ga_tooru_japan)
EXTERN_ROM(nekketsu_kakutou_densetsu_japan)
EXTERN_ROM(ninja_gaiden_usa)
EXTERN_ROM(ninja_gaiden_ii__the_dark_sword_of_chaos_usa)
EXTERN_ROM(nuts__milk_japan)
EXTERN_ROM(pacman_usa_namco)
EXTERN_ROM(pinball_world)
EXTERN_ROM(pooyan_japan)
EXTERN_ROM(quarth_japan)
EXTERN_ROM(river_city_ransom_usa)
EXTERN_ROM(road_fighter_japan)
EXTERN_ROM(rockman_japan_en)
EXTERN_ROM(rockman_2__dr_wily_no_nazo_japan)
EXTERN_ROM(rockman_3__dr_wily_no_saigo_japan)
EXTERN_ROM(rockman_4__aratanaru_yabou_japan)
EXTERN_ROM(rockman_5__blues_no_wana_japan)
EXTERN_ROM(rockman_6__shijou_saidai_no_tatakai_japan)
EXTERN_ROM(saiyuuki_world_japan)
EXTERN_ROM(shin_jinrui__the_new_type_japan)
EXTERN_ROM(sky_destroyer_japan)
EXTERN_ROM(snow_brothers_usa)
EXTERN_ROM(star_soldier_japan)
EXTERN_ROM(super_c_usa)
EXTERN_ROM(super_mario_bros_world)
EXTERN_ROM(super_mario_bros_2_usa)
EXTERN_ROM(super_mario_bros_3_usa)
EXTERN_ROM(takahashi_meijin_no_boukenjima_japan)
EXTERN_ROM(teenage_mutant_ninja_turtles_usa)
EXTERN_ROM(teenage_mutant_ninja_turtles_ii__the_arcade_game_usa)
EXTERN_ROM(teenage_mutant_ninja_turtles_iii__the_manhattan_project_usa)
EXTERN_ROM(tetris_usa)
EXTERN_ROM(tetris_2_usa)
EXTERN_ROM(tiny_toon_adventures_usa)
EXTERN_ROM(tiny_toon_adventures_2__trouble_in_wackyland_usa)
EXTERN_ROM(twinbee_japan)
EXTERN_ROM(yie_ar_kungfu_japan_rev_14)


const PresetROM kPresetRoms[] = {
  {PRESET_ROM(arkanoid_usa)},
  {PRESET_ROM(balloon_fight_usa)},
  {PRESET_ROM(battlecity_japan)},
  {PRESET_ROM(bomber_man_ii_japan)},
  {PRESET_ROM(castlevania_usa)},
  {PRESET_ROM(castlevania_ii__simons_quest_usa)},
  {PRESET_ROM(chackn_pop_japan)},
  {PRESET_ROM(chip_n_dale__rescue_rangers_usa)},
  {PRESET_ROM(chip_n_dale__rescue_rangers_2_usa)},
  {PRESET_ROM(circus_charlie_japan)},
  {PRESET_ROM(clu_clu_land_world)},
  {PRESET_ROM(contra_u)},
  {PRESET_ROM(crystalis_usa)},
  {PRESET_ROM(donkey_kong_usa_gamecube_edition)},
  {PRESET_ROM(dr_mario_japan_usa)},
  {PRESET_ROM(dragon_warrior_usa_rev_a)},
  {PRESET_ROM(dragon_warrior_2_u)},
  {PRESET_ROM(excitebike_japan_usa)},
  {PRESET_ROM(field_combat_japan)},
  {PRESET_ROM(final_fantasy_usa)},
  {PRESET_ROM(flipull__an_exciting_cube_game_japan_en)},
  {PRESET_ROM(ghostsn_goblins_usa)},
  {PRESET_ROM(goonies_japan)},
  {PRESET_ROM(gradius_usa)},
  {PRESET_ROM(ice_climber_japan)},
  {PRESET_ROM(kirbys_adventure_usa_rev_a)},
  {PRESET_ROM(kuniokun_no_nekketsu_soccer_league_japan)},
  {PRESET_ROM(legend_of_zelda_the_usa_rev_a)},
  {PRESET_ROM(lode_runner_usa)},
  {PRESET_ROM(lunar_pool_usa)},
  {PRESET_ROM(mappy_japan)},
  {PRESET_ROM(mario_bros_usa_gamecube_edition)},
  {PRESET_ROM(metroid_usa)},
  {PRESET_ROM(milons_secret_castle_usa)},
  {PRESET_ROM(mitsume_ga_tooru_japan)},
  {PRESET_ROM(nekketsu_kakutou_densetsu_japan)},
  {PRESET_ROM(ninja_gaiden_usa)},
  {PRESET_ROM(ninja_gaiden_ii__the_dark_sword_of_chaos_usa)},
  {PRESET_ROM(nuts__milk_japan)},
  {PRESET_ROM(pacman_usa_namco)},
  {PRESET_ROM(pinball_world)},
  {PRESET_ROM(pooyan_japan)},
  {PRESET_ROM(quarth_japan)},
  {PRESET_ROM(river_city_ransom_usa)},
  {PRESET_ROM(road_fighter_japan)},
  {PRESET_ROM(rockman_japan_en)},
  {PRESET_ROM(rockman_2__dr_wily_no_nazo_japan)},
  {PRESET_ROM(rockman_3__dr_wily_no_saigo_japan)},
  {PRESET_ROM(rockman_4__aratanaru_yabou_japan)},
  {PRESET_ROM(rockman_5__blues_no_wana_japan)},
  {PRESET_ROM(rockman_6__shijou_saidai_no_tatakai_japan)},
  {PRESET_ROM(saiyuuki_world_japan)},
  {PRESET_ROM(shin_jinrui__the_new_type_japan)},
  {PRESET_ROM(sky_destroyer_japan)},
  {PRESET_ROM(snow_brothers_usa)},
  {PRESET_ROM(star_soldier_japan)},
  {PRESET_ROM(super_c_usa)},
  {PRESET_ROM(super_mario_bros_world)},
  {PRESET_ROM(super_mario_bros_2_usa)},
  {PRESET_ROM(super_mario_bros_3_usa)},
  {PRESET_ROM(takahashi_meijin_no_boukenjima_japan)},
  {PRESET_ROM(teenage_mutant_ninja_turtles_usa)},
  {PRESET_ROM(teenage_mutant_ninja_turtles_ii__the_arcade_game_usa)},
  {PRESET_ROM(teenage_mutant_ninja_turtles_iii__the_manhattan_project_usa)},
  {PRESET_ROM(tetris_usa)},
  {PRESET_ROM(tetris_2_usa)},
  {PRESET_ROM(tiny_toon_adventures_usa)},
  {PRESET_ROM(tiny_toon_adventures_2__trouble_in_wackyland_usa)},
  {PRESET_ROM(twinbee_japan)},
  {PRESET_ROM(yie_ar_kungfu_japan_rev_14)},
};
namespace specials {
EXTERN_ROM(extra_mario_bros)
EXTERN_ROM(funny_princess)
EXTERN_ROM(kamikaze_v3)
EXTERN_ROM(luigis_chronicles)
EXTERN_ROM(special_mario)
EXTERN_ROM(tank_1990)

const PresetROM kPresetRoms[] = {
  {PRESET_ROM(extra_mario_bros)},
  {PRESET_ROM(funny_princess)},
  {PRESET_ROM(kamikaze_v3)},
  {PRESET_ROM(luigis_chronicles)},
  {PRESET_ROM(special_mario)},
  {PRESET_ROM(tank_1990)},
};
} // namespace specials {
} // namespace preset_roms

#undef EXTERN_ROM
#endif  // PRESET_ROMS_PRESET_ROMS_H_