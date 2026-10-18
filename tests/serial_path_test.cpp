#include "serial_path.hpp"
#include <stdexcept>
static void check(bool ok) { if(!ok) throw std::runtime_error("Serial path validation failed"); }
int main() {
    check(!sk150::validSerialPath(""));
    check(!sk150::validSerialPath("COM3\n"));
    check(!sk150::validSerialPath(std::string("COM3\0extra",10)));
#ifdef _WIN32
    check(sk150::validSerialPath("COM3"));
    check(sk150::validSerialPath("com12"));
    check(sk150::validSerialPath("\\\\.\\COM123"));
    for(const char* path:{"COM0","COM01","COM","COM3:","COM-1","COM3x","/dev/ttyUSB0","\\\\.\\PhysicalDrive0"})
        check(!sk150::validSerialPath(path));
#else
    check(sk150::validSerialPath("/dev/ttyUSB0"));
    check(!sk150::validSerialPath("COM3"));
#endif
}
