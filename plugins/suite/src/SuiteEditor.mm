//
//  SuiteEditor.mm
//  p31-suite
//
//  Owns the suite view and one editor per shown panel: the same editors the
//  analyzers' own plugins have, each in its pane. A panel's editor exists
//  only while the panel is shown, so a hidden one costs nothing. Every
//  panel's right-click menu gets the Panels submenu, which shows and hides
//  panels and resets the arrangement.
//

#include "SuiteEditor.h"

#include "ScopeEditor.h"
#include "SpectrogramEditor.h"
#include "SpectrumEditor.h"
#include "SuitePlugin.h"

#import <p31/render/EditorRootView.h>
#import <p31/render/Menus.h>
#import <p31/render/SuiteView.h>

#include <algorithm>
#include <array>

namespace {

using namespace p31::suite;

// As the Panels menu lists them.
constexpr std::array<std::pair<int, const char*>, kPanelCount> kPanelNames{{
    {PanelSpectrum, "Spectrum"},
    {PanelScope, "Scope"},
    {PanelSpectrogram, "Spectrogram"},
}};

uint32_t points(CGFloat length) {
    return static_cast<uint32_t>(std::max<CGFloat>(0.0, length));
}

class SuiteEditor final : public p31::Editor {
public:
    SuiteEditor(SuitePlugin& plugin, uint32_t width, uint32_t height) : _plugin(plugin) {
        _view = [[P31SuiteView alloc] initWithFrame:NSMakeRect(0, 0, width, height)];

        SuiteEditor* editor = this;
        _view.onLayoutEdit = ^(const Layout& layout) {
            editor->_plugin.setLayout(layout);
            editor->showLayout();
        };
        _view.requestResize = ^BOOL(NSSize size) {
            return editor->_plugin.requestEditorResize(points(size.width), points(size.height));
        };

        showLayout();
    }

    ~SuiteEditor() override {
        // An edit still waiting for its turn finds nobody.
        _view.onLayoutEdit = nil;
        for (auto& panel : _panels) panel.reset();
        [_view removeFromSuperview];
        _view = nil;
    }

    bool attach(void* parent) override {
        NSView* parentView = (__bridge NSView*)parent;
        if (parentView == nil) return false;
        [parentView addSubview:_view];
        return true;
    }

    void setSize(uint32_t width, uint32_t height) override {
        [_view setFrameSize:NSMakeSize(width, height)];
    }

    void* view() const override { return (__bridge void*)_view; }

    void setVisible(bool visible) override {
        _view.hidden = !visible;
        for (auto& panel : _panels) {
            if (panel) panel->setVisible(visible);
        }
    }

    void settingsChanged() override {
        showLayout();
        for (auto& panel : _panels) {
            if (panel) panel->settingsChanged();
        }
    }

private:
    std::unique_ptr<p31::Editor> makePanel(int panel, uint32_t width, uint32_t height) {
        switch (panel) {
            case PanelSpectrum: return makeSpectrumEditor(_plugin.spectrum(), width, height);
            case PanelScope: return makeScopeEditor(_plugin.scope(), width, height);
            default: return makeSpectrogramEditor(_plugin.spectrogram(), width, height);
        }
    }

    // Show the plugin's arrangement: editors of hidden panels go, the view
    // places the panes, panels that are new get their editor.
    void showLayout() {
        const Layout& layout = _plugin.layout();
        for (int panel = 0; panel < kPanelCount; ++panel) {
            if (!layout.contains(panel)) _panels[panel].reset();
        }
        [_view setLayout:layout];

        SuiteEditor* editor = this;
        for (int panel : layout.panels()) {
            if (_panels[panel]) continue;
            NSView* pane = [_view paneForPanel:panel];
            _panels[panel] = makePanel(panel, points(pane.bounds.size.width), points(pane.bounds.size.height));

            // In a pane, the editor has no grip of its own, shows its
            // settings over the whole window and can be moved and hidden.
            P31EditorRootView* root = (__bridge P31EditorRootView*)_panels[panel]->view();
            root.requestResize = nil;
            root.settingsHost = _view;
            root.toolbarAccessories = [_view toolbarItemsForPanel:panel];
            root.extendMenu = ^(NSMenu* menu) { editor->addPanelsMenu(menu); };
            _panels[panel]->attach((__bridge void*)pane);
        }
    }

    void addPanelsMenu(NSMenu* menu) {
        const Layout layout = _plugin.layout();
        const NSSize size = _view.bounds.size;
        // The handlers run while the menu's panel may be on its way out:
        // they only ask the view, which comes back once the menu is done.
        P31SuiteView* view = _view;
        SuitePlugin* plugin = &_plugin;

        NSMenu* panels = [[NSMenu alloc] initWithTitle:@"Panels"];
        panels.autoenablesItems = NO;
        for (const auto& [panel, name] : kPanelNames) {
            const int id = panel;
            const bool shown = layout.contains(id);
            P31MenuItem* item = [P31MenuItem itemWithTitle:@(name) handler:^{
                Layout next = layout;
                if (shown ? next.remove(id) : next.show(id, size.width, size.height)) [view requestLayout:next];
            }];
            item.state = shown ? NSControlStateValueOn : NSControlStateValueOff;
            // The last panel stays.
            item.enabled = !shown || layout.count() > 1;
            [panels addItem:item];
        }
        [panels addItem:[NSMenuItem separatorItem]];
        [panels addItem:[P31MenuItem itemWithTitle:@"Reset Layout" handler:^{
            [view requestLayout:plugin->defaultLayout()];
        }]];
        [panels addItem:[NSMenuItem separatorItem]];
        [panels addItem:[P31MenuItem itemWithTitle:@"Use Current Layout for New Instances" handler:^{
            plugin->saveDefaultLayout();
        }]];
        P31MenuItem* clear = [P31MenuItem itemWithTitle:@"Reset New Instances to Factory Layout" handler:^{
            plugin->clearDefaultLayout();
        }];
        clear.enabled = _plugin.hasDefaultLayout();
        [panels addItem:clear];

        [menu addItem:[NSMenuItem separatorItem]];
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:@"Panels" action:nil keyEquivalent:@""];
        item.submenu = panels;
        [menu addItem:item];
    }

    SuitePlugin& _plugin;
    P31SuiteView* _view = nil;
    std::array<std::unique_ptr<p31::Editor>, kPanelCount> _panels;
};

} // namespace

std::unique_ptr<p31::Editor> makeSuiteEditor(SuitePlugin& plugin, uint32_t width, uint32_t height) {
    return std::make_unique<SuiteEditor>(plugin, width, height);
}
