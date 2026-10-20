/* Bigger game heaps: room for the skins of many players.
 *
 * The game sizes its heaps for the Wii U (decomp hd/hd_sys_0203E584.cpp, hd/hd_sys_0203E9EC.cpp):
 *  - getRootHeapSizes 0203E5F4: the main root heap (heap 6, hdRootGetMainHeap 0203E8F4) is
 *    0x1F200000, carved from sead's root heap (createRootHeap 0203E67C, parent sSeadRootHeaps[0]);
 *  - the root task 0203EE2C: from the main heap, "command heap" overhead(4) + 0xE00000 and
 *    "system heap" 0x16C00000 - that; the "data heap" (overhead(0x2000) + 0x0D400000) is made in the
 *    system heap and the rest of the system heap is the object parent heap,
 *    ResourceControlHD::mpObjectParentHeap +0x2040 (store at 0203F244), where the skins load.
 * The runtime's MEM2 goes up to 0x60000000 (runtime.h mem::kMem2End, 0x50000000 on the console),
 * its default heap is sead's arena (runtime/src/hle/coreinit_mem.cpp): the main heap and the system heap are both made bigger by the same amount,
 * which ends up free in the object parent heap. The amount is what sead's root heap can still give
 * once the root heaps made after the main one (HG_LATER_ROOT_HEAPS) and a margin are kept, at most
 * HG_WANTED; nothing changes when there is no room.
 *
 * Every heap goes through hdRootCreateExpHeap 02753004 (size, SafeString* name, parent, direction,
 * flag; register arguments only, see the generated code): a size of 0 asks the parent's
 * getMaxAllocatableSize (vtable +0x7C, alignment 4), the call used here to measure the room. */
#define HG_MAIN_SIZE 0x1F200000u        /* getRootHeapSizes()[5], the main heap */
#define HG_SYSTEM_TOTAL 0x16C00000u     /* command heap + system heap */
#define HG_COMMAND_BASE 0x00E00000u     /* command heap without the heap header */
#define HG_LATER_ROOT_HEAPS 0x04D00000u /* root heaps 7 to 12 (createRootHeaps 0203E6A4) */
#define HG_MARGIN 0x08000000u           /* left to sead's root heap: the game's own 125 MiB, 63 failed
                                         * (mDoExt_createGameHeap, m_Do_machine.cpp line 196) */
#define HG_WANTED 0x10000000u           /* at most 256 MiB more */
#define HG_MAX_ALLOC_SLOT 0x7C

/* 0.3.36: sead's arena. With MEM2 up to 0x60000000 the log still gave 701 MiB of room in sead's
 * root heap (made bigger by 0): the arena is not all of the default heap. Generated code (no decomp
 * source): 02756D18 (EU 027575D4; called once by 02755F80 at boot) takes (r3 the arena's record,
 * r4 the size wanted), keeps min(wanted, MEMGetAllocatableSizeForExpHeapEx(MEMGetBaseHeapHandle(1),
 * 4)), allocates it from the default heap and stores the size at +4, the memory at +0. The wanted
 * size is made HG_ARENA_MORE bigger; the min with the default heap's room stays the game's. */
#define HG_ARENA_MORE 0x10000000u
/* 0.3.38: 0.3.37 drew no model either. Until the cause is known the game's sizes are the default:
 * option "heap_mode" (manifest), read at boot: 0 the game's heaps (default), 1 sead's arena bigger
 * only, 2 the arena, the main heap and the system heap bigger (0.3.37). Tests 1 and 2 tell whether
 * the arena's memory above 0x50000000 or the bigger game heaps break the drawing. */
static int hg_mode(void) {
    static int mode = -1;
    if (mode < 0) {
        mode = wwhd_config_int("heap_mode", 1);
        if (mode < 0 || mode > 2) mode = 1;
        wwhd_log_int("multiplayer: heaps: mode (0 game, 1 arena and skin heap, 2 arena and game heaps)", mode);
    }
    return mode;
}

