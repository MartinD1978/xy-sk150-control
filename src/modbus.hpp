#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <optional>
#include <vector>

namespace sk150 {
constexpr unsigned DeviceMaxVoltage = 3600; // Documented XY-SK150 range: 0–36 V.
struct VoltageLimit {
    bool enabled = true;
    unsigned centivolts = 1200;
    unsigned maximum() const;
    void validate(unsigned voltage) const;
};
enum class Parity { None, Even, Odd };
struct SerialSettings {
    unsigned baud=115200, dataBits=8, stopBits=1, slave=1;
    Parity parity=Parity::None;
    void validate() const;
};
enum class RegulationMode { CV, CC };
uint16_t crc(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> voltageRequest(unsigned centivolts, VoltageLimit limit = {});
std::vector<uint8_t> outputRequest(unsigned enabled);
unsigned parseVoltage(std::string text);
unsigned parseCurrent(std::string text);
std::vector<uint8_t> currentRequest(unsigned milliamps);
unsigned parseOvp(std::string text);
unsigned parseOcp(std::string text);
unsigned parseOpp(std::string text);
std::vector<uint8_t> ovpRequest(unsigned centivolts);
std::vector<uint8_t> ocpRequest(unsigned milliamps);
std::vector<uint8_t> oppRequest(unsigned deciwatts);
class Port {
    int fd_ = -1;
    SerialSettings settings_;
    std::vector<uint8_t> transact(const std::vector<uint8_t>& request);
public:
    explicit Port(const std::string& path, SerialSettings settings = {});
    ~Port();
    Port(const Port&) = delete;
    Port& operator=(const Port&) = delete;
    std::array<uint16_t,6> readValues();
    void setVoltage(unsigned centivolts, VoltageLimit limit = {});
    void setTargets(std::optional<unsigned> voltage, std::optional<unsigned> current, VoltageLimit limit = {});
    void setOutputGuarded(unsigned enabled, VoltageLimit limit = {});
    std::array<uint16_t,2> readProtections(); // M0: 0x0053 OVP, 0x0054 OCP
    void setOvp(unsigned centivolts);
    void setOcp(unsigned milliamps);
    uint16_t readOpp();
    void setOpp(unsigned deciwatts);
    uint16_t readProtectionStatus();
    bool readOutput();
    RegulationMode readMode();
    void setOutput(unsigned enabled, VoltageLimit limit = {});
};
}
