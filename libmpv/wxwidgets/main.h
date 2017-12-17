#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include <mpv/client.h>

class MpvApp : public wxApp
{
public:
    bool OnInit() override;
};

class MpvFrame : public wxFrame
{
public:
    MpvFrame();
    
    bool Destroy() override;
    bool Autofit(int percent, bool larger = true, bool smaller = true);

private:
    void MpvCreate(int64_t wid);
    void MpvDestroy();

    void OnKeyDown(wxKeyEvent &event);
    void OnDropFiles(wxDropFilesEvent &event);

    void OnMpvEvent(mpv_event &event);
    void OnMpvWakeupEvent(wxThreadEvent &event);

    mpv_handle *mpv = nullptr;
    
    wxDECLARE_EVENT_TABLE();
};
