// Multiplayer skins: other Link models, one per player (examples/guest-mods/multiplayer-connection
// skin.h). A skin is an archive the game loads as an object, Object/MpSk_<id>.szs, which no disc has:
// it is read from the multiplayer mod's Data folder, skin_<id>.szs, made on the player's machine by
// tools/multiplayer/make_skin.py (or the launcher) from a Link model mod. Nothing else is redirected.
#pragma once
#include <string>

namespace mp_skins {

// the multiplayer mod's Data folder (empty: no skins); set when that mod is loaded
void set_directory(const std::string& data_dir);
// the file to read instead of the game's for a content path the game opens; empty when the path is
// not a skin archive or the skin file is not there (any thread)
std::string archive_for(const std::string& guest);
// the skin id of a guest path ".../Object/MpSk_<id>.szs" (a-z 0-9 _, 1 to 12, any case in the
// path; lower case returned), empty when it is not one
std::string skin_id(const std::string& guest);

}  // namespace mp_skins
