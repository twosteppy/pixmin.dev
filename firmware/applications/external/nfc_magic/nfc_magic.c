#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_box.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <nfc/nfc.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <string.h>
#include <stdlib.h>

typedef enum {
    NfcSceneMain,
    NfcSceneReadCard,
    NfcSceneCloneCard,
    NfcSceneUIDSpoof,
    NfcSceneEMV,
    NfcSceneNTAG,
    NfcSceneOutput,
    NfcSceneCount,
} NfcMagicScene;

typedef enum {
    NfcViewMenu,
    NfcViewSubMenu,
    NfcViewTextBox,
    NfcViewCount,
} NfcMagicView;

typedef enum {
    NfcEvtCardFound,
    NfcEvtCardLost,
    NfcEvtOpDone,
    NfcEvtOpFail,
} NfcMagicEvent;

typedef struct {
    uint8_t uid[10];
    uint8_t uid_len;
    uint8_t atqa[2];
    uint8_t sak;
    bool valid;
} NfcCardInfo;

typedef struct {
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Submenu* sub_submenu;
    TextBox* text_box;
    NotificationApp* notifications;
    Nfc* nfc;
    NfcCardInfo card;
    FuriString* output;
    FuriMutex* output_mutex;
    FuriThread* worker;
    bool worker_running;
} NfcMagic;

static void append_out(NfcMagic* app, const char* text) {
    furi_mutex_acquire(app->output_mutex, FuriWaitForever);
    furi_string_cat(app->output, text);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->output));
    furi_mutex_release(app->output_mutex);
}

static void uid_to_str(const uint8_t* uid, uint8_t len, char* out, size_t out_size) {
    size_t pos = 0;
    for(uint8_t i = 0; i < len && pos + 3 < out_size; i++) {
        pos += snprintf(out + pos, out_size - pos, "%02X", uid[i]);
        if(i < len - 1) out[pos++] = ':';
    }
    out[pos] = '\0';
}

static int32_t read_worker(void* ctx) {
    NfcMagic* app = ctx;
    char uid_str[32];

    append_out(app, "scanning...\n\nplace card on flipper\n");

    NfcDevice* dev = nfc_device_alloc();
    NfcPoller* poller = nfc_poller_alloc(app->nfc, NFC_PROTOCOL_ISO14443_3A);

    while(app->worker_running) {
        NfcCommand cmd = nfc_poller_detect(poller, dev);
        if(cmd == NfcCommandContinue) {
            app->card.uid_len = iso14443_3a_get_uid(nfc_device_get_data(dev, NFC_PROTOCOL_ISO14443_3A), app->card.uid, sizeof(app->card.uid));
            uid_to_str(app->card.uid, app->card.uid_len, uid_str, sizeof(uid_str));
            app->card.valid = true;

            furi_string_reset(app->output);
            char buf[256];
            snprintf(buf, sizeof(buf), "card found:\nUID: %s\nUID len: %u bytes\n\nSAK: %02X\n",
                uid_str, app->card.uid_len, app->card.sak);
            append_out(app, buf);

            notification_message(app->notifications, &sequence_success);
            break;
        }
        furi_delay_ms(50);
    }

    nfc_poller_free(poller);
    nfc_device_free(dev);
    return 0;
}

static void uid_spoof_worker(void* ctx);

static int32_t uid_spoof_thread(void* ctx) {
    NfcMagic* app = ctx;

    if(!app->card.valid) {
        append_out(app, "read a card first\n");
        return 1;
    }

    char uid_str[32];
    uid_to_str(app->card.uid, app->card.uid_len, uid_str, sizeof(uid_str));
    char buf[128];
    snprintf(buf, sizeof(buf), "emulating UID:\n%s\n\npress BACK to stop\n", uid_str);
    append_out(app, buf);

    NfcListener* listener = nfc_listener_alloc(app->nfc, NFC_PROTOCOL_ISO14443_3A, NULL);

    Iso14443_3aData uid_data;
    memset(&uid_data, 0, sizeof(uid_data));
    memcpy(uid_data.uid, app->card.uid, app->card.uid_len);
    uid_data.uid_len = app->card.uid_len;

    nfc_listener_set_data(listener, NFC_PROTOCOL_ISO14443_3A, &uid_data);
    nfc_listener_start(listener, NULL, NULL);

    while(app->worker_running) {
        furi_delay_ms(100);
    }

    nfc_listener_stop(listener);
    nfc_listener_free(listener);
    append_out(app, "\nemulation stopped\n");
    return 0;
}

