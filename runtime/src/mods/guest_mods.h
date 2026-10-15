// Mod SDK v2 runtime. Guest modules load only at startup, through the mod manager's trust flow.
#pragma once
#include <cstdint>
#include <vector>
#include "guest_identity.h"
struct Cpu;
namespace guestmods {
bool hooks_built();  // marker registered by the running generated game code, never the setting
inline constexpr uint32_t kRegionStart=0x7F000000,kRegionSize=0x01000000;
std::vector<ModIdentity> enabled_mods();
void init(); // after dispatch::init, before guest threads start
void frame(uint64_t step); // original full-step clock
void draw_frame(Cpu* cpu,uint64_t executed_step); // once per actual logic pass, including true60
void state_loaded(); // discard host HUD lists and texture handles after a full state load
void service(Cpu* cpu); // at the frame boundary (main thread, other threads idle): story layouts' builds after a load
// wwhd_hud_game_pixels: decodes a GX2Texture of the running game into RGBA8 (game_glyph::read_texture,
// set by guest_game_texture.cpp; null in tests without the decoder)
extern bool (*read_game_texture)(uint32_t texture,uint32_t& width,uint32_t& height,std::vector<uint8_t>& rgba);
}
