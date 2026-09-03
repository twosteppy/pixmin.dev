#include "pixmin_hub.h"

typedef enum {
    PixminMenuSubghz,
    PixminMenuWifi,
    PixminMenuNfc,
    PixminMenuRfid,
    PixminMenuBadUsb,
    PixminMenuIR,
    PixminMenuGpio,
    PixminMenuAbout,
} PixminMenuIndex;

static const SceneManagerHandlers pixmin_hub_scene_handlers = {
    .on_enter_handlers =
        {
            pixmin_hub_scene_main_on_enter,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            pixmin_hub_scene_about_on_enter,
        },
    .on_event_handlers =
        {
            pixmin_hub_scene_main_on_event,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            pixmin_hub_scene_about_on_event,
        },
    .on_exit_handlers =
        {
            pixmin_hub_scene_main_on_exit,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            pixmin_hub_scene_about_on_exit,
        },
    .scene_num = PixminHubSceneCount,
};

static void submenu_callback(void* ctx, uint32_t index) {
    PixminHub* app = ctx;
    scene_manager_handle_custom_event(app->scene_manager, index);
}

void pixmin_hub_scene_main_on_enter(void* ctx) {
    PixminHub* app = ctx;
    submenu_reset(app->submenu);

    submenu_add_item(app->submenu, "SubGHz Tools",     PixminMenuSubghz, submenu_callback, app);
    submenu_add_item(app->submenu, "WiFi / BLE",       PixminMenuWifi,   submenu_callback, app);
    submenu_add_item(app->submenu, "NFC Tools",        PixminMenuNfc,    submenu_callback, app);
    submenu_add_item(app->submenu, "RFID Tools",       PixminMenuRfid,   submenu_callback, app);
    submenu_add_item(app->submenu, "BadUSB Studio",    PixminMenuBadUsb, submenu_callback, app);
    submenu_add_item(app->submenu, "Infrared",         PixminMenuIR,     submenu_callback, app);
    submenu_add_item(app->submenu, "GPIO",             PixminMenuGpio,   submenu_callback, app);
    submenu_add_item(app->submenu, "About",            PixminMenuAbout,  submenu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, PixminHubViewSubmenu);
}

bool pixmin_hub_scene_main_on_event(void* ctx, SceneManagerEvent event) {
    PixminHub* app = ctx;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case PixminMenuSubghz:
            notification_message(app->notifications, &sequence_single_vibro);
            break;
        case PixminMenuWifi:
            notification_message(app->notifications, &sequence_single_vibro);
            break;
        case PixminMenuNfc:
            notification_message(app->notifications, &sequence_single_vibro);
            break;
        case PixminMenuRfid:
            notification_message(app->notifications, &sequence_single_vibro);
            break;
        case PixminMenuBadUsb:
            notification_message(app->notifications, &sequence_single_vibro);
            break;
        case PixminMenuIR:
            notification_message(app->notifications, &sequence_single_vibro);
            break;
        case PixminMenuGpio:
            notification_message(app->notifications, &sequence_single_vibro);
            break;
        case PixminMenuAbout:
            scene_manager_next_scene(app->scene_manager, PixminHubSceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void pixmin_hub_scene_main_on_exit(void* ctx) {
    PixminHub* app = ctx;
    submenu_reset(app->submenu);
}

void pixmin_hub_scene_about_on_enter(void* ctx) {
    PixminHub* app = ctx;
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box,
        "pixmin firmware\n"
        "v1.0.0\n"
        "\n"
        "pixmin.dev\n"
        "\n"
        "full subghz range\n"
        "27 protocols\n"
        "200+ captures\n"
        "80+ badusb payloads\n"
        "400+ ir codes\n"
        "6 wifi attack modes\n"
        "\n"
        "mit license\n"
        "no restrictions");
    view_dispatcher_switch_to_view(app->view_dispatcher, PixminHubViewTextBox);
}

bool pixmin_hub_scene_about_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

void pixmin_hub_scene_about_on_exit(void* ctx) {
    PixminHub* app = ctx;
    text_box_reset(app->text_box);
}

static bool pixmin_hub_custom_event_callback(void* ctx, uint32_t event) {
    PixminHub* app = ctx;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool pixmin_hub_back_event_callback(void* ctx) {
    PixminHub* app = ctx;
    return scene_manager_handle_back_event(app->scene_manager);
}

static PixminHub* pixmin_hub_alloc(void) {
    PixminHub* app = malloc(sizeof(PixminHub));
    memset(app, 0, sizeof(PixminHub));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->temp_str = furi_string_alloc();

    app->scene_manager = scene_manager_alloc(&pixmin_hub_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, pixmin_hub_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, pixmin_hub_back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, PixminHubViewSubmenu, submenu_get_view(app->submenu));

    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, PixminHubViewPopup, popup_get_view(app->popup));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(app->view_dispatcher, PixminHubViewTextBox, text_box_get_view(app->text_box));

    return app;
}

static void pixmin_hub_free(PixminHub* app) {
    view_dispatcher_remove_view(app->view_dispatcher, PixminHubViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, PixminHubViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, PixminHubViewTextBox);

    submenu_free(app->submenu);
    popup_free(app->popup);
    text_box_free(app->text_box);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_string_free(app->temp_str);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t pixmin_hub_app(void* p) {
    UNUSED(p);
    PixminHub* app = pixmin_hub_alloc();
    scene_manager_next_scene(app->scene_manager, PixminHubSceneMain);
    view_dispatcher_run(app->view_dispatcher);
    pixmin_hub_free(app);
    return 0;
}
