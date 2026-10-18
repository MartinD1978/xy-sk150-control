#pragma once
#include "modbus.hpp"
#include <algorithm>
#include <optional>
#include <stdexcept>
namespace sk150 {
constexpr unsigned DeviceMaxCurrent=7200;
struct Preset { bool configured=false; unsigned voltage=0,current=0; };
// Integer register units avoid rounding a value across a protection threshold.
inline bool allowed(unsigned voltage,unsigned current,VoltageLimit limit,const std::array<uint16_t,2>& protections) {
    return voltage<=limit.maximum()&&current<=DeviceMaxCurrent&&voltage<protections[0]&&current<protections[1];
}
inline void validateTargets(unsigned voltage,unsigned current,VoltageLimit limit,const std::array<uint16_t,2>& protections) {
    if(!allowed(voltage,current,limit,protections))
        throw std::runtime_error("Sollwerte erreichen OVP/OCP oder überschreiten das Spannungslimit / den Gerätebereich");
}
inline unsigned sliderMaximum(unsigned hardware,unsigned threshold,unsigned step) {
    return threshold==0 ? 0 : std::min(hardware,threshold-1)/step;
}
}
