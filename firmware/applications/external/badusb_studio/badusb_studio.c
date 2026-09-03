#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb_hid.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/popup.h>
#include <storage/storage.h>
#include <storage/storage_sd_api.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <toolbox/stream/file_stream.h>
#include <string.h>
#include <stdlib.h>

#define BADUSB_PATH     "/ext/badusb/pixmin"
#define BADUSB_MAX_FILES 128
#define BADUSB_MAX_PATH  256
#define LINE_BUF_SIZE    512
#define DEFAULT_DELAY_MS 50

typedef enum {
    BSSceneMain,
    BSSceneFiles,
    BSScenePreview,
    BSSceneRun,
    BSSceneCount,
} BSScene;

typedef enum {
    BSViewMenu,
    BSViewFileList,
    BSViewTextBox,
    BSViewCount,
} BSView;

typedef enum {
    BSEvtCategorySelected,
    BSEvtFileSelected,
    BSEvtRunScript,
} BSCustomEvent;

typedef struct {
    char name[64];
    char path[BADUSB_MAX_PATH];
} ScriptEntry;

typedef enum {
    BSRunIdle,
    BSRunRunning,
    BSRunDone,
    BSRunError,
} BSRunState;

typedef struct {
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Submenu* file_submenu;
    TextBox* text_box;
    NotificationApp* notifications;
    Storage* storage;

    ScriptEntry files[BADUSB_MAX_FILES];
    uint8_t file_count;
    uint8_t selected_file;

    FuriThread* run_thread;
    BSRunState run_state;
    FuriMutex* run_mutex;
    FuriString* preview_buf;
} BadUsbStudio;

static const char* category_labels[] = {
    "Recon",
    "Exfil",
    "Persistence",
    "Misc",
};
static const uint8_t category_count = 4;
static const char* category_paths[] = {
    "/ext/badusb/pixmin/recon",
    "/ext/badusb/pixmin/exfil",
    "/ext/badusb/pixmin/persistence",
    "/ext/badusb/pixmin/misc",
};

static uint16_t hid_char_to_key(char c) {
    if(c >= 'a' && c <= 'z') return HID_KEYBOARD_A + (c - 'a');
    if(c >= 'A' && c <= 'Z') return HID_KEYBOARD_A + (c - 'A');
    if(c >= '1' && c <= '9') return HID_KEYBOARD_1 + (c - '1');
    if(c == '0') return HID_KEYBOARD_0;
    if(c == ' ') return HID_KEYBOARD_SPACEBAR;
    if(c == '\n') return HID_KEYBOARD_RETURN;
    if(c == '\t') return HID_KEYBOARD_TAB;
    if(c == '.') return HID_KEYBOARD_DOT;
    if(c == '-') return HID_KEYBOARD_MINUS;
    if(c == '/') return HID_KEYBOARD_SLASH;
    if(c == '\\') return HID_KEYBOARD_BACKSLASH;
    if(c == '=') return HID_KEYBOARD_EQUAL_SIGN;
    if(c == '[') return HID_KEYBOARD_OPEN_BRACKET;
    if(c == ']') return HID_KEYBOARD_CLOSE_BRACKET;
    return 0;
}

static bool is_upper(char c) {
    return (c >= 'A' && c <= 'Z');
}

static void type_string(const char* str, uint32_t delay_ms) {
    for(size_t i = 0; str[i]; i++) {
        char c = str[i];
        uint16_t key = hid_char_to_key(c);
        if(key == 0) continue;
        uint8_t mods = is_upper(c) ? HID_KEYBOARD_MODIFIER_LEFT_SHIFT : 0;
        furi_hal_hid_kb_press(mods, key);
        furi_delay_ms(10);
        furi_hal_hid_kb_release(mods, key);
        furi_delay_ms(delay_ms);
    }
}

