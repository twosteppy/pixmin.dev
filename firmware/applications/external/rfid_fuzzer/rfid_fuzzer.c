#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/view.h>
#include <gui/canvas.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <lib/lfrfid/lfrfid_worker.h>
#include <lib/lfrfid/protocols/lfrfid_protocol_em4100.h>
#include <lib/lfrfid/protocols/lfrfid_protocol_hid_h10301.h>
#include <string.h>
#include <stdlib.h>

#define RFID_EMIT_DELAY_MS 150
#define RFID_MAX_UID_BITS   40

typedef enum {
    RFIDProtoEM4100,
    RFIDProtoHID26,
    RFIDProtoAWID26,
    RFIDProtoINDALA26,
    RFIDProtoCount,
} RFIDFuzzProto;

typedef enum {
    RFIDFuzzIdle,
    RFIDFuzzRunning,
    RFIDFuzzDone,
} RFIDFuzzState;

typedef enum {
    RFIDSceneMain,
    RFIDSceneRun,
    RFIDSceneCount,
} RFIDScene;

typedef enum {
    RFIDViewMenu,
    RFIDViewRun,
    RFIDViewCount,
} RFIDView;

typedef struct {
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    View* run_view;
    NotificationApp* notifications;
    LFRFIDWorker* worker;

    RFIDFuzzProto proto;
    uint64_t uid_current;
    uint64_t uid_start;
    uint64_t uid_end;
    uint32_t delay_ms;
    uint32_t emit_count;
    RFIDFuzzState state;
    FuriThread* worker_thread;
    FuriMutex* state_mutex;
} RFIDFuzzer;

static const char* proto_names[RFIDProtoCount] = {
    "EM4100",
    "HID Prox 26-bit",
    "AWID 26-bit",
    "INDALA 26-bit",
};

static void rfid_emit_em4100(RFIDFuzzer* app, uint64_t uid) {
    ProtocolId pid = LFRFIDProtocolEM4100;
    LFRFIDProtocolData data;
    memset(&data, 0, sizeof(data));

    data[4] = (uid >> 32) & 0xFF;
    data[3] = (uid >> 24) & 0xFF;
    data[2] = (uid >> 16) & 0xFF;
    data[1] = (uid >> 8)  & 0xFF;
    data[0] = uid & 0xFF;

    lfrfid_worker_emulate_start(app->worker, pid);
    lfrfid_worker_emulate_set_data(app->worker, pid, (uint8_t*)&data, sizeof(data));
    furi_delay_ms(app->delay_ms);
    lfrfid_worker_stop(app->worker);
}

static void rfid_emit_hid26(RFIDFuzzer* app, uint64_t uid) {
    ProtocolId pid = LFRFIDProtocolHIDH10301;
    LFRFIDProtocolData data;
    memset(&data, 0, sizeof(data));

    uint32_t code = (uint32_t)(uid & 0x1FFFFFF);
    data[2] = (code >> 16) & 0xFF;
    data[1] = (code >> 8)  & 0xFF;
    data[0] = code & 0xFF;

    lfrfid_worker_emulate_start(app->worker, pid);
    lfrfid_worker_emulate_set_data(app->worker, pid, (uint8_t*)&data, sizeof(data));
    furi_delay_ms(app->delay_ms);
    lfrfid_worker_stop(app->worker);
}

static int32_t fuzz_worker(void* ctx) {
    RFIDFuzzer* app = ctx;

    furi_mutex_acquire(app->state_mutex, FuriWaitForever);
    app->state = RFIDFuzzRunning;
    app->uid_current = app->uid_start;
    furi_mutex_release(app->state_mutex);

    while(true) {
        furi_mutex_acquire(app->state_mutex, FuriWaitForever);
        if(app->state != RFIDFuzzRunning) {
            furi_mutex_release(app->state_mutex);
            break;
        }
        if(app->uid_current > app->uid_end) {
            app->state = RFIDFuzzDone;
            furi_mutex_release(app->state_mutex);
            break;
        }
        uint64_t uid = app->uid_current++;
        app->emit_count++;
        RFIDFuzzProto proto = app->proto;
        furi_mutex_release(app->state_mutex);

        switch(proto) {
        case RFIDProtoEM4100:   rfid_emit_em4100(app, uid); break;
        case RFIDProtoHID26:    rfid_emit_hid26(app, uid);  break;
        case RFIDProtoAWID26:   rfid_emit_em4100(app, uid); break;
        case RFIDProtoINDALA26: rfid_emit_em4100(app, uid); break;
        default: break;
        }
    }

    lfrfid_worker_stop(app->worker);
    return 0;
}

