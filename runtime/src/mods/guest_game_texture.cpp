// The game texture decoder behind wwhd_hud_game_pixels (guest_mods.h read_game_texture): the port's
// own CPU decoder of GX2 textures in guest memory (game_glyph::read_texture). Kept out of
// guest_mods.cpp so its unit test links without the texture decoder.
#include "guest_mods.h"
#include "../game_glyph.h"

namespace {
bool read(uint32_t texture, uint32_t& width, uint32_t& height, std::vector<uint8_t>& rgba) {
    game_glyph::Bitmap b;
    if (!game_glyph::read_texture(texture, b) || b.width <= 0 || b.height <= 0) return false;
    width = uint32_t(b.width), height = uint32_t(b.height);
    rgba = std::move(b.rgba);
    return true;
}
const bool registered = (guestmods::read_game_texture = read, true);
}  // namespace