WWHD_GAME_ORIGINAL(0x02756D18, void, hg_arena_original, (u32 record, u32 wanted));
WWHD_REPLACE(0x02756D18, void, hg_arena, (u32 record, u32 wanted)) {
    if (hg_mode() < 1) {
        hg_arena_original(record, wanted);
        wwhd_log_int("multiplayer: heaps: sead's arena, the game's (MiB)", record ? (int)(*(const volatile u32*)(record + 4) >> 20) : -1);
        return;
    }
    u32 more = wanted < 0xFFFFFFFFu - HG_ARENA_MORE ? wanted + HG_ARENA_MORE : wanted;
    hg_arena_original(record, more);
    wwhd_log_int("multiplayer: heaps: sead's arena (MiB)", record ? (int)(*(const volatile u32*)(record + 4) >> 20) : -1);
}

/* 0.3.37: what goes above 0x50000000. 0.3.36 (arena +256 MiB, main heap +254 MiB): no model was
 * drawn at all (interp "models per step: 0.0" in play), the game ran. The main heap still ended
 * under 0x50000000; the root heaps made after it (7 to 12) went above: heap 7 is the play scene's
 * display allocator (0203E900, d_s_play_3.cpp phase_00: prepareDisplayHeap / attachDisplayHeap).
 * Deduced: the game's drawing does not take memory from 0x50000000 on (the runtime's draw path has
 * no such limit). So nothing of the game's own is put there any more:
 *  - before the bigger main heap, a reserve of HG_LATER_ROOT_HEAPS + HG_RESERVE_SLACK is made in
 *    sead's root heap and destroyed once the main heap is made after it (heap vtable +0x24 mDestroy,
 *    decomp d_resorce.h ResourceHeapVirtualsHD, used so in d_resorce.cpp): heaps 7 to 12 fill that
 *    room, at their usual places, under the main heap;
 *  - the data heap (hdRootCreateExpHeap(overhead(0x2000) + 0x0D400000, "data heap", system heap,
 *    ~0, 0), the tail of the system heap) is made at its head (direction 1): the end of the system
 *    heap, the only part above 0x50000000, is free object memory, used last (first fit). */
#define HG_RESERVE_SLACK 0x00400000u
#define HG_DATA_HEAP_BASE 0x0D400000u
#define HG_DESTROY_SLOT 0x24
WWHD_GAME_FUNC(0x0275320C, u32, hg_heap_overhead, (u32 kind));

static u32 hg_main, hg_command, hg_extra, hg_system, hg_root;
static int hg_later_root_heaps; /* root heaps 7 to 12 made after the main heap */
static int hg_watch_root;       /* the next heaps made in sead's root heap are logged */
static u32 hg_framework, hg_data; /* 0.3.49: sead's FrameworkCafe heap, the game's data heap (measured) */
#define HG_LATER_ROOT_COUNT 6
static u32 hg_skin_heap_make(void); /* below */
static int hg_system_done, hg_data_done;

static void hg_destroy(u32 heap) {
    u32 vtable = heap ? *(const volatile u32*)(heap + 0xC) : 0;
    u32 fn = vtable ? *(const volatile u32*)(vtable + HG_DESTROY_SLOT) : 0;
    if (fn) ((void (*)(u32))fn)(heap);
}

static u32 hg_max_alloc(u32 heap) {
    u32 vtable = heap ? *(const volatile u32*)(heap + 0xC) : 0;
    u32 fn = vtable ? *(const volatile u32*)(vtable + HG_MAX_ALLOC_SLOT) : 0;
    if (!fn) return 0;
    return ((u32(*)(u32, s32))fn)(heap, 4);
}

