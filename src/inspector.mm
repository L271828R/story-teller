#import <WebKit/WebKit.h>
#include "inspector.h"

void EnableWebInspector(wxWebView* webView) {
    WKWebView* wk = (WKWebView*)webView->GetNativeBackend();
    if (!wk) return;
    // macOS 13.3+ requires isInspectable; older versions use the preference key.
    if (@available(macOS 13.3, *)) {
        wk.inspectable = YES;
    } else {
        [wk.configuration.preferences setValue:@YES forKey:@"developerExtrasEnabled"];
    }
}
