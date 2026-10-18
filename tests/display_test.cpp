#include "display.hpp"
#include <clocale>
#include <iostream>
#include <stdexcept>
static void check(bool ok) { if(!ok) throw std::runtime_error("Anzeigetest fehlgeschlagen"); }
int main() {
    // The C locale reproduces the old non-ASCII wxFormatString crash without GTK.
    for(const char* locale : {"C", "C.UTF-8"}) {
        if(!std::setlocale(LC_ALL,locale)) continue;
        check(sk150::measurementLabel(10,false,"V")=="10.00 V");
        check(sk150::measurementLabel(7.1,true,"A")=="7.100 A");
        check(sk150::modeLabel(sk150::RegulationMode::CV,true)==wxString::FromUTF8("CV · Konstantspannung"));
        check(sk150::modeLabel(sk150::RegulationMode::CC,true)==wxString::FromUTF8("CC · Konstantstrom"));
        check(sk150::modeLabel(sk150::RegulationMode::CC,false)=="CV/CC: unbekannt");
        check(sk150::protectionLabel(0,true)==wxString::FromUTF8("Schutzstatus: keine Auslösung gemeldet"));
        check(sk150::protectionLabel(1,true).Contains("OVP"));
        check(sk150::protectionLabel(2,true).Contains("OCP"));
        check(sk150::protectionLabel(99,true).Contains("99"));
        check(sk150::protectionLabel(1,false)=="Schutzstatus: unbekannt / veraltet");
        std::cout << "Messwertanzeige / " << locale << ": OK\n";
    }
}
