#include "modbus.hpp"
#include "display.hpp"
#include "dialogs.hpp"
#include "controls.hpp"
#include <wx/statline.h>
#include <wx/fileconf.h>
#include <wx/stdpaths.h>
#include <wx/wx.h>
#include <wx/timer.h>
#include <wx/dcbuffer.h>
#include <wx/graphics.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

// Native child controls retain accessibility; only the card surface is painted.
class MeasurementCard : public wxPanel {
    wxColour fill_;
public:
    explicit MeasurementCard(wxWindow* parent)
        : wxPanel(parent,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxBORDER_NONE) {
        fill_=wxColour(0,0,0);
        SetBackgroundColour(fill_);
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT,[this](wxPaintEvent&) {
            wxAutoBufferedPaintDC dc(this);
            dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour())); dc.Clear();
            const auto size=GetClientSize();
            const double edge=FromDIP(3), shadow=FromDIP(5), radius=FromDIP(12);
            const double width=size.x-shadow-1, height=size.y-shadow-1;
            if(width<=2*edge||height<=2*edge) return;
            const auto shadowColour=GetParent()->GetBackgroundColour().ChangeLightness(65);
            std::unique_ptr<wxGraphicsContext> graphics(wxGraphicsContext::Create(dc));
            if(graphics) {
                graphics->SetPen(*wxTRANSPARENT_PEN);
                graphics->SetBrush(wxBrush(shadowColour));
                graphics->DrawRoundedRectangle(shadow,shadow,width,height,radius);
                // A dark graphite bevel and drop shadow raise the black display without a white outline.
                graphics->SetBrush(graphics->CreateLinearGradientBrush(0,0,width,height,
                                      wxColour(80,80,80),wxColour(22,22,22)));
                graphics->DrawRoundedRectangle(0.5,0.5,width,height,radius);
                graphics->SetBrush(wxBrush(fill_));
                graphics->DrawRoundedRectangle(edge,edge,width-2*edge,height-2*edge,radius-edge);
            } else {
                dc.SetPen(*wxTRANSPARENT_PEN); dc.SetBrush(wxBrush(shadowColour));
                dc.DrawRoundedRectangle(shadow,shadow,width,height,radius);
                dc.SetBrush(wxBrush(wxColour(80,80,80)));
                dc.DrawRoundedRectangle(0,0,width,height,radius);
                dc.SetBrush(wxBrush(wxColour(22,22,22)));
                dc.DrawRoundedRectangle(edge,edge,width-edge,height-edge,radius-edge);
                dc.SetBrush(wxBrush(fill_));
                dc.DrawRoundedRectangle(edge,edge,width-2*edge,height-2*edge,radius-edge);
            }
        });
        Bind(wxEVT_SIZE,[this](wxSizeEvent& event){ Refresh(false); event.Skip(); });
    }
};

class InfoCard : public wxPanel {
public:
    explicit InfoCard(wxWindow* parent)
        : wxPanel(parent,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxBORDER_NONE) {
        SetBackgroundColour(parent->GetBackgroundColour().ChangeLightness(106));
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT,[this](wxPaintEvent&) {
            wxAutoBufferedPaintDC dc(this);
            const auto surface=GetParent()->GetBackgroundColour();
            dc.SetBackground(wxBrush(surface)); dc.Clear();
            const auto size=GetClientSize();
            const double shadow=FromDIP(4),radius=FromDIP(12);
            const double width=size.x-shadow-1,height=size.y-shadow-1;
            if(width<2*radius||height<2*radius) return;
            std::unique_ptr<wxGraphicsContext> graphics(wxGraphicsContext::Create(dc));
            if(graphics) {
                graphics->SetPen(*wxTRANSPARENT_PEN);
                graphics->SetBrush(wxBrush(surface.ChangeLightness(94)));
                graphics->DrawRoundedRectangle(shadow,shadow,width,height,radius);
                graphics->SetBrush(wxBrush(surface.ChangeLightness(89)));
                graphics->DrawRoundedRectangle(FromDIP(2),FromDIP(2),width,height,radius);
                graphics->SetBrush(wxBrush(GetBackgroundColour()));
                graphics->DrawRoundedRectangle(0.5,0.5,width,height,radius);
            } else {
                dc.SetPen(*wxTRANSPARENT_PEN); dc.SetBrush(wxBrush(surface.ChangeLightness(90)));
                dc.DrawRoundedRectangle(shadow,shadow,width,height,radius);
                dc.SetBrush(wxBrush(GetBackgroundColour()));
                dc.DrawRoundedRectangle(0,0,width,height,radius);
            }
        });
        Bind(wxEVT_SIZE,[this](wxSizeEvent& event){Refresh(false);event.Skip();});
    }
};

