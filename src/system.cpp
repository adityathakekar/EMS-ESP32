/*
 * EMS-ESP - https://github.com/emsesp/EMS-ESP
 * Copyright 2020  Paul Derbyshire
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "system.h"
#include "emsesp.h" // for send_raw_telegram() command

#if defined(EMSESP_DEBUG)
#include "test/test.h"
#endif

#ifndef EMSESP_STANDALONE
#ifdef ESP_IDF_VERSION_MAJOR // IDF 4+
#if CONFIG_IDF_TARGET_ESP32  // ESP32/PICO-D4
#include "../esp32/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "../esp32s2/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32C3
#include "../esp32c3/rom/rtc.h"
#else
#error Target CONFIG_IDF_TARGET is not supported
#endif
#else // ESP32 Before IDF 4.0
#include "../rom/rtc.h"
#endif
#endif

namespace emsesp {

uuid::log::Logger System::logger_{F_(system), uuid::log::Facility::KERN};

#ifndef EMSESP_STANDALONE
uuid::syslog::SyslogService System::syslog_;
#endif

// init statics
uint32_t System::heap_start_ = 1; // avoid using 0 to divide-by-zero later
PButton  System::myPButton_;
bool     System::restart_requested_ = false;

// send on/off to a gpio pin
// value: true = HIGH, false = LOW
// e.g. http://ems-esp/api?device=system&cmd=pin&data=1&id=2
bool System::command_pin(const char * value, const int8_t id) {
    if (!is_valid_gpio(id)) {
        LOG_INFO(F("invalid GPIO number"));
        return false;
    }

    bool v = false;
    if (Helpers::value2bool(value, v)) {
        pinMode(id, OUTPUT);
        digitalWrite(id, v);
        LOG_INFO(F("GPIO %d set to %s"), id, v ? "HIGH" : "LOW");
        return true;
    }

    return false;
}

// send raw to ems
bool System::command_send(const char * value, const int8_t id) {
    EMSESP::send_raw_telegram(value); // ignore id
    return true;
}

// fetch device values
bool System::command_fetch(const char * value, const int8_t id) {
    std::string value_s(14, '\0');
    if (Helpers::value2string(value, value_s)) {
        if (value_s == "all") {
            LOG_INFO(F("Requesting data from EMS devices"));
            EMSESP::fetch_device_values();
            return true;
        } else if (value_s == read_flash_string(F_(boiler))) {
            EMSESP::fetch_device_values_type(EMSdevice::DeviceType::BOILER);
            return true;
        } else if (value_s == read_flash_string(F_(thermostat))) {
            EMSESP::fetch_device_values_type(EMSdevice::DeviceType::THERMOSTAT);
            return true;
        } else if (value_s == read_flash_string(F_(solar))) {
            EMSESP::fetch_device_values_type(EMSdevice::DeviceType::SOLAR);
            return true;
        } else if (value_s == read_flash_string(F_(mixer))) {
            EMSESP::fetch_device_values_type(EMSdevice::DeviceType::MIXER);
            return true;
        }
    }

    EMSESP::fetch_device_values(); // default if no name or id is given
    return true;
}

// mqtt publish
bool System::command_publish(const char * value, const int8_t id) {
    std::string value_s(14, '\0');
    if (Helpers::value2string(value, value_s)) {
        if (value_s == "ha") {
            EMSESP::publish_all(true); // includes HA
            LOG_INFO(F("Publishing all data to MQTT, including HA configs"));
            return true;
        } else if (value_s == read_flash_string(F_(boiler))) {
            EMSESP::publish_device_values(EMSdevice::DeviceType::BOILER);
            return true;
        } else if (value_s == read_flash_string(F_(thermostat))) {
            EMSESP::publish_device_values(EMSdevice::DeviceType::THERMOSTAT);
            return true;
        } else if (value_s == read_flash_string(F_(solar))) {
            EMSESP::publish_device_values(EMSdevice::DeviceType::SOLAR);
            return true;
        } else if (value_s == read_flash_string(F_(mixer))) {
            EMSESP::publish_device_values(EMSdevice::DeviceType::MIXER);
            return true;
        } else if (value_s == "other") {
            EMSESP::publish_other_values();
            return true;
        } else if (value_s == read_flash_string(F_(dallassensor))) {
            EMSESP::publish_sensor_values(true);
            return true;
        }
    }

    EMSESP::publish_all(); // ignore value and id
    LOG_INFO(F("Publishing all data to MQTT"));
    return true;
}

// syslog level
bool System::command_syslog_level(const char * value, const int8_t id) {
    uint8_t s = 0xff;
    if (Helpers::value2enum(value, s, FL_(enum_syslog_level))) {
        EMSESP::webSettingsService.update(
            [&](WebSettings & settings) {
                settings.syslog_level = (int8_t)s - 1;
                return StateUpdateResult::CHANGED;
            },
            "local");
        EMSESP::system_.syslog_start();
        return true;
    }
    return false;
}

// watch
bool System::command_watch(const char * value, const int8_t id) {
    uint8_t w = 0xff;
    if (Helpers::value2enum(value, w, FL_(enum_watch))) {
        if (w == 0 || EMSESP::watch() == EMSESP::Watch::WATCH_OFF) {
            EMSESP::watch_id(0);
        }
        EMSESP::watch(w);
        return true;
    }
    uint16_t i = Helpers::hextoint(value);
    if (i) {
        EMSESP::watch_id(i);
        if (EMSESP::watch() == EMSESP::Watch::WATCH_OFF) {
            EMSESP::watch(EMSESP::Watch::WATCH_ON);
        }
        return true;
    }
    return false;
}

// restart EMS-ESP
void System::system_restart() {
    LOG_INFO(F("Restarting EMS-ESP..."));
    Shell::loop_all();
    delay(1000); // wait a second
#ifndef EMSESP_STANDALONE
    ESP.restart();
#endif
}

// saves all settings
void System::wifi_reconnect() {
    LOG_INFO(F("Wifi reconnecting..."));
    Shell::loop_all();
    EMSESP::console_.loop();
    delay(1000);                                                                   // wait a second
    EMSESP::webSettingsService.save();                                             // local settings
    EMSESP::esp8266React.getNetworkSettingsService()->callUpdateHandlers("local"); // in case we've changed ssid or password
}

// format fs
// format the FS. Wipes everything.
void System::format(uuid::console::Shell & shell) {
    auto msg = F("Formatting file system. This will reset all settings to their defaults");
    shell.logger().warning(msg);
    shell.flush();

    EMSuart::stop();

#ifndef EMSESP_STANDALONE
    LITTLEFS.format();
#endif

    System::system_restart();
}

void System::syslog_start() {
    bool was_enabled = syslog_enabled_;
    EMSESP::webSettingsService.read([&](WebSettings & settings) {
        syslog_enabled_       = settings.syslog_enabled;
        syslog_level_         = settings.syslog_level;
        syslog_mark_interval_ = settings.syslog_mark_interval;
        syslog_host_          = settings.syslog_host;
        syslog_port_          = settings.syslog_port;
    });
#ifndef EMSESP_STANDALONE
    if (syslog_enabled_) {
        // start & configure syslog
        if (!was_enabled) {
            syslog_.start();
            EMSESP::logger().info(F("Starting Syslog"));
        }
        syslog_.log_level((uuid::log::Level)syslog_level_);
        syslog_.mark_interval(syslog_mark_interval_);
        syslog_.destination(syslog_host_.c_str(), syslog_port_);
        syslog_.hostname(hostname().c_str());

        // register the command
        Command::add(EMSdevice::DeviceType::SYSTEM, F_(syslog_level), System::command_syslog_level, F("changes syslog level"), CommandFlag::ADMIN_ONLY);

    } else if (was_enabled) {
        // in case service is still running, this flushes the queue
        // https://github.com/emsesp/EMS-ESP/issues/496
        EMSESP::logger().info(F("Stopping Syslog"));
        syslog_.log_level((uuid::log::Level)-1);
        syslog_.mark_interval(0);
        syslog_.destination("");
    }
#endif
}

// read all the settings except syslog from the config files and store locally
void System::get_settings() {
    EMSESP::webSettingsService.read([&](WebSettings & settings) {
        // Button
        pbutton_gpio_ = settings.pbutton_gpio;

        // ADC
        analog_enabled_ = settings.analog_enabled;

        // Sysclock
        low_clock_ = settings.low_clock;

        // LED
        hide_led_ = settings.hide_led;
        led_gpio_ = settings.led_gpio;

        // Board profile
        board_profile_ = settings.board_profile;

        // Ethernet PHY
        phy_type_ = settings.phy_type;
    });
}

// adjust WiFi settings
// this for problem solving mesh and connection issues, and also get EMS bus-powered more stable by lowering power
void System::wifi_tweak() {
#if defined(EMSESP_WIFI_TWEAK)
    // Default Tx Power is 80 = 20dBm <-- default
    // WIFI_POWER_19_5dBm = 78,// 19.5dBm
    // WIFI_POWER_19dBm = 76,// 19dBm
    // WIFI_POWER_18_5dBm = 74,// 18.5dBm
    // WIFI_POWER_17dBm = 68,// 17dBm
    // WIFI_POWER_15dBm = 60,// 15dBm
    // WIFI_POWER_13dBm = 52,// 13dBm
    // WIFI_POWER_11dBm = 44,// 11dBm
    // WIFI_POWER_8_5dBm = 34,// 8.5dBm
    // WIFI_POWER_7dBm = 28,// 7dBm
    // WIFI_POWER_5dBm = 20,// 5dBm
    // WIFI_POWER_2dBm = 8,// 2dBm
    // WIFI_POWER_MINUS_1dBm = -4// -1dBm
    wifi_power_t p1 = WiFi.getTxPower();
    (void)WiFi.setTxPower(WIFI_POWER_17dBm);
    wifi_power_t p2 = WiFi.getTxPower();
    bool         s1 = WiFi.getSleep();
    WiFi.setSleep(false); // turn off sleep - WIFI_PS_NONE
    bool s2 = WiFi.getSleep();
#if defined(EMSESP_DEBUG)
    LOG_DEBUG(F("[DEBUG] Adjusting WiFi - Tx power %d->%d, Sleep %d->%d"), p1, p2, s1, s2);
#endif
#endif
}

// check for valid ESP32 pins. This is very dependent on which ESP32 board is being used.
// Typically you can't use 1, 6-11, 12, 14, 15, 20, 24, 28-31 and 40+
// we allow 0 as it has a special function on the NodeMCU apparently
// See https://diyprojects.io/esp32-how-to-use-gpio-digital-io-arduino-code/#.YFpVEq9KhjG
// and https://nodemcu.readthedocs.io/en/dev-esp32/modules/gpio/
bool System::is_valid_gpio(uint8_t pin) {
    if ((pin == 1) || (pin >= 6 && pin <= 12) || (pin >= 14 && pin <= 15) || (pin == 20) || (pin == 24) || (pin >= 28 && pin <= 31) || (pin > 40)) {
        return false; // bad pin
    }
    return true;
}

// first call. Sets memory and starts up the UART Serial bridge
void System::start(uint32_t heap_start) {
#if defined(EMSESP_DEBUG)
    show_mem("Startup");
#endif

    // set the inital free mem, only on first boot
    if (heap_start_ < 2) {
        heap_start_ = heap_start;
    }

    // load in all the settings first
    get_settings();

#ifndef EMSESP_STANDALONE
    // disable bluetooth module
    periph_module_disable(PERIPH_BT_MODULE);
    if (low_clock_) {
        setCpuFrequencyMhz(160);
    }
#endif

    EMSESP::esp8266React.getNetworkSettingsService()->read([&](NetworkSettings & networkSettings) {
        hostname(networkSettings.hostname.c_str()); // sets the hostname
        LOG_INFO(F("System name: %s"), hostname().c_str());
    });

    commands_init();     // console & api commands
    led_init(false);     // init LED
    adc_init(false);     // analog ADC
    button_init(false);  // the special button
    network_init(false); // network
    syslog_start();      // start Syslog

    EMSESP::init_uart(); // start UART
}

// adc and bluetooth
void System::adc_init(bool refresh) {
    if (refresh) {
        get_settings();
    }
#ifndef EMSESP_STANDALONE
    // disable ADC
    /*
    if (!analog_enabled_) {
        adc_power_release(); // turn off ADC to save power if not needed
    }
    */