static void main_menu_cb(void* ctx, uint32_t idx) {
    NfcMagic* app = ctx;
    furi_string_reset(app->output);

    app->worker_running = true;

    switch(idx) {
    case 0:
        app->worker = furi_thread_alloc_ex("NfcRead", 4096, read_worker, app);
        furi_thread_start(app->worker);
        scene_manager_next_scene(app->scene_manager, NfcSceneOutput);
        break;
    case 1:
        append_out(app, "clone: read source first,\nthen write to magic card.\n\nread source card now...\n");
        app->worker = furi_thread_alloc_ex("NfcRead", 4096, read_worker, app);
        furi_thread_start(app->worker);
        scene_manager_next_scene(app->scene_manager, NfcSceneOutput);
        break;
    case 2:
        app->worker = furi_thread_alloc_ex("NfcSpoof", 4096, uid_spoof_thread, app);
        furi_thread_start(app->worker);
        scene_manager_next_scene(app->scene_manager, NfcSceneOutput);
        break;
    case 3:
        append_out(app, "tap a contactless EMV card\n");
        app->worker = furi_thread_alloc_ex("NfcEMV", 4096, read_worker, app);
        furi_thread_start(app->worker);
        scene_manager_next_scene(app->scene_manager, NfcSceneOutput);
        break;
    default:
        break;
    }
}

static void nfc_scene_main_on_enter(void* ctx) {
    NfcMagic* app = ctx;
    submenu_reset(app->submenu);
    submenu_add_item(app->submenu, "Read Card",      0, main_menu_cb, app);
    submenu_add_item(app->submenu, "Clone to Magic", 1, main_menu_cb, app);
    submenu_add_item(app->submenu, "Emulate UID",    2, main_menu_cb, app);
    submenu_add_item(app->submenu, "Read EMV",       3, main_menu_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcViewMenu);
}

static bool nfc_scene_main_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

static void nfc_scene_main_on_exit(void* ctx) {
    NfcMagic* app = ctx;
    submenu_reset(app->submenu);
}

static void nfc_scene_output_on_enter(void* ctx) {
    NfcMagic* app = ctx;
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_focus(app->text_box, TextBoxFocusEnd);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcViewTextBox);
}

static bool nfc_scene_output_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

static void nfc_scene_output_on_exit(void* ctx) {
    NfcMagic* app = ctx;
    app->worker_running = false;
    if(app->worker) {
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }
    text_box_reset(app->text_box);
}

static const SceneManagerHandlers nfc_scene_handlers = {
    .on_enter_handlers = {
        nfc_scene_main_on_enter,
        NULL, NULL, NULL, NULL, NULL,
        nfc_scene_output_on_enter,
    },
    .on_event_handlers = {
        nfc_scene_main_on_event,
        NULL, NULL, NULL, NULL, NULL,
        nfc_scene_output_on_event,
    },
    .on_exit_handlers = {
        nfc_scene_main_on_exit,
        NULL, NULL, NULL, NULL, NULL,
        nfc_scene_output_on_exit,
    },
    .scene_num = NfcSceneCount,
};

static bool nfc_custom_cb(void* ctx, uint32_t event) {
    NfcMagic* app = ctx;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool nfc_back_cb(void* ctx) {
    NfcMagic* app = ctx;
    return scene_manager_handle_back_event(app->scene_manager);
}

int32_t nfc_magic_app(void* p) {
    UNUSED(p);
    NfcMagic* app = malloc(sizeof(NfcMagic));
    memset(app, 0, sizeof(NfcMagic));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->nfc = nfc_alloc();
    app->output = furi_string_alloc();
    app->output_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    app->scene_manager = scene_manager_alloc(&nfc_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, nfc_custom_cb);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, nfc_back_cb);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, NfcViewMenu, submenu_get_view(app->submenu));

    app->sub_submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, NfcViewSubMenu, submenu_get_view(app->sub_submenu));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(app->view_dispatcher, NfcViewTextBox, text_box_get_view(app->text_box));

    scene_manager_next_scene(app->scene_manager, NfcSceneMain);
    view_dispatcher_run(app->view_dispatcher);

    app->worker_running = false;
    if(app->worker) {
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
    }
    nfc_free(app->nfc);

    view_dispatcher_remove_view(app->view_dispatcher, NfcViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, NfcViewSubMenu);
    view_dispatcher_remove_view(app->view_dispatcher, NfcViewTextBox);
    submenu_free(app->submenu);
    submenu_free(app->sub_submenu);
    text_box_free(app->text_box);
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);
    furi_mutex_free(app->output_mutex);
    furi_string_free(app->output);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
