#pragma once
#include "modbus.hpp"
#include "display.hpp"
#include "resources.hpp"
#include "connection.hpp"
#include <wx/wx.h>
#include <wx/bmpbndl.h>
#include <wx/statbmp.h>
#include <wx/combobox.h>
#include <wx/spinctrl.h>
#include <filesystem>
#include <functional>
#include <set>
#include <cstring>
#include "serial_path.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

class PowerButton : public wxButton {
    sk150::ConnectionState shown_=sk150::ConnectionState::Disconnected;
    bool phase_=false, initialized_=false,plug_=true;
public:
    explicit PowerButton(wxWindow* parent,bool plug=true)
        : wxButton(parent,wxID_ANY,wxEmptyString,wxDefaultPosition,parent->FromDIP(wxSize(72,72)),wxBU_EXACTFIT),plug_(plug) {
        SetMinSize(parent->FromDIP(wxSize(72,72)));
        SetState(sk150::ConnectionState::Disconnected,false);
    }
    void SetState(sk150::ConnectionState state, bool phase) {
        if(initialized_&&state==shown_&&(state!=sk150::ConnectionState::Error||phase==phase_)) return;
        shown_=state; phase_=phase; initialized_=true;
        const char* colour=state==sk150::ConnectionState::Connected ? "#228b44" :
                           state==sk150::ConnectionState::Error&&!phase ? "#782929" : "#c82a32";
        // Native raised frame around a coloured, rounded vector face.
        const wxString path=plug_ ?
            wxString("M20 11 V21 M36 11 V21 M17 21 H39 V26 C39 34 34 38 28 38 C22 38 17 34 17 26 Z M28 38 V47") :
            wxString("M19 16 A17 17 0 1 0 37 16 M28 10 V29");
        wxString svg=wxString::Format("<svg xmlns='http://www.w3.org/2000/svg' width='56' height='56' viewBox='0 0 56 56'><rect x='1' y='1' width='54' height='54' rx='7' fill='%s'/><path d='%s' fill='none' stroke='white' stroke-width='4' stroke-linecap='round' stroke-linejoin='round'/></svg>",wxString::FromUTF8(colour),path);
        SetBitmap(wxBitmapBundle::FromSVG(svg.ToUTF8().data(),FromDIP(wxSize(56,56))));
        const wxString hint=state==sk150::ConnectionState::Connected ? wxS("Verbunden · Klicken zum Trennen") :
                         state==sk150::ConnectionState::Error ? wxS("Kommunikationsfehler · Klicken zum Trennen") :
                         state==sk150::ConnectionState::Connecting ? wxS("Verbindung wird aufgebaut · Klicken zum Abbrechen") :
                         state==sk150::ConnectionState::Stopping ? wxS("Verbindung wird getrennt") : wxS("Nicht verbunden · Klicken zum Verbinden");
        SetToolTip(hint); SetName(hint);
        Enable(state!=sk150::ConnectionState::Stopping);
    }
};

