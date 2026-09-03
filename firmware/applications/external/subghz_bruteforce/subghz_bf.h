#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_subghz.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/popup.h>
#include <gui/canvas.h>
#include <gui/view.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#define SUBGHZ_BF_MAX_KEY_BITS 64
#define SUBGHZ_BF_TX_DELAY_MS  15
#define SUBGHZ_BF_REPEAT_COUNT 3

typedef enum {
    ProtocolPrinceton,
    ProtocolCAME12,
    ProtocolCAME24,
    ProtocolCAMEAtomo,
    ProtocolNiceFlo12,
    ProtocolNiceFlo24,
    ProtocolAnsonic12,
    ProtocolHoltek12,
    ProtocolPT2260,
    ProtocolSMC5326,
    ProtocolUNILARM,
    ProtocolCount,
} BruteProtocol;

typedef enum {
    Freq315MHz,
    Freq433_92MHz,
    Freq868_35MHz,
    FreqCount,
} BruteFrequency;

typedef enum {
    BFStateIdle,
    BFStateRunning,
    BFStateDone,
    BFStateError,
} BFState;

typedef struct {
    uint32_t frequency;
    BruteProtocol protocol;
    uint8_t bit_count;
    uint64_t key_start;
    uint64_t key_end;
    uint64_t key_current;
    uint32_t delay_ms;
    uint8_t repeat;
    BFState state;
    uint32_t tx_count;
} BruteConfig;

typedef struct {
    uint8_t data[9];
    uint8_t size;
} BrutePacket;

typedef enum {
    BFViewMenu,
    BFViewConfig,
    BFViewRun,
    BFViewCount,
} BFView;

typedef enum {
    BFSceneMain,
    BFSceneConfig,
    BFSceneRun,
    BFSceneCount,
} BFScene;

typedef enum {
    BFEventStart,
    BFEventStop,
    BFEventConfigDone,
} BFCustomEvent;

typedef struct {
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    VariableItemList* var_list;
    View* run_view;
    NotificationApp* notifications;
    BruteConfig config;
    FuriThread* worker_thread;
    FuriMutex* state_mutex;
} SubGhzBrute;

extern const char* protocol_names[ProtocolCount];
extern const char* freq_names[FreqCount];
extern const uint32_t freq_values[FreqCount];
extern const uint8_t protocol_bits[ProtocolCount];

void subghz_brute_scene_main_on_enter(void* ctx);
bool subghz_brute_scene_main_on_event(void* ctx, SceneManagerEvent event);
void subghz_brute_scene_main_on_exit(void* ctx);

void subghz_brute_scene_config_on_enter(void* ctx);
bool subghz_brute_scene_config_on_event(void* ctx, SceneManagerEvent event);
void subghz_brute_scene_config_on_exit(void* ctx);

void subghz_brute_scene_run_on_enter(void* ctx);
bool subghz_brute_scene_run_on_event(void* ctx, SceneManagerEvent event);
void subghz_brute_scene_run_on_exit(void* ctx);

int32_t subghz_brute_app(void* p);
