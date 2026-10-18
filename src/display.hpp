#pragma once
#include <wx/string.h>
#include "modbus.hpp"

namespace sk150 {
// Keep printf format strings ASCII. Decode UTF-8 UI text explicitly so that
// wxFormatString never depends on the process locale for non-ASCII literals.
inline wxString measurementLabel(double value, bool threeDecimals, const char* unit) {
    return wxString::Format(threeDecimals ? "%.3f" : "%.2f", value)
           + " " + wxString::FromUTF8(unit);
}
inline wxString protectionLabel(unsigned code, bool valid) {
    if(!valid) return "Schutzstatus: unbekannt / veraltet";
    if(code==0) return wxString::FromUTF8("Schutzstatus: keine Auslösung gemeldet");
    const char* names[]={"", "OVP · Überspannung", "OCP · Überstrom", "OPP · Überleistung",
                         "LVP · Unterspannung", "OAH", "OHP", "OTP · Übertemperatur", "OEP", "OWH", "ICP", "ETP"};
    if(code<12) return wxString::FromUTF8("Schutz ausgelöst: ")+wxString::FromUTF8(names[code]);
    return wxString::FromUTF8("Unbekannter Schutzcode: ")+wxString::Format("%u",code);
}
inline wxString modeLabel(RegulationMode mode, bool valid) {
    if(!valid) return "CV/CC: unbekannt";
    return wxString::FromUTF8(mode==RegulationMode::CV ? "CV · Konstantspannung" : "CC · Konstantstrom");
}
}