class PortDialog : public wxDialog {
    wxComboBox* port_;
    wxChoice *baud_, *bits_, *parity_, *stop_;
    wxSpinCtrl* slave_;
public:
    wxString selected;
    sk150::SerialSettings selectedSettings;
    PortDialog(wxWindow* parent,const wxString& current,sk150::SerialSettings settings={})
        : wxDialog(parent,wxID_ANY,wxS("Schnittstelle auswählen"),wxDefaultPosition,wxDefaultSize,wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER) {
        auto outer=new wxBoxSizer(wxVERTICAL);
        outer->Add(new wxStaticText(this,wxID_ANY,wxS("Serielle Schnittstelle auswählen oder Pfad eingeben:")),0,wxALL,14);
        auto row=new wxBoxSizer(wxHORIZONTAL);
        port_=new wxComboBox(this,wxID_ANY,current,wxDefaultPosition,FromDIP(wxSize(370,-1)));
        row->Add(port_,1,wxRIGHT,8); auto refresh=new wxButton(this,wxID_ANY,"Aktualisieren"); row->Add(refresh);
        outer->Add(row,0,wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM,14);
        auto grid=new wxFlexGridSizer(2,10,18);
        auto addChoice=[&](const wxString& label,const wxArrayString& choices,const wxString& name) {
            grid->Add(new wxStaticText(this,wxID_ANY,label),0,wxALIGN_CENTER_VERTICAL);
            auto choice=new wxChoice(this,wxID_ANY,wxDefaultPosition,FromDIP(wxSize(200,-1)),choices);
            choice->SetName(name); grid->Add(choice,0,wxEXPAND); return choice;
        };
        baud_=addChoice("Baudrate:",{"1200","2400","4800","9600","19200","38400","57600","115200","230400"},"baud");
        baud_->SetStringSelection(wxString::Format("%u",settings.baud));
        bits_=addChoice("Datenbits:",{"5","6","7","8"},"data-bits"); bits_->SetSelection(settings.dataBits-5);
        parity_=addChoice(wxS("Parität:"),{"Keine (N)","Gerade (E)","Ungerade (O)"},"parity"); parity_->SetSelection(static_cast<int>(settings.parity));
        stop_=addChoice("Stoppbits:",{"1","2"},"stop-bits"); stop_->SetSelection(settings.stopBits-1);
        grid->Add(new wxStaticText(this,wxID_ANY,"Modbus-Slave-Adresse:"),0,wxALIGN_CENTER_VERTICAL);
        slave_=new wxSpinCtrl(this,wxID_ANY,wxEmptyString,wxDefaultPosition,FromDIP(wxSize(200,-1)),wxSP_ARROW_KEYS,1,247,settings.slave);
        slave_->SetName("slave"); grid->Add(slave_);
        outer->Add(grid,0,wxLEFT|wxRIGHT|wxBOTTOM,14);
        outer->Add(new wxStaticText(this,wxID_ANY,wxS("Nur die Verbindung am Rechner wird konfiguriert.\nDie Geräteeinstellungen bleiben unverändert.\nSK150 bisher: 115200 Baud, 8N1, Slave 1.")),0,wxLEFT|wxRIGHT|wxBOTTOM,14);
        outer->Add(CreateSeparatedButtonSizer(wxOK|wxCANCEL),0,wxEXPAND|wxALL,12);
        SetSizerAndFit(outer);
        if(auto cancel=wxDynamicCast(FindWindow(wxID_CANCEL),wxButton)) cancel->SetLabel("Abbrechen");
        auto scan=[this] {
            const auto value=port_->GetValue(); std::set<std::string> ports;
#ifdef _WIN32
            std::vector<char> names(65536);
            if(QueryDosDeviceA(nullptr,names.data(),static_cast<DWORD>(names.size()))) {
                for(const char* name=names.data(); *name; name+=std::strlen(name)+1)
                    if(sk150::validSerialPath(name)) ports.insert(name);
            }
#else
            std::error_code error;
            for(const char* directory:{"/dev","/dev/serial/by-id"}) {
                for(std::filesystem::directory_iterator it(directory,error),end; !error&&it!=end; it.increment(error)) {
                    auto name=it->path().filename().string();
                    if(std::string(directory)!="/dev"||name.rfind("ttyUSB",0)==0||name.rfind("ttyACM",0)==0||name.rfind("ttyS",0)==0)
                        ports.insert(it->path().string());
                }
                error.clear();
            }
#endif
            if(!value.empty()) ports.insert(value.ToStdString());
            port_->Clear(); for(const auto& path:ports) port_->Append(wxString::FromUTF8(path));
            port_->SetValue(value);
        };
        scan(); refresh->Bind(wxEVT_BUTTON,[scan](wxCommandEvent&){scan();});
        Bind(wxEVT_BUTTON,[this](wxCommandEvent&){
            selected=port_->GetValue(); selected.Trim().Trim(false);
            if(!sk150::validSerialPath(selected.ToStdString())) {
                wxMessageBox(wxS("Bitte eine gültige Schnittstelle angeben (Windows: COM3; Linux: /dev/ttyUSB0)."),"Schnittstelle",wxOK|wxICON_WARNING,this); return;
            }
            unsigned long baud=0,bits=0,stop=0;
            if(!baud_->GetStringSelection().ToULong(&baud)||!bits_->GetStringSelection().ToULong(&bits)||!stop_->GetStringSelection().ToULong(&stop)||parity_->GetSelection()==wxNOT_FOUND) return;
            selectedSettings.baud=baud; selectedSettings.dataBits=bits; selectedSettings.stopBits=stop;
            selectedSettings.parity=static_cast<sk150::Parity>(parity_->GetSelection()); selectedSettings.slave=slave_->GetValue();
            try { selectedSettings.validate(); }
            catch(const std::exception& e) { wxMessageBox(wxString::FromUTF8(e.what()),"Verbindungsparameter",wxOK|wxICON_WARNING,this); return; }
            EndModal(wxID_OK);
        },wxID_OK);
    }
};