WWHD_GAME_ORIGINAL(0x02753004, u32, hg_create_exp_heap_original, (u32 size, u32 name, u32 parent, u32 direction, u32 flag));
WWHD_REPLACE(0x02753004, u32, hg_create_exp_heap, (u32 size, u32 name, u32 parent, u32 direction, u32 flag)) {
    if (!hg_main && size == HG_MAIN_SIZE && parent) {
        hg_root = parent; /* sead's root heap (createRootHeap 0203E67C, sSeadRootHeaps[0]) */
        u32 room = hg_max_alloc(parent), keep = HG_MAIN_SIZE + HG_LATER_ROOT_HEAPS + HG_MARGIN;
        u32 extra = room > keep && hg_mode() >= 2 ? room - keep : 0u;
        if (extra > HG_WANTED) extra = HG_WANTED;
        extra &= ~0xFFFFFu;
        wwhd_log_int("multiplayer: heaps: room in sead's root heap (MiB)", (int)(room >> 20));
        u32 reserve = extra ? hg_create_exp_heap_original(HG_LATER_ROOT_HEAPS + HG_RESERVE_SLACK, name, parent, 1, 0) : 0;
        if (extra && !reserve) {
            wwhd_log("multiplayer: heaps: no room for the later root heaps' reserve, the game's size is kept");
            extra = 0;
        }
        u32 heap = hg_create_exp_heap_original(size + extra, name, parent, direction, flag);
        if (reserve) hg_destroy(reserve); /* heaps 7 to 12 go there, under the main heap */
        if (!heap && extra) {
            wwhd_log("multiplayer: heaps: the bigger main heap could not be made, the game's size is kept");
            extra = 0;
            heap = hg_create_exp_heap_original(size, name, parent, direction, flag);
        }
        hg_main = heap;
        hg_extra = extra;
        wwhd_log_int("multiplayer: heaps: main heap made bigger by (MiB)", (int)(extra >> 20));
        return heap;
    }
    if (hg_watch_root > 0 && parent == hg_root) { /* what the game makes after the skins' heap */
        --hg_watch_root;
        u32 heap = hg_create_exp_heap_original(size, name, parent, direction, flag);
        const char* text = name ? (const char*)*(const volatile u32*)name : 0;
        wwhd_log("multiplayer: heaps: the game makes in sead's root heap:");
        wwhd_log(text ? text : "(no name)");
        wwhd_log_int("multiplayer: heaps:   size asked (KiB, 0: all)", (int)(size >> 10));
        wwhd_log_hex("multiplayer: heaps:   at", heap);
        return heap;
    }
    if (!hg_framework && hg_later_root_heaps == HG_LATER_ROOT_COUNT && parent == hg_root) {
        /* 0.3.49: the next heap of sead's root heap after the root heaps and the skins' heap is sead's
         * "FrameworkCafe" (log 0.3.46), size 0: all that is left; measured (hg_measure) */
        hg_framework = hg_create_exp_heap_original(size, name, parent, direction, flag);
        return hg_framework;
    }
    if (!hg_data && direction == 0xFFFFFFFFu && flag == 0 && size == hg_heap_overhead(0x2000) + HG_DATA_HEAP_BASE) {
        hg_data = hg_create_exp_heap_original(size, name, parent, direction, flag); /* the data heap */
        return hg_data;
    }
    if (hg_main && parent == hg_root && hg_later_root_heaps < HG_LATER_ROOT_COUNT) {
        /* 0.3.40: the skins' heap right after the last root heap (12), before the game takes all of
         * sead's root heap that is left (log 0.3.39: 0 MiB of room there at the first skin) */
        u32 heap = hg_create_exp_heap_original(size, name, parent, direction, flag);
        if (++hg_later_root_heaps == HG_LATER_ROOT_COUNT) hg_skin_heap_make();
        return heap;
    }
    if (hg_main && parent == hg_main) {
        if (!hg_command && size >= HG_COMMAND_BASE && size < HG_COMMAND_BASE + 0x1000u) hg_command = size;
        else if (hg_extra && hg_command && !hg_system_done && size == HG_SYSTEM_TOTAL - hg_command) {
            hg_system_done = 1;
            u32 heap = hg_create_exp_heap_original(size + hg_extra, name, parent, direction, flag);
            if (heap) {
                hg_system = heap;
                wwhd_log_int("multiplayer: heaps: system heap (object parent heap) made bigger by (MiB)", (int)(hg_extra >> 20));
                wwhd_log_hex("multiplayer: heaps:   main heap at", hg_main);
                wwhd_log_hex("multiplayer: heaps:   system heap at", heap);
                return heap;
            }
            wwhd_log("multiplayer: heaps: the bigger system heap could not be made, the game's size is kept");
        }
    }
    if (hg_system && parent == hg_system && !hg_data_done && direction == 0xFFFFFFFFu &&
        size == hg_heap_overhead(0x2000) + HG_DATA_HEAP_BASE) {
        hg_data_done = 1;
        u32 heap = hg_create_exp_heap_original(size, name, parent, 1, flag); /* at the system heap's head */
        if (heap) {
            wwhd_log_hex("multiplayer: heaps:   data heap at the system heap's head", heap);
            return heap;
        }
    }
    return hg_create_exp_heap_original(size, name, parent, direction, flag);
}

