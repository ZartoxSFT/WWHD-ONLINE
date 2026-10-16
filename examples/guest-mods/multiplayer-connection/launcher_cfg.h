/* launcher.cfg: settings written by the launcher into this mod's Data folder
 * (<user>/ModManager/Data/multiplayer-connection/launcher.cfg) and read with wwhd_file_read.
 * It replaces the GameCube launcher's writes into Dolphin memory (0x803A9520 cursor colour,
 * 0x803A9524/0x803A9528 tunic colours): the game is never written from outside.
 *
 * Format: UTF-8/ASCII lines "key=value", '#' comments, CR/LF or LF, at most 4096 bytes.
 * Keys: uid (exactly 8 bytes), host, port (1..65535), color, color2, ship_color, cursor
 * (RRGGBB with an optional '#'), puppet (0/1), hide_names (0/1: no name above any player),
 * outfit (auto, hero, casual: the clothes, outfit.h), name (1..16 printable ASCII characters, shown
 * above the player's head on the other clients), skin (another Link model, skin.h: 1..12 of a-z 0-9 _,
 * the file skin_<id>.szs next to launcher.cfg; empty for Link). Unknown keys are ignored; a malformed value is ignored on its own
 * (the key keeps its manifest option), so a half-written file never breaks the mod.
 * Freestanding C, shared by the PowerPC guest and the native tests. */
#pragma once

#define MP_CFG_MAX_BYTES 4096u

typedef struct mp_launcher_cfg {
    int has_uid;
    char uid[9];
    int has_host;
    char host[256];
    int has_port;
    unsigned int port;
    int has_color, has_color2, has_ship_color, has_cursor;
    unsigned int color, color2, ship_color, cursor; /* 0xRRGGBB */
    int has_puppet;
    int puppet;
    int has_hide_names;
    int hide_names;
    int has_outfit;
    int outfit;          /* -1 auto (the save), 0 hero's clothes, 1 casual clothes */
    int has_name;
    char name[17];
    int has_skin;
    char skin[13];       /* "" for Link */
} mp_launcher_cfg;

/* "RRGGBB" with an optional leading '#'; 0 when malformed. */
static inline int mp_parse_rgb(const char* text, unsigned int length, unsigned int* rgb) {
    unsigned int value = 0, i = 0;
    if (length && text[0] == '#') ++i;
    if (length - i != 6) return 0;
    for (; i != length; ++i) {
        char c = text[i];
        unsigned int d = c >= '0' && c <= '9' ? (unsigned int)(c - '0')
                       : c >= 'a' && c <= 'f' ? (unsigned int)(c - 'a' + 10)
                       : c >= 'A' && c <= 'F' ? (unsigned int)(c - 'A' + 10) : 16u;
        if (d == 16u) return 0;
        value = (value << 4) | d;
    }
    *rgb = value;
    return 1;
}

static inline int mp_cfg_key(const char* key, unsigned int length, const char* wanted) {
    unsigned int i = 0;
    for (; i != length; ++i)
        if (!wanted[i] || wanted[i] != key[i]) return 0;
    return wanted[i] == 0;
}