class LimitDialog : public wxDialog {
    wxCheckBox* enabled_;
    wxTextCtrl* maximum_;
public:
    sk150::VoltageLimit value;
    LimitDialog(wxWindow* parent,sk150::VoltageLimit current)
        : wxDialog(parent,wxID_ANY,"Spannungslimit"),value(current) {
        auto outer=new wxBoxSizer(wxVERTICAL);
        enabled_=new wxCheckBox(this,wxID_ANY,"Spannungslimit aktivieren"); enabled_->SetValue(current.enabled);
        outer->Add(enabled_,0,wxALL,14);
        auto row=new wxBoxSizer(wxHORIZONTAL);
        row->Add(new wxStaticText(this,wxID_ANY,"Maximale Spannung:"),0,wxALIGN_CENTER_VERTICAL|wxRIGHT,10);
        maximum_=new wxTextCtrl(this,wxID_ANY,wxString::Format("%.2f",current.centivolts/100.0),wxDefaultPosition,FromDIP(wxSize(100,-1)));
        maximum_->Enable(current.enabled); row->Add(maximum_,0,wxRIGHT,6);
        row->Add(new wxStaticText(this,wxID_ANY,"V"),0,wxALIGN_CENTER_VERTICAL); outer->Add(row,0,wxLEFT|wxRIGHT|wxBOTTOM,14);
        outer->Add(new wxStaticText(this,wxID_ANY,wxS("Ohne eigenes Limit: Gerätebereich bis 36,00 V.\nÄndert keine Spannung und keinen Schutzwert im Gerät.")),0,wxLEFT|wxRIGHT|wxBOTTOM,14);
        outer->Add(CreateSeparatedButtonSizer(wxOK|wxCANCEL),0,wxEXPAND|wxALL,12); SetSizerAndFit(outer);
        if(auto ok=wxDynamicCast(FindWindow(wxID_OK),wxButton)) ok->SetLabel(wxS("Übernehmen"));
        if(auto cancel=wxDynamicCast(FindWindow(wxID_CANCEL),wxButton)) cancel->SetLabel("Abbrechen");
        enabled_->Bind(wxEVT_CHECKBOX,[this](wxCommandEvent&){ maximum_->Enable(enabled_->GetValue()); });
        Bind(wxEVT_BUTTON,[this](wxCommandEvent&){
            try {
                value.enabled=enabled_->GetValue();
                // Retain the last valid numeric limit when disabling it.
                if(value.enabled) value.centivolts=sk150::parseVoltage(maximum_->GetValue().ToStdString());
                value.maximum(); EndModal(wxID_OK);
            } catch(const std::exception& e) { wxMessageBox(wxString::FromUTF8(e.what()),wxS("Ungültiges Spannungslimit"),wxOK|wxICON_WARNING,this); }
        },wxID_OK);
    }
};