/* 0.3.39: a heap of the skins' own. Crash instance 3 21:17 (0.3.38, mode 0, Outset: a skin
 * changed, rooms left and entered again): in the resource thread, ResMgr writing to guest 0 (EU
 * 02613568 = USA 02612E64 ResMgr_loadArchive, as in 0.3.24). An archive is loaded into a frame heap
 * of size 0, all of the largest free block of its parent (hd_res_load_task.cpp
 * ResLoadTask::startLoad: parent = ResourceControlHD::selectObjectHeap 02607F80, then
 * createExpHeap(0, name, parent, 1, 0) and adjust): a skin unloaded leaves a hole among the game's
 * objects (~23 MB each, in mpObjectParentHeap), the next archive found too small a block, and
 * skin_room measured the total free size, not the largest block.
 * So with heap_mode 1 (the default) the game keeps its own heaps as they are and the skins load
 * elsewhere: sead's arena is HG_ARENA_MORE bigger (hg_arena), and after the root heaps a heap is made at
 * the end of sead's root heap with all its room but HG_MARGIN (the game keeps at least the room it
 * had), at most HG_SKIN_HEAP_MAX. selectObjectHeap (02607F80; generated code: r3 the control, r4 the
 * name's SafeString, r5 and r6 passed on to NeedsSeparateHeap 02603740 as they are) gives it for the
 * archives "MpSk_..." (skin.h). Their holes are among skins only. */
#define HG_SKIN_HEAP_MAX 0x10000000u
#define HG_SKIN_HEAP_MIN 0x02000000u
static u32 hg_skin_heap;
static int hg_skin_heap_tried;
static char hg_skin_heap_name[] = "MpSkinHeap";
static u32 hg_skin_heap_key[2];

static u32 hg_skin_heap_get(void) { return hg_skin_heap; }
static u32 hg_skin_heap_top;
#define HG_LOW_LIMIT 0x50000000u
#define HG_LOW_GUARD 0x00100000u

/* 0.3.44 test (option "skin_heap_high"): the 3rd skin, the first one past 0x50000000 in the
 * skins' heap (made from its head upwards), drew black (its shape right). The head of the heap up
 * to 0x50100000 is taken (heap vtable +0x34 alloc(heap, size, alignment), as hdRootCreateExpHeap
 * calls it), so the first skin loads above 0x50000000: black again means the address. */
#define HG_ALLOC_SLOT 0x34
#define HG_HIGH_FROM 0x50100000u
static u32 hg_heap_alloc(u32 heap, u32 size, s32 alignment) {
    u32 vtable = heap ? *(const volatile u32*)(heap + 0xC) : 0;
    u32 fn = vtable ? *(const volatile u32*)(vtable + HG_ALLOC_SLOT) : 0;
    return fn ? ((u32(*)(u32, u32, s32))fn)(heap, size, alignment) : 0u;
}
/* 0.3.45: in 0.3.44 a probe of alignment 4 came from the top (5BC73640, under the heap's header),
 * while the skins load at the bottom (a heap of size 0, all the largest block, then adjusted): the
 * test took nothing. Probed and taken with alignment -4 (the other end), checked by address. */