#endif
}

// button single click
void System::button_OnClick(PButton & b) {
    LOG_DEBUG(F("Button pressed - single click"));
}

// button double click
void System::button_OnDblClick(PButton & b) {
    LOG_DEBUG(F("Button pressed - double click - reconnect"));
    EMSESP::system_.wifi_reconnect();
}

// button long press
void System::button_OnLongPress(PButton & b) {
    LOG_DEBUG(F("Button pressed - long press"));
}

// button indefinite press
void System::button_OnVLongPress(PButton & b) {
    LOG_DEBUG(F("Button pressed - very long press"));
#ifndef EMSESP_STANDALONE
    LOG_WARNING(F("Performing factory reset..."));
    EMSESP::console_.loop();

#ifdef EMSESP_DEBUG
    Test::listDir(LITTLEFS, FS_CONFIG_DIRECTORY, 3);
#endif

    EMSESP::esp8266React.factoryReset();
#endif
}

// push button
void System::button_init(bool refresh) {
    if (refresh) {
        get_settings();
    }

    if (is_valid_gpio(pbutton_gpio_)) {
        if (!myPButton_.init(pbutton_gpio_, HIGH)) {
            LOG_INFO(F("Multi-functional button not detected"));
        } else {
            LOG_INFO(F("Multi-functional button enabled"));
        }
    } else {
        LOG_WARNING(F("Invalid button GPIO. Check config."));
    }

    myPButton_.onClick(BUTTON_Debounce, button_OnClick);
    myPButton_.onDblClick(BUTTON_DblClickDelay, button_OnDblClick);
    myPButton_.onLongPress(BUTTON_LongPressDelay, button_OnLongPress);
    myPButton_.onVLongPress(BUTTON_VLongPressDelay, button_OnVLongPress);
}

