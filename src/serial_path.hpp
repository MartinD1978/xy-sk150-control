#pragma once
#include <string>
namespace sk150 {
inline const char* defaultSerialPath() {
#ifdef _WIN32
    return "COM3";
#else
    return "/dev/ttyUSB0";
#endif
}
inline bool validSerialPath(std::string path) {
    if(path.empty()||path.find_first_of("\r\n")!=std::string::npos||path.find('\0')!=std::string::npos) return false;
#ifdef _WIN32
    if(path.rfind("\\\\.\\",0)==0) path=path.substr(4);
    if(path.size()<4) return false;
    if((path[0]!='C'&&path[0]!='c')||(path[1]!='O'&&path[1]!='o')||(path[2]!='M'&&path[2]!='m')) return false;
    if(path[3]<'1'||path[3]>'9'||path.size()>8) return false;
    for(size_t n=4;n<path.size();++n) if(path[n]<'0'||path[n]>'9') return false;
    return true;
#else
    return path[0]=='/';
#endif
}
}