static void run_draw_cb(Canvas* canvas, void* model) {
    RFIDFuzzer* app = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "RFID Fuzzer");

    furi_mutex_acquire(app->state_mutex, FuriWaitForever);
    uint64_t uid = app->uid_current;
    uint32_t count = app->emit_count;
    uint64_t total = app->uid_end - app->uid_start + 1;
    RFIDFuzzState state = app->state;
    furi_mutex_release(app->state_mutex);

    canvas_set_font(canvas, FontSecondary);
    char buf[32];
    snprintf(buf, sizeof(buf), "UID: %05llX", uid);
    canvas_draw_str(canvas, 2, 26, buf);
    snprintf(buf, sizeof(buf), "Emitted: %lu", count);
    canvas_draw_str(canvas, 2, 38, buf);

    uint64_t done = uid - app->uid_start;
    uint8_t pct = (total > 0) ? (uint8_t)((done * 100) / total) : 0;
    canvas_draw_box(canvas, 2, 44, pct, 5);
    canvas_draw_frame(canvas, 2, 44, 100, 5);

    const char* s =
        (state == RFIDFuzzRunning) ? "Fuzzing..." :
        (state == RFIDFuzzDone)    ? "Done!" : "Idle";
    canvas_draw_str(canvas, 2, 58, s);
}

static bool run_input_cb(InputEvent* event, void* ctx) {
    RFIDFuzzer* app = ctx;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        furi_mutex_acquire(app->state_mutex, FuriWaitForever);
        if(app->state == RFIDFuzzRunning) app->state = RFIDFuzzIdle;
        furi_mutex_release(app->state_mutex);
        return true;
    }
    return false;
}

static void main_menu_cb(void* ctx, uint32_t idx) {
    RFIDFuzzer* app = ctx;
    app->proto = (RFIDFuzzProto)(idx % RFIDProtoCount);
    app->uid_start = 0;
    app->uid_end = 0xFFFF;
    app->uid_current = 0;
    app->emit_count = 0;
    app->delay_ms = RFID_EMIT_DELAY_MS;
    app->state = RFIDFuzzIdle;
    app->worker_thread = furi_thread_alloc_ex("RFIDFuzz", 2048, fuzz_worker, app);
    furi_thread_start(app->worker_thread);
    scene_manager_next_scene(app->scene_manager, RFIDSceneRun);
}

static void rfid_scene_main_on_enter(void* ctx) {
    RFIDFuzzer* app = ctx;
    submenu_reset(app->submenu);
    for(uint8_t i = 0; i < RFIDProtoCount; i++) {
        submenu_add_item(app->submenu, proto_names[i], i, main_menu_cb, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, RFIDViewMenu);
}

static bool rfid_scene_main_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

static void rfid_scene_main_on_exit(void* ctx) {
    RFIDFuzzer* app = ctx;
    submenu_reset(app->submenu);
}

static void rfid_scene_run_on_enter(void* ctx) {
    RFIDFuzzer* app = ctx;
    view_set_draw_callback(app->run_view, run_draw_cb);
    view_set_input_callback(app->run_view, run_input_cb);
    view_set_context(app->run_view, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, RFIDViewRun);
}

static bool rfid_scene_run_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

static void rfid_scene_run_on_exit(void* ctx) {
    RFIDFuzzer* app = ctx;
    furi_mutex_acquire(app->state_mutex, FuriWaitForever);
    app->state = RFIDFuzzIdle;
    furi_mutex_release(app->state_mutex);
    if(app->worker_thread) {
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
        app->worker_thread = NULL;
    }
    lfrfid_worker_stop(app->worker);
}

static const SceneManagerHandlers rfid_scene_handlers = {
    .on_enter_handlers = { rfid_scene_main_on_enter, rfid_scene_run_on_enter },
    .on_event_handlers = { rfid_scene_main_on_event, rfid_scene_run_on_event },
    .on_exit_handlers  = { rfid_scene_main_on_exit,  rfid_scene_run_on_exit  },
    .scene_num = RFIDSceneCount,
};

static bool rfid_custom_cb(void* ctx, uint32_t event) {
    RFIDFuzzer* app = ctx;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool rfid_back_cb(void* ctx) {
    RFIDFuzzer* app = ctx;
    return scene_manager_handle_back_event(app->scene_manager);
}

int32_t rfid_fuzzer_app(void* p) {
    UNUSED(p);
    RFIDFuzzer* app = malloc(sizeof(RFIDFuzzer));
    memset(app, 0, sizeof(RFIDFuzzer));

    app->state_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->worker = lfrfid_worker_alloc();

    app->scene_manager = scene_manager_alloc(&rfid_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, rfid_custom_cb);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, rfid_back_cb);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, RFIDViewMenu, submenu_get_view(app->submenu));

    app->run_view = view_alloc();
    view_dispatcher_add_view(app->view_dispatcher, RFIDViewRun, app->run_view);

    scene_manager_next_scene(app->scene_manager, RFIDSceneMain);
    view_dispatcher_run(app->view_dispatcher);

    if(app->worker_thread) {
        furi_mutex_acquire(app->state_mutex, FuriWaitForever);
        app->state = RFIDFuzzIdle;
        furi_mutex_release(app->state_mutex);
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
    }
    lfrfid_worker_stop(app->worker);
    lfrfid_worker_free(app->worker);

    view_dispatcher_remove_view(app->view_dispatcher, RFIDViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, RFIDViewRun);
    submenu_free(app->submenu);
    view_free(app->run_view);
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);
    furi_mutex_free(app->state_mutex);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
