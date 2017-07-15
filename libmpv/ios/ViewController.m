@import GLKit;
@import OpenGLES;

#import "ViewController.h"


#import <mpv/client.h>
#import <mpv/opengl_cb.h>

#import <stdio.h>
#import <stdlib.h>


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
    void *addr = CFBundleGetFunctionPointerForName(CFBundleGetBundleWithIdentifier(CFSTR("com.apple.opengles")), symbolName);
    CFRelease(symbolName);
    return addr;
}

static void glupdate(void *ctx);

@interface MpvClientOGLView : GLKView
    @property mpv_opengl_cb_context *mpvGL;
@end

@implementation MpvClientOGLView {
    GLint defaultFBO;
}
    
- (void)awakeFromNib
{
    [super awakeFromNib];
    self.context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    [EAGLContext setCurrentContext:self.context];

    // Configure renderbuffers created by the view
    self.drawableColorFormat = GLKViewDrawableColorFormatRGBA8888;
    self.drawableDepthFormat = GLKViewDrawableDepthFormatNone;
    self.drawableStencilFormat = GLKViewDrawableStencilFormatNone;
    
    defaultFBO = -1;
}
    
- (void)fillBlack
{
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
}
    
- (void)drawRect
{
    if (defaultFBO == -1)
    {
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &defaultFBO);
    }

    if (self.mpvGL)
    {
        mpv_opengl_cb_draw(self.mpvGL,
                           defaultFBO,
                           self.bounds.size.width * self.contentScaleFactor,
                           -self.bounds.size.height * self.contentScaleFactor);
    }
}

- (void)drawRect:(CGRect)rect
{
    [self drawRect];
}

@end



static void wakeup(void *);


static void glupdate(void *ctx)
{
    MpvClientOGLView *glView = (__bridge MpvClientOGLView *)ctx;
    // I'm still not sure what the best way to handle this is, but this
    // works.
    dispatch_async(dispatch_get_main_queue(), ^{
        [glView display];
    });
}


@interface ViewController ()
    
@property (nonatomic) IBOutlet MpvClientOGLView *glView;
- (void) readEvents;

@end

static void wakeup(void *context)
{
    ViewController *a = (__bridge ViewController *) context;
    [a readEvents];
}



@implementation ViewController {
    mpv_handle *mpv;
    dispatch_queue_t queue;
}
    
- (void)viewDidLoad {
    [super viewDidLoad];
    
    // Do any additional setup after loading the view, typically from a nib.

    mpv = mpv_create();
    if (!mpv) {
        printf("failed creating context\n");
        exit(1);
    }
    
    // request important errors
    NSString *logFile = [NSTemporaryDirectory() stringByAppendingPathComponent:@"mpv-log.txt"];
    NSLog(@"%@", logFile);
    check_error(mpv_set_option_string(mpv, "log-file", logFile.UTF8String));
    check_error(mpv_request_log_messages(mpv, "warn"));
    check_error(mpv_initialize(mpv));
    check_error(mpv_set_option_string(mpv, "vo", "opengl-cb"));
        
    mpv_opengl_cb_context *mpvGL = mpv_get_sub_api(mpv, MPV_SUB_API_OPENGL_CB);
    if (!mpvGL) {
        puts("libmpv does not have the opengl-cb sub-API.");
        exit(1);
    }
        
    [self.glView display];

    // pass the mpvGL context to our view
    self.glView.mpvGL = mpvGL;
    int r = mpv_opengl_cb_init_gl(mpvGL, NULL, get_proc_address, NULL);
    if (r < 0) {
        puts("gl init has failed.");
        exit(1);
    }
    mpv_opengl_cb_set_update_callback(mpvGL, glupdate, (__bridge void *)self.glView);
    
    // Deal with MPV in the background.
    queue = dispatch_queue_create("mpv", DISPATCH_QUEUE_SERIAL);
    dispatch_async(queue, ^{
        // Register to be woken up whenever mpv generates new events.
        mpv_set_wakeup_callback(mpv, wakeup, (__bridge void *)self);
        // Load the indicated file
        
        const char *cmd[] = {"loadfile", "http://download.blender.org/peach/bigbuckbunny_movies/BigBuckBunny_640x360.m4v", NULL};
        check_error(mpv_command(mpv, cmd));
        check_error(mpv_set_option_string(mpv, "loop", "inf"));
    });
}
    
- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}
    
- (void)handleEvent:(mpv_event *)event
{
    switch (event->event_id) {
        case MPV_EVENT_SHUTDOWN: {
            mpv_detach_destroy(mpv);
            mpv_opengl_cb_uninit_gl(self.glView.mpvGL);
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

- (void)readEvents
{
    dispatch_async(queue, ^{
        while (mpv) {
            mpv_event *event = mpv_wait_event(mpv, 0);
            if (event->event_id == MPV_EVENT_NONE)
            {
                break;
            }
            [self handleEvent:event];
        }
    });
}
    
@end