class ProtectionDialog : public wxDialog {
    wxTextCtrl *ovp_,*ocp_;
    wxButton *setOvp_,*setOcp_;
    wxStaticText *current_,*feedback_;
    bool initialized_=false;
public:
    ProtectionDialog(wxWindow* parent,std::function<bool(bool,unsigned)> submit)
        : wxDialog(parent,wxID_ANY,wxS("OVP/OCP · Geräteschutz M0"),wxDefaultPosition,wxDefaultSize,wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER) {
        auto outer=new wxBoxSizer(wxVERTICAL);
        current_=new wxStaticText(this,wxID_ANY,wxS("Gerätewerte noch nicht verfügbar")); outer->Add(current_,0,wxALL,14);
        auto grid=new wxFlexGridSizer(4,10,10);
        grid->Add(new wxStaticText(this,wxID_ANY,"OVP:"),0,wxALIGN_CENTER_VERTICAL);
        ovp_=new wxTextCtrl(this,wxID_ANY,"",wxDefaultPosition,FromDIP(wxSize(100,-1))); grid->Add(ovp_);
        grid->Add(new wxStaticText(this,wxID_ANY,wxS("V (0–38,00)")),0,wxALIGN_CENTER_VERTICAL);
        setOvp_=new wxButton(this,wxID_ANY,"OVP setzen"); grid->Add(setOvp_);
        grid->Add(new wxStaticText(this,wxID_ANY,"OCP:"),0,wxALIGN_CENTER_VERTICAL);
        ocp_=new wxTextCtrl(this,wxID_ANY,"",wxDefaultPosition,FromDIP(wxSize(100,-1))); grid->Add(ocp_);
        grid->Add(new wxStaticText(this,wxID_ANY,wxS("A (0–7,200)")),0,wxALIGN_CENTER_VERTICAL);
        setOcp_=new wxButton(this,wxID_ANY,"OCP setzen"); grid->Add(setOcp_);
        outer->Add(grid,0,wxLEFT|wxRIGHT|wxBOTTOM,14);
        outer->Add(new wxStaticText(this,wxID_ANY,wxS("Abschaltschwellen in Datengruppe M0.\nAktive Gruppe und Schutzwirkung am Gerät prüfen.\nDie normale CC-Strombegrenzung wird nicht verändert.")),0,wxLEFT|wxRIGHT|wxBOTTOM,14);
        feedback_=new wxStaticText(this,wxID_ANY,wxS("Nur Setzen schreibt ans Gerät."),wxDefaultPosition,FromDIP(wxSize(560,46)),wxST_NO_AUTORESIZE);
        outer->Add(feedback_,0,wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM,14);
        outer->Add(CreateSeparatedButtonSizer(wxCLOSE),0,wxEXPAND|wxALL,12); SetSizerAndFit(outer);
        if(auto close=wxDynamicCast(FindWindow(wxID_CLOSE),wxButton)) close->SetLabel(wxS("Schließen"));
        setOvp_->Disable(); setOcp_->Disable();
        auto send=[this,submit](bool ovp) {
            try {
                unsigned value=ovp ? sk150::parseOvp(ovp_->GetValue().ToStdString()) : sk150::parseOcp(ocp_->GetValue().ToStdString());
                if(submit(ovp,value)) { setOvp_->Disable(); setOcp_->Disable(); ovp_->Disable(); ocp_->Disable(); feedback_->SetLabel(wxS("Schreibauftrag läuft …")); }
                else feedback_->SetLabel(wxS("Nicht bereit: Verbindung und Gerätewerte prüfen."));
            } catch(const std::exception& e) { wxMessageBox(wxString::FromUTF8(e.what()),"Eingabe abgelehnt",wxOK|wxICON_WARNING,this); }
        };
        setOvp_->Bind(wxEVT_BUTTON,[send](wxCommandEvent&){send(true);});
        setOcp_->Bind(wxEVT_BUTTON,[send](wxCommandEvent&){send(false);});
        Bind(wxEVT_BUTTON,[this](wxCommandEvent&){EndModal(wxID_CLOSE);},wxID_CLOSE);
    }
    void RefreshValues(bool valid,bool busy,const std::array<uint16_t,2>& values,const wxString& result) {
        if(valid) {
            current_->SetLabel(wxString::FromUTF8("Gelesen aus M0: OVP ")+sk150::measurementLabel(values[0]/100.0,false,"V")+
                               "   /   OCP "+sk150::measurementLabel(values[1]/1000.0,true,"A"));
            if(!initialized_) {
                ovp_->ChangeValue(wxString::Format("%.2f",values[0]/100.0));
                ocp_->ChangeValue(wxString::Format("%.3f",values[1]/1000.0)); initialized_=true;
            }
        } else current_->SetLabel("OVP/OCP (M0): unbekannt oder veraltet");
        setOvp_->Enable(valid&&!busy); setOcp_->Enable(valid&&!busy);
        ovp_->Enable(!busy); ocp_->Enable(!busy);
        feedback_->SetLabel(result); feedback_->Wrap(FromDIP(560));
    }
};

