// Build with: g++ -o main main.cpp `wx-config-gtk3 --libs --cxxflags` `pkg-config --cflags --libs gtk+-3.0` -lmpv

#include "main.h"

#include <clocale>
#include <string>

#include <wx/display.h>

#ifdef __WXGTK__
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#endif

wxIMPLEMENT_APP(MpvApp);

bool MpvApp::OnInit()
{
    std::setlocale(LC_NUMERIC, "C");
    (new MpvFrame)->Show(true);
    return true;
}

wxDECLARE_APP(MpvApp);

wxDEFINE_EVENT(WX_MPV_WAKEUP, wxThreadEvent);

wxBEGIN_EVENT_TABLE(MpvFrame, wxFrame)
    EVT_CHAR_HOOK(MpvFrame::OnKeyDown)
    EVT_DROP_FILES(MpvFrame::OnDropFiles)
wxEND_EVENT_TABLE()

MpvFrame::MpvFrame()
    : wxFrame(nullptr, wxID_ANY, "mpv")
{
    SetBackgroundColour(wxColour(*wxBLACK));
    Center();
    DragAcceptFiles(true);

    auto panel = new wxPanel(this, wxID_ANY,
                             wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS);
    uint64_t wid;
#if defined(__WXMSW__)
    wid = reinterpret_cast<int64_t>(panel->GetHandle());
#elif defined(__WXGTK__)
    GtkWidget *widget = panel->GetHandle();
    gtk_widget_realize(widget);
    wid = GDK_WINDOW_XID(gtk_widget_get_parent_window(widget));
#else
    #error cannot determine wid
#endif
    MpvCreate(wid);

    if (wxGetApp().argc == 2) {
        const std::string filepath(wxGetApp().argv[1].utf8_str().data());
        const char *cmd[] = { "loadfile", filepath.c_str(), nullptr };
        mpv_command(mpv, cmd);
    }
}

bool MpvFrame::Destroy()
{
    MpvDestroy();
    return wxFrame::Destroy();
}

void MpvFrame::MpvCreate(int64_t wid)
{
    MpvDestroy();

    mpv = mpv_create();
    if (!mpv)
        throw std::runtime_error("failed to create mpv instance");

    Bind(WX_MPV_WAKEUP, &MpvFrame::OnMpvWakeupEvent, this);
    mpv_set_wakeup_callback(mpv, [](void *data) {
        auto window = reinterpret_cast<MpvFrame *>(data);
        if (window) {
            auto event = new wxThreadEvent(WX_MPV_WAKEUP);
            window->GetEventHandler()->QueueEvent(event);
        }
    }, this);

    if (mpv_set_property(mpv, "wid", MPV_FORMAT_INT64, &wid) < 0)
        throw std::runtime_error("failed to set mpv wid");

    if (mpv_initialize(mpv) < 0)
        throw std::runtime_error("failed to initialize mpv");

    mpv_observe_property(mpv, 0, "media-title", MPV_FORMAT_NONE);
}

void MpvFrame::MpvDestroy()
{
    Unbind(WX_MPV_WAKEUP, &MpvFrame::OnMpvWakeupEvent, this);

    if (mpv) {
        mpv_terminate_destroy(mpv);
        mpv = nullptr;
    }
}

bool MpvFrame::Autofit(int percent, bool larger, bool smaller)
{
    int64_t w, h;
    if (!mpv || mpv_get_property(mpv, "dwidth", MPV_FORMAT_INT64, &w) < 0 ||
                mpv_get_property(mpv, "dheight", MPV_FORMAT_INT64, &h) < 0 ||
                w <= 0 || h <= 0)
        return false;

    int screen_id = wxDisplay::GetFromWindow(this);
    if (screen_id == wxNOT_FOUND)
        return false;

    wxRect screen = wxDisplay(screen_id).GetClientArea();
    const int n_w = (int)(screen.width * percent * 0.01);
    const int n_h = (int)(screen.height * percent * 0.01);

    if ((larger && (w > n_w || h > n_h)) ||
        (smaller && (w < n_w || h < n_h)))
    {
        const float asp = w / (float)h;
        const float n_asp = n_w / (float)n_h;
        if (asp > n_asp) {
            w = n_w;
            h = (int)(n_w / asp);
        } else {
            w = (int)(n_h * asp);
            h = n_h;
        }
    }

    const wxRect rc = GetScreenRect();
    SetClientSize(w, h);
    const wxRect n_rc = GetScreenRect();

    Move(rc.x + rc.width / 2 - n_rc.width / 2,
         rc.y + rc.height / 2 - n_rc.height / 2);
    return true;
}

void MpvFrame::OnKeyDown(wxKeyEvent &event)
{
    if (mpv && event.GetKeyCode() == WXK_SPACE)
        mpv_command_string(mpv, "cycle pause");
    event.Skip();
}

void MpvFrame::OnDropFiles(wxDropFilesEvent &event)
{
    int size = event.GetNumberOfFiles();
    if (!size || !mpv)
        return;

    auto files = event.GetFiles();
    if (!files)
        return;

    for (int i = 0; i < size; ++i) {
        const std::string filepath(files[i].utf8_str().data());
        const char *cmd[] = {
            "loadfile",
            filepath.c_str(),
            i == 0 ? "replace" : "append-play",
            NULL
        };
        mpv_command_async(mpv, 0, cmd);
    }
}

void MpvFrame::OnMpvEvent(mpv_event &event)
{
    if (!mpv)
        return;

    switch (event.event_id) {
    case MPV_EVENT_VIDEO_RECONFIG:
        // something like --autofit-larger=95%
        Autofit(95, true, false);
        break;
    case MPV_EVENT_PROPERTY_CHANGE: {
        mpv_event_property *prop = (mpv_event_property *)event.data;
        if (strcmp(prop->name, "media-title") == 0) {
            char *data = nullptr;
            if (mpv_get_property(mpv, prop->name, MPV_FORMAT_OSD_STRING, &data) < 0) {
                SetTitle("mpv");
            } else {
                wxString title = wxString::FromUTF8(data);
                if (!title.IsEmpty())
                    title += " - ";
                title += "mpv";
                SetTitle(title);
                mpv_free(data);
            }
        }
        break;
    }
    case MPV_EVENT_SHUTDOWN:
        MpvDestroy();
        break;
    default:
        break;
    }
}

void MpvFrame::OnMpvWakeupEvent(wxThreadEvent &)
{
    while (mpv) {
        mpv_event *e = mpv_wait_event(mpv, 0);
        if (e->event_id == MPV_EVENT_NONE)
            break;
        OnMpvEvent(*e);
    }
}
