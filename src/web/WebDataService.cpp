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

#include "emsesp.h"

namespace emsesp {

using namespace std::placeholders; // for `_1` etc

WebDataService::WebDataService(AsyncWebServer * server, SecurityManager * securityManager)
    : _device_dataHandler(DEVICE_DATA_SERVICE_PATH,
                          securityManager->wrapCallback(std::bind(&WebDataService::device_data, this, _1, _2), AuthenticationPredicates::IS_AUTHENTICATED))
    , _writevalue_dataHandler(WRITE_VALUE_SERVICE_PATH,
                              securityManager->wrapCallback(std::bind(&WebDataService::write_value, this, _1, _2), AuthenticationPredicates::IS_ADMIN))
    , _writesensor_dataHandler(WRITE_SENSOR_SERVICE_PATH,
                               securityManager->wrapCallback(std::bind(&WebDataService::write_sensor, this, _1, _2), AuthenticationPredicates::IS_ADMIN)) {
    server->on(EMSESP_DATA_SERVICE_PATH,
               HTTP_GET,
               securityManager->wrapRequest(std::bind(&WebDataService::all_devices, this, _1), AuthenticationPredicates::IS_AUTHENTICATED));
    server->on(SCAN_DEVICES_SERVICE_PATH,
               HTTP_GET,
               securityManager->wrapRequest(std::bind(&WebDataService::scan_devices, this, _1), AuthenticationPredicates::IS_AUTHENTICATED));

    _device_dataHandler.setMethod(HTTP_POST);
    _device_dataHandler.setMaxContentLength(256);
    server->addHandler(&_device_dataHandler);

    _writevalue_dataHandler.setMethod(HTTP_POST);
    _writevalue_dataHandler.setMaxContentLength(256);
    server->addHandler(&_writevalue_dataHandler);

    _writesensor_dataHandler.setMethod(HTTP_POST);
    _writesensor_dataHandler.setMaxContentLength(256);
    server->addHandler(&_writesensor_dataHandler);
}

void WebDataService::scan_devices(AsyncWebServerRequest * request) {
    EMSESP::scan_devices();
    request->send(200);
}

void WebDataService::all_devices(AsyncWebServerRequest * request) {
    AsyncJsonResponse * response = new AsyncJsonResponse(false, EMSESP_JSON_SIZE_XLARGE_DYN);
    JsonObject          root     = response->getRoot();

    JsonArray devices = root.createNestedArray("devices");
    for (auto & emsdevice : EMSESP::emsdevices) {
        if (emsdevice) {
            JsonObject obj = devices.createNestedObject();
            obj["i"]       = emsdevice->unique_id();        // id
            obj["t"]       = emsdevice->device_type_name(); // type
            obj["b"]       = emsdevice->brand_to_string();  // brand

            // shortname - we prefix the count to make it unique
            uint8_t device_index = EMSESP::device_index(emsdevice->device_type(), emsdevice->unique_id());
            if (device_index) {
                char s[10];
                obj["sn"] = emsdevice->device_type_name() + Helpers::smallitoa(s, device_index);
            } else {
                obj["sn"] = emsdevice->device_type_name();
            }

            obj["n"] = emsdevice->name();       // name
            obj["d"] = emsdevice->device_id();  // deviceid
            obj["p"] = emsdevice->product_id(); // productid
            obj["v"] = emsdevice->version();    // version
        }
    }

    // always create sensors, even if it's empty
    JsonArray sensors = root.createNestedArray("sensors");
    if (EMSESP::have_sensors()) {
        uint8_t i = 1;
        for (const auto & sensor : EMSESP::sensor_devices()) {
            JsonObject obj = sensors.createNestedObject();
            obj["n"]       = i++;                                // no
            obj["i"]       = sensor.to_string(true);             // id
            obj["t"]       = (float)(sensor.temperature_c) / 10; // temp
            obj["o"]       = (float)(sensor.offset()) / 10;      // offset
        }
    }

    if (EMSESP::system_.analog_enabled()) {
        root["analog"] = EMSESP::system_.analog();
    }

    response->setLength();
    request->send(response);
}

// The unique_id is the unique record ID from the Web table to identify which device to load
// Compresses the JSON using MsgPack https://msgpack.org/index.html
void WebDataService::device_data(AsyncWebServerRequest * request, JsonVariant & json) {
    if (json.is<JsonObject>()) {
        MsgpackAsyncJsonResponse * response = new MsgpackAsyncJsonResponse(false, EMSESP_JSON_SIZE_XXLARGE_DYN);
        for (const auto & emsdevice : EMSESP::emsdevices) {
            if (emsdevice) {
                if (emsdevice->unique_id() == json["id"]) {
                    // wait max 2.5 sec for updated data (post_send_delay is 2 sec)
                    for (uint16_t i = 0; i < (emsesp::TxService::POST_SEND_DELAY + 500) && EMSESP::wait_validate(); i++) {
                        delay(1);
                    }
                    EMSESP::wait_validate(0); // reset in case of timeout
#ifndef EMSESP_STANDALONE
                    JsonObject root = response->getRoot();
                    emsdevice->generate_values_web(root);
#endif
                    response->setLength();
                    request->send(response);
                    return;
                }
            }
        }
    }

    // invalid
    AsyncWebServerResponse * response = request->beginResponse(200);
    request->send(response);
}

// takes a command and its data value from a specific Device, from the Web
// assumes the service has been checked for admin authentication
void WebDataService::write_value(AsyncWebServerRequest * request, JsonVariant & json) {
    if (json.is<JsonObject>()) {
        JsonObject dv        = json["devicevalue"];
        uint8_t    unique_id = json["id"];

        // using the unique ID from the web find the real device type
        // id is the selected device
        for (const auto & emsdevice : EMSESP::emsdevices) {
            if (emsdevice) {
                if (emsdevice->unique_id() == unique_id) {
                    // parse the command as it could have a hc or wwc prefixed, e.g. hc2/seltemp
                    const char * cmd = dv["c"];                                // the command
                    int8_t       id  = -1;                                     // default
                    cmd              = Command::parse_command_string(cmd, id); // extract hc or wwc

                    // create JSON for output
                    AsyncJsonResponse * response = new AsyncJsonResponse(false, EMSESP_JSON_SIZE_SMALL);
                    JsonObject          output   = response->getRoot();

                    // the data could be in any format, but we need string
                    // authenticated is always true
                    JsonVariant data        = dv["v"]; // the value in any format
                    uint8_t     return_code = CommandRet::OK;
                    uint8_t     device_type = emsdevice->device_type();
                    if (data.is<const char *>()) {
                        return_code = Command::call(device_type, cmd, data.as<const char *>(), true, id, output);
                    } else if (data.is<int>()) {
                        char s[10];
                        return_code = Command::call(device_type, cmd, Helpers::render_value(s, data.as<int16_t>(), 0), true, id, output);
                    } else if (data.is<float>()) {
                        char s[10];
                        return_code = Command::call(device_type, cmd, Helpers::render_value(s, (float)data.as<float>(), 1), true, id, output);
                    } else if (data.is<bool>()) {
                        return_code = Command::call(device_type, cmd, data.as<bool>() ? "true" : "false", true, id, output);
                    }

                    // write debug
                    if (return_code != CommandRet::OK) {
                        EMSESP::logger().err(F("Write command failed %s (%s)"), (const char *)output["message"], Command::return_code_string(return_code).c_str());
                    } else {
                        EMSESP::logger().debug(F("Write command successful"));
                    }

                    response->setCode((return_code == CommandRet::OK) ? 200 : 204);
                    response->setLength();
                    request->send(response);
                    return;
                }
            }
        }
    }

    AsyncWebServerResponse * response = request->beginResponse(204); // Write command failed
    request->send(response);
}

// takes a sensorname and optional offset from the Web
void WebDataService::write_sensor(AsyncWebServerRequest * request, JsonVariant & json) {
    bool ok = false;
    if (json.is<JsonObject>()) {
        JsonObject sensor = json;

        // if valid add.
        uint8_t no = sensor["no"];
        if (no > 0 && no < 100) {
            char        name[20];
            std::string id = sensor["id"];
            strlcpy(name, id.c_str(), sizeof(name));
            float   offset   = sensor["offset"]; // this will be a float value. We'll convert it to int and * 10 it
            int16_t offset10 = offset * 10;
            char    idstr[3];
            ok = EMSESP::dallassensor_.update(Helpers::itoa(idstr, no, 10), name, offset10);
        }
    }

    AsyncWebServerResponse * response = request->beginResponse(ok ? 200 : 204);
    request->send(response);
}

} // namespace emsesp