static void hg_skin_heap_test_high(void) {
    if (!wwhd_config_bool("skin_heap_high", 0)) return;
    u32 probe = hg_heap_alloc(hg_skin_heap, 16, -4);
    wwhd_log_hex("multiplayer: heaps: test, probe of alignment -4 at", probe);
    if (!probe || probe >= HG_HIGH_FROM) {
        wwhd_log("multiplayer: heaps: test, the bottom is already above 0x50100000: nothing taken");
        return;
    }
    u32 taken = hg_heap_alloc(hg_skin_heap, HG_HIGH_FROM - probe - 0x100, -4);
    wwhd_log_hex("multiplayer: heaps: test, block taken up to 0x50100000 at", taken);
    wwhd_log_int("multiplayer: heaps: test, skins' heap largest block now (KiB)", (int)(hg_max_alloc(hg_skin_heap) >> 10));
}

static u32 hg_skin_heap_make(void) {
    if (hg_skin_heap || hg_skin_heap_tried || hg_mode() != 1 || !hg_root) return hg_skin_heap;
    hg_skin_heap_tried = 1;
    u32 room = hg_max_alloc(hg_root);
    u32 size = room > HG_MARGIN ? room - HG_MARGIN : 0u;
    if (size > HG_SKIN_HEAP_MAX) size = HG_SKIN_HEAP_MAX;
    size &= ~0xFFFFFu;
    wwhd_log_int("multiplayer: heaps: room in sead's root heap after the root heaps (MiB)", (int)(room >> 20));
    if (size < HG_SKIN_HEAP_MIN) {
        wwhd_log("multiplayer: heaps: no room for the skins' heap, skins go with the game's objects");
        return 0;
    }
    hg_skin_heap_key[0] = (u32)hg_skin_heap_name;
    hg_skin_heap_key[1] = skin_string_vtable();
    /* 0.3.46: under 0x50000000. Test 0.3.45 (option skin_heap_high): a skin loaded above
     * 0x50000000 is not drawn (one skin, invisible); 0.3.43 drew the 3rd skin, the first one past it,
     * black. The cause is not found (runtime: no limit in the texture, buffer or invalidate paths).
     * So the skins' heap is made at the head of the room left (direction 1), up to 0x50000000 less
     * HG_LOW_GUARD; the heap the game makes next with all the rest (size 0) gets the memory above.
     * Where the head is: a small heap made there and destroyed (vtable +0x24).
     * 0.3.47: tested, nothing of the game was drawn: the game's next heap there (size 0, all the
     * rest) is sead's "FrameworkCafe", which makes "GraphicsSystem" in it, then above 0x50000000.
     * Kept only as the option "skin_heap_low"; by default the skins' heap is at the end again. */
    if (wwhd_config_bool("skin_heap_low", 0)) { /* 0.3.47: off, see below */
        u32 probe = hg_create_exp_heap_original(0x1000u, (u32)hg_skin_heap_key, hg_root, 1, 0);
        if (probe) hg_destroy(probe);
        wwhd_log_hex("multiplayer: heaps: room left starts at", probe);
        u32 under = probe && probe < HG_LOW_LIMIT - HG_LOW_GUARD ? HG_LOW_LIMIT - HG_LOW_GUARD - probe : 0u;
        if (size > under) size = under & ~0xFFFFFu;
        if (size < HG_SKIN_HEAP_MIN) {
            wwhd_log("multiplayer: heaps: no room under 0x50000000 for the skins' heap, skins go with the game's objects");
            return 0;
        }
        hg_skin_heap = hg_create_exp_heap_original(size, (u32)hg_skin_heap_key, hg_root, 1, 0); /* at the head */
        hg_watch_root = 4;
    } else {
        hg_skin_heap = hg_create_exp_heap_original(size, (u32)hg_skin_heap_key, hg_root, 0xFFFFFFFFu, 0); /* at the end */
    }
    if (hg_skin_heap) {
        wwhd_log_int("multiplayer: heaps: skins' heap made (MiB)", (int)(size >> 20));
        wwhd_log_hex("multiplayer: heaps:   at", hg_skin_heap);
        hg_skin_heap_test_high();
        /* 0.3.47: the top of the heap (a probe of alignment 4 comes from there, test 0.3.44); a skin
         * loads at the start of the largest free block, top - largest (skin.h skin_room) */
        hg_skin_heap_top = hg_heap_alloc(hg_skin_heap, 16, 4);
        wwhd_log_hex("multiplayer: heaps:   its top near", hg_skin_heap_top);
    } else {
        wwhd_log("multiplayer: heaps: the skins' heap could not be made, skins go with the game's objects");
    }
    return hg_skin_heap;
}

