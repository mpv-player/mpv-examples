// Build with: gcc -o gtk.so gtk.c `pkg-config --cflags mpv` `pkg-config --cflags --libs gtk+-3.0 x11` -pthread -shared -fPIC

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// For mpv_gtk_helper_run() and mpv_gtk_helper_done(). Also pulls in headers.
#include "mpv_gtk_helper.inc"

// The following code is partially derived from the GTK tutorial example.

struct plugin_context {
    mpv_gtk_helper_context *helper;
    GtkWidget *pbar;
};

static gboolean delete_event( GtkWidget *widget,
                              GdkEvent  *event,
                              gpointer   data )
{
    // Exit UI.

    struct plugin_context *ctx = data;

    if (ctx->helper) {
        mpv_gtk_helper_context_destroy(ctx->helper);
        ctx->helper = NULL;
    }

    return FALSE;
}

static gboolean handle_mpv_events(void *data)
{
    struct plugin_context *ctx = data;

    if (!ctx->helper)
        return FALSE;

    while (1) {
        mpv_event *event = mpv_wait_event(ctx->helper->mpv, 0);
        if (event->event_id == MPV_EVENT_NONE)
            break;

        printf("event: %s\n", mpv_event_name(event->event_id));

        if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            mpv_event_property *prop = event->data;
            if (prop->format == MPV_FORMAT_INT64)
                gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ctx->pbar),
                                              *(int64_t*)prop->data / 100.0);
        }

        if (event->event_id == MPV_EVENT_SHUTDOWN) {
            mpv_gtk_helper_context_destroy(ctx->helper);
            ctx->helper = NULL;
            break;
        }
    }

    return FALSE;
}

static void wakeup_mpv(void *data)
{
    // wakeup_mpv is called in context of an arbitrary mpv thread.
    // Run our GUI code on the GTK thread by notifying the mainloop.
    g_idle_add(handle_mpv_events, data);
}

static void setup_gtk_stuff(mpv_gtk_helper_context *helper)
{
    // Out of severe lazyness, you don't free ctx. This is left as exercise
    // to the reader.
    struct plugin_context *ctx = calloc(1, sizeof(*ctx));
    ctx->helper = helper;

    // Make mpv notify us if there are new events.
    mpv_set_wakeup_callback(ctx->helper->mpv, wakeup_mpv, ctx);

    mpv_observe_property(ctx->helper->mpv, 0, "percent-pos", MPV_FORMAT_INT64);

    GtkWidget *window;

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect (window, "delete-event",
                      G_CALLBACK (delete_event), ctx);
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    ctx->pbar = gtk_progress_bar_new ();
    gtk_container_add (GTK_CONTAINER (window), ctx->pbar);
    gtk_widget_show (ctx->pbar);
    gtk_widget_show (window);
}

int mpv_open_cplugin(mpv_handle *handle)
{
    printf("Hello world from C plugin '%s'!\n", mpv_client_name(handle));

    return mpv_gtk_helper_run(handle, &setup_gtk_stuff);
}
