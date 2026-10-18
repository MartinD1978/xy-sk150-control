#define SK150_GUI_TEST
#include "../src/main.cpp"
#include <wx/dcclient.h>
#include <wx/dcmemory.h>
#include <wx/dcscreen.h>
#include <wx/evtloop.h>
#include <iostream>
#include <fstream>
#include <wx/filename.h>
#include <stdexcept>

template<class T> static T* First(wxWindow* parent) {
    for(auto child:parent->GetChildren()) {
        if(auto found=dynamic_cast<T*>(child)) return found;
        if(auto found=First<T>(child)) return found;
    }
    return nullptr;
}
static wxDialog* ActiveDialog(const wxString& title) {
    for(auto top:wxTopLevelWindows) if(auto dialog=dynamic_cast<wxDialog*>(top)) if(dialog->GetTitle()==title) return dialog;
    return nullptr;
}
static void Click(wxWindow* control,int id=wxID_ANY) {
    wxCommandEvent event(wxEVT_BUTTON,id==wxID_ANY ? control->GetId() : id);
    control->GetEventHandler()->ProcessEvent(event);
}
static bool capturing=false;
static void Capture(wxWindow* window,const wxString& file) {
    capturing=true; window->Raise(); window->Layout(); window->Refresh();
    const auto request=file+".request";
    wxEventLoop loop; wxTimer snapshot;
    int attempts=0;
    snapshot.Bind(wxEVT_TIMER,[&](wxTimerEvent&) {
        if(++attempts==2) {
            const auto rect=window->GetScreenRect();
            std::ofstream message(request.ToStdString());
            message<<rect.x<<' '<<rect.y<<' '<<rect.width<<' '<<rect.height;
        }
        if((attempts>2&&!wxFileExists(request))||attempts>40) loop.Exit();
    });
    snapshot.Start(50); loop.Run(); snapshot.Stop(); capturing=false;
    if(attempts>40||!wxFileExists(file)) throw std::runtime_error("Screenshot failed");
}