// set the LED to on or off when in normal operating mode
void System::led_init(bool refresh) {
    if (refresh) {
        get_settings();
    }

    if ((led_gpio_ != 0) && is_valid_gpio(led_gpio_)) {
        pinMode(led_gpio_, OUTPUT); // 0 means disabled
        digitalWrite(led_gpio_, hide_led_ ? !LED_ON : LED_ON);
    }
}

// returns true if OTA is uploading
bool System::upload_status() {
#if defined(EMSESP_STANDALONE)
    return false;
#else
    return upload_status_ || Update.isRunning();
#endif
}

void System::upload_status(bool in_progress) {
    // if we've just started an upload
    if ((!upload_status_) && (in_progress)) {
        EMSuart::stop();
    }
    upload_status_ = in_progress;
}

// checks system health and handles LED flashing wizardry
void System::loop() {
    // check if we're supposed to do a reset/restart
    if (restart_requested()) {
        this->system_restart();
    }

#ifndef EMSESP_STANDALONE
    myPButton_.check(); // check button press

    if (syslog_enabled_) {
        syslog_.loop();
    }

    led_monitor();  // check status and report back using the LED
    system_check(); // check system health
    if (analog_enabled_) {
        measure_analog();
    }

    // send out heartbeat
    uint32_t currentMillis = uuid::get_uptime();
    if (!last_heartbeat_ || (currentMillis - last_heartbeat_ > SYSTEM_HEARTBEAT_INTERVAL)) {
        last_heartbeat_ = currentMillis;
        send_heartbeat();
    }
#ifndef EMSESP_STANDALONE

#if defined(EMSESP_DEBUG)
/*
    static uint32_t last_memcheck_ = 0;
    if (currentMillis - last_memcheck_ > 10000) { // 10 seconds
        last_memcheck_ = currentMillis;
        show_mem("core");
    }
    */
#endif

#endif

#endif
}

void System::show_mem(const char * note) {
#ifndef EMSESP_STANDALONE
    static uint32_t old_free_heap = 0;
    uint32_t        free_heap     = ESP.getFreeHeap();
    LOG_INFO(F("(%s) Free heap: %lu (~%lu)"), note, free_heap, (uint32_t)Helpers::abs(free_heap - old_free_heap));
    old_free_heap = free_heap;
#endif
}

