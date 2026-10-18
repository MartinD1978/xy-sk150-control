#include "modbus.hpp"
#include "setpoints.hpp"
#include <algorithm>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "serial_path.hpp"
#else
#include <fcntl.h>
#include <poll.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#endif

namespace sk150 {
uint16_t crc(const std::vector<uint8_t>& b) {
    uint16_t c = 0xffff;
    for (auto v : b) { c ^= v; for (int i=0;i<8;++i) c = (c&1) ? (c>>1)^0xa001 : c>>1; }
    return c;
}
static std::vector<uint8_t> frame(std::vector<uint8_t> b) {
    auto c=crc(b); b.push_back(c&255); b.push_back(c>>8); return b;
}
unsigned VoltageLimit::maximum() const {
    if(enabled && centivolts>DeviceMaxVoltage)
        throw std::runtime_error("Spannungslimit muss zwischen 0 und 36,00 V liegen");
    return enabled ? centivolts : DeviceMaxVoltage;
}
void VoltageLimit::validate(unsigned voltage) const {
    if(voltage>maximum())
        throw std::runtime_error("Spannung überschreitet das eingestellte Limit bzw. 36,00 V");
}
std::vector<uint8_t> voltageRequest(unsigned v, VoltageLimit limit) {
    // Whitelisted voltage write; no generic register write API.
    limit.validate(v);
    return frame({1,6,0,0,static_cast<uint8_t>(v>>8),static_cast<uint8_t>(v&255)});
}
std::vector<uint8_t> outputRequest(unsigned enabled) {
    if(enabled>1) throw std::runtime_error("Ausgangswert muss 0 oder 1 sein");
    return frame({1,6,0,0x12,0,static_cast<uint8_t>(enabled)});
}
static unsigned parseDecimal(std::string s, unsigned places, unsigned maximum) {
    auto start=s.find_first_not_of(" \t\r\n");
    if(start==std::string::npos) throw std::runtime_error("Wert fehlt");
    s=s.substr(start,s.find_last_not_of(" \t\r\n")-start+1);
    unsigned scale=1;
    for(unsigned n=0;n<places;++n) scale*=10;
    unsigned whole=0, fraction=0, decimals=0; bool dot=false, digit=false;
    for(char c:s) {
        if(c=='.'||c==',') { if(dot||!digit) throw std::runtime_error("Ungültiger Dezimalwert"); dot=true; continue; }
        if(c<'0'||c>'9') throw std::runtime_error("Nur positive Dezimalzahlen oder Null erlaubt");
        digit=true;
        if(dot) {
            if(++decimals>places) throw std::runtime_error("Zu viele Nachkommastellen");
            fraction=fraction*10+unsigned(c-'0');
        } else {
            whole=whole*10+unsigned(c-'0');
            if(whole>maximum/scale) throw std::runtime_error("Wert außerhalb des erlaubten Bereichs");
        }
    }
    if(dot&&decimals==0) throw std::runtime_error("Nachkommastellen fehlen");
    while(decimals++<places) fraction*=10;
    unsigned value=whole*scale+fraction;
    if(value>maximum) throw std::runtime_error("Wert außerhalb des erlaubten Bereichs");
    return value;
}
unsigned parseVoltage(std::string text) { return parseDecimal(text,2,DeviceMaxVoltage); }
unsigned parseCurrent(std::string text) { return parseDecimal(text,3,DeviceMaxCurrent); }
std::vector<uint8_t> currentRequest(unsigned value) {
    if(value>DeviceMaxCurrent) throw std::runtime_error("Strom muss zwischen 0 und 7,200 A liegen");
    return frame({1,6,0,1,static_cast<uint8_t>(value>>8),static_cast<uint8_t>(value&255)});
}
unsigned parseOvp(std::string text) { return parseDecimal(text,2,3800); }
unsigned parseOcp(std::string text) { return parseDecimal(text,3,7200); }
unsigned parseOpp(std::string text) { return parseDecimal(text,1,1500); }
std::vector<uint8_t> ovpRequest(unsigned v) {
    if(v>3800) throw std::runtime_error("OVP muss zwischen 0 und 38,00 V liegen");
    return frame({1,6,0,0x53,static_cast<uint8_t>(v>>8),static_cast<uint8_t>(v&255)});
}
std::vector<uint8_t> ocpRequest(unsigned v) {
    if(v>7200) throw std::runtime_error("OCP muss zwischen 0 und 7,200 A liegen");
    return frame({1,6,0,0x54,static_cast<uint8_t>(v>>8),static_cast<uint8_t>(v&255)});
}
std::vector<uint8_t> oppRequest(unsigned v) {
    if(v>1500) throw std::runtime_error("OPP muss zwischen 0 und 150,0 W liegen");
    return frame({1,6,0,0x55,static_cast<uint8_t>(v>>8),static_cast<uint8_t>(v&255)});
}
#ifdef _WIN32
static unsigned baudCode(unsigned baud) {
    switch(baud) {
        case 1200: case 2400: case 4800: case 9600: case 19200:
        case 38400: case 57600: case 115200: case 230400: return baud;
        default: throw std::runtime_error("Nicht unterstützte Baudrate");
    }
}
#else
static speed_t baudCode(unsigned baud) {
    switch(baud) {
        case 1200:return B1200; case 2400:return B2400; case 4800:return B4800;
        case 9600:return B9600; case 19200:return B19200; case 38400:return B38400;
        case 57600:return B57600; case 115200:return B115200; case 230400:return B230400;
        default:throw std::runtime_error("Nicht unterstützte Baudrate");
    }
}
#endif
void SerialSettings::validate() const {
    baudCode(baud);
    if(dataBits<5||dataBits>8) throw std::runtime_error("Datenbits müssen zwischen 5 und 8 liegen");
    if(stopBits!=1&&stopBits!=2) throw std::runtime_error("Stoppbits müssen 1 oder 2 sein");
    if(slave<1||slave>247) throw std::runtime_error("Modbus-Slave-Adresse muss zwischen 1 und 247 liegen");
    if(parity!=Parity::None&&parity!=Parity::Even&&parity!=Parity::Odd) throw std::runtime_error("Ungültige Parität");
}
#ifdef _WIN32
static void fail(const char* context) {
    throw std::runtime_error(std::string(context)+" (Windows-Fehler "+std::to_string(GetLastError())+")");
}
Port::Port(const std::string& path, SerialSettings settings):settings_(settings) {
    settings_.validate();
    if(!validSerialPath(path)) throw std::runtime_error("Bitte einen COM-Port angeben, z. B. COM3");
    std::string name=path;
    if(name.rfind("\\\\.\\",0)==0) name=name.substr(4);
    const std::string device="\\\\.\\"+name;
    handle_=CreateFileA(device.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
    if(handle_==INVALID_HANDLE_VALUE) fail("Seriellen Port öffnen");
    try {
        DCB d{}; d.DCBlength=sizeof d;
        if(!GetCommState(handle_,&d)) fail("Portkonfiguration lesen");
        d.BaudRate=settings.baud; d.ByteSize=static_cast<BYTE>(settings.dataBits);
        d.Parity=settings.parity==Parity::None?NOPARITY:settings.parity==Parity::Even?EVENPARITY:ODDPARITY;
        d.StopBits=settings.stopBits==1?ONESTOPBIT:TWOSTOPBITS;
        d.fBinary=TRUE; d.fParity=settings.parity!=Parity::None;
        d.fOutxCtsFlow=FALSE; d.fOutxDsrFlow=FALSE; d.fDtrControl=DTR_CONTROL_DISABLE;
        d.fDsrSensitivity=FALSE; d.fTXContinueOnXoff=TRUE; d.fOutX=FALSE; d.fInX=FALSE;
        d.fErrorChar=FALSE; d.fNull=FALSE; d.fRtsControl=RTS_CONTROL_DISABLE; d.fAbortOnError=FALSE;
        if(!SetCommState(handle_,&d)) fail("Portkonfiguration setzen");
        DCB applied{}; applied.DCBlength=sizeof applied;
        if(!GetCommState(handle_,&applied)) fail("Portkonfiguration prüfen");
        if(applied.BaudRate!=d.BaudRate||applied.ByteSize!=d.ByteSize||applied.Parity!=d.Parity||applied.StopBits!=d.StopBits)
            throw std::runtime_error("Der serielle Treiber übernimmt die gewählten Parameter nicht");
        // Return available bytes promptly; the overlapped deadline bounds each transaction.
        COMMTIMEOUTS timeouts{};
        timeouts.ReadIntervalTimeout=MAXDWORD;
        if(!SetCommTimeouts(handle_,&timeouts)) fail("Zeitlimits setzen");
        if(!PurgeComm(handle_,PURGE_RXCLEAR|PURGE_TXCLEAR)) fail("Portpuffer leeren");
    } catch(...) { CloseHandle(handle_); handle_=INVALID_HANDLE_VALUE; throw; }
}
Port::~Port() { if(handle_!=INVALID_HANDLE_VALUE) CloseHandle(handle_); }
#else
static void fail(const char* context) { throw std::runtime_error(std::string(context)+": "+strerror(errno)); }
Port::Port(const std::string& path, SerialSettings settings):settings_(settings) {
    settings_.validate();
    fd_=open(path.c_str(),O_RDWR|O_NOCTTY|O_NONBLOCK|O_CLOEXEC);
    if(fd_<0) fail("Seriellen Port öffnen");
    try {
        if(ioctl(fd_,TIOCEXCL)<0) fail("Port exklusiv reservieren");
        termios t{}; if(tcgetattr(fd_,&t)<0) fail("Portkonfiguration lesen");
        cfmakeraw(&t);
        if(cfsetispeed(&t,baudCode(settings_.baud))<0||cfsetospeed(&t,baudCode(settings_.baud))<0) fail("Baudrate setzen");
        t.c_cflag &= ~(PARENB|PARODD|CSTOPB|CSIZE|CRTSCTS);
#ifdef CMSPAR
        t.c_cflag &= ~CMSPAR;
#endif
        t.c_iflag &= ~(IXON|IXOFF|IXANY);
        const tcflag_t sizes[]={CS5,CS6,CS7,CS8};
        t.c_cflag |= sizes[settings_.dataBits-5]|CLOCAL|CREAD;
        if(settings_.stopBits==2) t.c_cflag |= CSTOPB;
        if(settings_.parity!=Parity::None) {
            t.c_cflag |= PARENB;
            if(settings_.parity==Parity::Odd) t.c_cflag |= PARODD;
            t.c_iflag |= INPCK;
        } t.c_cc[VMIN]=0; t.c_cc[VTIME]=0;
        if(tcsetattr(fd_,TCSANOW,&t)<0) fail("Portkonfiguration setzen");
        termios applied{};
        if(tcgetattr(fd_,&applied)<0) fail("Portkonfiguration prüfen");
        const tcflag_t frameMask=CSIZE|CSTOPB|PARENB|PARODD;
        if((applied.c_cflag&frameMask)!=(t.c_cflag&frameMask)||
           cfgetispeed(&applied)!=baudCode(settings_.baud)||cfgetospeed(&applied)!=baudCode(settings_.baud))
            throw std::runtime_error("Der serielle Treiber übernimmt die gewählten Parameter nicht");
        if(tcflush(fd_,TCIOFLUSH)<0) fail("Portpuffer leeren");
    } catch(...) { close(fd_); fd_=-1; throw; }
}
Port::~Port() { if(fd_>=0) close(fd_); }
#endif
std::vector<uint8_t> Port::transact(const std::vector<uint8_t>& request) {
    auto req=request;
    req[0]=static_cast<uint8_t>(settings_.slave);
    const auto requestCrc=crc(std::vector<uint8_t>(req.begin(),req.end()-2));
    req[req.size()-2]=requestCrc&255; req.back()=requestCrc>>8;
    using Clock=std::chrono::steady_clock;
    // Fresh input and a quiet interval separate transactions, also after failures.
    // At low baud rates respect at least 3.5 complete character times.
    const unsigned bits=1+settings_.dataBits+settings_.stopBits+(settings_.parity==Parity::None?0:1);
    const unsigned gap=std::max(5000u,(3500000u*bits+settings_.baud-1)/settings_.baud);
    std::this_thread::sleep_for(std::chrono::microseconds(gap));
#ifdef _WIN32
    if(!PurgeComm(handle_,PURGE_RXCLEAR)) fail("Empfangspuffer leeren");
#else
    if(tcflush(fd_,TCIFLUSH)<0) fail("Empfangspuffer leeren");
#endif
    auto deadline=Clock::now()+std::chrono::milliseconds(500);
#ifdef _WIN32
    auto transfer=[&](bool sending,uint8_t* data,size_t size) -> size_t {
        if(Clock::now()>=deadline) throw std::runtime_error("Modbus-Zeitüberschreitung (500 ms)");
        OVERLAPPED operation{}; operation.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if(!operation.hEvent) fail("Ereignis anlegen");
        DWORD count=0;
        BOOL ok=sending ? WriteFile(handle_,data,static_cast<DWORD>(size),&count,&operation)
                        : ReadFile(handle_,data,static_cast<DWORD>(size),&count,&operation);
        DWORD error=ok?ERROR_SUCCESS:GetLastError();
        if(!ok&&error==ERROR_IO_PENDING) {
            auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-Clock::now()).count();
            DWORD result=WaitForSingleObject(operation.hEvent,static_cast<DWORD>(std::max<int64_t>(0,remaining)));
            if(result!=WAIT_OBJECT_0) {
                // Settle cancellation before the stack buffer/event can be released.
                CancelIoEx(handle_,&operation);
                GetOverlappedResult(handle_,&operation,&count,TRUE);
                CloseHandle(operation.hEvent);
                throw std::runtime_error("Modbus-Zeitüberschreitung / Verbindung unterbrochen");
            }
            ok=GetOverlappedResult(handle_,&operation,&count,FALSE);
            error=ok?ERROR_SUCCESS:GetLastError();
        }
        CloseHandle(operation.hEvent);
        if(!ok) { SetLastError(error); fail(sending?"Senden":"Empfangen"); }
        if(!count) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return count;
    };
#else
    auto wait=[&](short events) {
        for(;;) {
            int ms=static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline-Clock::now()).count());
            if(ms<=0) throw std::runtime_error("Modbus-Zeitüberschreitung (500 ms)");
            pollfd p{fd_,events,0}; int n=poll(&p,1,ms);
            if(n<0) { if(errno==EINTR) continue; fail("Port abfragen"); }
            if(n==0) throw std::runtime_error("Modbus-Zeitüberschreitung (500 ms)");
            if(p.revents&(POLLERR|POLLHUP|POLLNVAL)) throw std::runtime_error("Serielle Verbindung unterbrochen");
            if(p.revents&events) return;
        }
    };
    #endif
    size_t sent=0;
    while(sent<req.size()) {
#ifdef _WIN32
        sent+=transfer(true,req.data()+sent,req.size()-sent);
#else
        wait(POLLOUT); auto n=write(fd_,req.data()+sent,req.size()-sent);
        if(n<0) { if(errno==EINTR||errno==EAGAIN) continue; fail("Senden"); } sent+=static_cast<size_t>(n);
#endif
    }
    std::vector<uint8_t> out; size_t expected=3;
    while(out.size()<expected) {
        uint8_t buffer[256];
#ifdef _WIN32
        auto n=transfer(false,buffer,std::min(sizeof buffer,expected-out.size()));
#else
        wait(POLLIN); auto n=read(fd_,buffer,std::min(sizeof buffer,expected-out.size()));
        if(n<0) { if(errno==EINTR||errno==EAGAIN) continue; fail("Empfangen"); }
#endif
        if(n==0) continue;
        out.insert(out.end(),buffer,buffer+n);
        if(out.size()>=3) {
            if(out[0]!=req[0]) throw std::runtime_error("Falsche Slave-Adresse");
            if(out[1]==(req[1]|0x80)) expected=5;
            else if(out[1]!=req[1]) throw std::runtime_error("Falscher Funktionscode");
            else if(req[1]==3) { if(out[2]!=req[5]*2) throw std::runtime_error("Falsche Registeranzahl"); expected=5+req[5]*2; }
            else expected=8;
        }
    }
    auto c=crc(std::vector<uint8_t>(out.begin(),out.end()-2));
    if(out[out.size()-2]!=(c&255)||out.back()!=(c>>8)) throw std::runtime_error("CRC-Fehler");
    if(out[1]&0x80) throw std::runtime_error("Modbus-Ausnahme "+std::to_string(out[2]));
    if(req[1]==6&&out!=req) throw std::runtime_error("Schreibbestätigung stimmt nicht überein");
    return out;
}
std::array<uint16_t,6> Port::readValues() {
    auto b=transact(frame({1,3,0,0,0,6})); std::array<uint16_t,6> v{};
    for(size_t i=0;i<6;++i) v[i]=static_cast<uint16_t>((b[3+i*2]<<8)|b[4+i*2]);
    return v;
}
std::array<uint16_t,2> Port::readProtections() {
    auto b=transact(frame({1,3,0,0x53,0,2}));
    std::array<uint16_t,2> values{static_cast<uint16_t>((b[3]<<8)|b[4]),static_cast<uint16_t>((b[5]<<8)|b[6])};
    if(values[0]>3800||values[1]>7200) throw std::runtime_error("Ungültige OVP/OCP-Gerätewerte");
    return values;
}
void Port::setOvp(unsigned v) {
    auto request=ovpRequest(v);
    readProtections();
    transact(request);
    if(readProtections()[0]!=v) throw std::runtime_error("OVP nicht durch Rücklesen bestätigt");
}
void Port::setOcp(unsigned v) {
    auto request=ocpRequest(v);
    readProtections();
    transact(request);
    if(readProtections()[1]!=v) throw std::runtime_error("OCP nicht durch Rücklesen bestätigt");
}
uint16_t Port::readOpp() {
    auto b=transact(frame({1,3,0,0x55,0,1}));
    auto value=static_cast<uint16_t>((b[3]<<8)|b[4]);
    if(value>1500) throw std::runtime_error("Ungültiger OPP-Gerätewert");
    return value;
}
void Port::setOpp(unsigned value) {
    auto request=oppRequest(value);
    readOpp(); transact(request);
    if(readOpp()!=value) throw std::runtime_error("OPP nicht durch Rücklesen bestätigt");
}
uint16_t Port::readProtectionStatus() {
    auto b=transact(frame({1,3,0,0x10,0,1}));
    return static_cast<uint16_t>((b[3]<<8)|b[4]);
}
bool Port::readOutput() {
    auto b=transact(frame({1,3,0,0x12,0,1}));
    unsigned value=(b[3]<<8)|b[4];
    if(value>1) throw std::runtime_error("Ungültiger Ausgangszustand (erwartet 0/1)");
    return value==1;
}
RegulationMode Port::readMode() {
    auto b=transact(frame({1,3,0,0x11,0,1}));
    unsigned value=(b[3]<<8)|b[4];
    if(value>1) throw std::runtime_error("Ungültiger CV/CC-Zustand (erwartet 0/1)");
    return value==0 ? RegulationMode::CV : RegulationMode::CC;
}
void Port::setOutput(unsigned enabled, VoltageLimit limit) {
    auto request=outputRequest(enabled);
    if(enabled) {
        // Also enforce the enable guard in the protocol layer, on fresh values.
        auto values=readValues();
        limit.validate(values[0]);
        limit.validate(values[2]);
    }
    transact(request);
}
void Port::setVoltage(unsigned v, VoltageLimit limit) { transact(voltageRequest(v,limit)); }
void Port::setTargets(std::optional<unsigned> voltage,std::optional<unsigned> current,VoltageLimit limit) {
    // Validate the entire requested pair before the first write. Recheck fresh
    // protection registers before each subsequent write; never retry a write.
    auto values=readValues();
    auto protections=readProtections();
    const unsigned targetV=voltage.value_or(values[0]),targetI=current.value_or(values[1]);
    validateTargets(targetV,targetI,limit,protections);
    auto writeOne=[&](bool isVoltage,unsigned value) {
        auto actual=readValues();
        auto guards=readProtections();
        validateTargets(targetV,targetI,limit,guards);
        // Reject front-panel changes that invalidate the unchanged half of a single-value command.
        if(!voltage&&actual[0]!=targetV) throw std::runtime_error("Spannung wurde zwischenzeitlich am Gerät geändert");
        if(!current&&actual[1]!=targetI) throw std::runtime_error("Strom wurde zwischenzeitlich am Gerät geändert");
        transact(isVoltage ? voltageRequest(value,limit) : currentRequest(value));
        auto confirmed=readValues();
        if(confirmed[isVoltage?0:1]!=value) throw std::runtime_error("Sollwert nicht durch Rücklesen bestätigt");
    };
    // Apply a lower current ceiling before changing voltage. Otherwise change
    // voltage first, then raise current. Two FC06 writes are not atomic.
    const bool currentFirst=current&&targetI<values[1];
    if(currentFirst) writeOne(false,targetI);
    if(voltage) writeOne(true,targetV);
    if(current&&!currentFirst) writeOne(false,targetI);
    values=readValues();
    if(values[0]!=targetV||values[1]!=targetI) throw std::runtime_error("Sollwertpaar nicht durch Rücklesen bestätigt");
}
void Port::setOutputGuarded(unsigned enabled,VoltageLimit limit) {
    if(enabled) {
        const auto values=readValues();
        validateTargets(values[0],values[1],limit,readProtections());
    }
    setOutput(enabled,limit);
}

}
