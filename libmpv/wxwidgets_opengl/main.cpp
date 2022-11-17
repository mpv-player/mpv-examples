// Build with: g++ -o main main.cpp `wx-config --libs --cxxflags --gl_libs` -lmpv

#include "main.h"

#include <clocale>
#include <string>

#include <wx/dcbuffer.h>
#include <wx/display.h>

wxIMPLEMENT_APP(MpvApp);

bool MpvApp::OnInit()
{
    std::setlocale(LC_NUMERIC, "C");
    (new MpvFrame)->Show(true);
    return true;
}

wxBEGIN_EVENT_TABLE(MpvGLCanvas, wxGLCanvas)
    EVT_SIZE(MpvGLCanvas::OnSize)
    EVT_PAINT(MpvGLCanvas::OnPaint)
    EVT_ERASE_BACKGROUND(MpvGLCanvas::OnErase)
wxEND_EVENT_TABLE()

MpvGLCanvas::MpvGLCanvas(wxWindow *parent)
    : wxGLCanvas(parent)
{
    SetBackgroundStyle(wxBG_STYLE_CUSTOM);
    glContext = new wxGLContext(this);
}

MpvGLCanvas::~MpvGLCanvas()
{
    OnRender = nullptr;
    OnSwapBuffers = nullptr;

    if (glContext)
        delete glContext;
}

bool MpvGLCanvas::SetCurrent() const
{
    if (!glContext)
        return false;
    return wxGLCanvas::SetCurrent(*glContext);
}

bool MpvGLCanvas::SwapBuffers()
{
    bool result = wxGLCanvas::SwapBuffers();
    if (OnSwapBuffers)
        OnSwapBuffers(this);
    return result;
}

void MpvGLCanvas::OnSize(wxSizeEvent &)
{
    Update();
}

void MpvGLCanvas::OnErase(wxEraseEvent &event)
{
    // do nothing to skip erase
}

void MpvGLCanvas::Render()
{
    wxClientDC(this);
    DoRender();
}

void MpvGLCanvas::OnPaint(wxPaintEvent &)
{
    wxAutoBufferedPaintDC(this);
    DoRender();
}

void MpvGLCanvas::DoRender()
{
    SetCurrent();
    if (OnRender)
        OnRender(this, GetSize().x, GetSize().y);
    SwapBuffers();
}

void *MpvGLCanvas::GetProcAddress(const char *name)
{
    SetCurrent();

#ifdef __WINDOWS__
    void *result = (void *)::wglGetProcAddress(name);
    if (!result) {
        HMODULE dll = ::LoadLibrary(wxT("opengl32.dll"));
        if (dll) {
            result = (void *)::GetProcAddress(dll, name);
            ::FreeLibrary(dll);
        }
    }
    return result;
#else
    return (void *)::glxGetProcAddressARB(name);
#endif
}

wxDECLARE_APP(MpvApp);

wxDEFINE_EVENT(WX_MPV_WAKEUP, wxThreadEvent);
wxDEFINE_EVENT(WX_MPV_REDRAW, wxThreadEvent);

wxBEGIN_EVENT_TABLE(MpvFrame, wxFrame)
    EVT_SIZE(MpvFrame::OnSize)
    EVT_CHAR_HOOK(MpvFrame::OnKeyDown)
    EVT_DROP_FILES(MpvFrame::OnDropFiles)
wxEND_EVENT_TABLE()

MpvFrame::MpvFrame()
    : wxFrame(nullptr, wxID_ANY, "mpv")
{
    SetBackgroundColour(wxColour(*wxBLACK));
    Center();
    DragAcceptFiles(true);

    glCanvas = new MpvGLCanvas(this);
    glCanvas->SetClientSize(GetClientSize());
    glCanvas->OnRender = std::bind(&MpvFrame::DoMpvDraw, this,
                                   std::placeholders::_2, std::placeholders::_3);
    glCanvas->OnSwapBuffers = std::bind(&MpvFrame::DoMpvFlip, this);

    MpvCreate();

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

void MpvFrame::MpvCreate()
{
    MpvDestroy();

    mpv = mpv_create();
    if (!mpv)
        throw std::runtime_error("failed to create mpv instance");

    if (mpv_initialize(mpv) < 0)
        throw std::runtime_error("failed to initialize mpv");

    mpv_opengl_init_params gl_init_params{
        [](void *ctx, const char *name) {
        auto glCanvas = reinterpret_cast<MpvGLCanvas *>(ctx);
        return glCanvas ? glCanvas->GetProcAddress(name) : nullptr;
        }, glCanvas, nullptr
    };
    mpv_render_param params[]{
        {MPV_RENDER_PARAM_API_TYPE,
        const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    if (mpv_render_context_create(&mpv_gl, mpv, params) < 0)
        throw std::runtime_error("failed to initialize mpv GL context");

    mpv_observe_property(mpv, 0, "media-title", MPV_FORMAT_NONE);

    Bind(WX_MPV_WAKEUP, &MpvFrame::OnMpvWakeupEvent, this);
    mpv_set_wakeup_callback(mpv, [](void *data) {
        auto window = reinterpret_cast<MpvFrame *>(data);
        if (window) {
            auto event = new wxThreadEvent(WX_MPV_WAKEUP);
            window->GetEventHandler()->QueueEvent(event);
        }
    }, this);

    Bind(WX_MPV_REDRAW, &MpvFrame::OnMpvRedrawEvent, this);
    mpv_render_context_set_update_callback(mpv_gl, [](void *data) {
        auto window = reinterpret_cast<MpvFrame *>(data);
        if (window) {
            auto event = new wxThreadEvent(WX_MPV_REDRAW);
            window->GetEventHandler()->QueueEvent(event);
        }
    }, this);
}

void MpvFrame::MpvDestroy()
{
    Unbind(WX_MPV_WAKEUP, &MpvFrame::OnMpvWakeupEvent, this);
    Unbind(WX_MPV_REDRAW, &MpvFrame::OnMpvRedrawEvent, this);

    if (mpv_gl) {
        mpv_render_context_set_update_callback(mpv_gl, nullptr, nullptr);
        mpv_render_context_free(mpv_gl);
        mpv_gl = nullptr;
    }

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

void MpvFrame::OnSize(wxSizeEvent &event)
{
    if (glCanvas)
        glCanvas->SetClientSize(GetClientSize());
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

void MpvFrame::OnMpvRedrawEvent(wxThreadEvent &)
{
    if (glCanvas)
        glCanvas->Render();
}

void MpvFrame::DoMpvDraw(int w, int h)
{
    if (mpv_gl) {
        mpv_opengl_fbo mpfbo{0, w, h, 0};
        int flip_y{1};
        mpv_render_param params[]{
            {MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo},
            {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
            {MPV_RENDER_PARAM_INVALID, nullptr}
        };
        mpv_render_context_render(mpv_gl, params);
    }
}

void MpvFrame::DoMpvFlip()
{
    if (mpv_gl)
        mpv_render_context_report_swap(mpv_gl);
}
