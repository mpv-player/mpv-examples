#pragma once

#include <functional>

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include <wx/glcanvas.h>

#include <mpv/client.h>
#include <mpv/render_gl.h>

class MpvApp : public wxApp
{
public:
    bool OnInit() override;
};

class MpvGLCanvas : public wxGLCanvas
{
public:
    MpvGLCanvas(wxWindow *parent);
    ~MpvGLCanvas();

    void Render();
    bool SetCurrent() const;
    bool SwapBuffers() override;
    void *GetProcAddress(const char *name);

    std::function<void (wxGLCanvas *, int w, int h)> OnRender = nullptr;
    std::function<void (wxGLCanvas *)> OnSwapBuffers = nullptr;

private:
    void OnSize(wxSizeEvent &event);
    void OnPaint(wxPaintEvent &event);
    void OnErase(wxEraseEvent &event);

    void DoRender();

    wxGLContext *glContext = nullptr;

    wxDECLARE_EVENT_TABLE();
};

class MpvFrame : public wxFrame
{
public:
    MpvFrame();

    bool Destroy() override;
    bool Autofit(int percent, bool larger = true, bool smaller = true);

private:
    void MpvCreate();
    void MpvDestroy();

    void OnSize(wxSizeEvent &event);
    void OnKeyDown(wxKeyEvent &event);
    void OnDropFiles(wxDropFilesEvent &event);

    void OnMpvEvent(mpv_event &event);
    void OnMpvWakeupEvent(wxThreadEvent &event);
    void OnMpvRedrawEvent(wxThreadEvent &event);

    void DoMpvDraw(int w, int h);
    void DoMpvFlip();

    mpv_handle *mpv = nullptr;
    mpv_render_context *mpv_gl = nullptr;
    MpvGLCanvas *glCanvas = nullptr;

    wxDECLARE_EVENT_TABLE();
};
