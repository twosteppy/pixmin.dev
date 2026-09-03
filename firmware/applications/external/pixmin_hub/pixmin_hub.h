#pragma once

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
#include <storage/storage.h>

typedef enum {
    PixminHubSceneMain,
    PixminHubSceneSubghz,
    PixminHubSceneWifi,
    PixminHubSceneNfc,
    PixminHubSceneRfid,
    PixminHubSceneBadUsb,
    PixminHubSceneIR,
    PixminHubSceneGpio,
    PixminHubSceneAbout,
    PixminHubSceneCount,
} PixminHubScene;

typedef enum {
    PixminHubViewSubmenu,
    PixminHubViewPopup,
    PixminHubViewTextBox,
    PixminHubViewCount,
} PixminHubView;

typedef struct {
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Popup* popup;
    TextBox* text_box;
    NotificationApp* notifications;
    Storage* storage;
    FuriString* temp_str;
} PixminHub;

void pixmin_hub_scene_main_on_enter(void* ctx);
bool pixmin_hub_scene_main_on_event(void* ctx, SceneManagerEvent event);
void pixmin_hub_scene_main_on_exit(void* ctx);

void pixmin_hub_scene_about_on_enter(void* ctx);
bool pixmin_hub_scene_about_on_event(void* ctx, SceneManagerEvent event);
void pixmin_hub_scene_about_on_exit(void* ctx);

int32_t pixmin_hub_app(void* p);
