//
//  Editor.h
//  p31-plugin-common
//
//  The plugin's GUI as PluginBase sees it: a platform view that exists while
//  the host shows the editor. Implemented per plugin in Objective-C++; every
//  call arrives on the main thread.
//

#pragma once

#include <cstdint>

namespace p31 {

class Editor {
public:
    virtual ~Editor() = default;

    // Embed the editor in the host's window. `parent` is an NSView*.
    virtual bool attach(void* parent) = 0;
    // New size in points.
    virtual void setSize(uint32_t width, uint32_t height) = 0;
    virtual void setVisible(bool visible) = 0;
};

// Editor size in points.
struct EditorSizeLimits {
    uint32_t defaultWidth = 420;
    uint32_t defaultHeight = 460;
    uint32_t minWidth = 160;
    uint32_t minHeight = 160;
    uint32_t maxWidth = 4096;
    uint32_t maxHeight = 4096;
};

} // namespace p31