// create the json for heartbeat
bool System::heartbeat_json(JsonObject & output) {
    uint8_t bus_status = EMSESP::bus_status();
    if (bus_status == EMSESP::BUS_STATUS_TX_ERRORS) {
        output["bus_status"] = FJSON("txerror");
    } else if (bus_status == EMSESP::BUS_STATUS_CONNECTED) {
        output["bus_status"] = FJSON("connected");
    } else {
        output["bus_status"] = FJSON("disconnected");
    }

    output["uptime"] = uuid::log::format_timestamp_ms(uuid::get_uptime_ms(), 3);

    output["uptime_sec"] = uuid::get_uptime_sec();
    output["rxreceived"] = EMSESP::rxservice_.telegram_count();
    output["rxfails"]    = EMSESP::rxservice_.telegram_error_count();
    output["txreads"]    = EMSESP::txservice_.telegram_read_count();
    output["txwrites"]   = EMSESP::txservice_.telegram_write_count();
    output["txfails"]    = EMSESP::txservice_.telegram_fail_count();
    if (Mqtt::enabled()) {
        output["mqttfails"] = Mqtt::publish_fails();
    }
    if (EMSESP::dallas_enabled()) {
        output["dallasfails"] = EMSESP::sensor_fails();
    }

#ifndef EMSESP_STANDALONE
    output["freemem"] = ESP.getFreeHeap() / 1000L; // kilobytes
#endif

    if (analog_enabled_) {
        output["adc"] = analog_;
    }

#ifndef EMSESP_STANDALONE
    if (!ethernet_connected_) {
        int8_t rssi            = WiFi.RSSI();
        output["rssi"]         = rssi;
        output["wifistrength"] = wifi_quality(rssi);
    }
#endif

    return true;
}

// send periodic MQTT message with system information
void System::send_heartbeat() {
    // don't send heartbeat if WiFi or MQTT is not connected
    if (!Mqtt::connected()) {
        return;
    }

    StaticJsonDocument<EMSESP_JSON_SIZE_SMALL> doc;
    JsonObject                                 json = doc.to<JsonObject>();

    if (heartbeat_json(json)) {
        Mqtt::publish(F_(heartbeat), doc.as<JsonObject>()); // send to MQTT with retain off. This will add to MQTT queue.
    }
}

// measure and moving average adc
void System::measure_analog() {
    static uint32_t measure_last_ = 0;

    if (!measure_last_ || (uint32_t)(uuid::get_uptime() - measure_last_) >= SYSTEM_MEASURE_ANALOG_INTERVAL) {
        measure_last_ = uuid::get_uptime();
#if defined(EMSESP_STANDALONE)
        uint16_t a = 0;
#else
        uint16_t a = analogReadMilliVolts(ADC1_CHANNEL_0_GPIO_NUM);
#endif
        static uint32_t sum_ = 0;

        if (!analog_) { // init first time
            analog_ = a;
            sum_    = a * 512;
        } else { // simple moving average filter
            sum_    = (sum_ * 511) / 512 + a;
            analog_ = sum_ / 512;
        }
    }
}

// sets rate of led flash
void System::set_led_speed(uint32_t speed) {
    led_flash_speed_ = speed;
    led_monitor();
}

// initializes network
void System::network_init(bool refresh) {
    if (refresh) {
        get_settings();
    }

    last_system_check_ = 0; // force the LED to go from fast flash to pulse
    send_heartbeat();

    if (phy_type_ == PHY_type::PHY_TYPE_NONE) {
        return;
    }

    uint8_t          phy_addr;   // I²C-address of Ethernet PHY (0 or 1 for LAN8720, 31 for TLK110)
    int              power;      // Pin# of the enable signal for the external crystal oscillator (-1 to disable for internal APLL source)
    int              mdc;        // Pin# of the I²C clock signal for the Ethernet PHY
    int              mdio;       // Pin# of the I²C IO signal for the Ethernet PHY
    eth_phy_type_t   type;       // Type of the Ethernet PHY (LAN8720 or TLK110)
    eth_clock_mode_t clock_mode; // ETH_CLOCK_GPIO0_IN or ETH_CLOCK_GPIO0_OUT, ETH_CLOCK_GPIO16_OUT, ETH_CLOCK_GPIO17_OUT for 50Hz inverted clock

    if (phy_type_ == PHY_type::PHY_TYPE_LAN8720) {
        phy_addr   = 1;
        power      = 16;
        mdc        = 23;
        mdio       = 18;
        type       = ETH_PHY_LAN8720;
        clock_mode = ETH_CLOCK_GPIO0_IN;
    } else if (phy_type_ == PHY_type::PHY_TYPE_TLK110) {
        phy_addr   = 31;
        power      = -1;
        mdc        = 23;
        mdio       = 18;
        type       = ETH_PHY_TLK110;
        clock_mode = ETH_CLOCK_GPIO0_IN;
    } else {
        return; // no valid profile
    }

    // special case for Olimex ESP32-EVB (LAN8720) (different power and phy_addr)
    if (board_profile_.equals("OLIMEX")) {
        phy_addr   = 0;
        power      = -1;
        mdc        = 23;
        mdio       = 18;
        type       = ETH_PHY_LAN8720;
        clock_mode = ETH_CLOCK_GPIO0_IN;
    }

    ETH.begin(phy_addr, power, mdc, mdio, type, clock_mode);
}