static uint16_t parse_key_name(const char* name) {
    if(strcmp(name, "ENTER") == 0) return HID_KEYBOARD_RETURN;
    if(strcmp(name, "ESC") == 0) return HID_KEYBOARD_ESCAPE;
    if(strcmp(name, "TAB") == 0) return HID_KEYBOARD_TAB;
    if(strcmp(name, "SPACE") == 0) return HID_KEYBOARD_SPACEBAR;
    if(strcmp(name, "BACKSPACE") == 0) return HID_KEYBOARD_DELETE;
    if(strcmp(name, "DELETE") == 0) return HID_KEYBOARD_DELETE_FORWARD;
    if(strcmp(name, "F1") == 0) return HID_KEYBOARD_F1;
    if(strcmp(name, "F2") == 0) return HID_KEYBOARD_F2;
    if(strcmp(name, "F3") == 0) return HID_KEYBOARD_F3;
    if(strcmp(name, "F4") == 0) return HID_KEYBOARD_F4;
    if(strcmp(name, "F5") == 0) return HID_KEYBOARD_F5;
    if(strcmp(name, "F6") == 0) return HID_KEYBOARD_F6;
    if(strcmp(name, "F7") == 0) return HID_KEYBOARD_F7;
    if(strcmp(name, "F8") == 0) return HID_KEYBOARD_F8;
    if(strcmp(name, "F9") == 0) return HID_KEYBOARD_F9;
    if(strcmp(name, "F10") == 0) return HID_KEYBOARD_F10;
    if(strcmp(name, "F11") == 0) return HID_KEYBOARD_F11;
    if(strcmp(name, "F12") == 0) return HID_KEYBOARD_F12;
    if(strcmp(name, "UP") == 0) return HID_KEYBOARD_UP_ARROW;
    if(strcmp(name, "DOWN") == 0) return HID_KEYBOARD_DOWN_ARROW;
    if(strcmp(name, "LEFT") == 0) return HID_KEYBOARD_LEFT_ARROW;
    if(strcmp(name, "RIGHT") == 0) return HID_KEYBOARD_RIGHT_ARROW;
    if(strcmp(name, "HOME") == 0) return HID_KEYBOARD_HOME;
    if(strcmp(name, "END") == 0) return HID_KEYBOARD_END;
    if(strcmp(name, "PAGEUP") == 0) return HID_KEYBOARD_PAGE_UP;
    if(strcmp(name, "PAGEDOWN") == 0) return HID_KEYBOARD_PAGE_DOWN;
    if(strlen(name) == 1) return hid_char_to_key(name[0]);
    return 0;
}

static uint8_t parse_modifier(const char* tok) {
    if(strcmp(tok, "CTRL") == 0 || strcmp(tok, "CONTROL") == 0)
        return HID_KEYBOARD_MODIFIER_LEFT_CTRL;
    if(strcmp(tok, "SHIFT") == 0)
        return HID_KEYBOARD_MODIFIER_LEFT_SHIFT;
    if(strcmp(tok, "ALT") == 0)
        return HID_KEYBOARD_MODIFIER_LEFT_ALT;
    if(strcmp(tok, "GUI") == 0 || strcmp(tok, "WINDOWS") == 0 || strcmp(tok, "COMMAND") == 0)
        return HID_KEYBOARD_MODIFIER_LEFT_GUI;
    return 0;
}