static inline void mp_cfg_value(const char* key, unsigned int key_length, const char* value,
                                unsigned int length, mp_launcher_cfg* cfg) {
    unsigned int rgb;
    if (mp_cfg_key(key, key_length, "uid")) {
        if (length != 8) return;
        for (unsigned int i = 0; i != 8; ++i) cfg->uid[i] = value[i];
        cfg->uid[8] = 0;
        cfg->has_uid = 1;
    } else if (mp_cfg_key(key, key_length, "host")) {
        if (length == 0 || length >= sizeof(cfg->host)) return;
        for (unsigned int i = 0; i != length; ++i) cfg->host[i] = value[i];
        cfg->host[length] = 0;
        cfg->has_host = 1;
    } else if (mp_cfg_key(key, key_length, "port")) {
        unsigned int port = 0;
        if (length == 0 || length > 5) return;
        for (unsigned int i = 0; i != length; ++i) {
            if (value[i] < '0' || value[i] > '9') return;
            port = port * 10 + (unsigned int)(value[i] - '0');
        }
        if (port == 0 || port > 65535) return;
        cfg->port = port;
        cfg->has_port = 1;
    } else if (mp_cfg_key(key, key_length, "color")) {
        if (mp_parse_rgb(value, length, &rgb)) { cfg->color = rgb; cfg->has_color = 1; }
    } else if (mp_cfg_key(key, key_length, "color2")) {
        if (mp_parse_rgb(value, length, &rgb)) { cfg->color2 = rgb; cfg->has_color2 = 1; }
    } else if (mp_cfg_key(key, key_length, "ship_color")) {
        if (mp_parse_rgb(value, length, &rgb)) {
            cfg->ship_color = rgb;
            cfg->has_ship_color = 1;
        }
    } else if (mp_cfg_key(key, key_length, "cursor")) {
        if (mp_parse_rgb(value, length, &rgb)) { cfg->cursor = rgb; cfg->has_cursor = 1; }
    } else if (mp_cfg_key(key, key_length, "name")) {
        if (length == 0 || length > 16) return;
        for (unsigned int i = 0; i != length; ++i)
            if (value[i] < 0x20 || value[i] > 0x7E) return;
        for (unsigned int i = 0; i != length; ++i) cfg->name[i] = value[i];
        cfg->name[length] = 0;
        cfg->has_name = 1;
    } else if (mp_cfg_key(key, key_length, "hide_names")) {
        if (length == 1 && (value[0] == '0' || value[0] == '1')) {
            cfg->hide_names = value[0] == '1';
            cfg->has_hide_names = 1;
        }
    } else if (mp_cfg_key(key, key_length, "outfit")) {
        if (length == 4 && value[0] == 'a' && value[1] == 'u' && value[2] == 't' && value[3] == 'o') {
            cfg->outfit = -1;
            cfg->has_outfit = 1;
        } else if (length == 4 && value[0] == 'h' && value[1] == 'e' && value[2] == 'r' && value[3] == 'o') {
            cfg->outfit = 0;
            cfg->has_outfit = 1;
        } else if (length == 6 && value[0] == 'c' && value[1] == 'a' && value[2] == 's' && value[3] == 'u' &&
                   value[4] == 'a' && value[5] == 'l') {
            cfg->outfit = 1;
            cfg->has_outfit = 1;
        }
    } else if (mp_cfg_key(key, key_length, "skin")) {
        if (length > 12) return;
        for (unsigned int i = 0; i != length; ++i) {
            char c = value[i];
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return;
        }
        for (unsigned int i = 0; i != 13; ++i) cfg->skin[i] = i < length ? value[i] : 0;
        cfg->has_skin = 1;
    } else if (mp_cfg_key(key, key_length, "puppet")) {
        if (length == 1 && (value[0] == '0' || value[0] == '1')) {
            cfg->puppet = value[0] == '1';
            cfg->has_puppet = 1;
        }
    }
}

/* Parses text[0..size); returns 0 only when the size is out of bounds. */
static inline int mp_parse_launcher_cfg(const char* text, unsigned int size, mp_launcher_cfg* cfg) {
    char* raw = (char*)cfg;
    for (unsigned int i = 0; i != sizeof(*cfg); ++i) raw[i] = 0;
    if (size > MP_CFG_MAX_BYTES) return 0;
    unsigned int pos = 0;
    while (pos < size) {
        unsigned int start = pos, end, eq = (unsigned int)-1;
        while (pos < size && text[pos] != '\n') {
            if (text[pos] == '=' && eq == (unsigned int)-1) eq = pos;
            ++pos;
        }
        end = pos;
        ++pos; /* skip '\n' */
        if (end > start && text[end - 1] == '\r') --end;
        while (start < end && (text[start] == ' ' || text[start] == '\t')) ++start;
        if (start == end || text[start] == '#' || eq == (unsigned int)-1 || eq >= end) continue;
        unsigned int key_end = eq, value_start = eq + 1, value_end = end;
        while (key_end > start && (text[key_end - 1] == ' ' || text[key_end - 1] == '\t')) --key_end;
        while (value_start < value_end && (text[value_start] == ' ' || text[value_start] == '\t')) ++value_start;
        while (value_end > value_start && (text[value_end - 1] == ' ' || text[value_end - 1] == '\t')) --value_end;
        mp_cfg_value(text + start, key_end - start, text + value_start, value_end - value_start, cfg);
    }
    return 1;
}