// check health of system, done every few seconds
void System::system_check() {
    if (!last_system_check_ || ((uint32_t)(uuid::get_uptime() - last_system_check_) >= SYSTEM_CHECK_FREQUENCY)) {
        last_system_check_ = uuid::get_uptime();

#ifndef EMSESP_STANDALONE
        if (!ethernet_connected() && (WiFi.status() != WL_CONNECTED)) {
            set_led_speed(LED_WARNING_BLINK_FAST);
            system_healthy_ = false;
            return;
        }
#endif

        // not healthy if bus not connected
        if (!EMSbus::bus_connected()) {
            if (system_healthy_) {
                LOG_ERROR(F("Error: No connection to the EMS bus"));
            }
            system_healthy_ = false;
            set_led_speed(LED_WARNING_BLINK); // flash every 1/2 second from now on
        } else {
            // if it was unhealthy but now we're better, make sure the LED is solid again cos we've been healed
            if (!system_healthy_) {
                system_healthy_ = true;
                send_heartbeat();
                if (led_gpio_) {
                    digitalWrite(led_gpio_, hide_led_ ? !LED_ON : LED_ON);
                }
            }
        }
    }
}

// commands - takes static function pointers
void System::commands_init() {
    Command::add(EMSdevice::DeviceType::SYSTEM,
                 F_(pin),
                 System::command_pin,
                 F("sets a GPIO on/off"),
                 CommandFlag::MQTT_SUB_FLAG_NOSUB | CommandFlag::ADMIN_ONLY); // dont create a MQTT topic for this

    Command::add(EMSdevice::DeviceType::SYSTEM, F_(send), System::command_send, F("sends a telegram"), CommandFlag::ADMIN_ONLY);
    Command::add(EMSdevice::DeviceType::SYSTEM, F_(fetch), System::command_fetch, F("refreshes all EMS values"), CommandFlag::ADMIN_ONLY);
    Command::add(EMSdevice::DeviceType::SYSTEM, F_(restart), System::command_restart, F("restarts EMS-ESP"), CommandFlag::ADMIN_ONLY);
    Command::add(EMSdevice::DeviceType::SYSTEM, F_(watch), System::command_watch, F("watch incoming telegrams"));

    if (Mqtt::enabled()) {
        Command::add(EMSdevice::DeviceType::SYSTEM, F_(publish), System::command_publish, F("forces a MQTT publish"));
    }

    // these commands will return data in JSON format
    Command::add(EMSdevice::DeviceType::SYSTEM, F_(info), System::command_info, F("show system status"));
    Command::add(EMSdevice::DeviceType::SYSTEM, F_(settings), System::command_settings, F("shows system settings"));
    Command::add(EMSdevice::DeviceType::SYSTEM, F_(commands), System::command_commands, F("shows system commands"));

#if defined(EMSESP_DEBUG)
    Command::add(EMSdevice::DeviceType::SYSTEM, F("test"), System::command_test, F("runs a specific test"));
#endif

    // MQTT subscribe "ems-esp/system/#"
    Mqtt::subscribe(EMSdevice::DeviceType::SYSTEM, "system/#", nullptr); // use empty function callback
}

// flashes the LED
void System::led_monitor() {
    if (!led_gpio_) {
        return;
    }

    static uint32_t led_last_blink_ = 0;

    if (!led_last_blink_ || (uint32_t)(uuid::get_uptime() - led_last_blink_) >= led_flash_speed_) {
        led_last_blink_ = uuid::get_uptime();

        // if bus_not_connected or network not connected, start flashing
        if (!system_healthy_) {
            digitalWrite(led_gpio_, !digitalRead(led_gpio_));
        }
    }
}

// Return the quality (Received Signal Strength Indicator) of the WiFi network as a %
//  High quality: 90% ~= -55dBm
//  Medium quality: 50% ~= -75dBm
//  Low quality: 30% ~= -85dBm
//  Unusable quality: 8% ~= -96dBm
int8_t System::wifi_quality(int8_t dBm) {
    if (dBm <= -100) {
        return 0;
    }

    if (dBm >= -50) {
        return 100;
    }
    return 2 * (dBm + 100);
}

// print users to console
void System::show_users(uuid::console::Shell & shell) {
    shell.printfln(F("Users:"));

#ifndef EMSESP_STANDALONE
    EMSESP::esp8266React.getSecuritySettingsService()->read([&](SecuritySettings & securitySettings) {
        for (const User & user : securitySettings.users) {
            shell.printfln(F(" username: %s, password: %s, is_admin: %s"), user.username.c_str(), user.password.c_str(), user.admin ? F("yes") : F("no"));
        }
    });
#endif

    shell.println();
}

