//
//  SuiteLayoutTests.cpp
//  p31-core-tests
//
//  Unit tests for P31 Suite's panel arrangement: the edits (hide, show, move,
//  swap), the geometry and the stored text.
//

#include <p31/suite/SuiteLayout.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

using namespace p31::suite;

namespace {

Rect paneRect(const Layout& layout, int panel, double w = 1000, double h = 600) {
    for (const Layout::Pane& p : layout.arrange(w, h).panes) {
        if (p.panel == panel) return p.rect;
    }
    return {-1, -1, -1, -1};
}

bool same(const Rect& r, double x, double y, double w, double h) {
    return r.x == x && r.y == y && r.w == w && r.h == h;
}

// Panes and gaps tile the window: no overlap, nothing left over.
void checkTiles(const Layout& layout, double w, double h) {
    const Layout::Arrangement a = layout.arrange(w, h);
    CHECK(a.panes.size() == static_cast<size_t>(layout.count()));
    CHECK(a.dividers.size() + 1 == a.panes.size());
    double area = 0.0;
    for (const Layout::Pane& p : a.panes) {
        CHECK(p.rect.x >= 0.0);
        CHECK(p.rect.y >= 0.0);
        CHECK(p.rect.x + p.rect.w <= w);
        CHECK(p.rect.y + p.rect.h <= h);
        CHECK(p.rect.w == std::round(p.rect.w));
        CHECK(p.rect.h == std::round(p.rect.h));
        area += p.rect.w * p.rect.h;
    }
    for (const Layout::Divider& d : a.dividers) area += d.rect.w * d.rect.h;
    CHECK(area == doctest::Approx(w * h));
    for (size_t i = 0; i < a.panes.size(); ++i) {
        for (size_t j = i + 1; j < a.panes.size(); ++j) {
            const Rect &p = a.panes[i].rect, &q = a.panes[j].rect;
            const bool overlap = p.x < q.x + q.w && q.x < p.x + p.w && p.y < q.y + q.h && q.y < p.y + p.h;
            CHECK_FALSE(overlap);
        }
    }
}

} // namespace

TEST_CASE("SuiteLayout: the default shows every panel") {
    const Layout layout;
    CHECK(layout.count() == kPanelCount);
    // Scope in the left 30 %, spectrum over spectrogram in the rest.
    CHECK(same(paneRect(layout, PanelScope), 0, 0, 300, 600));
    CHECK(same(paneRect(layout, PanelSpectrum), 301, 0, 699, 300));
    CHECK(same(paneRect(layout, PanelSpectrogram), 301, 301, 699, 299));
    checkTiles(layout, 1000, 600);
    checkTiles(layout, 1281, 733);
}

TEST_CASE("SuiteLayout: hiding a panel gives its area to the neighbour") {
    Layout layout;
    CHECK(layout.remove(PanelSpectrum));
    CHECK_FALSE(layout.contains(PanelSpectrum));
    CHECK(same(paneRect(layout, PanelSpectrogram), 301, 0, 699, 600));
    CHECK_FALSE(layout.remove(PanelSpectrum));   // already hidden

    CHECK(layout.remove(PanelScope));
    CHECK(same(paneRect(layout, PanelSpectrogram), 0, 0, 1000, 600));
    // The last panel stays.
    CHECK_FALSE(layout.remove(PanelSpectrogram));
    CHECK(layout.count() == 1);
    checkTiles(layout, 1000, 600);
}

