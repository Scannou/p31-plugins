//
//  SuiteLayout.cpp
//  p31-core
//

#include "p31/suite/SuiteLayout.h"

#include "p31/core/StateCodec.h"

#include <algorithm>
#include <cmath>

namespace p31::suite {

namespace {
    constexpr double kMinRatio = 0.02;
    constexpr double kMaxRatio = 0.98;
    constexpr double kRatioUnits = 10000.0;   // ratios are stored in 1/10000

    // Node kinds in the stored text.
    constexpr int64_t kKindPanel = 0;
    constexpr int64_t kKindSideBySide = 1;
    constexpr int64_t kKindStacked = 2;

    constexpr size_t kMaxNodes = 2 * kPanelCount - 1;

    bool validPanel(int panel) { return panel >= 0 && panel < kPanelCount; }
    bool horizontal(Edge edge) { return edge == Edge::Left || edge == Edge::Right; }
    bool leading(Edge edge) { return edge == Edge::Left || edge == Edge::Top; }

    std::string nodeKey(size_t index, const char* field) {
        return "layout." + std::to_string(index) + "." + field;
    }
}

Layout::Layout() {
    _nodes = {
        {-1, true, 0.84},
        {-1, true, 0.3},
        {PanelScope, true, 0.5},
        {-1, false, 0.5},
        {PanelSpectrum, true, 0.5},
        {PanelSpectrogram, true, 0.5},
        {PanelMeter, true, 0.5},
    };
}

size_t Layout::subtreeSize(size_t node) const {
    if (_nodes[node].panel >= 0) return 1;
    const size_t first = subtreeSize(node + 1);
    return 1 + first + subtreeSize(node + 1 + first);
}

int Layout::leafIndex(int panel) const {
    if (!validPanel(panel)) return -1;
    for (size_t i = 0; i < _nodes.size(); ++i) {
        if (_nodes[i].panel == panel) return static_cast<int>(i);
    }
    return -1;
}

bool Layout::contains(int panel) const { return leafIndex(panel) >= 0; }

std::vector<int> Layout::panels() const {
    std::vector<int> out;
    for (const Node& n : _nodes) {
        if (n.panel >= 0) out.push_back(n.panel);
    }
    return out;
}

bool Layout::remove(int panel) {
    const int leaf = leafIndex(panel);
    if (leaf < 0 || _nodes.size() == 1) return false;
    // Without the leaf and the split above it, the other side's nodes sit
    // where the split was.
    for (size_t split = 0; split < _nodes.size(); ++split) {
        if (_nodes[split].panel >= 0) continue;
        const size_t first = split + 1, second = first + subtreeSize(first);
        if (static_cast<size_t>(leaf) != first && static_cast<size_t>(leaf) != second) continue;
        _nodes.erase(_nodes.begin() + leaf);
        _nodes.erase(_nodes.begin() + static_cast<std::ptrdiff_t>(split));
        return true;
    }
    return false;
}

bool Layout::insert(int panel, int target, Edge edge) {
    const int leaf = leafIndex(target);
    if (!validPanel(panel) || contains(panel) || leaf < 0) return false;
    const Node split{-1, horizontal(edge), 0.5};
    const Node added{panel, true, 0.5};
    const auto at = _nodes.begin() + leaf;
    if (leading(edge)) _nodes.insert(at, {split, added});
    else {
        _nodes.insert(at + 1, added);
        _nodes.insert(_nodes.begin() + leaf, split);
    }
    return true;
}

bool Layout::insertAtEdge(int panel, Edge edge) {
    if (!validPanel(panel) || contains(panel)) return false;
    const double share = 1.0 / (count() + 1);
    const Node added{panel, true, 0.5};
    if (leading(edge)) {
        _nodes.insert(_nodes.begin(), {Node{-1, horizontal(edge), share}, added});
    } else {
        _nodes.insert(_nodes.begin(), Node{-1, horizontal(edge), 1.0 - share});
        _nodes.push_back(added);
    }
    return true;
}

bool Layout::show(int panel, double width, double height) {
    if (!validPanel(panel) || contains(panel)) return false;
    const Arrangement a = arrange(width, height);
    const auto largest = std::max_element(a.panes.begin(), a.panes.end(), [](const Pane& x, const Pane& y) {
        return x.rect.w * x.rect.h < y.rect.w * y.rect.h;
    });
    return insert(panel, largest->panel, largest->rect.w >= largest->rect.h ? Edge::Right : Edge::Bottom);
}

bool Layout::move(int panel, int target, Edge edge) {
    if (panel == target || !contains(target)) return false;
    Layout next = *this;
    if (!next.remove(panel) || !next.insert(panel, target, edge)) return false;
    *this = next;
    return true;
}

bool Layout::moveToEdge(int panel, Edge edge) {
    Layout next = *this;
    if (!next.remove(panel) || !next.insertAtEdge(panel, edge)) return false;
    *this = next;
    return true;
}

bool Layout::swap(int a, int b) {
    const int la = leafIndex(a), lb = leafIndex(b);
    if (la < 0 || lb < 0 || la == lb) return false;
    std::swap(_nodes[la].panel, _nodes[lb].panel);
    return true;
}

#pragma mark - geometry

double Layout::Divider::ratioAt(double position) const {
    if (room <= 0.0) return 0.5;
    return (position - kGap / 2.0 - from) / room;
}

// The length a subtree needs along one axis for every panel in it to have
// its minimum size.
double Layout::minLength(size_t node, bool alongWidth) const {
    const Node& n = _nodes[node];
    if (n.panel >= 0) return alongWidth ? kMinPanelWidth : kMinPanelHeight;
    const size_t first = node + 1, second = first + subtreeSize(first);
    const double a = minLength(first, alongWidth), b = minLength(second, alongWidth);
    return n.sideBySide == alongWidth ? a + kGap + b : std::max(a, b);
}

// Returns the node after the subtree.
size_t Layout::place(size_t node, Rect rect, Arrangement& out) const {
    const Node& n = _nodes[node];
    if (n.panel >= 0) {
        out.panes.push_back({n.panel, rect});
        return node + 1;
    }
    const size_t first = node + 1, second = first + subtreeSize(first);
    const double from = n.sideBySide ? rect.x : rect.y;
    const double length = n.sideBySide ? rect.w : rect.h;
    const double gapSize = std::min(kGap, length);
    const double room = length - gapSize;
    const double minA = minLength(first, n.sideBySide), minB = minLength(second, n.sideBySide);
    double a = std::round(room * n.ratio);
    // Too small for both minimums: each gives way in proportion.
    if (room >= minA + minB) a = std::clamp(a, minA, room - minB);
    else a = std::round(room * minA / (minA + minB));

    Rect ra = rect, rb = rect, gap = rect;
    if (n.sideBySide) {
        ra.w = a;
        gap.x = rect.x + a;
        gap.w = gapSize;
        rb.x = gap.x + gapSize;
        rb.w = room - a;
    } else {
        ra.h = a;
        gap.y = rect.y + a;
        gap.h = gapSize;
        rb.y = gap.y + gapSize;
        rb.h = room - a;
    }
    out.dividers.push_back({static_cast<int>(node), n.sideBySide, gap, from, room});
    place(first, ra, out);
    return place(second, rb, out);
}

Layout::Arrangement Layout::arrange(double width, double height) const {
    Arrangement out;
    place(0, {0.0, 0.0, std::max(0.0, std::round(width)), std::max(0.0, std::round(height))}, out);
    return out;
}

void Layout::setRatio(int split, double ratio) {
    if (split < 0 || static_cast<size_t>(split) >= _nodes.size() || _nodes[split].panel >= 0) return;
    if (!std::isfinite(ratio)) return;
    _nodes[split].ratio = std::clamp(ratio, kMinRatio, kMaxRatio);
}

#pragma mark - text

std::string Layout::encode() const {
    StateWriter w;
    w.put("layout.count", static_cast<int64_t>(_nodes.size()));
    for (size_t i = 0; i < _nodes.size(); ++i) {
        const Node& n = _nodes[i];
        if (n.panel >= 0) {
            w.put(nodeKey(i, "kind"), kKindPanel);
            w.put(nodeKey(i, "value"), n.panel);
        } else {
            w.put(nodeKey(i, "kind"), n.sideBySide ? kKindSideBySide : kKindStacked);
            w.put(nodeKey(i, "value"), std::lround(n.ratio * kRatioUnits));
        }
    }
    return w.text();
}

// One tree, every panel at most once.
bool Layout::wellFormed(const std::vector<Node>& nodes) {
    bool seen[kPanelCount] = {};
    // Subtrees still to come while walking the preorder.
    size_t open = 1;
    for (const Node& n : nodes) {
        if (open == 0) return false;
        if (n.panel >= 0) {
            if (!validPanel(n.panel) || seen[n.panel]) return false;
            seen[n.panel] = true;
            --open;
        } else {
            ++open;
        }
    }
    return open == 0;
}

bool Layout::decode(std::string_view text) {
    const StateReader r(text);
    int64_t count = 0;
    if (!r.get("layout.count", count) || count < 1 || count > static_cast<int64_t>(kMaxNodes)) return false;
    std::vector<Node> nodes;
    for (size_t i = 0; i < static_cast<size_t>(count); ++i) {
        int64_t kind = 0, value = 0;
        if (!r.get(nodeKey(i, "kind"), kind) || !r.get(nodeKey(i, "value"), value)) return false;
        if (kind == kKindPanel) {
            if (value < 0 || value >= kPanelCount) return false;
            nodes.push_back({static_cast<int>(value), true, 0.5});
        } else if (kind == kKindSideBySide || kind == kKindStacked) {
            const double ratio = std::clamp(static_cast<double>(value) / kRatioUnits, kMinRatio, kMaxRatio);
            nodes.push_back({-1, kind == kKindSideBySide, ratio});
        } else {
            return false;
        }
    }
    if (!wellFormed(nodes)) return false;
    _nodes = std::move(nodes);
    return true;
}

} // namespace p31::suite