/* The room a skin can load into (skin.h skin_room): the largest block, and since 0.3.47 only its
 * part under 0x50000000 (a skin loaded above is not drawn: test 0.3.45). 0 without skins' heap. */
static u32 hg_skin_heap_room(void) {
    if (!hg_skin_heap) return 0u;
    u32 largest = hg_max_alloc(hg_skin_heap);
    /* 0.3.50: the 0x50000000 limit was the mod's own draw guard (puppet.h draw_ptr), fixed: all of
     * the skins' heap can be used again */
    if (!wwhd_config_bool("skin_heap_low_only", 0)) return largest;
    if (!hg_skin_heap_top || hg_skin_heap_top < largest) return largest;
    u32 start = hg_skin_heap_top - largest;
    return start >= HG_LOW_LIMIT ? 0u : (HG_LOW_LIMIT - start < largest ? HG_LOW_LIMIT - start : largest);
}

/* 0.3.41: "MpSk_" anywhere in the string (0.3.40 wanted it first: the skins still loaded with the
 * game's objects, the skins' heap's largest block never moved, same crash). */
static int hg_is_skin_name(u32 safe_string) {
    const char* top = safe_string ? (const char*)*(const volatile u32*)safe_string : 0;
    if (!top) return 0;
    for (int i = 0; i != 96 && top[i]; ++i)
        if (top[i] == 'M' && top[i + 1] == 'p' && top[i + 2] == 'S' && top[i + 3] == 'k' && top[i + 4] == '_') return 1;
    return 0;
}

WWHD_GAME_ORIGINAL(0x02607F80, u32, hg_select_object_heap_original, (u32 control, u32 name, u32 path, u32 heap));
WWHD_REPLACE(0x02607F80, u32, hg_select_object_heap, (u32 control, u32 name, u32 path, u32 heap)) {
    /* ResLoadTask::startLoad (hd_res_load_task.cpp) gives &request->mResource as the name and the
     * request as the path: its mArchive (FixedString64 at +0, text pointer at +0) is the archive */
    if (hg_skin_heap && (hg_is_skin_name(name) || hg_is_skin_name(path))) {
        static u32 logged;
        if (logged++ < 4) wwhd_log("multiplayer: heaps: a skin archive goes to the skins' heap");
        return hg_skin_heap;
    }
    return hg_select_object_heap_original(control, name, path, heap);
}

/* 0.3.42: crash instance 3 21:52 (0.3.41, 3rd skin, the skins in their heap): read of guest 0x0C in
 * ResourceControlHD::selectStageArchiveHeap (EU 02604388 = USA 02603C84, decomp d_resorce.cpp: the
 * sea's LOD archives; it calls getFreeSize on each of mStageArchiveHeaps[22] (+0x20D0) with no null
 * check). createStageHeaps (02603ACC) makes them inside a 1.5 MiB heap cut from mpStageParentHeap
 * (+0x2044, the 14 MiB "command heap" of the root task 0203EE2C): it was full, they stayed null.
 * Replaced (generated code: r3 the control, r4 the name; two arguments): when the stage heap or one
 * of those heaps is missing the archive goes to the stage heap, else the stage parent heap, logged.
 * And each scene load logs the free and largest block of the resource heaps (hg_log_heaps), to find
 * what fills the command heap. */
#define HG_CONTROL 0x101F4F28           /* dComIfG_resControl() */
#define HG_RESOURCE_HEAP 0x203C
#define HG_OBJECT_PARENT_HEAP 0x2040
#define HG_STAGE_PARENT_HEAP 0x2044
#define HG_STAGE_HEAP 0x204C
#define HG_STAGE_ARCHIVE_HEAPS 0x20D0
#define HG_FREE_SLOT 0x74

