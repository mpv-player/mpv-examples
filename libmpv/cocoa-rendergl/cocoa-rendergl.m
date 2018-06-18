// Plays a video from the command line in an opengl view in its own window.

// Build with: clang -o cocoa-rendergl cocoa-rendergl.m `pkg-config --libs --cflags mpv` -framework Cocoa -framework OpenGL

#import <mpv/client.h>
#import <mpv/render_gl.h>

#import <stdio.h>
#import <stdlib.h>
#import <OpenGL/gl.h>

#import <Cocoa/Cocoa.h>


static inline void check_error(int status)
{
    if (status < 0) {
        printf("mpv API error: %s\n", mpv_error_string(status));
        exit(1);
    }
}

static void *get_proc_address(void *ctx, const char *name)
{
    CFStringRef symbolName = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingASCII);
    void *addr = CFBundleGetFunctionPointerForName(CFBundleGetBundleWithIdentifier(CFSTR("com.apple.opengl")), symbolName);
    CFRelease(symbolName);
    return addr;
}

static void glupdate(void *ctx);

@interface MpvClientOGLView : NSOpenGLView
@property mpv_render_context *mpvGL;
- (instancetype)initWithFrame:(NSRect)frame;
- (void)drawRect;
- (void)fillBlack;
@end

@implementation MpvClientOGLView
- (instancetype)initWithFrame:(NSRect)frame
{
    // make sure the pixel format is double buffered so we can use
    // [[self openGLContext] flushBuffer].
    NSOpenGLPixelFormatAttribute attributes[] = {
        NSOpenGLPFADoubleBuffer,
        0
    };
    self = [super initWithFrame:frame
                    pixelFormat:[[NSOpenGLPixelFormat alloc] initWithAttributes:attributes]];

    if (self) {
        [self setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
        // swap on vsyncs
        GLint swapInt = 1;
        [[self openGLContext] setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
        [[self openGLContext] makeCurrentContext];
        self.mpvGL = nil;
    }
    return self;
}

- (void)fillBlack
{
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
}

- (void)drawRect
{
    if (self.mpvGL) {
        mpv_render_param params[] = {
            // Specify the default framebuffer (0) as target. This will
            // render onto the entire screen. If you want to show the video
            // in a smaller rectangle or apply fancy transformations, you'll
            // need to render into a separate FBO and draw it manually.
            {MPV_RENDER_PARAM_OPENGL_FBO, &(mpv_opengl_fbo){
                .fbo = 0,
                .w = self.bounds.size.width,
                .h = self.bounds.size.height,
            }},
            // Flip rendering (needed due to flipped GL coordinate system).
            {MPV_RENDER_PARAM_FLIP_Y, &(int){1}},
            {0}
        };
        // See render_gl.h on what OpenGL environment mpv expects, and
        // other API details.
        mpv_render_context_render(self.mpvGL, params);
    } else
        [self fillBlack];
    [[self openGLContext] flushBuffer];
}

- (void)drawRect:(NSRect)dirtyRect
{
    [self drawRect];
}
@end

@interface CocoaWindow : NSWindow
@property(retain, readonly) MpvClientOGLView *glView;
@property(retain, readonly) NSButton *pauseButton;
@end

@implementation CocoaWindow
- (BOOL)canBecomeMainWindow { return YES; }
- (BOOL)canBecomeKeyWindow { return YES; }
- (void)initOGLView {
    NSRect bounds = [[self contentView] bounds];
    // window coordinate origin is bottom left
    NSRect glFrame = NSMakeRect(bounds.origin.x, bounds.origin.y + 30, bounds.size.width, bounds.size.height - 30);
    _glView = [[MpvClientOGLView alloc] initWithFrame:glFrame];
    [self.contentView addSubview:_glView];

    NSRect buttonFrame = NSMakeRect(bounds.origin.x, bounds.origin.y, 60, 30);
    _pauseButton = [[NSButton alloc] initWithFrame:buttonFrame];
    _pauseButton.buttonType = NSToggleButton;
    // button target has to be the delegate (it holds the mpv context
    // pointer), so that's set later.
    _pauseButton.action = @selector(togglePause:);
    _pauseButton.title = @"Pause";
    _pauseButton.alternateTitle = @"Play";
    [self.contentView addSubview:_pauseButton];
}
@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
{
    mpv_handle *mpv;
    dispatch_queue_t queue;
    CocoaWindow *window;
}
@end

static void wakeup(void *);

@implementation AppDelegate

- (void)createWindow {

    int mask = NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|
               NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskResizable;

    window = [[CocoaWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, 1280, 720)
                  styleMask:mask
                    backing:NSBackingStoreBuffered
                      defer:NO];

    // force a minimum size to stop opengl from exploding.
    [window setMinSize:NSMakeSize(200, 200)];
    [window initOGLView];
    [window setTitle:@"cocoa-rendergl example"];
    [window makeMainWindow];
    [window makeKeyAndOrderFront:nil];

    NSMenu *m = [[NSMenu alloc] initWithTitle:@"AMainMenu"];
    NSMenuItem *item = [m addItemWithTitle:@"Apple" action:nil keyEquivalent:@""];
    NSMenu *sm = [[NSMenu alloc] initWithTitle:@"Apple"];
    [m setSubmenu:sm forItem:item];
    [sm addItemWithTitle: @"quit" action:@selector(terminate:) keyEquivalent:@"q"];
    [NSApp setMenu:m];
    [NSApp activateIgnoringOtherApps:YES];
}

- (void) applicationDidFinishLaunching:(NSNotification *)notification {
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    atexit_b(^{
        // Because activation policy has just been set to behave like a real
        // application, that policy must be reset on exit to prevent, among
        // other things, the menubar created here from remaining on screen.
        [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
    });

    // Read filename
    NSArray *args = [NSProcessInfo processInfo].arguments;
    if (args.count < 2) {
        NSLog(@"Expected filename on command line");
        exit(1);
    }
    NSString *filename = args[1];

    [self createWindow];
    window.pauseButton.target = self;

    mpv = mpv_create();
    if (!mpv) {
        printf("failed creating context\n");
        exit(1);
    }

    check_error(mpv_set_option_string(mpv, "input-media-keys", "yes"));
    // request important errors
    check_error(mpv_request_log_messages(mpv, "warn"));

    check_error(mpv_initialize(mpv));
    check_error(mpv_set_option_string(mpv, "vo", "libmpv"));
    
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, MPV_RENDER_API_TYPE_OPENGL},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &(mpv_opengl_init_params){
            .get_proc_address = get_proc_address,
        }},
        {0}
    };
    
    mpv_render_context *mpvGL;
    if (mpv_render_context_create(&mpvGL, mpv, params) < 0) {
        puts("failed to initialize mpv GL context");
        exit(1);
    }
    // pass the mpvGL context to our view
    window.glView.mpvGL = mpvGL;
    
    mpv_render_context_set_update_callback(mpvGL, glupdate, (__bridge void *)window.glView);

    // Deal with MPV in the background.
    queue = dispatch_queue_create("mpv", DISPATCH_QUEUE_SERIAL);
    dispatch_async(queue, ^{
        // Register to be woken up whenever mpv generates new events.
        mpv_set_wakeup_callback(mpv, wakeup, (__bridge void *)self);
        // Load the indicated file
        const char *cmd[] = {"loadfile", filename.UTF8String, NULL};
        check_error(mpv_command(mpv, cmd));
    });
}

