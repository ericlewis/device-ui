#pragma once

#include "graphics/common/MeshtasticView.h"
#include "meshtastic/clientonly.pb.h"
#include <set>

class MapPanel;

/**
 * @brief GUI view for e.g. T-Deck
 * Handles creation of display driver and controller.
 * Note: due to static callbacks in lvgl this class is modelled as
 *       a singleton with static callback members
 */
class TFTView_320x240 : public MeshtasticView
{
  public:
    void init(IClientBase *client) override;
    bool setupUIConfig(const meshtastic_DeviceUIConfig &uiconfig) override;
    void task_handler(void) override;

    // methods to update view
    void setMyInfo(uint32_t nodeNum) override;
    void setDeviceMetaData(int hw_model, const char *version, bool has_bluetooth, bool has_wifi, bool has_eth,
                           bool can_shutdown) override;
    void addOrUpdateNode(uint32_t nodeNum, uint8_t channel, uint32_t lastHeard, const meshtastic_User &cfg) override;
    void addNode(uint32_t nodeNum, uint8_t channel, const char *userShort, const char *userLong, uint32_t lastHeard, eRole role,
                 bool hasKey, bool unmessagable) override;
    void updateNode(uint32_t nodeNum, uint8_t channel, const meshtastic_User &cfg) override;
    void updatePosition(uint32_t nodeNum, int32_t lat, int32_t lon, int32_t alt, uint32_t sats, uint32_t precision) override;
#if defined(T_LORA_PAGER)
    void updateLocalGPSStatus(const LocalGPSStatus &status) override;
#endif
    void updateMetrics(uint32_t nodeNum, uint32_t bat_level, float voltage, float chUtil, float airUtil) override;
    void updateEnvironmentMetrics(uint32_t nodeNum, const meshtastic_EnvironmentMetrics &metrics) override;
    void updateAirQualityMetrics(uint32_t nodeNum, const meshtastic_AirQualityMetrics &metrics) override;
    void updatePowerMetrics(uint32_t nodeNum, const meshtastic_PowerMetrics &metrics) override;
    void updateSignalStrength(uint32_t nodeNum, int32_t rssi, float snr) override;
    void updateHopsAway(uint32_t nodeNum, uint8_t hopsAway) override;
    void updateConnectionStatus(const meshtastic_DeviceConnectionStatus &status) override;

    // methods to update device config
    void updateChannelConfig(const meshtastic_Channel &ch) override;
    void updateDeviceConfig(const meshtastic_Config_DeviceConfig &cfg) override;
    void updatePositionConfig(const meshtastic_Config_PositionConfig &cfg) override;
    void updatePowerConfig(const meshtastic_Config_PowerConfig &cfg) override;
    void updateNetworkConfig(const meshtastic_Config_NetworkConfig &cfg) override;
    void updateDisplayConfig(const meshtastic_Config_DisplayConfig &cfg) override;
    void updateLoRaConfig(const meshtastic_Config_LoRaConfig &cfg) override;
    void updateBluetoothConfig(const meshtastic_Config_BluetoothConfig &cfg, uint32_t id = 0) override;
    void updateSecurityConfig(const meshtastic_Config_SecurityConfig &cfg) override;
    void updateSessionKeyConfig(const meshtastic_Config_SessionkeyConfig &cfg) override;

    // methods to update module config
    void updateMQTTModule(const meshtastic_ModuleConfig_MQTTConfig &cfg) override;
    void updateSerialModule(const meshtastic_ModuleConfig_SerialConfig &cfg) override {}
    void updateExtNotificationModule(const meshtastic_ModuleConfig_ExternalNotificationConfig &cfg) override;
    void updateStoreForwardModule(const meshtastic_ModuleConfig_StoreForwardConfig &cfg) override {}
    void updateRangeTestModule(const meshtastic_ModuleConfig_RangeTestConfig &cfg) override {}
    void updateTelemetryModule(const meshtastic_ModuleConfig_TelemetryConfig &cfg) override {}
    void updateCannedMessageModule(const meshtastic_ModuleConfig_CannedMessageConfig &) override {}
    void updateAudioModule(const meshtastic_ModuleConfig_AudioConfig &cfg) override {}
    void updateRemoteHardwareModule(const meshtastic_ModuleConfig_RemoteHardwareConfig &cfg) override {}
    void updateNeighborInfoModule(const meshtastic_ModuleConfig_NeighborInfoConfig &cfg) override {}
    void updateAmbientLightingModule(const meshtastic_ModuleConfig_AmbientLightingConfig &cfg) override {}
    void updateDetectionSensorModule(const meshtastic_ModuleConfig_DetectionSensorConfig &cfg) override {}
    void updatePaxCounterModule(const meshtastic_ModuleConfig_PaxcounterConfig &cfg) override {}
    void updateFileinfo(const meshtastic_FileInfo &fileinfo) override {}
    void updateRingtone(const char rtttl[231]) override;