static u32 hg_free(u32 heap) {
    u32 vtable = heap ? *(const volatile u32*)(heap + 0xC) : 0;
    u32 fn = vtable ? *(const volatile u32*)(vtable + HG_FREE_SLOT) : 0;
    return fn ? ((u32(*)(u32))fn)(heap) : 0u;
}

static void hg_log_heap(const char* what, u32 heap) {
    wwhd_log(what);
    if (!heap) {
        wwhd_log("multiplayer: heaps:     (none)");
        return;
    }
    wwhd_log_int("multiplayer: heaps:     free (KiB)", (int)(hg_free(heap) >> 10));
    wwhd_log_int("multiplayer: heaps:     largest block (KiB)", (int)(hg_max_alloc(heap) >> 10));
}

/* 0.3.49: step 2, memory under 0x50000000 the game does not use. Each scene load logs, for every
 * heap under 0x50000000, its free size and the least it had since the start (a session through the
 * sea, islands, a dungeon and a boss gives the room that can be taken safely). The root heaps come
 * from their getters (decomp hd_sys_0203E584.cpp, no argument: 0203E8AC heap 0 ... 0203E918 heap 9,
 * 0203E924 heap 12; heap 6 is the main heap, its free part is outside its sub-heaps). */
WWHD_GAME_FUNC(0x0203E8AC, u32, hg_root_heap_0, (void));
WWHD_GAME_FUNC(0x0203E8B8, u32, hg_root_heap_1, (void));
WWHD_GAME_FUNC(0x0203E8C4, u32, hg_root_heap_2, (void));
WWHD_GAME_FUNC(0x0203E8D0, u32, hg_root_heap_3, (void));
WWHD_GAME_FUNC(0x0203E8DC, u32, hg_root_heap_4, (void));
WWHD_GAME_FUNC(0x0203E8E8, u32, hg_root_heap_5, (void));
WWHD_GAME_FUNC(0x0203E8F4, u32, hg_root_heap_6, (void));
WWHD_GAME_FUNC(0x0203E900, u32, hg_root_heap_7, (void));
WWHD_GAME_FUNC(0x0203E90C, u32, hg_root_heap_8, (void));
WWHD_GAME_FUNC(0x0203E918, u32, hg_root_heap_9, (void));
WWHD_GAME_FUNC(0x0203E924, u32, hg_root_heap_12, (void));
#define HG_MEASURED 16
static u32 hg_least[HG_MEASURED];

static void hg_measure_one(int k, const char* what, u32 heap) {
    if (!heap) return;
    u32 free_bytes = hg_free(heap);
    if (!hg_least[k] || free_bytes < hg_least[k]) hg_least[k] = free_bytes ? free_bytes : 1u;
    char line[96];
    int n = 0;
    for (const char* t = "multiplayer: heaps: measure "; *t; ++t) line[n++] = *t;
    for (; *what && n != 60; ++what) line[n++] = *what;
    line[n] = 0;
    wwhd_log(line);
    wwhd_log_int("multiplayer: heaps:     free now (KiB)", (int)(free_bytes >> 10));
    wwhd_log_int("multiplayer: heaps:     least free since the start (KiB)", (int)(hg_least[k] >> 10));
    wwhd_log_hex("multiplayer: heaps:     at", heap);
}

static void hg_measure(void) {
    u32 control = WWHD_GAME_DATA(HG_CONTROL, u32);
    hg_measure_one(0, "root heap 0 (64 MiB)", hg_root_heap_0());
    hg_measure_one(1, "root heap 1 (17 MiB)", hg_root_heap_1());
    hg_measure_one(2, "root heap 2 (17 MiB)", hg_root_heap_2());
    hg_measure_one(3, "root heap 3 (20 MiB)", hg_root_heap_3());
    hg_measure_one(4, "root heap 4 (4 MiB)", hg_root_heap_4());
    hg_measure_one(5, "root heap 5 (128 MiB)", hg_root_heap_5());
    hg_measure_one(6, "root heap 6, main (499 MiB, outside its sub-heaps)", hg_root_heap_6());
    hg_measure_one(7, "root heap 7, display (1 MiB)", hg_root_heap_7());
    hg_measure_one(8, "root heap 8 (10 MiB)", hg_root_heap_8());
    hg_measure_one(9, "root heap 9 (48 MiB)", hg_root_heap_9());
    hg_measure_one(10, "root heap 12 (1 MiB)", hg_root_heap_12());
    hg_measure_one(11, "data heap (212 MiB)", hg_data);
    hg_measure_one(12, "sead FrameworkCafe (the rest)", hg_framework);
    if (control) {
        hg_measure_one(13, "object parent heap", *(const volatile u32*)(control + HG_OBJECT_PARENT_HEAP));
        hg_measure_one(14, "stage parent heap (command heap)", *(const volatile u32*)(control + HG_STAGE_PARENT_HEAP));
    }
    hg_measure_one(15, "skins' heap", hg_skin_heap);
}