class PowerLimitDialog : public wxDialog {
    wxTextCtrl* watts_;
    wxButton* set_;
    wxStaticText *current_,*feedback_;
    bool initialized_=false;
public:
    PowerLimitDialog(wxWindow* parent,std::function<bool(unsigned)> submit)
        : wxDialog(parent,wxID_ANY,wxS("Leistungsbegrenzung · OPP M0")) {
        auto outer=new wxBoxSizer(wxVERTICAL);
        current_=new wxStaticText(this,wxID_ANY,wxS("OPP-Gerätewert noch nicht verfügbar")); outer->Add(current_,0,wxALL,14);
        auto row=new wxBoxSizer(wxHORIZONTAL);
        row->Add(new wxStaticText(this,wxID_ANY,"Maximale Leistung:"),0,wxALIGN_CENTER_VERTICAL|wxRIGHT,10);
        watts_=new wxTextCtrl(this,wxID_ANY,"",wxDefaultPosition,FromDIP(wxSize(100,-1))); row->Add(watts_,0,wxRIGHT,6);
        row->Add(new wxStaticText(this,wxID_ANY,wxS("W (0–150,0)")),0,wxALIGN_CENTER_VERTICAL|wxRIGHT,12);
        set_=new wxButton(this,wxID_ANY,"OPP setzen"); set_->Disable(); row->Add(set_);
        outer->Add(row,0,wxLEFT|wxRIGHT|wxBOTTOM,14);
        outer->Add(new wxStaticText(this,wxID_ANY,wxS("Überleistungs-Abschaltschwelle in Datengruppe M0.\nAktive Gruppe und Schutzwirkung am Gerät prüfen.\nKein Konstantleistungsmodus (CW).")),0,wxLEFT|wxRIGHT|wxBOTTOM,14);
        feedback_=new wxStaticText(this,wxID_ANY,wxS("Nur Setzen schreibt ans Gerät."),wxDefaultPosition,FromDIP(wxSize(520,46)),wxST_NO_AUTORESIZE);
        outer->Add(feedback_,0,wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM,14);
        outer->Add(CreateSeparatedButtonSizer(wxCLOSE),0,wxEXPAND|wxALL,12); SetSizerAndFit(outer);
        if(auto close=wxDynamicCast(FindWindow(wxID_CLOSE),wxButton)) close->SetLabel(wxS("Schließen"));
        set_->Bind(wxEVT_BUTTON,[this,submit](wxCommandEvent&) {
            try {
                auto value=sk150::parseOpp(watts_->GetValue().ToStdString());
                if(submit(value)) { set_->Disable(); watts_->Disable(); feedback_->SetLabel(wxS("Schreibauftrag läuft …")); }
                else feedback_->SetLabel(wxS("Nicht bereit: Verbindung und Gerätewert prüfen."));
            } catch(const std::exception& e) { wxMessageBox(wxString::FromUTF8(e.what()),wxS("OPP: 0–150,0 W"),wxOK|wxICON_WARNING,this); }
        });
        Bind(wxEVT_BUTTON,[this](wxCommandEvent&){EndModal(wxID_CLOSE);},wxID_CLOSE);
    }
    void RefreshValues(bool valid,bool busy,unsigned value,const wxString& result) {
        if(valid) {
            current_->SetLabel(wxString::FromUTF8("Gelesen aus M0: OPP ")+wxString::Format("%.1f W",value/10.0));
            if(!initialized_) { watts_->ChangeValue(wxString::Format("%.1f",value/10.0)); initialized_=true; }
        } else current_->SetLabel("OPP (M0): unbekannt oder veraltet");
        set_->Enable(valid&&!busy); watts_->Enable(!busy);
        feedback_->SetLabel(result); feedback_->Wrap(FromDIP(520));
    }
};

