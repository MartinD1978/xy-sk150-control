#include "modbus.hpp"
#include "setpoints.hpp"
#include <iostream>
#include <stdexcept>
static void check(bool v) { if(!v) throw std::runtime_error("Test fehlgeschlagen"); }
template<class F> void rejected(F f) { bool failed=false; try { f(); } catch(const std::exception&) { failed=true; } check(failed); }
int main(int argc,char** argv) {
    try {
        if(argc==3) {
            std::string mode=argv[2];
            sk150::SerialSettings settings;
            if(mode.rfind("serial-",0)==0) { settings.baud=9600; settings.stopBits=2; settings.slave=17; }
            sk150::Port port(argv[1],settings);
            if(mode.rfind("targets-",0)==0) {
                if(mode=="targets-current") port.setTargets(std::nullopt,600);
                else if(mode=="targets-pair"||mode=="targets-lower-current") port.setTargets(1100,600);
                else if(mode=="targets-voltage") port.setTargets(1100,std::nullopt);
                else if(mode=="targets-output-guard") rejected([&]{port.setOutputGuarded(1);});
                else if(mode=="targets-ovp") rejected([&]{port.setTargets(1300,600,{false,0});});
                else if(mode=="targets-ocp") rejected([&]{port.setTargets(1100,800);});
                else if(mode=="targets-limit") rejected([&]{port.setTargets(1201,600);});
                else if(mode=="targets-invalid-current") rejected([&]{port.setTargets(1100,7201,{false,0});});
                else rejected([&]{port.setTargets(1100,600);});
            }
            else if(mode=="read") { auto v=port.readValues(); check(v==std::array<uint16_t,6>{1000,7100,999,1234,1234,2400}); }
            else if(mode=="mode-cv") check(port.readMode()==sk150::RegulationMode::CV);
            else if(mode=="mode-cc") check(port.readMode()==sk150::RegulationMode::CC);
            else if(mode=="mode-invalid"||mode=="mode-timeout") rejected([&]{port.readMode();});
            else if(mode=="write-24") port.setVoltage(2400,{true,2400});
            else if(mode=="write-unlimited") port.setVoltage(3600,{false,0});
            else if(mode=="guard-24") rejected([&]{port.setVoltage(2401,{true,2400});});
            else if(mode=="guard-device") rejected([&]{port.setVoltage(3601,{false,0});});
            else if(mode=="output-24") port.setOutput(1,{true,2400});
            else if(mode=="output-24-high") rejected([&]{port.setOutput(1,{true,2400});});
            else if(mode=="output-unlimited") port.setOutput(1,{false,0});
            else if(mode=="output-device-high") rejected([&]{port.setOutput(1,{false,0});});
            else if(mode.rfind("trip-",0)==0) {
                if(mode=="trip-timeout"||mode=="trip-crc") rejected([&]{port.readProtectionStatus();});
                else check(port.readProtectionStatus()==static_cast<uint16_t>(std::stoul(mode.substr(5))));
            }
            else if(mode=="serial-read") check(port.readValues()[0]==1000);
            else if(mode=="serial-write") port.setVoltage(1200);
            else if(mode=="serial-wrong-slave") rejected([&]{port.readValues();});
            else if(mode=="opp-read") check(port.readOpp()==1250);
            else if(mode=="opp-write") port.setOpp(1000);
            else if(mode=="opp-invalid") rejected([&]{port.readOpp();});
            else if(mode=="opp-guard") rejected([&]{port.setOpp(1501);});
            else if(mode=="opp-mismatch"||mode=="opp-timeout"||mode=="opp-preflight") rejected([&]{port.setOpp(1000);});
            else if(mode=="protection-read") check(port.readProtections()==std::array<uint16_t,2>{3800,7200});
            else if(mode=="protection-invalid") rejected([&]{port.readProtections();});
            else if(mode=="ovp-write") port.setOvp(1250);
            else if(mode=="ocp-write") port.setOcp(750);
            else if(mode=="ovp-mismatch"||mode=="ovp-timeout"||mode=="ovp-preflight") rejected([&]{port.setOvp(1250);});
            else if(mode=="ovp-guard") rejected([&]{port.setOvp(3801);});
            else if(mode=="ocp-guard") rejected([&]{port.setOcp(7201);});
            else if(mode=="output-read") check(port.readOutput());
            else if(mode=="output-read-error") rejected([&]{port.readOutput();});
            else if(mode=="output-on") port.setOutput(1);
            else if(mode=="output-off") port.setOutput(0);
            else if(mode=="output-high"||mode=="output-high-measured"||mode=="output-timeout") rejected([&]{port.setOutput(1);});
            else if(mode=="output-guard") rejected([&]{port.setOutput(2);});
            else if(mode=="output-error") rejected([&]{port.setOutput(0);});
            else if(mode=="write") port.setVoltage(1200);
            else if(mode=="guard") rejected([&]{port.setVoltage(1201);});
            else if(mode=="write-error") rejected([&]{port.setVoltage(1200);});
            else rejected([&]{port.readValues();});
            return 0;
        }
        check(sk150::parseCurrent("7,200")==7200);
        check(sk150::parseCurrent("0.050")==50);
        rejected([]{sk150::parseCurrent("7.201");});
        check(sk150::currentRequest(600)[3]==1);
        rejected([]{sk150::currentRequest(7201);});
        check(sk150::allowed(1200,799,{true,1200},{1300,800}));
        check(!sk150::allowed(1200,800,{true,1200},{1300,800}));
        check(!sk150::allowed(1300,500,{false,0},{1300,800}));
        check(sk150::sliderMaximum(7200,800,50)==15);
        check(sk150::sliderMaximum(7200,750,50)==14);
        check(sk150::sliderMaximum(3600,0,10)==0);
        check(sk150::parseOpp("150,0")==1500);
        check(sk150::parseOpp("12.5")==125);
        check(sk150::parseOpp("0")==0);
        for(auto text:{"150.1","151","1.01","-1","nan","42949672960"}) rejected([&]{sk150::parseOpp(text);});
        for(unsigned value=0;value<=1500;++value) {
            auto b=sk150::oppRequest(value); check(b[1]==6&&b[2]==0&&b[3]==0x55&&unsigned((b[4]<<8)|b[5])==value);
        }
        for(unsigned value:{1501u,65536u,4294967295u}) rejected([&]{sk150::oppRequest(value);});
        for(unsigned baud:{1200u,2400u,4800u,9600u,19200u,38400u,57600u,115200u,230400u}) {
            sk150::SerialSettings settings; settings.baud=baud; settings.validate();
        }
        for(auto invalid:{0,1,2,3,4}) {
            sk150::SerialSettings settings;
            if(invalid==0) settings.slave=0;
            if(invalid==1) settings.slave=248;
            if(invalid==2) settings.dataBits=9;
            if(invalid==3) settings.stopBits=3;
            if(invalid==4) settings.baud=12345;
            rejected([&]{settings.validate();});
        }
        check(sk150::parseOvp("38,00")==3800);
        check(sk150::parseOcp("7.2")==7200);
        check(sk150::parseOcp("0,075")==75);
        for(auto text:{"38.01","1.001","42949672960","-1"}) rejected([&]{sk150::parseOvp(text);});
        for(auto text:{"7.201","8","0.0001","nan","42949672960","-1"}) rejected([&]{sk150::parseOcp(text);});
        for(unsigned v=0;v<=3800;++v) {
            auto b=sk150::ovpRequest(v); check(b[1]==6&&b[2]==0&&b[3]==0x53&&unsigned((b[4]<<8)|b[5])==v);
        }
        for(unsigned v=0;v<=7200;++v) {
            auto b=sk150::ocpRequest(v); check(b[1]==6&&b[2]==0&&b[3]==0x54&&unsigned((b[4]<<8)|b[5])==v);
        }
        for(unsigned v:{3801u,65536u,4294967295u}) rejected([&]{sk150::ovpRequest(v);});
        for(unsigned v:{7201u,65536u,4294967295u}) rejected([&]{sk150::ocpRequest(v);});
        check(sk150::outputRequest(0)==std::vector<uint8_t>({1,6,0,0x12,0,0,0x29,0xcf}));
        check(sk150::outputRequest(1)[5]==1);
        for(unsigned v:{2u,1200u,65536u,4294967295u}) rejected([&]{sk150::outputRequest(v);});
        check(sk150::crc({1,3,0,0,0,6})==0xc8c5);
        for(unsigned v=0;v<=3600;++v) {
            auto b=sk150::voltageRequest(v,{false,0}); check(b.size()==8&&b[0]==1&&b[1]==6&&b[2]==0&&b[3]==0);
            check(unsigned((b[4]<<8)|b[5])==v);
        }
        for(unsigned v:{1201u,65535u,65536u,4294967295u}) rejected([&]{sk150::voltageRequest(v);});
        check(sk150::VoltageLimit{true,1800}.maximum()==1800);
        check(sk150::VoltageLimit{false,0}.maximum()==3600);
        check(sk150::voltageRequest(1800,{true,1800})[4]==7);
        rejected([]{sk150::voltageRequest(1801,{true,1800});});
        rejected([]{sk150::voltageRequest(3601,{false,0});});
        rejected([]{sk150::voltageRequest(0,{true,3601});});
        rejected([]{sk150::voltageRequest(65536,{false,0});});
        check(sk150::voltageRequest(0,{true,0})[5]==0);
        rejected([]{sk150::voltageRequest(1,{true,0});});
        check(sk150::parseVoltage("12.01")==1201);
        check(sk150::parseVoltage("13")==1300);
        check(sk150::parseVoltage("36,00")==3600);
        check(sk150::parseVoltage(" 12,00 ")==1200); check(sk150::parseVoltage("10.5")==1050); check(sk150::parseVoltage("0")==0);
        for(auto s:{"36.01","37","-1","nan","inf","1e1","",".","1.","0.001","1,2.3","42949672960","1 0"}) rejected([&]{sk150::parseVoltage(s);});
        std::cout<<"CRC, alle erlaubten Sollwerte, einstellbares Limit und Gerätebereich und Eingabeprüfung: OK\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