    // update internal time
    void updateTime(uint32_t time) override;

    void packetReceived(const meshtastic_MeshPacket &p) override;
    void handleResponse(uint32_t from, uint32_t id, const meshtastic_Routing &routing, const meshtastic_MeshPacket &p) override;
    void handleResponse(uint32_t from, uint32_t id, const meshtastic_RouteDiscovery &route) override;
    void handlePositionResponse(uint32_t from, uint32_t request_id, int32_t rx_rssi, float rx_snr, bool isNeighbor) override;
    void notifyRestoreMessages(int32_t percentage) override;
    void notifyMessagesRestored(void) override;
    void notifyConnected(const char *info) override;
    void notifyDisconnected(const char *info) override;
    void notifyResync(bool show) override;
    void notifyReboot(bool show) override;
    void notifyShutdown(void) override;
    void blankScreen(bool enable) override;
    void screenSaving(bool enabled) override;
    bool isScreenLocked(void) override;
    void newMessage(uint32_t from, uint32_t to, uint8_t ch, const char *msg, uint32_t &msgtime, bool restore = true) override;
    void restoreMessage(const LogMessage &msg) override;
    void removeNode(uint32_t nodeNum) override;

    enum BasicSettings {
        eNone,
        eSetup,
        eUsername,
        eDeviceRole,
        eRegion,
        eModemPreset,
        eChannel,
        eWifi,
        eLanguage,
        eScreenTimeout,
        eScreenLock,
        eScreenBrightness,
        eTheme,
        eInputControl,
        eAlertBuzzer,
        eBackupRestore,
        eReset,
        eReboot,
        eDisplayMode,
        eModifyChannel,
#if defined(T_LORA_PAGER)
        eGPS,
        eMQTT,
#endif
    };

  protected:
    struct NodeFilter {
        bool unknown;  // filter out unknown nodes
        bool mqtt;     // filter out via mqtt nodes
        bool offline;  // filter out offline nodes (>15min lastheard)
        bool position; // filter out nodes without position
        char *name;    // filter by name
        bool active;   // flag for active filter
    };

    struct NodeHighlight {
        bool chat;      // highlight nodes with active chats
        bool position;  // highlight nodes with position
        bool telemetry; // highlight nodes with telemetry
        bool iaq;       // highlight nodes with IAQ
        char *name;     // hightlight by name
        bool active;    // flag for active highlight;
    };

    typedef void (*UserWidgetFunc)(lv_obj_t *, void *, int);

    // initialize all ui screens
    virtual void init_screens(void);
    // update custom display string on boot screen
    virtual void updateBootMessage(const char *);
    // show initial setup panel to configure region and name
    virtual void requestSetup(void);
    // patch widgets on generated screens
    virtual void apply_hotfix(void);
    // update node counter display (online and filtered)
    virtual void updateNodesStatus(void);
    // display message popup
    virtual void showMessagePopup(uint32_t from, uint32_t to, uint8_t ch, const char *name);
    // hide new message popup
    virtual void hideMessagePopup(void);
    // display user widget (dynamically created)
    void showUserWidget(UserWidgetFunc createWidget);
    // display messages of a group channel
    virtual void addChat(uint32_t from, uint32_t to, uint8_t ch);
    // mark chat border to indicate a new message
    virtual void highlightChat(uint32_t from, uint32_t to, uint8_t ch);
    // display number of active chats
    virtual void updateActiveChats(void);
    // display new message popup
    virtual void showMessages(uint8_t channel);
    // display messages of a node
    virtual void showMessages(uint32_t nodeNum);
    // own chat message
    virtual void handleAddMessage(char *msg);
    // add own message to current chat
    virtual void addMessage(lv_obj_t *container, uint32_t msgTime, uint32_t requestId, char *msg, LogMessage::MsgStatus status);
    // add new message to container
    virtual void newMessage(uint32_t nodeNum, lv_obj_t *container, uint8_t channel, const char *msg);
    // create empty message container for node or group channel
    virtual lv_obj_t *newMessageContainer(uint32_t from, uint32_t to, uint8_t ch);
    // filter or highlight node
    virtual bool applyNodesFilter(uint32_t nodeNum, bool reset = false);
    // display message alert popup
    virtual void messageAlert(const char *alert, bool show);
    // mark sent message as received
    virtual void handleTextMessageResponse(uint32_t channelOrNode, uint32_t id, bool ack, bool err);
    // set node image based on role
    virtual void setNodeImage(uint32_t nodeNum, eRole role, bool unmessagable, lv_obj_t *img);
    // apply filter and count number of filtered nodes
    virtual void updateNodesFiltered(bool reset);
    // set last heard to now, update nodes online
    virtual void updateLastHeard(uint32_t nodeNum);
    // update last heard value on all node panels
    virtual void updateAllLastHeard(void);
    // update image and unread messages on home screen
    virtual void updateUnreadMessages(void);
    // update time display on home screen
    virtual void updateTime(void);
    // update SD card slot info
    virtual bool updateSDCard(void);
    // re-read only the card statistics (a co-processor may compute them in
    // the background), polling a bounded number of times until they arrive
    void refreshSDCardStats(void);
    void armSDCardStatsPoll(void);
    // release the card so it can be pulled safely; a tap mounts it again
    void ejectSDCard(void);
#if defined(HAS_SDCARD) || defined(SENSECAP_INDICATOR)
    void formatSDCardLabel(char *buf, size_t len);
#endif
    uint16_t sdStatsPolls = 0;
    // format SD card if invalid
    virtual void formatSDCard(void);
    // update time display on home screen
    virtual void updateFreeMem(void);
    // update distance to other node
    virtual void updateDistance(uint32_t nodeNum, int32_t lat, int32_t lon);
    // show map and load tiles
    virtual void loadMap(void);
    // add objects on map
    virtual void addOrUpdateMap(uint32_t nodeNum, int32_t lat, int32_t lon);
    // remove objects from map
    virtual void removeFromMap(uint32_t nodeNum);
    // set url provider and dropdown and return url if present
    virtual std::string setUrlProvider(const char *style);
    // show or hide URL template input
    virtual void showUrlInputArea(bool show);