class GuiTestApp : public wxApp {
    Window* window_=nullptr;
    wxTimer timer_{this};
    int phase_=0, ticks_=0;
    wxString shots_;
    std::string failure_;
    void Check(bool ok,const wxString& message) { if(!ok) throw std::runtime_error(message.ToUTF8().data()); }
    void Finish() { timer_.Stop(); if(window_) window_->Close(); }
    void DialogAction(const wxString& title,std::function<void(wxDialog*)> action) {
        CallAfter([this,title,action] {
            auto dialog=ActiveDialog(title);
            try { Check(dialog!=nullptr,"Dialog fehlt"); action(dialog); }
            catch(const std::exception& e) { failure_=e.what(); if(dialog) dialog->EndModal(wxID_CANCEL); }
        });
    }
    void TestDialogs() {
        Check(window_->GetMenuBar()->GetMenuCount()==5,wxS("Menüs fehlen"));
        Check(window_->limit_.enabled&&window_->limit_.centivolts==1200,"Startlimit nicht 12 V");
        DialogAction(wxS("Schnittstelle auswählen"),[this](wxDialog* dialog){
            auto choice=dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("baud",dialog));
            Check(choice&&choice->GetStringSelection()=="115200","Baud-Vorgabe falsch");
            choice->SetStringSelection("9600");
            dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("parity",dialog))->SetSelection(2);
            dynamic_cast<wxSpinCtrl*>(wxWindow::FindWindowByName("slave",dialog))->SetValue(99);
            dialog->EndModal(wxID_CANCEL);
        });
        window_->SelectPort();
        Check(window_->serial_.baud==115200&&window_->serial_.slave==1&&window_->serial_.parity==sk150::Parity::None,"Abbrechen veraendert Verbindungsparameter");
        DialogAction(wxS("Schnittstelle auswählen"),[this](wxDialog* dialog){
            auto combo=First<wxComboBox>(dialog); Check(combo!=nullptr,"Portliste fehlt");
            combo->SetValue(argv[1]);
            dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("baud",dialog))->SetStringSelection("19200");
            dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("data-bits",dialog))->SetStringSelection("8");
            dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("parity",dialog))->SetSelection(0);
            dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("stop-bits",dialog))->SetStringSelection("2");
            dynamic_cast<wxSpinCtrl*>(wxWindow::FindWindowByName("slave",dialog))->SetValue(17);
            Capture(dialog,shots_+"/connection-settings.png"); Click(dialog,wxID_OK);
        });
        window_->SelectPort(); Check(window_->portPath_==argv[1],wxS("Portauswahl nicht übernommen"));
        Check(window_->serial_.baud==19200&&window_->serial_.stopBits==2&&window_->serial_.slave==17,"Verbindungsparameter nicht uebernommen");
        Check(window_->GetTitle().Contains(argv[1]),"Schnittstelle fehlt im Fenstertitel");
        Check(window_->preferences_->ReadLong("slave",0)==17&&window_->preferences_->ReadLong("baud",0)==19200,"Verbindungsparameter nicht gespeichert");
        Check(!wxWindow::FindWindowByLabel("Nicht verbunden",window_),"Entfernte Verbindungszeile sichtbar");
        Check(!wxWindow::FindWindowByLabel(wxS("115200 Baud · 8N1 · Modbus RTU · Slave 1"),window_),"Entfernte Baud-Zeile sichtbar");
        DialogAction("Spannungslimit",[](wxDialog* dialog){
            First<wxTextCtrl>(dialog)->ChangeValue("24,00"); Click(dialog,wxID_OK);
        });
        window_->ConfigureLimit(); Check(window_->limit_.centivolts==2400,wxS("Limit übernehmen fehlgeschlagen"));
        DialogAction("Spannungslimit",[](wxDialog* dialog){
            First<wxTextCtrl>(dialog)->ChangeValue("30,00"); dialog->EndModal(wxID_CANCEL);
        });
        window_->ConfigureLimit(); Check(window_->limit_.centivolts==2400,wxS("Abbrechen verändert Limit"));
        DialogAction("Spannungslimit",[](wxDialog* dialog){
            First<wxCheckBox>(dialog)->SetValue(false); Click(dialog,wxID_OK);
        });
        window_->ConfigureLimit(); Check(!window_->limit_.enabled,"Limit deaktivieren fehlgeschlagen");
        DialogAction("Spannungslimit",[](wxDialog* dialog){
            First<wxCheckBox>(dialog)->SetValue(true); First<wxTextCtrl>(dialog)->ChangeValue("12"); Click(dialog,wxID_OK);
        });
        window_->ConfigureLimit(); Check(window_->limit_.enabled&&window_->limit_.centivolts==1200,"Limit reaktivieren fehlgeschlagen");
        DialogAction(wxS("OVP/OCP · Geräteschutz M0"),[this](wxDialog* dialog){
            auto button=wxWindow::FindWindowByLabel("OVP setzen",dialog);
            Check(button&&!button->IsEnabled(),wxS("OVP ohne Verbindung verfügbar")); dialog->EndModal(wxID_CLOSE);
        });
        window_->ConfigureProtections();
        DialogAction(wxS("Leistungsbegrenzung · OPP M0"),[this](wxDialog* dialog){
            auto button=wxWindow::FindWindowByLabel("OPP setzen",dialog);
            Check(button&&!button->IsEnabled(),"OPP ohne Verbindung verfuegbar"); dialog->EndModal(wxID_CLOSE);
        });
        window_->ConfigurePowerLimit();
        DialogAction(wxS("Über XY-SK150"),[this](wxDialog* dialog){
            Check(wxWindow::FindWindowByLabel("Version V0.2.1",dialog),"Version fehlt");
            Check(wxWindow::FindWindowByLabel(wxS("© 2026 Martin Dietl"),dialog),"Name fehlt");
            Check(!wxWindow::FindWindowByLabel(wxS("C++ · wxWidgets · Debian / Linux"),dialog),"Entfernte C++-Zeile vorhanden");
            Check(First<wxStaticBitmap>(dialog)!=nullptr,"Logo fehlt");
            Capture(dialog,shots_+"/about.png"); dialog->EndModal(wxID_OK);
        });
        ShowAbout(window_);
        DialogAction("Softwarelizenz und Logohinweis",[this](wxDialog* dialog){
            auto text=First<wxTextCtrl>(dialog);
            Check(text&&text->GetValue().Contains("Copyright (c) 2026 Martin Dietl")&&text->GetValue().Contains(wxS("Logo-Ausnahme"))&&text->GetValue().Contains("dietl.martin@mail.de")&&text->GetValue().Contains(wxS("Martin-Dietl-Softwarelizenz"))&&text->GetValue().Contains("Version 1.1")&&text->GetValue().Contains(wxS("Schulen"))&&text->GetValue().Contains(wxS("Behörden")),"Lizenz/Logo-Ausnahme fehlt");
            dialog->EndModal(wxID_OK);
        });
        ShowLicense(window_);
        Check(failure_.empty(),failure_.c_str());
        DialogAction(wxS("Speicherplätze"),[](wxDialog* dialog){
            auto check=First<wxCheckBox>(dialog);check->SetValue(true);
            First<wxTextCtrl>(dialog)->ChangeValue("9,00");Click(dialog,wxID_OK);
        });
        window_->ConfigurePresets();
        Check(window_->presets_[0].configured&&window_->presets_[0].voltage==900,"Speicherplatz nicht gespeichert");
        { auto restored=new Window(argv[2]);Check(restored->presets_[0].configured&&restored->presets_[0].voltage==900,"Speicherplatz nach Neustart verloren");restored->Destroy(); }
        window_->presets_[8]={true,1500,500};window_->presets_[9]={true,1200,1000};
        window_->Tick(); Capture(window_,shots_+"/main-disconnected.png");
        window_->ToggleConnection();
    }
    void Step() {
        if(capturing) return;
        try {
            if(++ticks_>100) throw std::runtime_error("GUI-Test Timeout");
            if(phase_==0) {
                bool ready=false;
                { std::lock_guard<std::mutex> lock(window_->state_->mutex); ready=window_->state_->healthy&&window_->state_->communicationError.empty(); }
                if(!ready) return;
                window_->Tick();
                Check(window_->connectionState_==sk150::ConnectionState::Connected,wxS("Grüner Zustand fehlt"));
                Check(!window_->GetMenuBar()->IsEnabled(Window::MenuPort),wxS("Port während Verbindung auswählbar"));
                Check(window_->readings_[4]->GetLabel()=="5.42","Leistungsanzeige falsch");
                const auto card=window_->readings_[2]->GetParent()->GetSize();
                const auto info=window_->infoBox_->GetSize();
                Check(std::abs(info.x-(card.x*2+16))<4&&std::abs(info.y-card.y)<4,"Infobox nicht in gewuenschter Groesse");
                Check(window_->infoBox_->GetPosition().x==window_->readings_[2]->GetParent()->GetPosition().x,"Infobox nicht links ausgerichtet");
                Check(window_->limitStatus_->GetParent()==window_->infoBox_&&window_->tripStatus_->GetParent()==window_->infoBox_,"Limit/Schutzstatus nicht in Infobox");
                Check(window_->presetButtons_[0]->IsEnabled(),"Gueltiger Speicher gesperrt");
                Check(!window_->presetButtons_[8]->IsEnabled()&&!window_->presetButtons_[9]->IsEnabled(),"Grenzwert-Speicher aktiv");
                Check(window_->presetButtons_[8]->GetBackgroundColour()==wxColour(190,48,48),"Gesperrter Speicher nicht rot");
                Check(window_->voltageSlider_->slider->GetMax()==360&&window_->currentSlider_->slider->GetMax()==144,"Reglerskalen falsch");
                window_->voltageSlider_->slider->SetValue(360);wxCommandEvent slide(wxEVT_SLIDER,window_->voltageSlider_->slider->GetId());window_->voltageSlider_->slider->GetEventHandler()->ProcessEvent(slide);
                Check(window_->voltage_->GetValue()=="12.00","Regler ueberschreitet Limit");
                window_->currentSlider_->slider->SetValue(144);wxCommandEvent currentSlide(wxEVT_SLIDER,window_->currentSlider_->slider->GetId());window_->currentSlider_->slider->GetEventHandler()->ProcessEvent(currentSlide);
                Check(window_->current_->GetValue()=="0.700","Regler erreicht/ueberschreitet OCP");
                DialogAction(wxS("Gerät · Informationen"),[this](wxDialog* dialog){Check(window_->inputVoltage_->GetLabel().Contains("24.53"),"Eingangsspannung fehlt");Capture(dialog,shots_+"/device-info.png");dialog->EndModal(wxID_OK);});
                window_->ShowDevice();
                Capture(window_,shots_+"/main-connected.png");
                phase_=1;
                DialogAction(wxS("OVP/OCP · Geräteschutz M0"),[this](wxDialog* dialog){
                    auto button=wxWindow::FindWindowByLabel("OVP setzen",dialog);
                    Check(button&&button->IsEnabled(),wxS("OVP trotz gültigen Werten gesperrt"));
                    Capture(dialog,shots_+"/protections.png"); dialog->EndModal(wxID_CLOSE);
                });
                window_->ConfigureProtections();
                DialogAction(wxS("Leistungsbegrenzung · OPP M0"),[this](wxDialog* dialog){
                    auto button=wxWindow::FindWindowByLabel("OPP setzen",dialog);
                    Check(button&&button->IsEnabled(),"OPP trotz gueltigem Wert gesperrt");
                    Check(First<wxTextCtrl>(dialog)->GetValue()=="125.0","OPP-Skalierung falsch");
                    Capture(dialog,shots_+"/power-limit.png"); dialog->EndModal(wxID_CLOSE);
                });
                window_->ConfigurePowerLimit();
                Click(window_->presetButtons_[0]);phase_=3;
            } else if(phase_>=3) {
                unsigned voltage=0,current=0;bool busy=false,output=false;
                {std::lock_guard<std::mutex> lock(window_->state_->mutex);busy=window_->state_->busy;voltage=window_->state_->values[0];current=window_->state_->values[1];output=window_->state_->output;}
                if(busy) return;
                window_->Tick();
                if(phase_==3) {
                    Check(voltage==900&&current==0,"Speicher-Klick nicht direkt uebernommen");
                    window_->current_->SetValue("0,600");Click(window_->currentSet_);phase_=4;
                }else if(phase_==4) {
                    Check(current==600,"Strom setzen fehlgeschlagen");
                    window_->voltage_->SetValue("10,50");wxCommandEvent enter(wxEVT_TEXT_ENTER,window_->voltage_->GetId());window_->voltage_->GetEventHandler()->ProcessEvent(enter);phase_=5;
                }else if(phase_==5) {
                    Check(voltage==1050,"Enter auf Spannung fehlgeschlagen");Click(window_->outputButton_);phase_=6;
                }else if(phase_==6) {
                    Check(!output,"Ausgangstaster schaltet nicht aus");Click(window_->outputButton_);phase_=7;
                }else {
                    Check(output,"Ausgangstaster schaltet nicht ein");
                    window_->ToggleConnection();phase_=1;
                }
            } else if(phase_==1) {
                if(!window_->state_->done) return;
                window_->Tick();
                Check(window_->connectionState_==sk150::ConnectionState::Disconnected,"Trennen fehlgeschlagen");
                Check(window_->GetMenuBar()->IsEnabled(Window::MenuPort),"Port nach Trennen gesperrt");
                window_->portPath_="/dev/nonexistent-sk150-gui-test";
                window_->ToggleConnection(); phase_=2;
            } else {
                if(!window_->state_->done) return;
                window_->Tick();
                Check(window_->connectionState_==sk150::ConnectionState::Error,wxS("Öffnungsfehler nicht sichtbar"));
                window_->connect_->SetState(sk150::ConnectionState::Error,true);
                const auto bright=window_->connect_->GetBitmap().ConvertToImage();
                window_->connect_->SetState(sk150::ConnectionState::Error,false);
                const auto dim=window_->connect_->GetBitmap().ConvertToImage();
                Check(bright.GetRed(6,6)!=dim.GetRed(6,6),"Fehler blinkt nicht");
                Capture(window_,shots_+"/main-error.png");
                window_->ToggleConnection(); window_->Tick();
                Check(window_->connectionState_==sk150::ConnectionState::Disconnected,wxS("Fehler lässt sich nicht trennen"));
                Check(failure_.empty(),failure_.c_str());
                std::cout<<"GUI menus, dialogs, connection, error and disconnect: OK\n";
                Finish();
            }
        } catch(const std::exception& e) { failure_=e.what(); std::cerr<<failure_<<'\n'; Finish(); }
    }
public:
    bool OnInit() override {
        if(argc!=4) return false;
        wxInitAllImageHandlers(); shots_=argv[3];
        window_=new Window(argv[2]); window_->Show();
        CallAfter([this]{
            try { TestDialogs(); Bind(wxEVT_TIMER,[this](wxTimerEvent&){Step();}); timer_.Start(100); }
            catch(const std::exception& e){failure_=e.what();std::cerr<<failure_<<'\n';Finish();}
        });
        return true;
    }
    int OnRun() override { const int code=wxApp::OnRun(); return failure_.empty() ? code : 1; }
};
wxIMPLEMENT_APP(GuiTestApp);
