#import <ScreenSaver/ScreenSaver.h>

// NSPrincipalClass named in Info.plist -- legacyScreenSaver instantiates one
// of these per display via initWithFrame:isPreview:, same "one view per
// screen" shape runMultiDisplayStreamLoop hand-rolls on Windows/Linux
// (core/app_loop.cpp), except the OS does the enumeration/placement for us
// here, so this class only ever drives a single StreamField for its own
// frame.
@interface GlyphRainView : ScreenSaverView
@end
