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

#ifndef EMSESP_MIXER_H
#define EMSESP_MIXER_H

#include "emsesp.h"

namespace emsesp {

class Mixer : public EMSdevice {
  public:
    Mixer(uint8_t device_type, uint8_t device_id, uint8_t product_id, const std::string & version, const std::string & name, uint8_t flags, uint8_t brand);

    virtual bool publish_ha_device_config();

  private:
    static uuid::log::Logger logger_;

    void process_MMPLUSStatusMessage_HC(std::shared_ptr<const Telegram> telegram);
    void process_MMPLUSStatusMessage_WWC(std::shared_ptr<const Telegram> telegram);
    void process_IPMStatusMessage(std::shared_ptr<const Telegram> telegram);
    void process_IPMTempMessage(std::shared_ptr<const Telegram> telegram);
    void process_IPMSetMessage(std::shared_ptr<const Telegram> telegram);
    void process_MMStatusMessage(std::shared_ptr<const Telegram> telegram);
    void process_MMConfigMessage(std::shared_ptr<const Telegram> telegram);
    void process_MMSetMessage(std::shared_ptr<const Telegram> telegram);
    void process_HpPoolStatus(std::shared_ptr<const Telegram> telegram);

    bool set_flowSetTemp(const char * value, const int8_t id);
    bool set_pump(const char * value, const int8_t id);
    bool set_activated(const char * value, const int8_t id);
    bool set_setValveTime(const char * value, const int8_t id);

    enum class Type {
        NONE,
        HC,  // heating circuit
        WWC, // warm water circuit
        MP   // pool

    };

  private:
    uint16_t flowTempHc_;
    uint16_t flowTempVf_;
    uint8_t  pumpStatus_;
    int8_t   status_;
    uint8_t  flowSetTemp_;
    uint8_t  activated_;
    uint8_t  setValveTime_;

    int16_t poolTemp_;
    int8_t  poolShuntStatus_;
    int8_t  poolShunt_;

    Type     type_             = Type::NONE;
    uint16_t hc_               = EMS_VALUE_USHORT_NOTSET;
    int8_t   poolShuntStatus__ = EMS_VALUE_INT_NOTSET; // temp value
    uint8_t  id_;
};

} // namespace emsesp

#endif