struct Command {
    enum Kind { Voltage, Current, Preset, Output, Ovp, Ocp, Opp } kind;
    unsigned value;
    sk150::VoltageLimit limit;
    unsigned current=0;
};
struct Shared {
    std::mutex mutex;
    std::condition_variable wake;
    bool stop=false, healthy=false, busy=false, attemptedRead=false;
    std::optional<Command> command;
    std::array<uint16_t,6> values{};
    bool output=false, outputHealthy=false, modeHealthy=false;
    std::array<uint16_t,2> protections{};
    bool protectionsHealthy=false, tripHealthy=false, oppHealthy=false;
    uint16_t opp=0;
    uint16_t trip=0;
    sk150::RegulationMode mode=sk150::RegulationMode::CV;
    std::string outputStatus="Ausgang: unbekannt", communicationError;
    std::string status="Verbinde …", result="Noch kein Schreibauftrag.";
    std::chrono::steady_clock::time_point updated{};
    std::atomic<bool> done{false};
};
static void run(std::shared_ptr<Shared> s, std::string path, sk150::SerialSettings settings) {
    try {
        sk150::Port port(path,settings);
        for(;;) {
            std::optional<Command> command;
            { std::lock_guard<std::mutex> lock(s->mutex); if(s->stop) break; command=s->command; s->command.reset(); }
            bool attempted=false;
            try {
                // Read before any write; discard the command on failed preflight.
                auto values=port.readValues();
                if(command) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    { std::lock_guard<std::mutex> lock(s->mutex); if(s->stop) break; }
                    attempted=true;
                    if(command->kind==Command::Voltage) port.setTargets(command->value,std::nullopt,command->limit);
                    else if(command->kind==Command::Current) port.setTargets(std::nullopt,command->value,command->limit);
                    else if(command->kind==Command::Preset) port.setTargets(command->value,command->current,command->limit);
                    else if(command->kind==Command::Output) port.setOutputGuarded(command->value,command->limit);
                    else if(command->kind==Command::Ovp) port.setOvp(command->value);
                    else if(command->kind==Command::Ocp) port.setOcp(command->value);
                    else port.setOpp(command->value);
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    values=port.readValues();
                    if(command->kind==Command::Voltage&&values[0]!=command->value)
                        throw std::runtime_error("Rücklesen des Spannungssollwerts weicht ab");
                }
                const auto measuredAt=std::chrono::steady_clock::now();
                bool output=false, outputHealthy=false, modeHealthy=false;
                std::string outputStatus, communicationError;
                auto mode=sk150::RegulationMode::CV;
                try {
                    output=port.readOutput(); outputHealthy=true;
                    outputStatus=output ? "Ausgang: EIN" : "Ausgang: AUS";
                } catch(const std::exception& e) {
                    outputStatus=std::string("Ausgang: unbekannt · ")+e.what(); communicationError=e.what();
                }
                if(command&&command->kind==Command::Output&&(!outputHealthy||output!=(command->value==1)))
                    throw std::runtime_error("Ausgangsschaltung nicht durch Rücklesen bestätigt");
                // CV/CC is a read-only device status, never inferred from the load.
                try { mode=port.readMode(); modeHealthy=true; } catch(const std::exception& e) { communicationError=e.what(); }
                std::array<uint16_t,2> protections{};
                bool protectionsHealthy=false;
                try { protections=port.readProtections(); protectionsHealthy=true; } catch(const std::exception& e) { communicationError=e.what(); }
                bool tripHealthy=false;
                uint16_t trip=0;
                try { trip=port.readProtectionStatus(); tripHealthy=true; } catch(const std::exception& e) { communicationError=e.what(); }
                bool oppHealthy=false; uint16_t opp=0;
                try { opp=port.readOpp(); oppHealthy=true; } catch(const std::exception& e) { communicationError=e.what(); }
                std::lock_guard<std::mutex> lock(s->mutex);
                s->attemptedRead=true; s->communicationError=communicationError;
                s->opp=opp; s->oppHealthy=oppHealthy;
                s->trip=trip; s->tripHealthy=tripHealthy;
                s->protections=protections; s->protectionsHealthy=protectionsHealthy;
                s->output=output; s->outputHealthy=outputHealthy; s->outputStatus=outputStatus;
                s->mode=mode; s->modeHealthy=modeHealthy;
                s->values=values; s->updated=measuredAt; s->healthy=true;
                s->status="Verbunden · Messwerte aktuell";
                if(command) {
                    s->busy=false;
                    s->result=command->kind==Command::Voltage ? "Spannung geschrieben und zurückgelesen." :
                              command->kind==Command::Current ? "Strombegrenzung geschrieben und zurückgelesen." :
                              command->kind==Command::Preset ? "Speicherplatz geschrieben und zurückgelesen." :
                              command->kind==Command::Output ? "Ausgang geschaltet und zurückgelesen." :
                              command->kind==Command::Ovp ? "OVP (M0) geschrieben und zurückgelesen." :
                              command->kind==Command::Ocp ? "OCP (M0) geschrieben und zurückgelesen." :
                                                           "OPP (M0) geschrieben und zurückgelesen.";
                }
            } catch(const std::exception& e) {
                std::lock_guard<std::mutex> lock(s->mutex);
                s->oppHealthy=false; s->attemptedRead=true; s->tripHealthy=false; s->protectionsHealthy=false; s->healthy=false; s->outputHealthy=false; s->modeHealthy=false; s->status=e.what();
                if(command) {
                    s->busy=false;
                    s->result=attempted ? "Schreibauftrag unbestätigt; Gerätewert prüfen! Keine Wiederholung." : "Schreibauftrag verworfen: Lesen fehlgeschlagen.";
                }
            }
            std::unique_lock<std::mutex> lock(s->mutex);
            s->wake.wait_for(lock,std::chrono::milliseconds(250),[&]{ return s->stop||s->command.has_value(); });
        }
    } catch(const std::exception& e) {
        std::lock_guard<std::mutex> lock(s->mutex);
        s->oppHealthy=false; s->attemptedRead=true; s->tripHealthy=false; s->protectionsHealthy=false; s->healthy=false; s->outputHealthy=false; s->modeHealthy=false; s->status=e.what();
    }
    {
        std::lock_guard<std::mutex> lock(s->mutex);
        s->oppHealthy=false; s->attemptedRead=true; s->tripHealthy=false; s->protectionsHealthy=false; s->healthy=false; s->outputHealthy=false; s->modeHealthy=false;
        if(s->stop) s->status="Getrennt";
    }
    s->done=true;
}
class Window : public wxFrame {
    friend class GuiTestApp;
    enum { MenuPort=wxID_HIGHEST+1, MenuLimit, MenuProtections, MenuPowerLimit, MenuDevice };
    wxTextCtrl *voltage_,*current_;
    MarkedSlider *voltageSlider_,*currentSlider_;
    PowerButton* outputButton_;
    wxButton* currentSet_;
    std::array<sk150::Preset,10> presets_{};
    std::array<wxButton*,10> presetButtons_{};
    wxDialog* deviceDialog_=nullptr;
    wxStaticText* inputVoltage_=nullptr;
    PowerButton* connect_;
    wxButton *set_;
    wxPanel* infoBox_;
    wxStaticText *tripStatus_, *lastTrip_;
    int measurementHeight_=132;
    wxStaticText *status_, *result_, *output_, *cv_, *cc_, *limitStatus_;
    ProtectionDialog* protectionDialog_=nullptr;
    PowerLimitDialog* powerLimitDialog_=nullptr;
    std::optional<uint16_t> lastTripCode_;
    sk150::VoltageLimit limit_;
    sk150::SerialSettings serial_;
    sk150::ConnectionState connectionState_=sk150::ConnectionState::Disconnected;
    wxString portPath_="/dev/ttyUSB0";
    std::unique_ptr<wxFileConfig> preferences_;
    int displayedMode_=-2;
    std::array<wxStaticText*,6> readings_{};
    std::array<wxStaticText*,6> unitLabels_{};
    wxTimer timer_{this};
    std::shared_ptr<Shared> state_;
    std::thread worker_;
    bool closing_=false;

