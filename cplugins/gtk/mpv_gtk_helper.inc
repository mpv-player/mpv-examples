/*
 * This is a helper for running GTK UIs within a mpv C plugin. This helper
 * tries to take care of initializing GTK and running GTK code in a way
 * that won't conflict between multiple plugins, or if this runs within
 * libmpv with an existing GTK host.
 *
 * While this "works", there are unsolved theoretical race conditions. Most
 * likely they will remain theoretical.
 *
 * How to use:
 *
 * - call mpv_gtk_helper_run() in mpv_open_cplugin()
 * - create your GUI in the callback you pass to mpv_gtk_helper_run()
 * - once your GUI is built, you can return from the function - at this
 *   point, a GTK mainloop will run, your GUI should appear, and the GTK
 *   signals you connected should be working
 * - you need to keep mpv_gtk_helper_context around
 * - you can use mpv_gtk_helper_context.mpv to access libmpv functions
 * - once you're done or if you get MPV_EVENT_SHUTDOWN, you must call
 *   mpv_gtk_helper_context_destroy()
 * - this will destroy both the mpv_handle and mpv_gtk_helper_context
 * - it will most likely also destroy the GTK UI
 *
 * Caveats:
 *
 * - some race conditions
 * - mpv_gtk_helper_context_destroy()
 * - gtk_main_quit() will not work as expected
 * - deinitialization does not happen with GTK's agreement - instead, mpv
 *   will exit from main(), which kills the GTK thread without the chance
 *   to run additional deallocation and so on
 *
 * Additional build flags:
 *
 *   `pkg-config --cflags --libs gtk+-3.0 x11` -pthread
 *
 * License: anything you like as long as you won't sue me
 */

#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>

#include <mpv/client.h>
#include <gtk/gtk.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef struct mpv_gtk_helper_context mpv_gtk_helper_context;

typedef void (*mpv_gtk_helper_user_fn)(mpv_gtk_helper_context *ctx);

static int mpv_gtk_helper_run(mpv_handle *mpv, mpv_gtk_helper_user_fn fn);
static void mpv_gtk_helper_context_destroy(mpv_gtk_helper_context *ctx);

struct mpv_gtk_helper_context {
    mpv_handle *mpv;

    // Internal stuff.
    mpv_gtk_helper_user_fn user_fn;
    int return_value;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int running;
};

// Internal.
static gboolean mpv_gtk_helper_on_main_thread(void *data)
{
    mpv_gtk_helper_context *ctx = data;
    ctx->user_fn(ctx);
    return FALSE;
}

// Internal.
// If GTK is not running yet, this thread will be used as GTK's "main thread".
// If GTK is already running, we exit the thread immediately after dispatching
// the user code.
// This is done on a separate thread because if we call gtk_main(), we want it
// to be able to run other GTK plugins even when the current plugin exits and
// terminates.
static void *mpv_gtk_helper_thread(void *data)
{
    mpv_gtk_helper_context *ctx = data;

    pthread_detach(pthread_self());

    GMainContext *main_context = g_main_context_default();

    if (g_main_context_acquire(main_context)) {
        // If we can get here, it means no GTK mainloop is running (as
        // gtk_main() always uses the default GMainContext. So we run GTK
        // in this thread.
        // Keep in mind that g_main_context_acquire() records our thread's
        // handle as owner, so this must be run in a dedicated thread.
        // There is a subtle race condition:
        //    gtk_main() implicitly acquires/releases the main context by
        //    calling g_main_loop_run(), but the code outside of this call
        //    is unprotected - our own gtk_main() call can step over it.
        // => undefined behavior

        gtk_disable_setlocale();

        // We don't have these anymore, so make something up.
        int argc = 1;
        char **argv = (char*[]){"dummy", NULL};

        if (!gtk_init_check(&argc, &argv)) {
            ctx->return_value = -1;
            mpv_gtk_helper_context_destroy(ctx);
            return NULL;
        }

        ctx->user_fn(ctx);

        gtk_main();

        g_main_context_release(main_context);
    } else {
        // A GTK mainloop is running - run the plugin code on it.
        // There is a subtle race condition:
        //    If we fail to g_main_context_acquire(), it could still happen
        //    that the owner releases the GMainLoop before we get to make
        //    it run something with g_main_context_invoke().
        // => callback is enqueued but never called, the plugin won't "start"
        //    and mpv will get stuck waiting for it

        g_main_context_invoke(main_context, mpv_gtk_helper_on_main_thread, ctx);
    }

    return NULL;
}

// Run the helper. This function will call fn() on the right thread after
// doing basic GTK initialization. It will block until the user calls
// mpv_gtk_helper_context_destroy().
static int mpv_gtk_helper_run(mpv_handle *mpv, mpv_gtk_helper_user_fn fn)
{
    // Without this GTK's xlib use will conflict with mpv's.
    // There is a subtle race condition:
    //    The function is implemented in an unsafe way. It checks for a global
    //    flag whether it's initialized, instead of using pthread_once.
    // => undefined behavior
    XInitThreads();

    // Make mpv think initialization finished (i.e. continue loading).
    mpv_wait_event(mpv, -1);

    mpv_gtk_helper_context ctx = {
        .mpv = mpv,
        .user_fn = fn,
        .return_value = 0,
        .running = 1,
    };

    pthread_mutex_init(&ctx.mutex, NULL);
    pthread_cond_init(&ctx.cond, NULL);

    pthread_t thread;
    if (pthread_create(&thread, NULL, mpv_gtk_helper_thread, &ctx)) {
        ctx.return_value = -1;
        ctx.running = 0;
    }

    pthread_mutex_lock(&ctx.mutex);
    while (ctx.running)
        pthread_cond_wait(&ctx.cond, &ctx.mutex);
    pthread_mutex_unlock(&ctx.mutex);

    pthread_mutex_destroy(&ctx.mutex);
    pthread_cond_destroy(&ctx.cond);

    return ctx.return_value;
}

// Destroy the context and associated resources (in particular ctx->mpv).
// Once this function is called, ctx and ctx->mpv must be considered invalid.
// (Technically, ctx is deallocated lazily, but you can't know when exactly
// this happens.)
// Keep in mind that the process might be killed once this is called, because
// mpv might return from main().
static void mpv_gtk_helper_context_destroy(mpv_gtk_helper_context *ctx)
{
    pthread_mutex_lock(&ctx->mutex);
    ctx->running = 0;
    pthread_cond_broadcast(&ctx->cond);
    pthread_mutex_unlock(&ctx->mutex);
}
