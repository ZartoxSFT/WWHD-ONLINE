// mp_skins: which guest paths are multiplayer skins, and the file each one is read from.
#include "mods/mp_skins.h"

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

void log_msg(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
}

int main() {
    namespace fs = std::filesystem;
    using mp_skins::archive_for;
    using mp_skins::skin_id;

    assert(skin_id("/vol/content/Common/Object/MpSk_miku.szs") == "miku");
    assert(skin_id("Object/MpSk_miku.szs") == "miku");
    assert(skin_id("/vol/content/Common/Object/MPSK_Miku_2.SZS") == "miku_2");
    assert(skin_id("/vol/content/Common/Object/MpSk_.szs").empty());
    assert(skin_id("/vol/content/Common/Object/MpSk_abcdefghijklm.szs").empty()); /* 13 */
    assert(skin_id("/vol/content/Common/Object/MpSk_a-b.szs").empty());
    assert(skin_id("/vol/content/Common/Object/MpSk_../x.szs").empty());
    assert(skin_id("/vol/content/Common/Stage/MpSk_miku.szs").empty());
    assert(skin_id("/vol/content/Common/Object/Link.szs").empty());
    assert(skin_id("/vol/content/Common/Object/MpSk_miku.szs.bak").empty());
    assert(skin_id("MpSk_miku.szs").empty());

    const fs::path dir = fs::temp_directory_path() / "mp_skins_test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    std::ofstream(dir / "skin_miku.szs") << "Yaz0";

    assert(archive_for("/vol/content/Common/Object/MpSk_miku.szs").empty()); /* no folder yet */
    mp_skins::set_directory(dir.string());
    assert(fs::path(archive_for("/vol/content/Common/Object/MpSk_miku.szs")) == dir / "skin_miku.szs");
    assert(fs::path(archive_for("/vol/content/Common/Object/MPSK_MIKU.szs")) == dir / "skin_miku.szs");
    assert(archive_for("/vol/content/Common/Object/MpSk_other.szs").empty()); /* not installed */
    assert(archive_for("/vol/content/Common/Object/Link.szs").empty());
    mp_skins::set_directory("");
    assert(archive_for("/vol/content/Common/Object/MpSk_miku.szs").empty());

    fs::remove_all(dir);
    std::puts("mp_skins_test: ok");
    return 0;
}
