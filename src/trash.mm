#import <Foundation/Foundation.h>
#include "trash.h"

bool TrashPath(const std::string& path, std::string& outError) {
    @autoreleasepool {
        NSString* nsPath = [NSString stringWithUTF8String:path.c_str()];
        if (!nsPath) {
            outError = "invalid path encoding";
            return false;
        }
        NSURL* url = [NSURL fileURLWithPath:nsPath];
        NSError* err = nil;
        BOOL ok = [[NSFileManager defaultManager] trashItemAtURL:url
                                                resultingItemURL:nil
                                                           error:&err];
        if (!ok) {
            outError = err ? [[err localizedDescription] UTF8String]
                           : "unknown error";
            return false;
        }
        return true;
    }
}
