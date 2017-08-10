// Build with: gcc -o simple-streamcb simple-streamcb.c `pkg-config --libs --cflags mpv`

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <mpv/client.h>
#include <mpv/stream_cb.h>

static int64_t size_fn(void *cookie)
{
    FILE *fp = (FILE*) cookie;
    struct stat st;
    if(fstat(fileno(fp),&st)) {
        return MPV_ERROR_UNSUPPORTED;
    }
    return st.st_size;
}

static int64_t read_fn(void *cookie, char *buf, uint64_t nbytes)
{
    FILE *fp = cookie;
    size_t ret = fread(buf, 1, nbytes, fp);
    if (ret == 0) {
        return feof(fp) ? 0 : -1;
    }
    return ret;
}

static int64_t seek_fn(void *cookie, int64_t offset)
{
    FILE *fp = cookie;
    // not 64-bit safe
    int r = fseek(fp, offset, SEEK_SET);
    return r < 0 ? MPV_ERROR_GENERIC : r;
}

static void close_fn(void *cookie)
{
    FILE *fp = cookie;
    fclose(fp);
}

static int open_fn(void *user_data, char *uri, mpv_stream_cb_info *info)
{
    FILE *fp = fopen((char *)user_data, "r");
    info->cookie = fp;
    info->size_fn = size_fn;
    info->read_fn = read_fn;
    info->seek_fn = seek_fn;
    info->close_fn = close_fn;
    return fp ? 0 : MPV_ERROR_LOADING_FAILED;
}

static inline void check_error(int status)
{
    if (status < 0) {
        printf("mpv API error: %s\n", mpv_error_string(status));
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("pass a single media file as argument\n");
        return 1;
    }

    mpv_handle *ctx = mpv_create();
    if (!ctx) {
        printf("failed creating context\n");
        return 1;
    }

    // Enable default key bindings, so the user can actually interact with
    // the player (and e.g. close the window).
    check_error(mpv_set_option_string(ctx, "input-default-bindings", "yes"));

    mpv_set_option_string(ctx, "input-vo-keyboard", "yes");
    int val = 1;
    check_error(mpv_set_option(ctx, "osc", MPV_FORMAT_FLAG, &val));

    // Done setting up options.
    check_error(mpv_initialize(ctx));

    check_error(mpv_request_log_messages(ctx, "v"));

    check_error(mpv_stream_cb_add_ro(ctx, "myprotocol", argv[1], open_fn));

    // Play this file.
    const char *cmd[] = {"loadfile", "myprotocol://fake", NULL};
    check_error(mpv_command(ctx, cmd));

    // Let it play, and wait until the user quits.
    while (1) {
        mpv_event *event = mpv_wait_event(ctx, 10000);
        if (event->event_id == MPV_EVENT_LOG_MESSAGE) {
            struct mpv_event_log_message *msg = (struct mpv_event_log_message *)event->data;
            printf("[%s] %s: %s", msg->prefix, msg->level, msg->text);
            continue;
        }
        printf("event: %s\n", mpv_event_name(event->event_id));
        if (event->event_id == MPV_EVENT_SHUTDOWN)
            break;
    }

    mpv_terminate_destroy(ctx);
    return 0;
}