inline void ShowLicense(wxWindow* parent) {
    wxDialog dialog(parent,wxID_ANY,"Softwarelizenz und Logohinweis",wxDefaultPosition,wxDefaultSize,wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER);
    auto outer=new wxBoxSizer(wxVERTICAL);
    auto text=new wxTextCtrl(&dialog,wxID_ANY,wxString::FromUTF8(sk150::SoftwareLicense)+"\n\n"+wxString::FromUTF8(sk150::LicenseNotice),
                             wxDefaultPosition,dialog.FromDIP(wxSize(660,440)),wxTE_MULTILINE|wxTE_READONLY|wxHSCROLL);
    outer->Add(text,1,wxEXPAND|wxALL,12); outer->Add(dialog.CreateSeparatedButtonSizer(wxOK),0,wxEXPAND|wxALL,12);
    dialog.SetSizerAndFit(outer); dialog.ShowModal();
}
inline void ShowAbout(wxWindow* parent) {
    wxDialog dialog(parent,wxID_ANY,wxS("Über XY-SK150"),wxDefaultPosition,wxDefaultSize,wxDEFAULT_DIALOG_STYLE);
    auto outer=new wxBoxSizer(wxVERTICAL);
    auto logoSvg=wxString::FromUTF8(sk150::MdLogoSvg);
    logoSvg.Replace("#000000",dialog.GetForegroundColour().GetAsString(wxC2S_HTML_SYNTAX));
    auto bundle=wxBitmapBundle::FromSVG(logoSvg.ToUTF8().data(),dialog.FromDIP(wxSize(160,90)));
    if(bundle.IsOk()) outer->Add(new wxStaticBitmap(&dialog,wxID_ANY,bundle),0,wxALIGN_CENTER|wxALL,18);
    auto addText=[&](const wxString& text,bool large=false) {
        auto label=new wxStaticText(&dialog,wxID_ANY,text,wxDefaultPosition,wxDefaultSize,wxALIGN_CENTER_HORIZONTAL);
        if(large) label->SetFont(label->GetFont().MakeBold().Scale(1.4));
        outer->Add(label,0,wxALIGN_CENTER|wxLEFT|wxRIGHT|wxBOTTOM,12);
    };
    addText("XY-SK150 Steuerung",true); addText("Version V0.2.1");
    addText(wxS("Steuerung und Messwertanzeige\nfür das XY-SK150-Netzteil."));
    addText(wxS("© 2026 Martin Dietl"));
    addText("dietl.martin@mail.de"); addText(wxS("Martin-Dietl-Softwarelizenz 1.1 · MD-Logo ausgenommen"));
    auto buttons=new wxBoxSizer(wxHORIZONTAL);
    auto license=new wxButton(&dialog,wxID_ANY,wxS("Lizenz anzeigen …"));
    auto close=new wxButton(&dialog,wxID_OK,wxS("Schließen")); close->SetDefault();
    buttons->Add(license,0,wxRIGHT,12); buttons->Add(close);
    outer->Add(buttons,0,wxALIGN_RIGHT|wxALL,16);
    dialog.SetSizerAndFit(outer); dialog.CentreOnParent();
    license->Bind(wxEVT_BUTTON,[&dialog](wxCommandEvent&){ShowLicense(&dialog);});
    dialog.ShowModal();
}
