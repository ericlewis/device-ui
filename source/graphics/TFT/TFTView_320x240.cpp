#if HAS_TFT && defined(VIEW_320x240) || defined(VIEW_240x320)

#include "graphics/view/TFT/TFTView_320x240.h"
#include "Arduino.h"
#include "graphics/common/BatteryLevel.h"
#include "graphics/common/LoRaPresets.h"
#include "graphics/common/Ringtones.h"
#include "graphics/common/ViewController.h"
#include "graphics/driver/DisplayDriver.h"
#include "graphics/driver/DisplayDriverFactory.h"
#include "graphics/map/AsyncTileService.h"
#include "graphics/map/CURLService.h"
#include "graphics/map/MapPanel.h"
#include "graphics/map/TileProvider.h"
#include "graphics/map/URLService.h"
#include "graphics/view/TFT/Themes.h"
#include "images.h"
#include "input/InputDriver.h"
#include "lv_i18n.h"
#include "lvgl_private.h"
#include "styles.h"
#include "ui.h"
#if defined(T_LORA_PAGER)
#include "graphics/view/TFT/PagerLayout.h"
#include "graphics/view/TFT/PagerHomeList.h"
#include "graphics/view/TFT/PagerDialogStyle.h"
#include "graphics/view/TFT/PagerMapControls.h"
#endif
#include "util/About.h"
#include "util/FileLoader.h"
#include "util/ILog.h"
#include "util/ISpiLock.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <iomanip>
#include <list>
#include <locale>
#include <random>
#include <sstream>
#include <time.h>

#if defined(ARCH_PORTDUINO)
#include "PortduinoFS.h"
fs::FS &fileSystem = PortduinoFS;
#else
#include "LittleFS.h"
fs::FS &fileSystem = LittleFS;
#endif

#if defined(ARCH_PORTDUINO)
#include "util/LinuxHelper.h"
// #include "graphics/map/LinuxFileSystemService.h"
#include "graphics/map/SDCardService.h"
#elif defined(SENSECAP_INDICATOR)
#include "graphics/map/RemoteSDService.h"
#elif defined(HAS_SD_MMC) || defined(SDCARD_SHARE_SPI)
#include "graphics/map/SDCardService.h"
#else
#include "graphics/map/SdFatService.h"
#endif
#include "graphics/common/SdCard.h"
#include "graphics/map/PMTileService.h"

#ifndef MAX_NUM_NODES_VIEW
#define MAX_NUM_NODES_VIEW 250
#endif

#ifndef PACKET_LOGS_MAX
#define PACKET_LOGS_MAX 200
#endif

LV_IMAGE_DECLARE(img_circle_image);
LV_IMAGE_DECLARE(img_no_tile_image);
LV_IMAGE_DECLARE(node_location_pin24_image);

#define CR_REPLACEMENT 0x0C              // dummy to record several lines in a one line textarea
#define THIS TFTView_320x240::instance() // need to use this in all static methods

#define LV_COLOR_HEX(C) {.blue = (C >> 0) & 0xff, .green = (C >> 8) & 0xff, .red = (C >> 16) & 0xff}

#define VALID_TIME(T) (T > 1000000 && T < UINT32_MAX)

constexpr lv_color_t colorRed = LV_COLOR_HEX(0xff5555);
constexpr lv_color_t colorDarkRed = LV_COLOR_HEX(0xa70a0a);
constexpr lv_color_t colorOrange = LV_COLOR_HEX(0xff8c04);
constexpr lv_color_t colorYellow = LV_COLOR_HEX(0xdbd251);
constexpr lv_color_t colorBlueGreen = LV_COLOR_HEX(0x05f6cb);
constexpr lv_color_t colorBlue = LV_COLOR_HEX(0x436C70);
constexpr lv_color_t colorGray = LV_COLOR_HEX(0x757575);
constexpr lv_color_t colorLightGray = LV_COLOR_HEX(0xAAFBFF);
constexpr lv_color_t colorMidGray = LV_COLOR_HEX(0x808080);
constexpr lv_color_t colorDarkGray = LV_COLOR_HEX(0x303030);
constexpr lv_color_t colorMesh = LV_COLOR_HEX(0x67ea94);

