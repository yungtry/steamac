/* wmtr_present_test: the overlay presenter (WMTR_PRESENT=overlay) attaches the game's CAMetalLayer as a
 * sublayer of the launcher's VMView layer and detaches it on release; called from a non-main thread like
 * the executor does. Oracle: the AppKit layer tree. */
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#include <stdlib.h>
#include "wmtr_host.h"

@interface TestVMView : NSView
@property(nonatomic) BOOL gameLayersVisible;
@property int policyApplied; /* the launcher's didSet re-applies visibility and Metal HUD placement */
@end
@implementation TestVMView
- (CALayer *)makeBackingLayer { return [CAMetalLayer layer]; }
- (void)setGameLayersVisible:(BOOL)v { _gameLayersVisible = v; _policyApplied++; }
@end

int wmtr_debug_layer_policy_frames(void *layer, int frames);
static int failures;
#define CHECK(c, m) do { if (!(c)) { failures++; fprintf(stderr, "FAIL: %s\n", m); } } while (0)

int main(void) {
  { /* the view re-applies its layer policy (Metal HUD) on a game layer's 1st, 10th and 60th frames only */
    CAMetalLayer *l = [CAMetalLayer layer];
    int n = wmtr_debug_layer_policy_frames((__bridge void *)l, 100);
    if (n != 3) { failures++; fprintf(stderr, "FAIL: policy re-applied %d times in 100 frames, want 3\n", n); }
  }
  @autoreleasepool {
    setenv("WMTR_PRESENT", "overlay", 1);
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    NSWindow *w = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 320, 200) styleMask:NSWindowStyleMaskBorderless
                                                backing:NSBackingStoreBuffered defer:NO];
    NSView *root = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 320, 200)];
    TestVMView *vm = [[TestVMView alloc] initWithFrame:NSMakeRect(0, 0, 320, 200)];
    vm.wantsLayer = YES;
    [root addSubview:vm];
    [w setContentView:root];
    id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
      uint64_t layer = 0;
      uint64_t view = wmtr_present_create_view(NULL, 0x1234, (void *)dev, &layer);
      dispatch_sync(dispatch_get_main_queue(), ^{
        CHECK(view == (uint64_t)vm, "returned view is not the VMView");
        CHECK(layer && [vm.layer.sublayers containsObject:(CALayer *)layer], "game layer not attached to the VMView layer");
        CHECK(layer && ((CAMetalLayer *)layer).device == dev, "layer device");
        CHECK(layer && NSEqualRects(NSRectFromCGRect(((CALayer *)layer).frame), NSRectFromCGRect(vm.layer.bounds)), "layer frame");
        CHECK(layer && ((CALayer *)layer).hidden, "game layer shown although no game has the guest's focus");
        CHECK(layer && ((CAMetalLayer *)layer).maximumDrawableCount == 2, "game layer does not default to 2 drawables");
        CHECK(((TestVMView *)vm).policyApplied >= 1, "view's layer policy not re-applied after adding the game layer");
      });
      wmtr_present_release_view(NULL, view);
      dispatch_async(dispatch_get_main_queue(), ^{
        dispatch_async(dispatch_get_main_queue(), ^{
          CHECK(![vm.layer.sublayers count], "game layer not detached");
          printf(failures ? "wmtr_present_test: %d FAILED\n" : "wmtr_present_test: all passed\n", failures);
          exit(failures != 0);
        });
      });
    });
    [NSApp run];
  }
  return 1;
}