    std::function<void(uint32_t id, uint16_t x, uint16_t y, uint8_t)> drawObjectCB;

    NodeFilter filter;
    NodeHighlight highlight;

  private:
    // view creation only via ViewFactory
    friend class ViewFactory;
    static TFTView_320x240 *instance(void);
    static TFTView_320x240 *instance(const DisplayDriverConfig &cfg);
    TFTView_320x240();
    TFTView_320x240(const DisplayDriverConfig *cfg, DisplayDriver *driver);

    void enterProgrammingMode(void);
    void updateTheme(void);
    void ui_events_init(void);
    void ui_set_active(lv_obj_t *b, lv_obj_t *p, lv_obj_t *tp);
    void showKeyboard(lv_obj_t *textArea);
    void hideKeyboard(lv_obj_t *panel);
    // abort a running slide animation and restore panel/keyboard position at once
    void resetKeyboardSlide(void);
    lv_obj_t *showQrCode(lv_obj_t *parent, const char *data);

    void enablePanel(lv_obj_t *panel);
    void disablePanel(lv_obj_t *panel);
    void setGroupFocus(lv_obj_t *panel);
    void setInputGroup(void);
    void updateInputControls(void);
#if defined(T_LORA_PAGER)
    void createPagerToggleSettings(void);
    void createPagerSettingsControls(void);
    void beginPagerSettings(void);
    void finishPagerSettings(void);
    static void ui_event_PagerHomeSettings(lv_event_t *e);
    static void ui_event_gps_button(lv_event_t *e);
    static void ui_event_mqtt_button(lv_event_t *e);
    static void ui_event_pager_settings_key(lv_event_t *e);
    lv_obj_t *gpsSettingsButton = nullptr;
    lv_obj_t *gpsSettingsLabel = nullptr;
    lv_obj_t *pagerToggleSettingsPanel = nullptr;
    lv_obj_t *pagerToggleSettingsTitle = nullptr;
    lv_obj_t *pagerToggleSettingsDropdown = nullptr;
    lv_obj_t *mqttSettingsButton = nullptr;
    lv_obj_t *mqttSettingsLabel = nullptr;
    lv_obj_t *wifiEnabledCheckbox = nullptr;
    lv_obj_t *radioEnabledCheckbox = nullptr;
    lv_obj_t *bannerEnabledCheckbox = nullptr;
    lv_obj_t *settingsReturnRow = nullptr;
    bool pagerSettingsSidebarDisabled = false;
    std::string pagerGPSDetails;
    uint32_t pagerPacketSatellites = 0;
    LocalGPSStatus pagerGPSStatus{};
    bool pagerGPSStatusKnown = false;
    void renderPagerGPSStatus(void);
#endif
    void initMessageInputSettings(void);
    bool saveDoubleSpacePeriod(bool enabled);
    void handleMessageInput(lv_event_t *e);
    lv_obj_t *doubleSpaceSwitch = nullptr;
    lv_obj_t *doubleSpaceHint = nullptr;
    bool doubleSpacePeriod = false;
    bool spacePending = false;
    uint32_t lastSpaceAt = 0;
    uint32_t lastSpaceCursor = 0;