void System::show_system(uuid::console::Shell & shell) {
    shell.printfln(F("Uptime: %s"), uuid::log::format_timestamp_ms(uuid::get_uptime_ms(), 3).c_str());

#ifndef EMSESP_STANDALONE
    shell.printfln(F("SDK version: %s"), ESP.getSdkVersion());
    shell.printfln(F("CPU frequency: %lu MHz"), ESP.getCpuFreqMHz());
    shell.printfln(F("Free heap: %lu bytes"), (uint32_t)ESP.getFreeHeap());
    shell.println();

    switch (WiFi.status()) {
    case WL_IDLE_STATUS:
        shell.printfln(F("WiFi: Idle"));
        break;

    case WL_NO_SSID_AVAIL:
        shell.printfln(F("WiFi: Network not found"));
        break;

    case WL_SCAN_COMPLETED:
        shell.printfln(F("WiFi: Network scan complete"));
        break;

    case WL_CONNECTED:
        shell.printfln(F("WiFi: Connected"));
        shell.printfln(F("SSID: %s"), WiFi.SSID().c_str());
        shell.printfln(F("BSSID: %s"), WiFi.BSSIDstr().c_str());
        shell.printfln(F("RSSI: %d dBm (%d %%)"), WiFi.RSSI(), wifi_quality(WiFi.RSSI()));
        shell.printfln(F("MAC address: %s"), WiFi.macAddress().c_str());
        shell.printfln(F("Hostname: %s"), WiFi.getHostname());
        shell.printfln(F("IPv4 address: %s/%s"), uuid::printable_to_string(WiFi.localIP()).c_str(), uuid::printable_to_string(WiFi.subnetMask()).c_str());
        shell.printfln(F("IPv4 gateway: %s"), uuid::printable_to_string(WiFi.gatewayIP()).c_str());
        shell.printfln(F("IPv4 nameserver: %s"), uuid::printable_to_string(WiFi.dnsIP()).c_str());
        if (WiFi.localIPv6().toString() != "0000:0000:0000:0000:0000:0000:0000:0000") {
            shell.printfln(F("IPv6 address: %s"), uuid::printable_to_string(WiFi.localIPv6()).c_str());
        }
        break;

    case WL_CONNECT_FAILED:
        shell.printfln(F("WiFi: Connection failed"));
        break;

    case WL_CONNECTION_LOST:
        shell.printfln(F("WiFi: Connection lost"));
        break;

    case WL_DISCONNECTED:
        shell.printfln(F("WiFi: Disconnected"));
        break;

    case WL_NO_SHIELD:
    default:
        shell.printfln(F("WiFi: Unknown"));
        break;
    }

    shell.println();

    // show Ethernet
    if (ethernet_connected_) {
        shell.printfln(F("Ethernet: Connected"));
        shell.printfln(F("MAC address: %s"), ETH.macAddress().c_str());
        shell.printfln(F("Hostname: %s"), ETH.getHostname());
        shell.printfln(F("IPv4 address: %s/%s"), uuid::printable_to_string(ETH.localIP()).c_str(), uuid::printable_to_string(ETH.subnetMask()).c_str());
        shell.printfln(F("IPv4 gateway: %s"), uuid::printable_to_string(ETH.gatewayIP()).c_str());
        shell.printfln(F("IPv4 nameserver: %s"), uuid::printable_to_string(ETH.dnsIP()).c_str());
        if (ETH.localIPv6().toString() != "0000:0000:0000:0000:0000:0000:0000:0000") {
            shell.printfln(F("IPv6 address: %s"), uuid::printable_to_string(ETH.localIPv6()).c_str());
        }
    } else {
        shell.printfln(F("Ethernet: disconnected"));
    }

    shell.println();
    if (!syslog_enabled_) {
        shell.printfln(F("Syslog: disabled"));
    } else {
        shell.printfln(F("Syslog: %s"), syslog_.started() ? "started" : "stopped");
        shell.print(F(" "));
        shell.printfln(F_(host_fmt), !syslog_host_.isEmpty() ? syslog_host_.c_str() : read_flash_string(F_(unset)).c_str());
        shell.printfln(F(" IP: %s"), uuid::printable_to_string(syslog_.ip()).c_str());
        shell.print(F(" "));
        shell.printfln(F_(port_fmt), syslog_port_);
        shell.print(F(" "));
        shell.printfln(F_(log_level_fmt), uuid::log::format_level_lowercase(static_cast<uuid::log::Level>(syslog_level_)));
        shell.print(F(" "));
        shell.printfln(F_(mark_interval_fmt), syslog_mark_interval_);
        shell.printfln(F(" Queued: %d"), syslog_.queued());
    }

#endif
}

// upgrade from previous versions of EMS-ESP
// returns true if an upgrade was done
bool System::check_upgrade() {
    return false;
}

// list commands
bool System::command_commands(const char * value, const int8_t id, JsonObject & output) {
    return Command::list(EMSdevice::DeviceType::SYSTEM, output);
}