    void Stop() {
        if(state_) {
            std::lock_guard<std::mutex> lock(state_->mutex);
            state_->stop=true; state_->command.reset(); state_->wake.notify_all();
        }
    }
    void UpdateMode(sk150::RegulationMode mode, bool valid) {
        const int active=valid ? (mode==sk150::RegulationMode::CV ? 0 : 1) : -1;
        if(active==displayedMode_) return;
        displayedMode_=active;
        const auto inactive=wxColour(170,170,170);
        int index=0;
        for(auto label:{cv_,cc_}) {
            const bool selected=index++==active;
            auto font=label->GetFont();
            font.SetWeight(selected ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL);
            label->SetFont(font);
            label->SetForegroundColour(selected ? wxColour(235,65,65) : inactive);
            label->SetToolTip(valid ? wxS("Aktiven Regelungsmodus meldet das Gerät") : wxS("Regelungsmodus unbekannt oder Messwerte veraltet"));
            label->Refresh();
        }
    }
    void DisableWrites() { set_->Disable(); currentSet_->Disable(); outputButton_->Disable(); for(auto button:presetButtons_) if(button) button->Disable(); }
    bool Queue(Command::Kind kind, unsigned value,unsigned current=0) {
        try {
            // AUS remains available even while the limit field contains invalid text.
            auto limit=limit_;
            if(kind==Command::Voltage) limit.validate(value);
            if(!state_) return false;
            std::lock_guard<std::mutex> lock(state_->mutex);
            if(closing_||state_->done||state_->stop||!state_->healthy||state_->busy||
               std::chrono::steady_clock::now()-state_->updated>std::chrono::milliseconds(1500)) return false;
            if(kind==Command::Voltage||kind==Command::Current||kind==Command::Preset) {
                if(!state_->protectionsHealthy) return false;
                sk150::validateTargets(kind==Command::Current?state_->values[0]:value,
                    kind==Command::Voltage?state_->values[1]:(kind==Command::Current?value:current),limit,state_->protections);
            }
            if((kind==Command::Ovp||kind==Command::Ocp)&&!state_->protectionsHealthy) return false;
            if(kind==Command::Opp&&!state_->oppHealthy) return false;
            if(kind==Command::Output) {
                if(!state_->outputHealthy) return false;
                if(value) {
                    if(!state_->protectionsHealthy) return false;
                    sk150::validateTargets(state_->values[0],state_->values[1],limit,state_->protections);
                    limit.validate(state_->values[2]);
                }
            }
            state_->command=Command{kind,value,limit,current}; state_->busy=true;
            state_->result="Schreibauftrag läuft …";
            DisableWrites(); GetMenuBar()->Enable(MenuLimit,false); state_->wake.notify_all();
            return true;
        } catch(const std::exception& e) {
            wxMessageBox(wxString::FromUTF8(e.what()),"Eingabe abgelehnt",wxOK|wxICON_WARNING,this);
        }
        return false;
    }
    void ToggleConnection() {
        if(closing_) return;
        if(state_) {
            std::lock_guard<std::mutex> lock(state_->mutex);
            if(!state_->stop) {
                state_->stop=true; state_->command.reset(); state_->wake.notify_all();
                if(state_->done) state_->status="Getrennt";
                DisableWrites(); return;
            }
            if(!state_->done) return;
        }
        if(worker_.joinable()) worker_.join();
        state_=std::make_shared<Shared>();
        lastTripCode_.reset(); lastTrip_->SetLabel("");
        for(auto reading:readings_) if(reading) reading->SetLabel(wxS("—"));
        UpdateMode(sk150::RegulationMode::CV,false);
        try { worker_=std::thread(run,state_,portPath_.ToStdString(),serial_); }
        catch(const std::exception& e) { state_->status=e.what(); state_->done=true; }
        Tick();
    }
    void SelectPort() {
        PortDialog dialog(this,portPath_,serial_);
        if(dialog.ShowModal()==wxID_OK) {
            portPath_=dialog.selected; serial_=dialog.selectedSettings;
            SetTitle(wxString::FromUTF8("XY-SK150 · ")+portPath_);
            preferences_->Write("serial_port",portPath_);
            preferences_->Write("baud",static_cast<long>(serial_.baud));
            preferences_->Write("data_bits",static_cast<long>(serial_.dataBits));
            preferences_->Write("parity",static_cast<long>(serial_.parity));
            preferences_->Write("stop_bits",static_cast<long>(serial_.stopBits));
            preferences_->Write("slave",static_cast<long>(serial_.slave));
            if(!preferences_->Flush()) wxMessageBox("Die Schnittstelle konnte nicht gespeichert werden.","Einstellungen",wxOK|wxICON_WARNING,this);
        }
    }
    void ConfigureLimit() {
        LimitDialog dialog(this,limit_);
        if(dialog.ShowModal()==wxID_OK) { limit_=dialog.value; Tick(); }
    }
    void ConfigureProtections() {
        ProtectionDialog dialog(this,[this](bool ovp,unsigned value){return Queue(ovp ? Command::Ovp : Command::Ocp,value);});
        protectionDialog_=&dialog;
        Tick(); dialog.CentreOnParent(); dialog.ShowModal();
        protectionDialog_=nullptr;
    }
    void ConfigurePowerLimit() {
        PowerLimitDialog dialog(this,[this](unsigned value){return Queue(Command::Opp,value);});
        powerLimitDialog_=&dialog;
        Tick(); dialog.CentreOnParent(); dialog.ShowModal();
        powerLimitDialog_=nullptr;
    }
    void ConfigurePresets() {
        PresetDialog dialog(this,presets_);
        if(dialog.ShowModal()!=wxID_OK) return;
        presets_=dialog.values;
        for(size_t i=0;i<presets_.size();++i) {
            const auto prefix=wxString::Format("preset_%zu_",i+1);
            preferences_->Write(prefix+"enabled",presets_[i].configured);
            preferences_->Write(prefix+"voltage",static_cast<long>(presets_[i].voltage));
            preferences_->Write(prefix+"current",static_cast<long>(presets_[i].current));
        }
        if(!preferences_->Flush()) wxMessageBox(wxS("Speicherplätze konnten nicht dauerhaft gespeichert werden."),"Speichern",wxOK|wxICON_WARNING,this);
        Tick();
    }
    void ShowDevice() {
        wxDialog dialog(this,wxID_ANY,wxS("Gerät · Informationen"));
        auto box=new wxBoxSizer(wxVERTICAL);
        inputVoltage_=new wxStaticText(&dialog,wxID_ANY,wxS("Eingangsspannung: unbekannt"),wxDefaultPosition,FromDIP(wxSize(360,30)));
        box->Add(inputVoltage_,0,wxALL,18);box->Add(dialog.CreateButtonSizer(wxOK),0,wxALIGN_RIGHT|wxALL,12);dialog.SetSizerAndFit(box);
        deviceDialog_=&dialog;Tick();dialog.CentreOnParent();dialog.ShowModal();deviceDialog_=nullptr;inputVoltage_=nullptr;
    }
    void Submit(bool voltage) {
        try { Queue(voltage?Command::Voltage:Command::Current,voltage?sk150::parseVoltage(voltage_->GetValue().ToStdString()):sk150::parseCurrent(current_->GetValue().ToStdString())); }
        catch(const std::exception& e) { wxMessageBox(wxString::FromUTF8(e.what()),"Eingabe abgelehnt",wxOK|wxICON_WARNING,this); }
    }
    void UpdatePresets(bool ready,bool valid,const std::array<uint16_t,2>& protections) {
        for(size_t i=0;i<presets_.size();++i) {
            const auto& preset=presets_[i];auto button=presetButtons_[i];
            const auto label=preset.configured?wxString::Format("%zu\n%.2f V\n%.3f A",i+1,preset.voltage/100.0,preset.current/1000.0):wxString::Format("%zu\n---\n---",i+1);
            const bool permitted=preset.configured&&valid&&sk150::allowed(preset.voltage,preset.current,limit_,protections);
            const bool enabled=ready&&permitted;
            const auto background=preset.configured&&!permitted?wxColour(190,48,48):wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
            const auto foreground=preset.configured&&!permitted?*wxWHITE:GetForegroundColour();
            const wxString hint=!preset.configured?wxS("Leer · Über … bearbeiten"):
                !valid?wxS("Gesperrt · OVP/OCP unbekannt oder veraltet"):
                !permitted?wxS("Gesperrt · Spannungslimit oder OVP/OCP überschritten/erreicht"):
                wxS("Klick überträgt Spannung und Strombegrenzung direkt");
            // Polling runs every 100 ms. Leave unchanged native controls alone,
            // especially disabled coloured buttons, to avoid erase/repaint flicker.
            bool changed=false;
            if(button->GetLabel()!=label) {button->SetLabel(label);changed=true;}
            if(button->IsEnabled()!=enabled) {button->Enable(enabled);changed=true;}
            if(button->GetBackgroundColour()!=background) {button->SetBackgroundColour(background);changed=true;}
            if(button->GetForegroundColour()!=foreground) {button->SetForegroundColour(foreground);changed=true;}
            if(button->GetToolTipText()!=hint) button->SetToolTip(hint);
            if(changed) button->Refresh(false);
        }
    }
    void Tick() {
        limitStatus_->SetLabel(limit_.enabled ? wxString::FromUTF8("Limit: ")+sk150::measurementLabel(limit_.centivolts/100.0,false,"V")
                                            : wxString::FromUTF8("Limit aus · Gerätebereich bis 36,00 V"));
        bool busy=false;
        bool controlsReady=false,guardsValid=false; std::array<uint16_t,2> guards{};
        auto connection=sk150::ConnectionState::Disconnected;
        if(state_) {
            if(state_->done&&worker_.joinable()) worker_.join();
            if(closing_&&state_->done) { Destroy(); return; }
            std::lock_guard<std::mutex> lock(state_->mutex);
            bool measured=state_->updated.time_since_epoch().count()!=0;
            bool fresh=measured&&state_->healthy&&!state_->stop&&!state_->done&&
                       std::chrono::steady_clock::now()-state_->updated<std::chrono::milliseconds(1500);
            busy=state_->busy&&!state_->done;
            const bool tripValid=fresh&&state_->tripHealthy;
            tripStatus_->SetLabel(sk150::protectionLabel(state_->trip,tripValid));
            const bool tripped=tripValid&&state_->trip!=0;
            auto tripFont=tripStatus_->GetFont();
            if(tripFont.GetWeight()!=(tripped ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL)) {
                tripFont.SetWeight(tripped ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL);
                tripStatus_->SetFont(tripFont);
            }
            tripStatus_->SetForegroundColour(tripped ? wxColour(190,25,30) : GetForegroundColour());
            if(tripped&&(!lastTripCode_||*lastTripCode_!=state_->trip)) {
                lastTripCode_=state_->trip;
                lastTrip_->SetLabel(wxString::FromUTF8("Zuletzt: ")+sk150::protectionLabel(state_->trip,true));
            }
            if(protectionDialog_) protectionDialog_->RefreshValues(fresh&&state_->protectionsHealthy,busy,state_->protections,wxString::FromUTF8(state_->result));
            if(powerLimitDialog_) powerLimitDialog_->RefreshValues(fresh&&state_->oppHealthy,busy,state_->opp,wxString::FromUTF8(state_->result));
            if(inputVoltage_) inputVoltage_->SetLabel(fresh?wxString::Format("Eingangsspannung: %.2f V",state_->values[5]/100.0):wxS("Eingangsspannung: unbekannt / veraltet"));
            output_->SetLabel(wxString::FromUTF8(fresh ? state_->outputStatus : "Ausgang: unbekannt / veraltet"));
            UpdateMode(state_->mode,fresh&&state_->modeHealthy);
            auto status=state_->healthy ? wxString("Messwerte aktuell") : wxString::FromUTF8(state_->status);
            bool error=!state_->healthy&&state_->attemptedRead&&!state_->stop;
            if(fresh&&(state_->values[0]>limit_.maximum()||state_->values[2]>limit_.maximum()))
                { status=wxString::FromUTF8("Spannung über dem aktiven Maximum · EIN gesperrt"); error=true; }
            else if(measured&&!fresh&&state_->healthy&&!state_->done&&!state_->stop)
                { status=wxString::FromUTF8("Messwerte veraltet"); error=true; }
            if(fresh&&!state_->communicationError.empty()) { status=wxString::FromUTF8("Kommunikationsfehler: ")+wxString::FromUTF8(state_->communicationError); error=true; }
            if(state_->stop) { status=state_->done ? wxString("Bereit") : wxString::FromUTF8("Verbindung wird getrennt …"); error=false; }
            status_->SetLabel(status); status_->SetToolTip(status); status_->Wrap(wxMax(100,infoBox_->GetClientSize().x-FromDIP(30)));
            status_->SetForegroundColour(error ? wxColour(190,25,30) : GetForegroundColour());
            auto statusFont=status_->GetFont();
            if(statusFont.GetWeight()!=(error ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL)) {
                statusFont.SetWeight(error ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL); status_->SetFont(statusFont);
            }
            result_->SetLabel(state_->result=="Noch kein Schreibauftrag." ? wxString{} : wxString::FromUTF8(state_->result));
            result_->SetToolTip(wxString::FromUTF8(state_->result));
            if(measured) {
                const double scales[]={100,1000,100,1000,100,100};
                const char* units[]={"V","A","V","A","W","V"};
                for(size_t i=0;i<6;++i) {
                    const double value=state_->values[i]/scales[i];
                    if(!readings_[i]) continue;
                    readings_[i]->SetLabel(unitLabels_[i] ? wxString::Format(i==3 ? "%.3f" : "%.2f",value)
                                                         : sk150::measurementLabel(value,i==1||i==3,units[i]));
                }
            }
            for(size_t i=0;i<readings_.size();++i) {
                if(!readings_[i]) continue;
                const bool card=unitLabels_[i]!=nullptr;
                readings_[i]->SetForegroundColour(card ? (fresh ? wxColour(255,255,255) : wxColour(180,180,180))
                                                      : (fresh ? GetForegroundColour() : wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT)));
                if(unitLabels_[i]) unitLabels_[i]->SetForegroundColour(fresh ? wxColour(255,255,255) : wxColour(180,180,180));
            }
            bool writable=!closing_&&fresh&&!busy;
            guardsValid=fresh&&state_->protectionsHealthy; guards=state_->protections;
            controlsReady=writable;
            const bool allowed=guardsValid&&sk150::allowed(state_->values[0],state_->values[1],limit_,guards);
            set_->Enable(writable&&guardsValid); currentSet_->Enable(writable&&guardsValid);
            outputButton_->SetState(fresh&&state_->outputHealthy&&state_->output ? sk150::ConnectionState::Connected:sk150::ConnectionState::Disconnected,false);
            outputButton_->SetToolTip(fresh&&state_->outputHealthy?(state_->output?wxS("Ausgang EIN · Klicken zum Ausschalten"):wxS("Ausgang AUS · Klicken zum Einschalten")):wxS("Ausgangszustand unbekannt"));
            outputButton_->SetName(outputButton_->GetToolTipText());
            outputButton_->Enable(writable&&state_->outputHealthy&&(state_->output||(allowed&&state_->values[2]<=limit_.maximum())));
            connection=sk150::connectionState(true,state_->stop,state_->done,state_->attemptedRead,fresh,state_->communicationError.empty());
        } else {
            if(protectionDialog_) protectionDialog_->RefreshValues(false,false,{},"Nicht verbunden");
            if(powerLimitDialog_) powerLimitDialog_->RefreshValues(false,false,0,"Nicht verbunden");
        }
        if(!state_) outputButton_->Disable();
        voltageSlider_->SetLimits(limit_,guardsValid,guards[0],controlsReady);
        currentSlider_->SetLimits(limit_,guardsValid,guards[1],controlsReady);
        UpdatePresets(controlsReady,guardsValid,guards);
        const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        connect_->SetState(connection,(now/500)%2!=0);
        if(closing_) connect_->Disable();
        connectionState_=connection;
        GetMenuBar()->Enable(MenuPort,!closing_&&connection==sk150::ConnectionState::Disconnected);
        GetMenuBar()->Enable(MenuLimit,!closing_&&!busy);
    }