TEST_CASE("SuiteLayout: a panel is shown beside another or along an edge") {
    Layout layout;
    layout.remove(PanelSpectrum);
    layout.remove(PanelScope);

    CHECK(layout.insert(PanelScope, PanelSpectrogram, Edge::Top));
    CHECK(same(paneRect(layout, PanelScope), 0, 0, 1000, 300));
    CHECK(same(paneRect(layout, PanelSpectrogram), 0, 301, 1000, 299));
    CHECK_FALSE(layout.insert(PanelScope, PanelSpectrogram, Edge::Left));    // already shown
    CHECK_FALSE(layout.insert(PanelSpectrum, PanelSpectrum, Edge::Left));    // target hidden
    CHECK_FALSE(layout.insert(7, PanelScope, Edge::Left));

    // Along the right edge, a third of the width for the third panel.
    CHECK(layout.insertAtEdge(PanelSpectrum, Edge::Right));
    CHECK(same(paneRect(layout, PanelSpectrum), 667, 0, 333, 600));
    CHECK(same(paneRect(layout, PanelScope), 0, 0, 666, 300));
    checkTiles(layout, 1000, 600);
}

TEST_CASE("SuiteLayout: show picks the largest panel and its longer side") {
    Layout layout;
    layout.remove(PanelSpectrogram);
    // Scope 300 x 600, spectrum 699 x 600: the spectrum is halved side by side.
    CHECK(layout.show(PanelSpectrogram, 1000, 600));
    CHECK(same(paneRect(layout, PanelSpectrum), 301, 0, 349, 600));
    CHECK(same(paneRect(layout, PanelSpectrogram), 651, 0, 349, 600));
    CHECK_FALSE(layout.show(PanelSpectrogram, 1000, 600));

    layout.remove(PanelSpectrogram);
    // In a tall window it goes below instead.
    CHECK(layout.show(PanelSpectrogram, 600, 1000));
    CHECK(paneRect(layout, PanelSpectrogram, 600, 1000).y > 0);
    checkTiles(layout, 600, 1000);
}

TEST_CASE("SuiteLayout: move and swap") {
    Layout layout;
    // The scope goes from the left column to under the spectrogram.
    CHECK(layout.move(PanelScope, PanelSpectrogram, Edge::Bottom));
    CHECK(same(paneRect(layout, PanelSpectrum), 0, 0, 1000, 300));
    CHECK(paneRect(layout, PanelScope).y > paneRect(layout, PanelSpectrogram).y);
    CHECK(layout.count() == kPanelCount);
    CHECK_FALSE(layout.move(PanelScope, PanelScope, Edge::Left));

    CHECK(layout.moveToEdge(PanelSpectrum, Edge::Left));
    CHECK(same(paneRect(layout, PanelSpectrum), 0, 0, 333, 600));

    const Rect a = paneRect(layout, PanelSpectrum), b = paneRect(layout, PanelScope);
    CHECK(layout.swap(PanelSpectrum, PanelScope));
    CHECK(same(paneRect(layout, PanelScope), a.x, a.y, a.w, a.h));
    CHECK(same(paneRect(layout, PanelSpectrum), b.x, b.y, b.w, b.h));
    CHECK_FALSE(layout.swap(PanelScope, PanelScope));

    // A hidden panel neither moves nor swaps, and the layout is left alone.
    layout.remove(PanelScope);
    const Layout before = layout;
    CHECK_FALSE(layout.move(PanelScope, PanelSpectrum, Edge::Left));
    CHECK_FALSE(layout.moveToEdge(PanelScope, Edge::Left));
    CHECK_FALSE(layout.swap(PanelScope, PanelSpectrum));
    CHECK(layout == before);

    // The last panel has nowhere to move to.
    layout.remove(PanelSpectrogram);
    CHECK_FALSE(layout.moveToEdge(PanelSpectrum, Edge::Top));
    CHECK(layout.count() == 1);
}