static void hg_log_heaps(void) {
    hg_measure();
    u32 control = WWHD_GAME_DATA(HG_CONTROL, u32);
    if (!control) return;
    hg_log_heap("multiplayer: heaps: resource heap", *(const volatile u32*)(control + HG_RESOURCE_HEAP));
    hg_log_heap("multiplayer: heaps: object parent heap", *(const volatile u32*)(control + HG_OBJECT_PARENT_HEAP));
    hg_log_heap("multiplayer: heaps: stage parent heap (command heap)", *(const volatile u32*)(control + HG_STAGE_PARENT_HEAP));
    if (hg_skin_heap) hg_log_heap("multiplayer: heaps: skins' heap", hg_skin_heap);
}

WWHD_GAME_ORIGINAL(0x02603C84, u32, hg_stage_archive_heap_original, (u32 control, u32 name));
WWHD_REPLACE(0x02603C84, u32, hg_stage_archive_heap, (u32 control, u32 name)) {
    u32 stage = *(const volatile u32*)(control + HG_STAGE_HEAP);
    int missing = !stage;
    for (u32 i = 0; i != 22 && !missing; ++i) missing = !*(const volatile u32*)(control + HG_STAGE_ARCHIVE_HEAPS + i * 4u);
    if (!missing) return hg_stage_archive_heap_original(control, name);
    static u32 logged;
    if (logged++ < 4) {
        wwhd_log("multiplayer: heaps: the sea's stage archive heaps are missing (no room): the archive goes elsewhere");
        hg_log_heaps();
    }
    return stage ? stage : *(const volatile u32*)(control + HG_STAGE_PARENT_HEAP);
}

/* 0.3.43: what filled the command heap. Log 0.3.42 (instance 3, Omasao): stage parent heap (the
 * 14 MiB command heap) 10065 KiB free, 8078 after shaggy, 6091 after amongus, 1634 after garfield
 * (at sea), then the sea's stage archive heaps could not be made and the next crash: about 2 MiB
 * per skin kept loaded. ResourceControlHD::setArchive (02604EB4, decomp d_resorce.cpp: the
 * registration of an archive the load task read) mounts it, its bfres and its shader archives
 * (hd_res_model.cpp: one sharcfb per model, the shader objects allocated in the given heap), in a
 * solid heap cut from resourceCallSelectHeap = selectStageHeap (02604094), which gives
 * mpStageParentHeap (+0x2044, the command heap) to any archive but the sea's LODs. The skins' file
 * data went to their heap (selectObjectHeap above), their mounting did not.
 * Replaced (generated code: r3 the control, r4 the name, r5 and r6 passed on to 02603740 as they
 * are): the skins' archives are mounted in the skins' heap too. */
WWHD_GAME_ORIGINAL(0x02604094, u32, hg_select_stage_heap_original, (u32 control, u32 name, u32 path, u32 heap));
WWHD_REPLACE(0x02604094, u32, hg_select_stage_heap, (u32 control, u32 name, u32 path, u32 heap)) {
    if (hg_skin_heap && (hg_is_skin_name(name) || hg_is_skin_name(path))) {
        static u32 logged;
        if (logged++ < 4) wwhd_log("multiplayer: heaps: a skin archive is mounted in the skins' heap");
        return hg_skin_heap;
    }
    return hg_select_stage_heap_original(control, name, path, heap);
}