public:
    explicit Window(const wxString& configPath=wxEmptyString):wxFrame(nullptr,wxID_ANY,wxS("XY-SK150 · Steuerung"),wxDefaultPosition,wxSize(1060,680)) {
        // GTK may paint the theme background grey while GetBackgroundColour()
        // still reports white. Use one explicit colour for the window, panel and
        // the exposed corners of the custom rounded measurement cards.
        const auto surface=wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);
        SetBackgroundColour(surface);
        auto panel=new wxPanel(this);
        panel->SetBackgroundColour(surface);
        auto outer=new wxBoxSizer(wxVERTICAL);
        auto fileMenu=new wxMenu; fileMenu->Append(wxID_EXIT,"Beenden\tAlt+F4");
        auto connectionMenu=new wxMenu; connectionMenu->Append(MenuPort,wxS("Schnittstelle auswählen …"));
        auto settingsMenu=new wxMenu; settingsMenu->Append(MenuLimit,wxS("Spannungslimit …")); settingsMenu->Append(MenuProtections,wxS("OVP/OCP …")); settingsMenu->Append(MenuPowerLimit,wxS("Leistungsbegrenzung …"));
        auto helpMenu=new wxMenu; helpMenu->Append(wxID_ABOUT,wxS("Über …"));
        auto menus=new wxMenuBar; menus->Append(fileMenu,"Datei"); menus->Append(connectionMenu,"Verbindung");
        auto deviceMenu=new wxMenu; deviceMenu->Append(MenuDevice,wxS("Informationen …"));menus->Append(deviceMenu,wxS("Gerät"));
        menus->Append(settingsMenu,"Einstellungen"); menus->Append(helpMenu,"Hilfe"); SetMenuBar(menus);
        preferences_=std::make_unique<wxFileConfig>("sk150-control",wxEmptyString,
            (configPath.empty() ? wxStandardPaths::Get().GetUserConfigDir()+"/sk150-control.conf" : configPath),wxEmptyString,wxCONFIG_USE_LOCAL_FILE);
        preferences_->Read("serial_port",&portPath_,"/dev/ttyUSB0");
        if(!portPath_.StartsWith("/")||portPath_.Contains("\n")||portPath_.Contains("\r")) portPath_="/dev/ttyUSB0";
        serial_.baud=static_cast<unsigned>(preferences_->ReadLong("baud",115200));
        serial_.dataBits=static_cast<unsigned>(preferences_->ReadLong("data_bits",8));
        serial_.parity=static_cast<sk150::Parity>(preferences_->ReadLong("parity",0));
        serial_.stopBits=static_cast<unsigned>(preferences_->ReadLong("stop_bits",1));
        serial_.slave=static_cast<unsigned>(preferences_->ReadLong("slave",1));
        try { serial_.validate(); } catch(const std::exception&) { serial_={}; }
        SetTitle(wxString::FromUTF8("XY-SK150 · ")+portPath_);
        for(size_t i=0;i<presets_.size();++i) {
            const auto prefix=wxString::Format("preset_%zu_",i+1);
            preferences_->Read(prefix+"enabled",&presets_[i].configured,false);
            const long voltage=preferences_->ReadLong(prefix+"voltage",0),current=preferences_->ReadLong(prefix+"current",0);
            if(voltage<0||voltage>3600||current<0||current>7200) presets_[i].configured=false;
            else {presets_[i].voltage=voltage;presets_[i].current=current;}
        }
        auto connectionRow=new wxBoxSizer(wxHORIZONTAL);
        infoBox_=new InfoCard(panel);
        auto info=new wxBoxSizer(wxVERTICAL);
        status_=new wxStaticText(infoBox_,wxID_ANY,"Bereit",wxDefaultPosition,FromDIP(wxSize(400,34)),wxST_NO_AUTORESIZE);
        info->Add(status_,0,wxEXPAND|wxLEFT|wxRIGHT|wxTOP,12);
        auto addInfo=[&](const wxString& text) {
            auto label=new wxStaticText(infoBox_,wxID_ANY,text,wxDefaultPosition,FromDIP(wxSize(400,18)),wxST_NO_AUTORESIZE);
            info->Add(label,0,wxEXPAND|wxLEFT|wxRIGHT,12); return label;
        };
        limitStatus_=addInfo(""); tripStatus_=addInfo("Schutzstatus: noch nicht gelesen");
        lastTrip_=addInfo(""); result_=addInfo("");
        for(auto child:infoBox_->GetChildren()) child->SetBackgroundColour(infoBox_->GetBackgroundColour());
        infoBox_->SetSizer(info);
        connectionRow->Add(infoBox_,0,wxEXPAND|wxRIGHT,16);
        auto rightColumn=new wxBoxSizer(wxHORIZONTAL);
        rightColumn->AddStretchSpacer();
        connect_=new PowerButton(panel); rightColumn->Add(connect_,0,wxALIGN_CENTER_VERTICAL);
        connectionRow->Add(rightColumn,1,wxEXPAND);
        outer->Add(connectionRow,0,wxEXPAND|wxALL,12);
        auto live=new wxBoxSizer(wxHORIZONTAL);
        auto addMeasurement=[&](size_t i, const char* title) {
            auto card=new MeasurementCard(panel);
            auto column=new wxBoxSizer(wxVERTICAL);
            const int padding=card->FromDIP(16);
            auto caption=new wxStaticText(card,wxID_ANY,title,wxDefaultPosition,wxDefaultSize,wxALIGN_LEFT|wxST_NO_AUTORESIZE);
            caption->SetBackgroundColour(card->GetBackgroundColour());
            caption->SetForegroundColour(*wxWHITE);
            auto header=new wxBoxSizer(wxHORIZONTAL);
            header->SetMinSize(-1,card->FromDIP(34));
            header->Add(caption,1,wxALIGN_CENTER_VERTICAL);
            if(i==2||i==3) {
                auto indicator=new wxStaticText(card,wxID_ANY,i==2 ? "CV" : "CC",
                                                wxDefaultPosition,wxDefaultSize,wxALIGN_RIGHT|wxST_NO_AUTORESIZE);
                indicator->SetBackgroundColour(card->GetBackgroundColour());
                // Reserve the bold width even when the indicator is inactive.
                auto font=indicator->GetFont().Scale(1.8);
                indicator->SetFont(font.MakeBold());
                const auto size=indicator->GetTextExtent(indicator->GetLabel());
                indicator->SetMinSize(wxSize(size.x+4,size.y+4));
                header->Add(indicator,0,wxALIGN_CENTER_VERTICAL|wxLEFT,8);
                if(i==2) cv_=indicator; else cc_=indicator;
            }
            column->Add(header,0,wxEXPAND|wxLEFT|wxRIGHT|wxTOP,padding);
            column->AddSpacer(card->FromDIP(4));
            readings_[i]=new wxStaticText(card,wxID_ANY,wxS("—"),wxDefaultPosition,wxDefaultSize,wxALIGN_LEFT|wxST_NO_AUTORESIZE);
            readings_[i]->SetBackgroundColour(card->GetBackgroundColour());
            readings_[i]->SetForegroundColour(*wxWHITE);
            readings_[i]->SetFont(readings_[i]->GetFont().MakeBold().Scale(3.0));
            // Stable dimensions prevent changing measurements from moving the cards.
            const auto extent=readings_[i]->GetTextExtent(i==2 ? "36.00" : (i==3 ? "7.100" : "655.35"));
            readings_[i]->SetMinSize(wxSize(extent.x+12,extent.y+8));
            unitLabels_[i]=new wxStaticText(card,wxID_ANY,i==2 ? "V" : (i==3 ? "A" : "W"),
                                            wxDefaultPosition,wxDefaultSize,wxALIGN_RIGHT|wxST_NO_AUTORESIZE);
            unitLabels_[i]->SetFont(readings_[i]->GetFont());
            unitLabels_[i]->SetBackgroundColour(card->GetBackgroundColour());
            unitLabels_[i]->SetForegroundColour(*wxWHITE);
            const auto unitExtent=unitLabels_[i]->GetTextExtent(unitLabels_[i]->GetLabel());
            unitLabels_[i]->SetMinSize(wxSize(unitExtent.x+4,extent.y+8));
            auto valueRow=new wxBoxSizer(wxHORIZONTAL);
            valueRow->Add(readings_[i],1,wxEXPAND);
            valueRow->AddSpacer(card->FromDIP(8));
            valueRow->Add(unitLabels_[i],0,wxEXPAND);
            column->Add(valueRow,0,wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM,padding);
            card->SetSizer(column);
            live->Add(card,1,wxEXPAND);
            if(i==2) measurementHeight_=card->GetBestSize().y;
        };
        addMeasurement(2,"Spannung");
        live->AddSpacer(16);
        addMeasurement(3,"Strom");
        live->AddSpacer(16);
        addMeasurement(4,"Leistung");
        outer->Add(live,0,wxEXPAND|wxALL,12);
        UpdateMode(sk150::RegulationMode::CV,false);

        auto settingsRow=new wxBoxSizer(wxHORIZONTAL);
        auto addSettings=[&](bool voltage) {
            auto section=new wxBoxSizer(wxVERTICAL);
            auto input=new wxBoxSizer(wxHORIZONTAL);
            auto edit=new wxTextCtrl(panel,wxID_ANY,voltage?"10,00":"0,500",wxDefaultPosition,FromDIP(wxSize(100,-1)),wxTE_PROCESS_ENTER);
            auto button=new wxButton(panel,wxID_ANY,"Setzen");
            input->Add(edit,1,wxRIGHT,8);input->Add(new wxStaticText(panel,wxID_ANY,voltage?"V":"A"),0,wxALIGN_CENTER_VERTICAL|wxRIGHT,8);input->Add(button);
            section->Add(new wxStaticText(panel,wxID_ANY,voltage?"Sollspannung":"Strombegrenzung"),0,wxBOTTOM,8);
            section->Add(input,0,wxEXPAND);
            auto slider=new MarkedSlider(panel,voltage,[this,voltage](unsigned value) {
                (voltage?voltage_:current_)->ChangeValue(wxString::Format(voltage?"%.2f":"%.3f",value/(voltage?100.0:1000.0)));
            });
            section->Add(slider,0,wxEXPAND|wxTOP,12);
            auto actual=new wxStaticText(panel,wxID_ANY,wxS("—"));
            // The acknowledged setpoint remains visible without replacing pending edits.
            actual->Hide();readings_[voltage?0:1]=actual;
            if(voltage) {voltage_=edit;set_=button;voltageSlider_=slider;}else {current_=edit;currentSet_=button;currentSlider_=slider;}
            slider->SetValue(voltage?1000:500);
            edit->Bind(wxEVT_TEXT,[slider,edit,voltage](wxCommandEvent&) {
                try {slider->SetValue(voltage?sk150::parseVoltage(edit->GetValue().ToStdString()):sk150::parseCurrent(edit->GetValue().ToStdString()));}catch(const std::exception&){}
            });
            edit->Bind(wxEVT_TEXT_ENTER,[this,voltage](wxCommandEvent&){Submit(voltage);});
            button->Bind(wxEVT_BUTTON,[this,voltage](wxCommandEvent&){Submit(voltage);});
            button->Disable();settingsRow->Add(section,1,wxEXPAND);
        };
        addSettings(true);settingsRow->AddSpacer(16);addSettings(false);settingsRow->AddSpacer(16);settingsRow->AddStretchSpacer();
        outer->Add(settingsRow,0,wxEXPAND|wxLEFT|wxRIGHT|wxTOP,12);
        outer->AddStretchSpacer();
        outer->AddSpacer(24);
        auto outputRow=new wxBoxSizer(wxHORIZONTAL);
        output_=new wxStaticText(panel,wxID_ANY,"Ausgang: unbekannt");
        outputButton_=new PowerButton(panel,false);outputButton_->Disable();
        outputRow->AddStretchSpacer();outputRow->Add(output_,0,wxALIGN_CENTER_VERTICAL|wxRIGHT,14);outputRow->Add(outputButton_);
        outer->Add(outputRow,0,wxEXPAND|wxLEFT|wxRIGHT,12);
        outer->AddSpacer(14);
        outer->Add(new wxStaticLine(panel),0,wxEXPAND|wxLEFT|wxRIGHT,12);
        outer->AddSpacer(14);
        auto memories=new wxBoxSizer(wxHORIZONTAL);
        for(size_t i=0;i<10;++i) {
            auto button=new wxButton(panel,wxID_ANY,wxString::Format("%zu\n---\n---",i+1),wxDefaultPosition,FromDIP(wxSize(86,72)),wxBU_EXACTFIT);
            presetButtons_[i]=button;memories->Add(button,1,wxEXPAND|wxRIGHT,6);
            button->Bind(wxEVT_BUTTON,[this,i](wxCommandEvent&) {
                if(!presets_[i].configured) return;
                if(Queue(Command::Preset,presets_[i].voltage,presets_[i].current)) {
                    voltage_->ChangeValue(wxString::Format("%.2f",presets_[i].voltage/100.0));current_->ChangeValue(wxString::Format("%.3f",presets_[i].current/1000.0));
                    voltageSlider_->SetValue(presets_[i].voltage);currentSlider_->SetValue(presets_[i].current);
                }
            });
        }
        auto configure=new wxButton(panel,wxID_ANY,wxS("…"),wxDefaultPosition,FromDIP(wxSize(36,72)),wxBU_EXACTFIT);
        configure->SetToolTip(wxS("Speicherplätze bearbeiten"));configure->Bind(wxEVT_BUTTON,[this](wxCommandEvent&){ConfigurePresets();});memories->Add(configure,0,wxEXPAND);
        outer->Add(memories,0,wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM,12);
        panel->SetSizer(outer);
        infoBox_->SetMinSize(wxSize(680,measurementHeight_));
        panel->Bind(wxEVT_SIZE,[this,panel](wxSizeEvent& event){
            const int available=panel->GetClientSize().x-24;
            infoBox_->SetMinSize(wxSize((available-32)*2/3+16,measurementHeight_));
            panel->Layout();event.Skip();
        });
        const auto minimum=outer->CalcMin();
        SetClientSize(wxSize(wxMax(1060,minimum.x),wxMax(600,minimum.y)));
        SetMinClientSize(wxSize(wxMax(1060,minimum.x),wxMax(600,minimum.y)));
        Bind(wxEVT_MENU,[this](wxCommandEvent&){Close();},wxID_EXIT);
        Bind(wxEVT_MENU,[this](wxCommandEvent&){SelectPort();},MenuPort);
        Bind(wxEVT_MENU,[this](wxCommandEvent&){ConfigureLimit();},MenuLimit);
        Bind(wxEVT_MENU,[this](wxCommandEvent&){ConfigureProtections();},MenuProtections);
        Bind(wxEVT_MENU,[this](wxCommandEvent&){ConfigurePowerLimit();},MenuPowerLimit);
        Bind(wxEVT_MENU,[this](wxCommandEvent&){ShowAbout(this);},wxID_ABOUT);
        connect_->Bind(wxEVT_BUTTON,[this](wxCommandEvent&){ToggleConnection();});
        Bind(wxEVT_MENU,[this](wxCommandEvent&){ShowDevice();},MenuDevice);
        outputButton_->Bind(wxEVT_BUTTON,[this](wxCommandEvent&) {
            bool on=false;
            if(state_) {std::lock_guard<std::mutex> lock(state_->mutex);on=state_->output;}
            Queue(Command::Output,on?0:1);
        });
        Bind(wxEVT_TIMER,[this](wxTimerEvent&){ Tick(); }); timer_.Start(100); Tick();
        Bind(wxEVT_CLOSE_WINDOW,[this](wxCloseEvent& e){
            if(worker_.joinable()&&!state_->done&&e.CanVeto()) { e.Veto(); closing_=true; Stop(); Disable(); }
            else { Stop(); if(worker_.joinable()) worker_.join(); Destroy(); }
        });
    }
    ~Window() override { Stop(); if(worker_.joinable()) worker_.join(); }
};
#ifndef SK150_GUI_TEST
class App : public wxApp { public: bool OnInit() override { (new Window)->Show(); return true; } };
wxIMPLEMENT_APP(App);

#endif