// children index of nodepanel lv objects (see addNode)
enum NodePanelIdx {
    node_img_idx,
    node_btn_idx,
    node_lbl_idx,
    node_lbs_idx,
    node_bat_idx,
    node_lh_idx,
    node_sig_idx,
    node_pos1_idx,
    node_pos2_idx,
    node_tm1_idx,
    node_tm2_idx
};

enum ScrollDirection {
    scrollDownLeft = 1,
    scrollDown = 2,
    scrollDownRight = 3,
    scrollLeft = 4,
    scrollRight = 6,
    scrollUpLeft = 7,
    scrollUp = 8,
    scrollUpRight = 9,
};

extern const char *firmware_version;

TFTView_320x240 *TFTView_320x240::gui = nullptr;
lv_obj_t *TFTView_320x240::currentPanel = nullptr;
lv_obj_t *TFTView_320x240::spinnerButton = nullptr;
uint32_t TFTView_320x240::currentNode = 0;
time_t TFTView_320x240::startTime = 0;
uint32_t TFTView_320x240::pinKeys = 0;
bool TFTView_320x240::screenLocked = false;
bool TFTView_320x240::screenUnlockRequest = false;
TFTView_320x240::KbdSlide TFTView_320x240::kbdSlideState = TFTView_320x240::eKbdHidden;
int32_t TFTView_320x240::kbdPanelBaseY = INT32_MIN;

// file scope so a running slide can be targeted for deletion by exec callback
static void kbdSlideAnimCB(void *var, int32_t v)
{
    lv_obj_set_y((lv_obj_t *)var, v);
}

TFTView_320x240 *TFTView_320x240::instance(void)
{
    if (!gui) {
        gui = new TFTView_320x240(nullptr, DisplayDriverFactory::create(320, 240));
    }
    return gui;
}

TFTView_320x240 *TFTView_320x240::instance(const DisplayDriverConfig &cfg)
{
    if (!gui) {
        gui = new TFTView_320x240(&cfg, DisplayDriverFactory::create(cfg));
    }
    return gui;
}

TFTView_320x240::TFTView_320x240(const DisplayDriverConfig *cfg, DisplayDriver *driver)
    : MeshtasticView(cfg, driver, new ViewController), screensInitialised(false), nodesFiltered(0), nodesChanged(true),
      processingFilter(false), packetLogEnabled(false), detectorRunning(false), cardDetected(false), formatSD(false),
      packetCounter(0), actTime(0), uptime(0), lastHeard(0), hasPosition(false), myLatitude(0), myLongitude(0),
      topNodeLL(nullptr), scans(0), selectedHops(0), chooseNodeSignalScanner(false), chooseNodeTraceRoute(false), qr(nullptr),
      db{}
{
    filter.active = false;
    highlight.active = false;
    objects.main_screen = nullptr;
}

/**
 * @brief Initialize view and boot screen
 * Note: We'll wait until we got our persistent data and then initialize
 *       the remaining screens.
 */