static void exec_script_line(const char* line, uint32_t* delay_ms) {
    if(line[0] == '#' || line[0] == '\0') return;

    if(strncmp(line, "REM ", 4) == 0) return;

    if(strncmp(line, "DELAY ", 6) == 0) {
        uint32_t ms = (uint32_t)atoi(line + 6);
        furi_delay_ms(ms);
        return;
    }

    if(strncmp(line, "DEFAULTDELAY ", 13) == 0 || strncmp(line, "DEFAULT_DELAY ", 14) == 0) {
        const char* p = line + (line[7] == '_' ? 14 : 13);
        *delay_ms = (uint32_t)atoi(p);
        return;
    }

    if(strncmp(line, "STRING ", 7) == 0) {
        type_string(line + 7, *delay_ms);
        return;
    }

    if(strncmp(line, "STRINGLN ", 9) == 0) {
        type_string(line + 9, *delay_ms);
        furi_hal_hid_kb_press(0, HID_KEYBOARD_RETURN);
        furi_delay_ms(10);
        furi_hal_hid_kb_release(0, HID_KEYBOARD_RETURN);
        return;
    }

    char tokens[8][32];
    uint8_t tok_count = 0;
    char tmp[LINE_BUF_SIZE];
    strncpy(tmp, line, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    char* tok = strtok(tmp, " ");
    while(tok && tok_count < 8) {
        strncpy(tokens[tok_count++], tok, 31);
        tok = strtok(NULL, " ");
    }

    if(tok_count == 0) return;

    uint8_t mods = 0;
    uint16_t key = 0;

    for(uint8_t i = 0; i < tok_count - 1; i++) {
        mods |= parse_modifier(tokens[i]);
    }

    if(mods == 0 && tok_count == 1) {
        mods = parse_modifier(tokens[0]);
        if(mods) {
            furi_hal_hid_kb_press(mods, 0);
            furi_delay_ms(50);
            furi_hal_hid_kb_release(mods, 0);
            return;
        }
    }

    key = parse_key_name(tokens[tok_count - 1]);
    if(mods == 0 && key == 0) {
        mods = parse_modifier(tokens[0]);
    }

    if(key == 0 && mods == 0) return;

    furi_hal_hid_kb_press(mods, key);
    furi_delay_ms(50);
    furi_hal_hid_kb_release(mods, key);
    furi_delay_ms(*delay_ms);
}

static int32_t run_worker(void* ctx) {
    BadUsbStudio* app = ctx;

    furi_mutex_acquire(app->run_mutex, FuriWaitForever);
    app->run_state = BSRunRunning;
    furi_mutex_release(app->run_mutex);

    char path[BADUSB_MAX_PATH];
    strncpy(path, app->files[app->selected_file].path, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';

    Stream* stream = file_stream_alloc(app->storage);
    if(!file_stream_open(stream, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        stream_free(stream);
        furi_mutex_acquire(app->run_mutex, FuriWaitForever);
        app->run_state = BSRunError;
        furi_mutex_release(app->run_mutex);
        return 1;
    }

    FuriHalUsbInterface* usb_mode_prev = furi_hal_usb_get_config();
    furi_hal_usb_set_config(&usb_hid, NULL);
    furi_delay_ms(1500);

    uint32_t delay_ms = DEFAULT_DELAY_MS;
    FuriString* line_str = furi_string_alloc();

    while(stream_read_line(stream, line_str)) {
        furi_mutex_acquire(app->run_mutex, FuriWaitForever);
        BSRunState st = app->run_state;
        furi_mutex_release(app->run_mutex);
        if(st != BSRunRunning) break;

        const char* ln = furi_string_get_cstr(line_str);
        size_t len = furi_string_size(line_str);
        char clean_line[LINE_BUF_SIZE];
        size_t clen = (len < LINE_BUF_SIZE - 1) ? len : LINE_BUF_SIZE - 2;
        strncpy(clean_line, ln, clen);
        clean_line[clen] = '\0';
        if(clen > 0 && clean_line[clen - 1] == '\n') clean_line[clen - 1] = '\0';
        if(clen > 0 && clean_line[clen - 2] == '\r') clean_line[clen - 2] = '\0';

        exec_script_line(clean_line, &delay_ms);
    }

    furi_string_free(line_str);
    stream_free(stream);

    furi_hal_hid_kb_release_all();
    furi_delay_ms(100);
    furi_hal_usb_set_config(usb_mode_prev, NULL);

    furi_mutex_acquire(app->run_mutex, FuriWaitForever);
    if(app->run_state == BSRunRunning) app->run_state = BSRunDone;
    furi_mutex_release(app->run_mutex);
    return 0;
}

static void load_files_from_dir(BadUsbStudio* app, const char* path) {
    app->file_count = 0;
    File* dir = storage_file_alloc(app->storage);
    if(!storage_dir_open(dir, path)) {
        storage_file_free(dir);
        return;
    }
    FileInfo fi;
    char name[64];
    while(storage_dir_read(dir, &fi, name, sizeof(name))) {
        if(fi.flags & FSF_DIRECTORY) continue;
        if(strstr(name, ".txt") == NULL && strstr(name, ".ducky") == NULL) continue;
        if(app->file_count >= BADUSB_MAX_FILES) break;
        strncpy(app->files[app->file_count].name, name, 63);
        snprintf(app->files[app->file_count].path, BADUSB_MAX_PATH - 1, "%s/%s", path, name);
        app->file_count++;
    }
    storage_dir_close(dir);
    storage_file_free(dir);
}

static void file_selected_cb(void* ctx, uint32_t idx) {
    BadUsbStudio* app = ctx;
    app->selected_file = (uint8_t)idx;
    scene_manager_handle_custom_event(app->scene_manager, BSEvtFileSelected);
}

static void category_selected_cb(void* ctx, uint32_t idx) {
    BadUsbStudio* app = ctx;
    load_files_from_dir(app, category_paths[idx]);
    scene_manager_handle_custom_event(app->scene_manager, BSEvtCategorySelected);
}

static void menu_cb(void* ctx, uint32_t idx) {
    BadUsbStudio* app = ctx;
    if(idx < category_count) {
        load_files_from_dir(app, category_paths[idx]);
    } else {
        load_files_from_dir(app, BADUSB_PATH);
    }
    scene_manager_handle_custom_event(app->scene_manager, BSEvtCategorySelected);
}

static void bs_scene_main_on_enter(void* ctx) {
    BadUsbStudio* app = ctx;
    submenu_reset(app->submenu);
    for(uint8_t i = 0; i < category_count; i++) {
        submenu_add_item(app->submenu, category_labels[i], i, category_selected_cb, app);
    }
    submenu_add_item(app->submenu, "All Scripts", category_count, menu_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BSViewMenu);
}

static bool bs_scene_main_on_event(void* ctx, SceneManagerEvent event) {
    BadUsbStudio* app = ctx;
    if(event.type == SceneManagerEventTypeCustom && event.event == BSEvtCategorySelected) {
        scene_manager_next_scene(app->scene_manager, BSSceneFiles);
        return true;
    }
    return false;
}

static void bs_scene_main_on_exit(void* ctx) {
    BadUsbStudio* app = ctx;
    submenu_reset(app->submenu);
}

static void bs_scene_files_on_enter(void* ctx) {
    BadUsbStudio* app = ctx;
    submenu_reset(app->file_submenu);
    for(uint8_t i = 0; i < app->file_count; i++) {
        submenu_add_item(app->file_submenu, app->files[i].name, i, file_selected_cb, app);
    }
    if(app->file_count == 0) {
        submenu_add_item(app->file_submenu, "(no scripts found)", 0xFF, NULL, NULL);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, BSViewFileList);
}

static bool bs_scene_files_on_event(void* ctx, SceneManagerEvent event) {
    BadUsbStudio* app = ctx;
    if(event.type == SceneManagerEventTypeCustom && event.event == BSEvtFileSelected) {
        app->run_state = BSRunIdle;
        app->run_thread = furi_thread_alloc_ex("BadUSBRun", 3072, run_worker, app);
        furi_thread_start(app->run_thread);
        scene_manager_next_scene(app->scene_manager, BSSceneRun);
        return true;
    }
    return false;
}

static void bs_scene_files_on_exit(void* ctx) {
    BadUsbStudio* app = ctx;
    submenu_reset(app->file_submenu);
}

static void bs_scene_run_on_enter(void* ctx) {
    BadUsbStudio* app = ctx;
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    furi_string_reset(app->preview_buf);
    furi_string_printf(app->preview_buf, "Running:\n%s\n\nPress BACK to abort.",
        app->files[app->selected_file].name);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->preview_buf));
    view_dispatcher_switch_to_view(app->view_dispatcher, BSViewTextBox);
}

static bool bs_scene_run_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

static void bs_scene_run_on_exit(void* ctx) {
    BadUsbStudio* app = ctx;
    furi_mutex_acquire(app->run_mutex, FuriWaitForever);
    if(app->run_state == BSRunRunning) app->run_state = BSRunIdle;
    furi_mutex_release(app->run_mutex);
    if(app->run_thread) {
        furi_thread_join(app->run_thread);
        furi_thread_free(app->run_thread);
        app->run_thread = NULL;
    }
    text_box_reset(app->text_box);
}

static const SceneManagerHandlers bs_scene_handlers = {
    .on_enter_handlers = {
        bs_scene_main_on_enter,
        bs_scene_files_on_enter,
        NULL,
        bs_scene_run_on_enter,
    },
    .on_event_handlers = {
        bs_scene_main_on_event,
        bs_scene_files_on_event,
        NULL,
        bs_scene_run_on_event,
    },
    .on_exit_handlers = {
        bs_scene_main_on_exit,
        bs_scene_files_on_exit,
        NULL,
        bs_scene_run_on_exit,
    },
    .scene_num = BSSceneCount,
};

static bool bs_custom_cb(void* ctx, uint32_t event) {
    BadUsbStudio* app = ctx;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool bs_back_cb(void* ctx) {
    BadUsbStudio* app = ctx;
    return scene_manager_handle_back_event(app->scene_manager);
}

static BadUsbStudio* badusb_studio_alloc(void) {
    BadUsbStudio* app = malloc(sizeof(BadUsbStudio));
    memset(app, 0, sizeof(BadUsbStudio));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->run_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->preview_buf = furi_string_alloc();

    app->scene_manager = scene_manager_alloc(&bs_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, bs_custom_cb);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, bs_back_cb);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, BSViewMenu, submenu_get_view(app->submenu));

    app->file_submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, BSViewFileList, submenu_get_view(app->file_submenu));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(app->view_dispatcher, BSViewTextBox, text_box_get_view(app->text_box));

    return app;
}

static void badusb_studio_free(BadUsbStudio* app) {
    if(app->run_thread) {
        furi_mutex_acquire(app->run_mutex, FuriWaitForever);
        app->run_state = BSRunIdle;
        furi_mutex_release(app->run_mutex);
        furi_thread_join(app->run_thread);
        furi_thread_free(app->run_thread);
    }

    view_dispatcher_remove_view(app->view_dispatcher, BSViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, BSViewFileList);
    view_dispatcher_remove_view(app->view_dispatcher, BSViewTextBox);

    submenu_free(app->submenu);
    submenu_free(app->file_submenu);
    text_box_free(app->text_box);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_string_free(app->preview_buf);
    furi_mutex_free(app->run_mutex);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t badusb_studio_app(void* p) {
    UNUSED(p);
    BadUsbStudio* app = badusb_studio_alloc();
    scene_manager_next_scene(app->scene_manager, BSSceneMain);
    view_dispatcher_run(app->view_dispatcher);
    badusb_studio_free(app);
    return 0;
}
