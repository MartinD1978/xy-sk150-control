#pragma once
#include "setpoints.hpp"
#include <wx/wx.h>
#include <wx/slider.h>
#include <wx/dcbuffer.h>
#include <functional>

// Native slider retains keyboard support; the scale above it carries labelled
// protection ticks in the same fixed register-unit coordinate system.
class MarkedSlider : public wxPanel {
    wxPanel* marks_;
    unsigned maximum_,step_;
    bool voltage_,valid_=false,limitEnabled_=false;
    unsigned limit_=0,protection_=0,allowed_=0;
public:
    wxSlider* slider;
    MarkedSlider(wxWindow* parent,bool voltage,std::function<void(unsigned)> changed)
        :wxPanel(parent),maximum_(voltage?3600:7200),step_(voltage?10:50),voltage_(voltage) {
        auto box=new wxBoxSizer(wxVERTICAL);
        marks_=new wxPanel(this,wxID_ANY,wxDefaultPosition,FromDIP(wxSize(-1,36)));
        marks_->SetBackgroundStyle(wxBG_STYLE_PAINT);
        marks_->Bind(wxEVT_PAINT,[this](wxPaintEvent&){
            wxAutoBufferedPaintDC dc(marks_); dc.SetBackground(wxBrush(GetBackgroundColour())); dc.Clear();
            const int width=marks_->GetClientSize().x,pad=FromDIP(10),bottom=FromDIP(36);
            auto position=[&](unsigned value){return pad+int(double(wxMax(0,width-2*pad))*std::min(value,maximum_)/maximum_);};
            struct Label { int tick,x,width; wxString text; wxColour colour; };
            std::vector<Label> labels;
            auto add=[&](unsigned value,const wxString& text,const wxColour& colour,bool alignLeft) {
                const int tick=position(value),textWidth=dc.GetTextExtent(text).x;
                labels.push_back({tick,wxMax(0,wxMin(width-textWidth,alignLeft?tick-textWidth-5:tick+5)),textWidth,text,colour});
            };
            if(voltage_&&limitEnabled_) add(limit_,wxString::Format("Limit %.2f V",limit_/100.0),wxColour(25,90,190),true);
            if(valid_) add(protection_,wxString::Format(voltage_?"OVP %.2f V":"OCP %.3f A",protection_/(voltage_?100.0:1000.0)),wxColour(190,30,30),false);
            // Clamping text to the right edge can push OVP over the limit label.
            // Move text only; ticks remain at the exact original coordinates.
            if(labels.size()==2) {
                if(labels[1].tick<labels[0].tick) std::swap(labels[0],labels[1]);
                auto& left=labels[0];auto& right=labels[1];
                const int gap=FromDIP(12);
                if(left.x+left.width+gap>right.x) {
                    left.x=wxMax(0,wxMin(left.x,right.x-left.width-gap));
                    right.x=wxMax(right.x,left.x+left.width+gap);
                }
            }
            for(const auto& label:labels) {
                dc.SetTextForeground(label.colour); dc.DrawText(label.text,label.x,0);
                dc.SetPen(wxPen(label.colour,FromDIP(2))); dc.DrawLine(label.tick,FromDIP(17),label.tick,bottom);
            }
        });
        box->Add(marks_,0,wxEXPAND);
        slider=new wxSlider(this,wxID_ANY,0,0,maximum_/step_,wxDefaultPosition,wxDefaultSize,wxSL_HORIZONTAL);
        slider->SetName(voltage?"voltage-slider":"current-slider");
        slider->SetLineSize(1); slider->SetPageSize(voltage?10:20);
        box->Add(slider,0,wxEXPAND);
        auto ends=new wxBoxSizer(wxHORIZONTAL);
        ends->Add(new wxStaticText(this,wxID_ANY,voltage?"0 V":"0 A")); ends->AddStretchSpacer();
        ends->Add(new wxStaticText(this,wxID_ANY,voltage?"18 V":"3,6 A")); ends->AddStretchSpacer();
        ends->Add(new wxStaticText(this,wxID_ANY,voltage?"36 V":"7,2 A"));box->Add(ends,0,wxEXPAND);
        SetSizer(box);
        slider->Bind(wxEVT_SLIDER,[this,changed](wxCommandEvent&){
            const unsigned selected=std::min(unsigned(slider->GetValue()),allowed_);
            slider->SetValue(selected); changed(selected*step_);
        });
        marks_->Bind(wxEVT_SIZE,[this](wxSizeEvent& e){marks_->Refresh();e.Skip();});
        slider->Disable();
    }
    void SetValue(unsigned units) { if(unsigned(slider->GetValue())!=units/step_) slider->SetValue(std::min(units/step_,maximum_/step_)); }
    void SetLimits(sk150::VoltageLimit limit,bool valid,unsigned protection,bool enabled) {
        limitEnabled_=voltage_&&limit.enabled;limit_=limit.centivolts;valid_=valid;protection_=protection;
        allowed_=sk150::sliderMaximum(maximum_,protection,step_);
        if(voltage_) allowed_=std::min(allowed_,limit.maximum()/step_);
        slider->Enable(valid&&enabled&&protection>0);
        marks_->Refresh();
        slider->SetToolTip(valid ? wxS("Sollwert vorbereiten · Erst Setzen oder Enter überträgt") : wxS("OVP/OCP unbekannt oder veraltet"));
    }
};
class PresetDialog : public wxDialog {
    std::array<wxCheckBox*,10> enabled_{};
    std::array<wxTextCtrl*,10> voltage_{},current_{};
public:
    std::array<sk150::Preset,10> values;
    PresetDialog(wxWindow* parent,const std::array<sk150::Preset,10>& presets)
        :wxDialog(parent,wxID_ANY,wxS("Speicherplätze")),values(presets) {
        auto box=new wxBoxSizer(wxVERTICAL);
        auto grid=new wxFlexGridSizer(3,8,18);
        for(auto label:{wxS("Platz aktiv"),wxS("Spannung · V"),wxS("Strombegrenzung · A")}) grid->Add(new wxStaticText(this,wxID_ANY,label));
        for(size_t i=0;i<10;++i) {
            enabled_[i]=new wxCheckBox(this,wxID_ANY,wxString::Format("%zu",i+1));enabled_[i]->SetValue(presets[i].configured);grid->Add(enabled_[i],0,wxALIGN_CENTER_VERTICAL);
            voltage_[i]=new wxTextCtrl(this,wxID_ANY,wxString::Format("%.2f",presets[i].voltage/100.0));grid->Add(voltage_[i]);
            current_[i]=new wxTextCtrl(this,wxID_ANY,wxString::Format("%.3f",presets[i].current/1000.0));grid->Add(current_[i]);
        }
        box->Add(grid,0,wxALL,16);box->Add(CreateSeparatedButtonSizer(wxOK|wxCANCEL),0,wxEXPAND|wxALL,12);SetSizerAndFit(box);
        FindWindow(wxID_OK)->SetLabel("Speichern");FindWindow(wxID_CANCEL)->SetLabel("Abbrechen");
        Bind(wxEVT_BUTTON,[this](wxCommandEvent&){
            try {
                auto proposed=values;
                for(size_t i=0;i<10;++i) {
                    proposed[i].configured=enabled_[i]->GetValue();
                    if(proposed[i].configured) { proposed[i].voltage=sk150::parseVoltage(voltage_[i]->GetValue().ToStdString()); proposed[i].current=sk150::parseCurrent(current_[i]->GetValue().ToStdString()); }
                }
                values=proposed;EndModal(wxID_OK);
            }catch(const std::exception& e){wxMessageBox(wxString::FromUTF8(e.what()),"Eingabe abgelehnt",wxOK|wxICON_WARNING,this);}
        },wxID_OK);CentreOnParent();
    }
};