// export all settings to JSON text
// http://ems-esp/api/system/settings
// value and id are ignored
bool System::command_settings(const char * value, const int8_t id, JsonObject & output) {
    JsonObject node;

    node            = output.createNestedObject("System");
    node["version"] = EMSESP_APP_VERSION;

    // hide ssid from this list
    EMSESP::esp8266React.getNetworkSettingsService()->read([&](NetworkSettings & settings) {
        node                     = output.createNestedObject("Network");
        node["hostname"]         = settings.hostname;
        node["static_ip_config"] = settings.staticIPConfig;
        node["enableIPv6"]       = settings.enableIPv6;
        JsonUtils::writeIP(node, "local_ip", settings.localIP);
        JsonUtils::writeIP(node, "gateway_ip", settings.gatewayIP);
        JsonUtils::writeIP(node, "subnet_mask", settings.subnetMask);
        JsonUtils::writeIP(node, "dns_ip_1", settings.dnsIP1);
        JsonUtils::writeIP(node, "dns_ip_2", settings.dnsIP2);
    });

#ifndef EMSESP_STANDALONE
    EMSESP::esp8266React.getAPSettingsService()->read([&](APSettings & settings) {
        node                   = output.createNestedObject("AP");
        node["provision_mode"] = settings.provisionMode;
        node["ssid"]           = settings.ssid;
        node["local_ip"]       = settings.localIP.toString();
        node["gateway_ip"]     = settings.gatewayIP.toString();
        node["subnet_mask"]    = settings.subnetMask.toString();
    });
#endif

    EMSESP::esp8266React.getMqttSettingsService()->read([&](MqttSettings & settings) {
        node            = output.createNestedObject("MQTT");
        node["enabled"] = settings.enabled;
#ifndef EMSESP_STANDALONE
        node["host"]          = settings.host;
        node["port"]          = settings.port;
        node["username"]      = settings.username;
        node["client_id"]     = settings.clientId;
        node["keep_alive"]    = settings.keepAlive;
        node["clean_session"] = settings.cleanSession;
#endif
        node["publish_time_boiler"]     = settings.publish_time_boiler;
        node["publish_time_thermostat"] = settings.publish_time_thermostat;
        node["publish_time_solar"]      = settings.publish_time_solar;
        node["publish_time_mixer"]      = settings.publish_time_mixer;
        node["publish_time_other"]      = settings.publish_time_other;
        node["publish_time_sensor"]     = settings.publish_time_sensor;
        node["ha_climate_format"]       = settings.ha_climate_format;
        node["ha_enabled"]              = settings.ha_enabled;
        node["mqtt_qos"]                = settings.mqtt_qos;
        node["mqtt_retain"]             = settings.mqtt_retain;
        node["send_response"]           = settings.send_response;
    });

#ifndef EMSESP_STANDALONE
    EMSESP::esp8266React.getNTPSettingsService()->read([&](NTPSettings & settings) {
        node              = output.createNestedObject("NTP");
        node["enabled"]   = settings.enabled;
        node["server"]    = settings.server;
        node["tz_label"]  = settings.tzLabel;
        node["tz_format"] = settings.tzFormat;
    });

    EMSESP::esp8266React.getOTASettingsService()->read([&](OTASettings & settings) {
        node            = output.createNestedObject("OTA");
        node["enabled"] = settings.enabled;
        node["port"]    = settings.port;
    });
#endif

    EMSESP::webSettingsService.read([&](WebSettings & settings) {
        node = output.createNestedObject("Settings");

        node["board_profile"] = settings.board_profile;
        node["tx_mode"]       = settings.tx_mode;
        node["ems_bus_id"]    = settings.ems_bus_id;

        node["syslog_enabled"]       = settings.syslog_enabled;
        node["syslog_level"]         = settings.syslog_level;
        node["syslog_mark_interval"] = settings.syslog_mark_interval;
        node["syslog_host"]          = settings.syslog_host;
        node["syslog_port"]          = settings.syslog_port;

        node["master_thermostat"] = settings.master_thermostat;

        node["shower_timer"] = settings.shower_timer;
        node["shower_alert"] = settings.shower_alert;

        node["rx_gpio"]      = settings.rx_gpio;
        node["tx_gpio"]      = settings.tx_gpio;
        node["dallas_gpio"]  = settings.dallas_gpio;
        node["pbutton_gpio"] = settings.pbutton_gpio;
        node["led_gpio"]     = settings.led_gpio;
        node["phy_type"]     = settings.phy_type;

        node["hide_led"]        = settings.hide_led;
        node["notoken_api"]     = settings.notoken_api;
        node["dallas_parasite"] = settings.dallas_parasite;
        node["dallas_format"]   = settings.dallas_format;
        node["bool_format"]     = settings.bool_format;
        node["enum_format"]     = settings.enum_format;
        node["analog_enabled"]  = settings.analog_enabled;
    });

    return true;
}

// export status information including the device information
// http://ems-esp/api/system/info
bool System::command_info(const char * value, const int8_t id, JsonObject & output) {
    JsonObject node;

    // System
    node = output.createNestedObject("System");

    node["version"]          = EMSESP_APP_VERSION;
    node["uptime"]           = uuid::log::format_timestamp_ms(uuid::get_uptime_ms(), 3);
    node["uptime (seconds)"] = uuid::get_uptime_sec();

#ifndef EMSESP_STANDALONE
    node["freemem"] = ESP.getFreeHeap() / 1000L; // kilobytes
#endif
    node["reset reason"] = EMSESP::system_.reset_reason(0) + " / " + EMSESP::system_.reset_reason(1);

    if (EMSESP::dallas_enabled()) {
        node["Dallas sensors"] = EMSESP::sensor_devices().size();
    }

#ifndef EMSESP_STANDALONE
    // Network
    node = output.createNestedObject("Network");
    if (WiFi.status() == WL_CONNECTED) {
        node["connection"]      = F("WiFi");
        node["hostname"]        = WiFi.getHostname();
        node["SSID"]            = WiFi.SSID();
        node["BSSID"]           = WiFi.BSSIDstr();
        node["RSSI"]            = WiFi.RSSI();
        node["MAC"]             = WiFi.macAddress();
        node["IPv4 address"]    = uuid::printable_to_string(WiFi.localIP()) + "/" + uuid::printable_to_string(WiFi.subnetMask());
        node["IPv4 gateway"]    = uuid::printable_to_string(WiFi.gatewayIP());
        node["IPv4 nameserver"] = uuid::printable_to_string(WiFi.dnsIP());
        if (WiFi.localIPv6().toString() != "0000:0000:0000:0000:0000:0000:0000:0000") {
            node["IPv6 address"] = uuid::printable_to_string(WiFi.localIPv6());
        }
    } else if (EMSESP::system_.ethernet_connected()) {
        node["connection"]      = F("Ethernet");
        node["hostname"]        = ETH.getHostname();
        node["MAC"]             = ETH.macAddress();
        node["IPv4 address"]    = uuid::printable_to_string(ETH.localIP()) + "/" + uuid::printable_to_string(ETH.subnetMask());
        node["IPv4 gateway"]    = uuid::printable_to_string(ETH.gatewayIP());
        node["IPv4 nameserver"] = uuid::printable_to_string(ETH.dnsIP());
        if (ETH.localIPv6().toString() != "0000:0000:0000:0000:0000:0000:0000:0000") {
            node["IPv6 address"] = uuid::printable_to_string(ETH.localIPv6());
        }
    }
#endif

    // Status
    node = output.createNestedObject("Status");

    switch (EMSESP::bus_status()) {
    case EMSESP::BUS_STATUS_OFFLINE:
        node["bus status"] = (F("disconnected"));
        break;
    case EMSESP::BUS_STATUS_TX_ERRORS:
        node["bus status"] = (F("connected, instable tx"));
        break;
    case EMSESP::BUS_STATUS_CONNECTED:
    default:
        node["bus status"] = (F("connected"));
        break;
    }

    if (EMSESP::bus_status() != EMSESP::BUS_STATUS_OFFLINE) {
        node["bus protocol"]         = EMSbus::is_ht3() ? F("HT3") : F("Buderus");
        node["telegrams received"]   = EMSESP::rxservice_.telegram_count();
        node["read requests sent"]   = EMSESP::txservice_.telegram_read_count();
        node["write requests sent"]  = EMSESP::txservice_.telegram_write_count();
        node["incomplete telegrams"] = EMSESP::rxservice_.telegram_error_count();
        node["tx fails"]             = EMSESP::txservice_.telegram_fail_count();
        node["rx line quality"]      = EMSESP::rxservice_.quality();
        node["tx line quality"]      = EMSESP::txservice_.quality();
        if (Mqtt::enabled()) {
            node["MQTT"]               = Mqtt::connected() ? F_(connected) : F_(disconnected);
            node["MQTT publishes"]     = Mqtt::publish_count();
            node["MQTT publish fails"] = Mqtt::publish_fails();
        }
        if (EMSESP::dallas_enabled()) {
            node["Dallas reads"] = EMSESP::sensor_reads();
            node["Dallas fails"] = EMSESP::sensor_fails();
        }
#ifndef EMSESP_STANDALONE
        if (EMSESP::system_.syslog_enabled_) {
            node["syslog IP"]     = syslog_.ip();
            node["syslog active"] = syslog_.started();
        }
#endif
    }

    // Devices - show EMS devices
    JsonArray devices = output.createNestedArray("Devices");
    for (const auto & device_class : EMSFactory::device_handlers()) {
        for (const auto & emsdevice : EMSESP::emsdevices) {
            if ((emsdevice) && (emsdevice->device_type() == device_class.first)) {
                JsonObject obj = devices.createNestedObject();
                obj["type"]    = emsdevice->device_type_name();
                obj["name"]    = emsdevice->to_string();
                char result[200];
                (void)emsdevice->show_telegram_handlers(result);
                if (result[0] != '\0') {
                    obj["handlers"] = result; // don't show hanlders if there aren't any
                }
            }
        }
    }

    return true;
}