void TFTView_320x240::init(IClientBase *client)
{
    ILOG_DEBUG("TFTView_320x240 init...");
    ILOG_DEBUG("TFTView_320x240 db size: %d", sizeof(TFTView_320x240));
    ILOG_DEBUG("### Images size in flash ###");
    uint32_t total_size = 0;
    for (int i = 0; i < sizeof(images) / sizeof(ext_img_desc_t); i++) {
        total_size += images[i].img_dsc->data_size;
        ILOG_DEBUG("    %s: %d", images[i].name, images[i].img_dsc->data_size);
    }
    ILOG_DEBUG("================================");
    ILOG_DEBUG("### Total size: %d bytes ###", total_size);

    MeshtasticView::init(client);

    ui_init_boot();
    // The logo remains a long-press programming-mode control, not a menu item.
    lv_obj_set_style_outline_opa(objects.boot_logo_button, LV_OPA_TRANSP, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_opa(objects.boot_logo_button, LV_OPA_TRANSP, LV_STATE_EDITED);
    FileLoader::init(&fileSystem);
    if (!FileLoader::loadBootImage(objects.boot_logo))
        lv_image_set_src(objects.boot_logo, &img_meshtastic_boot_logo_image);
    // if boot logo is too big remove the label and center the image
    lv_obj_update_layout(objects.boot_logo);
    if (lv_obj_get_height(objects.boot_logo) > lv_display_get_vertical_resolution(displaydriver->getDisplay()) / 2) {
        lv_obj_set_pos(objects.boot_logo, 0, 0);
        lv_obj_add_flag(objects.firmware_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(objects.firmware_label, firmware_version);
    }

    time(&lastrun60);
    time(&lastrun10);
    time(&lastrun5);
    time(&lastrun1);

    lv_obj_add_event_cb(objects.boot_logo_button, ui_event_LogoButton, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(objects.blank_screen_button, ui_event_BlankScreenButton, LV_EVENT_ALL, NULL);

    lv_timer_create(timer_event_programming_mode, 3000, NULL); // timer for programming mode button active
}

/**
 * @brief initialize UI with persistent data
 */
bool TFTView_320x240::setupUIConfig(const meshtastic_DeviceUIConfig &uiconfig)
{
    if (uiconfig.version == 1) {
        ILOG_INFO("setupUIConfig version %d", uiconfig.version);
        db.uiConfig = uiconfig;
        if (db.uiConfig.screen_timeout == 1) {
            db.uiConfig.screen_timeout = 30;
            controller->storeUIConfig(db.uiConfig);
        }
    } else {
        ILOG_WARN("invalid uiconfig version %d, reset UI settings to default", uiconfig.version);
        db.uiConfig.version = 1;
        db.uiConfig.screen_brightness = 153;
        db.uiConfig.screen_timeout = 30;
        controller->storeUIConfig(db.uiConfig);
    }

    lv_i18n_init(lv_i18n_language_pack);
    setLocale(db.uiConfig.language);

    if (state == MeshtasticView::eEnterProgrammingMode || state == MeshtasticView::eProgrammingMode ||
        state == MeshtasticView::eWaitingForReboot) {
        enterProgrammingMode();
        return false;
    }

    state = MeshtasticView::eSetupUIConfig;

    // now we have set language, continue creating all screens
    if (!screensInitialised)
        init_screens();

    // set language
    setLanguage(db.uiConfig.language);

    // TODO: set virtual keyboard according language
    //  setKeyboard(db.uiConfig.language);

    // set theme
    setTheme(db.uiConfig.theme);

    // grey out bell until we got the ringtone (0 = silent)
    Themes::recolorButton(objects.home_bell_button, false);
    Themes::recolorText(objects.home_bell_label, false);

    lv_obj_set_style_bg_img_recolor(objects.home_button, colorMesh, LV_PART_MAIN | LV_STATE_DEFAULT);

    // set brightness
    if (displaydriver->hasLight())
        THIS->setBrightness(db.uiConfig.screen_brightness);

    // set timeout
    THIS->setTimeout(db.uiConfig.screen_timeout);

    // set screen/settings lock
    char buf[40];
    lv_snprintf(buf, 40, _("Lock: %s/%s"), db.uiConfig.screen_lock ? _("on") : _("off"),
                db.uiConfig.settings_lock ? _("on") : _("off"));
    lv_label_set_text(objects.basic_settings_screen_lock_label, buf);

    // set node filter options
    meshtastic_NodeFilter &filter = db.uiConfig.node_filter;
    lv_obj_set_state(objects.nodes_filter_unknown_switch, LV_STATE_CHECKED, filter.unknown_switch);
    lv_obj_set_state(objects.nodes_filter_offline_switch, LV_STATE_CHECKED, filter.offline_switch);
    lv_obj_set_state(objects.nodes_filter_public_key_switch, LV_STATE_CHECKED, filter.public_key_switch);
    // lv_dropdown_set_selected(objects.nodes_filter_channel_dropdown, filter.channel);
    lv_dropdown_set_selected(objects.nodes_filter_hops_dropdown, filter.hops_away);
    // lv_obj_set_state(objects.nodes_filter_mqtt_switch, LV_STATE_CHECKED, filter.mqtt_switch);
    lv_obj_set_state(objects.nodes_filter_position_switch, LV_STATE_CHECKED, filter.position_switch);
    lv_textarea_set_text(objects.nodes_filter_name_area, filter.node_name);

    // set node highlight options
    meshtastic_NodeHighlight &highlight = db.uiConfig.node_highlight;
    lv_obj_set_state(objects.nodes_hl_active_chat_switch, LV_STATE_CHECKED, highlight.chat_switch);
    lv_obj_set_state(objects.nodes_hl_position_switch, LV_STATE_CHECKED, highlight.position_switch);
    lv_obj_set_state(objects.nodes_hl_telemetry_switch, LV_STATE_CHECKED, highlight.telemetry_switch);
    lv_obj_set_state(objects.nodes_hliaq_switch, LV_STATE_CHECKED, highlight.iaq_switch);
    lv_textarea_set_text(objects.nodes_hl_name_area, highlight.node_name);

    // initialize own node panel
    if (ownNode && objects.node_panel)
        nodes[ownNode] = objects.node_panel;

    // touch screen calibration data
    uint16_t *parameters = (uint16_t *)db.uiConfig.calibration_data.bytes;
    if (db.uiConfig.calibration_data.size == 16 && (parameters[0] || parameters[7])) {
#ifndef IGNORE_CALIBRATION_DATA
        bool done = displaydriver->calibrate(parameters);
        char buf[32];
        lv_snprintf(buf, sizeof(buf), _("Screen Calibration: %s"), done ? _("done") : _("default"));
        lv_label_set_text(objects.basic_settings_calibration_label, buf);
#endif
    }

    // update home panel bell text
    setBellText(db.uiConfig.alert_enabled, !db.silent);
    bool off = !db.uiConfig.alert_enabled && db.silent;
    Themes::recolorButton(objects.home_bell_button, !off);
    Themes::recolorText(objects.home_bell_label, !off);
    objects.home_bell_button->user_data = (void *)off;

    // check SD card
    updateSDCard();

    // function callback for the map panel node symbol
    drawObjectCB = [this](uint32_t id, uint16_t x, uint16_t y, uint8_t zoom) {
        auto img = nodeObjects[id];
        if (!x && !y && !zoom) {
            lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        lv_obj_move_foreground(img);
        lv_obj_clear_flag(img, LV_OBJ_FLAG_HIDDEN);
        if (zoom >= 10 || (zoom >= 7 && nodeObjects.size() < 10)) {
            lv_obj_clear_flag(img->spec_attr->children[0], LV_OBJ_FLAG_HIDDEN);
        } else {
            // hide text
            lv_obj_add_flag(img->spec_attr->children[0], LV_OBJ_FLAG_HIDDEN);
        }
        if (zoom >= 4) {
            // pin location image
            lv_img_set_src(img, &img_node_location_pin24_image);
            lv_img_set_zoom(img, 256);
            lv_obj_set_pos(img, x - 20, y - 24); // img has 40x35 size, needle at 24
            lv_image_set_inner_align(img, LV_IMAGE_ALIGN_TOP_MID);
            // lv_obj_set_style_align(img->spec_attr->children[0], LV_ALIGN_BOTTOM_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            // circle image
            lv_img_set_src(img, &img_circle_image);
            lv_img_set_zoom(img, (zoom - 1) * 50 + 80);
            lv_obj_set_pos(img, x - 20, y - 17); // img has 40x35 size, circle at center
            lv_image_set_inner_align(img, LV_IMAGE_ALIGN_CENTER);
            // lv_obj_set_style_align(img->spec_attr->children[0], LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    };

    lv_disp_trig_activity(NULL);
    return true;
}

/**
 * @brief display custom message on boot screen
 *        Note: currently, the firmware version field is used and set in main()/setup()
 */
void TFTView_320x240::updateBootMessage(const char *msg)
{
    if (msg)
        lv_label_set_text(objects.firmware_label, msg);
}

/**
 * @brief Initialize all screens and apply customizations
 *
 */
void TFTView_320x240::init_screens(void)
{
    ILOG_DEBUG("init screens...");
    state = MeshtasticView::eInitScreens;
    ui_init();
    apply_hotfix();
#if defined(T_LORA_PAGER)
    applyPagerHomeList();
    applyPagerChannelList();
    stylePagerListRow(objects.node_panel, objects.node_button);
    layoutPagerNodeNames(objects.user_name_short_label, objects.user_name_label);
#endif
    initMessageInputSettings();
