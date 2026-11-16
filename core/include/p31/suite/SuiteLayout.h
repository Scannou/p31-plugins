//
//  SuiteLayout.h
//  p31-core
//
//  How P31 Suite's panels share its window: a tree of splits, each dividing
//  its area between two children side by side or one above the other, with
//  a panel in every leaf. A panel that is not in the tree is hidden; at
//  least one always is. Pure C++, so it is unit-tested standalone; the view
//  only places what arrange() returns.
//

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace p31::suite {

// The panels. The values are stored in the state; do not renumber.
enum Panel {
    PanelSpectrum = 0,
    PanelScope = 1,
    PanelSpectrogram = 2,
    kPanelCount = 3,
};

enum class Edge { Left, Right, Top, Bottom };

// Points, y down.
struct Rect {
    double x = 0.0, y = 0.0, w = 0.0, h = 0.0;
};

constexpr double kGap = 1.0;             // between neighbouring panels
constexpr double kMinPanelWidth = 200.0;
constexpr double kMinPanelHeight = 100.0;

class Layout {
public:
    // The scope on the left, the spectrum above the spectrogram beside it.
    Layout();

    bool contains(int panel) const;
    // Panels shown, in tree order.
    std::vector<int> panels() const;
    int count() const { return static_cast<int>(panels().size()); }

    // Every edit returns false, changing nothing, when it does not apply.

    // Hide a panel: its neighbour takes over the area. Not the last one.
    bool remove(int panel);
    // Show a hidden panel beside `target`, in the half of its area at `edge`.
    bool insert(int panel, int target, Edge edge);
    // Show a hidden panel along a whole edge of the window, as wide (or
    // tall) as an equal share with the panels already there.
    bool insertAtEdge(int panel, Edge edge);
    // Show a hidden panel where there is most room: in the largest panel of
    // a `width` x `height` window, halved across its longer side.
    bool show(int panel, double width, double height);
    // The same for a panel that is shown: it leaves its place first.
    bool move(int panel, int target, Edge edge);
    bool moveToEdge(int panel, Edge edge);
    // Two shown panels trade places.
    bool swap(int a, int b);

    struct Pane {
        int panel = 0;
        Rect rect;
    };
    // The gap between a split's two sides.
    struct Divider {
        int split = 0;             // for setRatio; valid until the next edit
        bool sideBySide = true;    // the divider is a vertical line
        Rect rect;
        double from = 0.0;         // where the split's area starts along its axis
        double room = 0.0;         // length the two sides share
        // The ratio that puts the divider's middle at `position` (x or y).
        double ratioAt(double position) const;
    };
    struct Arrangement {
        std::vector<Pane> panes;
        std::vector<Divider> dividers;
    };
    // Whole points. Panels keep their minimum size while the window has
    // room for it, whatever the ratios say.
    Arrangement arrange(double width, double height) const;

    // The share of a split's area its first side (left or top) gets.
    void setRatio(int split, double ratio);

    // StateCodec text ("layout.*" keys).
    std::string encode() const;
    // false, changing nothing, when `text` holds no valid layout.
    bool decode(std::string_view text);

    bool operator==(const Layout& other) const { return encode() == other.encode(); }

private:
    // The tree in preorder: a split is followed by its first side's nodes,
    // then its second side's.
    struct Node {
        int panel = -1;            // >= 0: a leaf
        bool sideBySide = true;    // split: left | right, else top / bottom
        double ratio = 0.5;        // split: the first side's share
    };

    size_t subtreeSize(size_t node) const;
    int leafIndex(int panel) const;
    double minLength(size_t node, bool horizontal) const;
    size_t place(size_t node, Rect rect, Arrangement& out) const;
    static bool wellFormed(const std::vector<Node>& nodes);

    std::vector<Node> _nodes;
};

} // namespace p31::suite