#if defined(EMSESP_DEBUG)
// run a test, e.g. http://ems-esp/api?device=system&cmd=test&data=boiler
bool System::command_test(const char * value, const int8_t id) {
    Test::run_test(value, id);
    return true;
}
#endif

// takes a board profile and populates a data array with GPIO configurations
// data = led, dallas, rx, tx, button, phy_type
// returns false if profile is not found
bool System::load_board_profile(std::vector<uint8_t> & data, const std::string & board_profile) {
    if (board_profile == "S32") {
        data = {2, 18, 23, 5, 0, PHY_type::PHY_TYPE_NONE}; // BBQKees Gateway S32
    } else if (board_profile == "E32") {
        data = {2, 4, 5, 17, 33, PHY_type::PHY_TYPE_LAN8720}; // BBQKees Gateway E32
    } else if (board_profile == "MH-ET") {
        data = {2, 18, 23, 5, 0, PHY_type::PHY_TYPE_NONE}; // MH-ET Live D1 Mini
    } else if (board_profile == "NODEMCU") {
        data = {2, 18, 23, 5, 0, PHY_type::PHY_TYPE_NONE}; // NodeMCU 32S
    } else if (board_profile == "LOLIN") {
        data = {2, 18, 17, 16, 0, PHY_type::PHY_TYPE_NONE}; // Lolin D32
    } else if (board_profile == "OLIMEX") {
        data = {0, 0, 36, 4, 34, PHY_type::PHY_TYPE_LAN8720}; // Olimex ESP32-EVB (uses U1TXD/U1RXD/BUTTON, no LED or Dallas)
    } else {
        data = {EMSESP_DEFAULT_LED_GPIO,
                EMSESP_DEFAULT_DALLAS_GPIO,
                EMSESP_DEFAULT_RX_GPIO,
                EMSESP_DEFAULT_TX_GPIO,
                EMSESP_DEFAULT_PBUTTON_GPIO,
                EMSESP_DEFAULT_PHY_TYPE};
        return (board_profile == "CUSTOM");
    }

    return true;
}

// restart command - perform a hard reset by setting flag
bool System::command_restart(const char * value, const int8_t id) {
    restart_requested(true);
    return true;
}

const std::string System::reset_reason(uint8_t cpu) {
#ifndef EMSESP_STANDALONE
    switch (rtc_get_reset_reason(cpu)) {
    case 1:
        return ("Power on reset");
    // case 2 :reset pin not on esp32
    case 3:
        return ("Software reset");
    case 4:
        return ("Legacy watch dog reset");
    case 5:
        return ("Deep sleep reset");
    case 6:
        return ("Reset by SDIO");
    case 7:
        return ("Timer group0 watch dog reset");
    case 8:
        return ("Timer group1 watch dog reset");
    case 9:
        return ("RTC watch dog reset");
    case 10:
        return ("Intrusion reset CPU");
    case 11:
        return ("Timer group reset CPU");
    case 12:
        return ("Software reset CPU");
    case 13:
        return ("RTC watch dog reset: CPU");
    case 14:
        return ("APP CPU reseted by PRO CPU");
    case 15:
        return ("Brownout reset");
    case 16:
        return ("RTC watch dog reset: CPU+RTC");
    default:
        break;
    }
#endif
    return ("Unkonwn");
}

} // namespace emsesp
