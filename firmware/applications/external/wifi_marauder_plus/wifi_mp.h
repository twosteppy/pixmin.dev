#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_uart.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/popup.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include "marauder_cmds.h"

#define WIFI_MAX_RESULTS    64
#define WIFI_SSID_LEN       33
#define WIFI_BSSID_LEN      18
#define WIFI_LINE_BUF       512

typedef struct {
    char ssid[WIFI_SSID_LEN];
    char bssid[WIFI_BSSID_LEN];
    int8_t rssi;
    uint8_t channel;
    uint8_t enc;
    bool wps;
} WifiAP;

typedef struct {
    char mac[WIFI_BSSID_LEN];
    char probe[WIFI_SSID_LEN];
    int8_t rssi;
} WifiSTA;

typedef enum {
    WifiCatDeauth,
    WifiCatHandshake,
    WifiCatBeacon,
    WifiCatEvilTwin,
    WifiCatBLE,
    WifiCatRecon,
    WifiCatCount,
} WifiCategory;

typedef enum {
    WifiSceneMain,
    WifiSceneCategory,
    WifiSceneTargetSelect,
    WifiSceneOutput,
    WifiSceneCount,
} WifiScene;

typedef enum {
    WifiViewMenu,
    WifiViewSubMenu,
    WifiViewOutput,
    WifiViewCount,
} WifiView;

typedef enum {
    WifiEvtSend,
    WifiEvtStopAttack,
    WifiEvtScanDone,
    WifiEvtBack,
} WifiCustomEvent;

typedef struct {
    bool connected;
    char version[32];
    WifiAP ap_list[WIFI_MAX_RESULTS];
    uint8_t ap_count;
    WifiSTA sta_list[WIFI_MAX_RESULTS];
    uint8_t sta_count;
    uint8_t selected_ap;
    WifiCategory active_category;
    bool attack_running;
} WifiState;

typedef struct {
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Submenu* sub_submenu;
    TextBox* text_box;
    NotificationApp* notifications;

    WifiState wifi;

    FuriStreamBuffer* rx_stream;
    FuriThread* rx_thread;

    FuriString* output_buf;
    FuriMutex* output_mutex;
} WifiMarauder;

void wifi_mp_uart_send(WifiMarauder* app, const char* cmd);
void wifi_mp_uart_send_fmt(WifiMarauder* app, const char* fmt, ...);

void wifi_scene_main_on_enter(void* ctx);
bool wifi_scene_main_on_event(void* ctx, SceneManagerEvent event);
void wifi_scene_main_on_exit(void* ctx);

void wifi_scene_category_on_enter(void* ctx);
bool wifi_scene_category_on_event(void* ctx, SceneManagerEvent event);
void wifi_scene_category_on_exit(void* ctx);

void wifi_scene_target_on_enter(void* ctx);
bool wifi_scene_target_on_event(void* ctx, SceneManagerEvent event);
void wifi_scene_target_on_exit(void* ctx);

void wifi_scene_output_on_enter(void* ctx);
bool wifi_scene_output_on_event(void* ctx, SceneManagerEvent event);
void wifi_scene_output_on_exit(void* ctx);

int32_t wifi_marauder_app(void* p);
