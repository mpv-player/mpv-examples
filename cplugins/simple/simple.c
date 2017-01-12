// Build with: gcc -o simple.so simple.c `pkg-config --cflags mpv` -shared -fPIC
// Warning: do not link against libmpv.so! Read:
//    https://mpv.io/manual/master/#linkage-to-libmpv
// The pkg-config call is for adding the proper client.h include path.

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <mpv/client.h>

int mpv_open_cplugin(mpv_handle *handle)
{
    printf("Hello world from C plugin '%s'!\n", mpv_client_name(handle));
    while (1) {
        mpv_event *event = mpv_wait_event(handle, -1);
        printf("Got event: %d\n", event->event_id);
        if (event->event_id == MPV_EVENT_SHUTDOWN)
            break;
    }
    return 0;
}