static void glupdate(void *ctx)
{
    MpvClientOGLView *glView = (__bridge MpvClientOGLView *)ctx;
    // I'm still not sure what the best way to handle this is, but this
    // works.
    dispatch_async(dispatch_get_main_queue(), ^{
        [glView drawRect];
    });
}

- (void) handleEvent:(mpv_event *)event
{
    switch (event->event_id) {
    case MPV_EVENT_SHUTDOWN: {
        mpv_render_context_free(window.glView.mpvGL);
        mpv_terminate_destroy(mpv);
        mpv = NULL;
        printf("event: shutdown\n");
        break;
    }

    case MPV_EVENT_LOG_MESSAGE: {
        struct mpv_event_log_message *msg = (struct mpv_event_log_message *)event->data;
        printf("[%s] %s: %s", msg->prefix, msg->level, msg->text);
    }

    default:
        printf("event: %s\n", mpv_event_name(event->event_id));
    }
}

- (void)togglePause:(NSButton *)button
{
    if (mpv) {
        switch (button.state) {
            case NSOffState:
                {
                    int pause = 0;
                    mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &pause);
                }
                break;
            case NSOnState:
                {
                    int pause = 1;
                    mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &pause);
                }
                break;
            default:
                NSLog(@"This should never happen.");
        }
    }
}

- (void) readEvents
{
    dispatch_async(queue, ^{
        while (mpv) {
            mpv_event *event = mpv_wait_event(mpv, 0);
            if (event->event_id == MPV_EVENT_NONE)
                break;
            [self handleEvent:event];
        }
    });
}

static void wakeup(void *context)
{
    AppDelegate *a = (__bridge AppDelegate *) context;
    [a readEvents];
}

// quit when the window is closed.
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app
{
    return YES;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    NSLog(@"Terminating.");
    const char *args[] = {"quit", NULL};
    mpv_command(mpv, args);
    [window.glView clearGLContext];
    return NSTerminateNow;
}

@end

// Delete this if you already have a main.m.
int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        AppDelegate *delegate = [AppDelegate new];
        app.delegate = delegate;
        [app run];
    }
    return 0;
}