TEST_CASE("SuiteLayout: dividers set the ratios") {
    Layout layout;
    const Layout::Arrangement a = layout.arrange(1000, 600);
    REQUIRE(a.dividers.size() == 2);
    // The first is the vertical line right of the scope.
    const Layout::Divider& d = a.dividers[0];
    CHECK(d.sideBySide);
    CHECK(same(d.rect, 300, 0, 1, 600));
    CHECK(d.ratioAt(300.5) == doctest::Approx(0.3).epsilon(0.001));

    layout.setRatio(d.split, d.ratioAt(500.5));
    CHECK(same(paneRect(layout, PanelScope), 0, 0, 500, 600));

    // Dragged past the edge: the panels keep their minimum sizes.
    layout.setRatio(d.split, d.ratioAt(-50));
    CHECK(paneRect(layout, PanelScope).w == kMinPanelWidth);
    layout.setRatio(d.split, d.ratioAt(5000));
    CHECK(paneRect(layout, PanelSpectrum).w == kMinPanelWidth);

    // Not a split: ignored.
    const Layout before = layout;
    layout.setRatio(1, 0.9);
    layout.setRatio(99, 0.9);
    layout.setRatio(d.split, std::nan(""));
    CHECK(layout == before);
}

TEST_CASE("SuiteLayout: a window too small for the minimums shares what there is") {
    const Layout layout;
    checkTiles(layout, 200, 100);
    for (const Layout::Pane& p : layout.arrange(200, 100).panes) {
        CHECK(p.rect.w > 0.0);
        CHECK(p.rect.h > 0.0);
    }
    checkTiles(layout, 0, 0);
}

TEST_CASE("SuiteLayout: text round trip") {
    Layout layout;
    layout.move(PanelSpectrogram, PanelScope, Edge::Top);
    layout.setRatio(0, 0.4321);
    const std::string text = layout.encode();

    Layout loaded;
    CHECK_FALSE(loaded == layout);
    CHECK(loaded.decode("other=1\n" + text));
    CHECK(loaded == layout);
    CHECK(loaded.encode() == text);

    Layout one;
    one.remove(PanelScope);
    one.remove(PanelSpectrum);
    CHECK(one.encode() == "layout.count=1\nlayout.0.kind=0\nlayout.0.value=2\n");
    CHECK(loaded.decode(one.encode()));
    CHECK(loaded.count() == 1);
}

TEST_CASE("SuiteLayout: broken text is refused and changes nothing") {
    Layout layout;
    const Layout before = layout;
    CHECK_FALSE(layout.decode(""));
    CHECK_FALSE(layout.decode("layout.count=0\n"));
    CHECK_FALSE(layout.decode("layout.count=9\n"));
    // A node is missing.
    CHECK_FALSE(layout.decode("layout.count=3\nlayout.0.kind=1\nlayout.0.value=5000\nlayout.1.kind=0\nlayout.1.value=0\n"));
    // A split with one side.
    CHECK_FALSE(layout.decode("layout.count=2\nlayout.0.kind=1\nlayout.0.value=5000\nlayout.1.kind=0\nlayout.1.value=0\n"));
    // Two trees.
    CHECK_FALSE(layout.decode("layout.count=2\nlayout.0.kind=0\nlayout.0.value=0\nlayout.1.kind=0\nlayout.1.value=1\n"));
    // The same panel twice.
    CHECK_FALSE(layout.decode("layout.count=3\nlayout.0.kind=2\nlayout.0.value=5000\nlayout.1.kind=0\nlayout.1.value=1\n"
                              "layout.2.kind=0\nlayout.2.value=1\n"));
    // Unknown panel and kind.
    CHECK_FALSE(layout.decode("layout.count=1\nlayout.0.kind=0\nlayout.0.value=3\n"));
    CHECK_FALSE(layout.decode("layout.count=1\nlayout.0.kind=5\nlayout.0.value=0\n"));
    CHECK(layout == before);

    // A ratio out of range is pulled back in.
    CHECK(layout.decode("layout.count=3\nlayout.0.kind=1\nlayout.0.value=999999\nlayout.1.kind=0\nlayout.1.value=0\n"
                        "layout.2.kind=0\nlayout.2.value=1\n"));
    CHECK(layout.arrange(1000, 600).panes[1].rect.w == kMinPanelWidth);
}
