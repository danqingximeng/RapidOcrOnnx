#include "TextLayout.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace textlayout {

namespace {

const double EPS = 0.0001;
const double PARA_TH = 1.2; // line-height threshold, from Umi-OCR paragraph_parse.py

// ======================= GapTree =======================

struct GUnit {
    double x0, y0, x1, y1;
    int idx; // index into the kept-blocks vector
    int pos; // index into the y-sorted units vector
};

struct Gap {
    double l, r;
    int rStart;
};

struct Cut {
    double l, r;
    int rStart, rEnd;
};

struct GNode {
    double xLeft, xRight;
    int rTop, rBottom;
    std::vector<int> units;    // positions into the y-sorted units vector
    std::vector<int> children; // GNode indices
};

// Use gaps2 (this row) to update gaps1 (gaps under consideration).
// Returns the gaps of gaps1 that died this row.
void updateGaps(std::vector<Gap> &gaps, const std::vector<Gap> &rowGaps, std::vector<Gap> &del) {
    std::vector<char> f1(gaps.size(), 1);
    std::vector<char> f2(rowGaps.size(), 1);
    std::vector<Gap> newGaps;
    for (size_t i1 = 0; i1 < gaps.size(); ++i1) {
        for (size_t i2 = 0; i2 < rowGaps.size(); ++i2) {
            double interL = std::max(gaps[i1].l, rowGaps[i2].l);
            double interR = std::min(gaps[i1].r, rowGaps[i2].r);
            // <= not <: zero-width gaps are valid intersections (upstream fix 675b3457)
            if (interL <= interR) {
                newGaps.push_back(Gap{interL, interR, gaps[i1].rStart});
                f1[i1] = 0;
                f2[i2] = 0;
            }
        }
    }
    for (size_t i2 = 0; i2 < rowGaps.size(); ++i2) {
        if (f2[i2]) newGaps.push_back(rowGaps[i2]);
    }
    del.clear();
    for (size_t i1 = 0; i1 < gaps.size(); ++i1) {
        if (f1[i1]) del.push_back(gaps[i1]);
    }
    gaps.swap(newGaps);
}

// Split units into rows and vertical cuts (persistent inter-row gutters).
void getCutsRows(const std::vector<GUnit> &units, double pageL, double pageR,
                 std::vector<Cut> &cuts, std::vector<std::vector<GUnit> > &rows) {
    pageL -= 1; // keep page edges from coinciding with block edges
    pageR += 1;
    cuts.clear();
    rows.clear();
    std::vector<Gap> gaps;
    size_t unitIndex = 0;
    int rowIndex = 0;
    while (unitIndex < units.size()) {
        // gather one row: all following units whose top <= this row's first bottom
        const GUnit &topUnit = units[unitIndex];
        std::vector<GUnit> row;
        row.push_back(topUnit);
        double uBottom = topUnit.y1;
        for (size_t i = unitIndex + 1; i < units.size(); ++i) {
            if (units[i].y0 > uBottom) break;
            row.push_back(units[i]);
            unitIndex = i;
        }
        std::sort(row.begin(), row.end(), [](const GUnit &a, const GUnit &b) {
            if (a.x0 != b.x0) return a.x0 < b.x0;
            return a.x1 < b.x1;
        });
        // gaps between blocks of this row, plus page-edge gaps
        std::vector<Gap> rowGaps;
        double searchStart = pageL;
        for (size_t i = 0; i < row.size(); ++i) {
            if (row[i].x0 > searchStart) rowGaps.push_back(Gap{searchStart, row[i].x0, rowIndex});
            if (row[i].x1 > searchStart) searchStart = row[i].x1;
        }
        rowGaps.push_back(Gap{searchStart, pageR, rowIndex});
        // grow / retire cuts
        std::vector<Gap> del;
        updateGaps(gaps, rowGaps, del);
        int rowMax = rowIndex - 1;
        for (size_t i = 0; i < del.size(); ++i) {
            cuts.push_back(Cut{del[i].l, del[i].r, del[i].rStart, rowMax});
        }
        rows.push_back(row);
        unitIndex++;
        rowIndex++;
    }
    int rowMax = (int) rows.size() - 1;
    for (size_t i = 0; i < gaps.size(); ++i) {
        cuts.push_back(Cut{gaps[i].l, gaps[i].r, gaps[i].rStart, rowMax});
    }
    std::sort(cuts.begin(), cuts.end(), [](const Cut &a, const Cut &b) { return a.l < b.l; });
}

// Build the layout tree from cuts/rows; collect each node's unit positions in
// preorder (one node = one region, units top-to-bottom).
void getLayoutTree(const std::vector<Cut> &cuts, const std::vector<std::vector<GUnit> > &rows,
                   const std::vector<GUnit> &units,
                   std::vector<std::vector<int> > &preorderUnits) {
    preorderUnits.clear();
    if (rows.empty()) return;

    // per-row gap edge lists, rebuilt from the cuts
    std::vector<std::vector<std::pair<double, double> > > rowsGaps(rows.size());
    for (size_t c = 0; c < cuts.size(); ++c) {
        for (int r = cuts[c].rStart; r <= cuts[c].rEnd; ++r) {
            rowsGaps[r].push_back(std::make_pair(cuts[c].l, cuts[c].r));
        }
    }

    std::vector<GNode> nodes;
    GNode root;
    root.xLeft = cuts[0].l - 1;
    root.xRight = cuts[cuts.size() - 1].r + 1;
    root.rTop = -1;
    root.rBottom = -1;
    nodes.push_back(root);
    std::vector<int> completed; // indices of finished nodes, in completion order
    completed.push_back(0);
    std::vector<int> nowNodes;  // active node indices

    // attach a finished node to its parent: the lowest completed node whose
    // horizontal projection covers node_r, rightmost on tie (root always qualifies)
    auto complete = [&](int ni) {
        double nodeR = nodes[ni].xRight - 2;
        std::vector<int> maxNodes;
        int maxR = -2;
        for (size_t c = 0; c < completed.size(); ++c) {
            int ci = completed[c];
            if (nodeR < nodes[ci].xLeft || nodeR > nodes[ci].xRight + EPS) continue;
            if (nodes[ci].rBottom >= nodes[ni].rTop) continue;
            if (nodes[ci].rBottom > maxR) {
                maxR = nodes[ci].rBottom;
                maxNodes.clear();
                maxNodes.push_back(ci);
                continue;
            }
            if (nodes[ci].rBottom == maxR) maxNodes.push_back(ci);
        }
        int best = maxNodes[0];
        for (size_t i = 1; i < maxNodes.size(); ++i) {
            if (nodes[maxNodes[i]].xRight > nodes[best].xRight) best = maxNodes[i];
        }
        nodes[best].children.push_back(ni);
        completed.push_back(ni);
    };

    for (size_t r_i = 0; r_i < rows.size(); ++r_i) {
        const std::vector<std::pair<double, double> > &rowGaps = rowsGaps[r_i];

        // retire nodes whose left/right edge is not continued by this row's gaps
        std::vector<int> newNow;
        for (size_t n = 0; n < nowNodes.size(); ++n) {
            int ni = nowNodes[n];
            bool lFlag = false, rFlag = false, completedFlag = false;
            for (size_t g = 0; g < rowGaps.size(); ++g) {
                if (rowGaps[g].second == nodes[ni].xLeft) lFlag = true;
                if (rowGaps[g].first == nodes[ni].xRight) rFlag = true;
                if ((nodes[ni].xLeft < rowGaps[g].first && rowGaps[g].first < nodes[ni].xRight) ||
                    (nodes[ni].xLeft < rowGaps[g].second && rowGaps[g].second < nodes[ni].xRight)) {
                    completedFlag = true;
                    break;
                }
            }
            if (!lFlag || !rFlag) completedFlag = true;
            if (completedFlag) {
                complete(ni);
            } else {
                nodes[ni].rBottom = (int) r_i;
                newNow.push_back(ni);
            }
        }
        nowNodes.swap(newNow);

        // assign this row's units to columns (nodes), spawning nodes as needed
        size_t u_i = 0, g_i = 0;
        const std::vector<GUnit> &row = rows[r_i];
        while (u_i < row.size()) {
            double xL = rowGaps[g_i].second;
            double xR = rowGaps[g_i + 1].first;
            if (row[u_i].x0 + EPS > xR) {
                g_i++;
                continue;
            }
            bool flag = false;
            for (size_t n = 0; n < nowNodes.size(); ++n) {
                int ni = nowNodes[n];
                if (nodes[ni].xLeft == xL && nodes[ni].xRight == xR) {
                    nodes[ni].units.push_back(row[u_i].pos);
                    flag = true;
                    break;
                }
            }
            if (flag) {
                u_i++;
                continue;
            }
            GNode node;
            node.xLeft = xL;
            node.xRight = xR;
            node.rTop = (int) r_i;
            node.rBottom = (int) r_i;
            node.units.push_back(row[u_i].pos);
            nodes.push_back(node);
            nowNodes.push_back((int) nodes.size() - 1);
            u_i++;
        }
    }
    // attach remaining active nodes
    for (size_t n = 0; n < nowNodes.size(); ++n) {
        complete(nowNodes[n]);
    }

    // children left-to-right; units top-to-bottom
    for (size_t i = 0; i < nodes.size(); ++i) {
        std::sort(nodes[i].children.begin(), nodes[i].children.end(),
                  [&nodes](int a, int b) { return nodes[a].xLeft < nodes[b].xLeft; });
        std::sort(nodes[i].units.begin(), nodes[i].units.end(),
                  [&units](int a, int b) { return units[a].y0 < units[b].y0; });
    }

    // iterative preorder traversal, keeping only nodes that hold units
    std::vector<int> stack;
    stack.push_back(0);
    while (!stack.empty()) {
        int ni = stack.back();
        stack.pop_back();
        if (!nodes[ni].units.empty()) preorderUnits.push_back(nodes[ni].units);
        for (int c = (int) nodes[ni].children.size() - 1; c >= 0; --c) {
            stack.push_back(nodes[ni].children[c]);
        }
    }
}

// ======================= word separator =======================

bool isCjk(char32_t cp) {
    return (cp >= 0x4E00 && cp <= 0x9FFF)   // CJK unified
           || (cp >= 0x3040 && cp <= 0x30FF)  // Japanese
           || (cp >= 0x1100 && cp <= 0x11FF)  // Hangul Jamo
           || (cp >= 0x3130 && cp <= 0x318F)  // Hangul compat
           || (cp >= 0xAC00 && cp <= 0xD7AF)  // Hangul syllables
           || (cp >= 0x3000 && cp <= 0x303F)  // CJK symbols/punct
           || (cp >= 0xFE30 && cp <= 0xFE4F)  // CJK compat forms
           || (cp >= 0xFF00 && cp <= 0xFFEF); // fullwidth forms
}

// Approximation of unicodedata.category(c).startswith("P"): ASCII punctuation
// plus the General Punctuation block. Fullwidth/CJK punctuation is already
// covered by isCjk, which short-circuits before this check.
bool isPunct(char32_t cp) {
    if (cp >= 0x21 && cp <= 0x2F) return true;
    if (cp >= 0x3A && cp <= 0x40) return true;
    if (cp >= 0x5B && cp <= 0x60) return true;
    if (cp >= 0x7B && cp <= 0x7E) return true;
    return cp >= 0x2000 && cp <= 0x206F;
}

char32_t utf8First(const std::string &s) {
    if (s.empty()) return 0;
    const unsigned char *p = (const unsigned char *) s.data();
    unsigned char c = p[0];
    if (c < 0x80) return c;
    int len = (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
    if ((int) s.size() < len) len = (int) s.size();
    char32_t cp = c & (0xFFu >> (len + 1));
    for (int i = 1; i < len; ++i) {
        cp = (cp << 6) | (p[i] & 0x3F);
    }
    return cp;
}

char32_t utf8Last(const std::string &s) {
    if (s.empty()) return 0;
    const unsigned char *p = (const unsigned char *) s.data();
    size_t i = s.size() - 1;
    while (i > 0 && (p[i] & 0xC0) == 0x80) i--; // skip continuation bytes
    unsigned char c = p[i];
    int len = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
    if (i + len > s.size()) len = (int) (s.size() - i);
    char32_t cp = c & (0xFFu >> (len + 1));
    for (int k = 1; k < len; ++k) {
        cp = (cp << 6) | (p[i + k] & 0x3F);
    }
    return cp;
}

std::string wordSeparator(char32_t letter1, char32_t letter2) {
    if (isCjk(letter1) && isCjk(letter2)) return "";
    if (letter1 == '-') return "";
    if (isPunct(letter2)) return "";
    return " ";
}

// ======================= paragraph parse =======================

// Predict the end separator of each block within one region given in output
// (reading) order. Port of Umi-OCR paragraph_parse.py without its defensive
// y-sort: callers always feed final output order, so ends stay consistent
// with how consumers join the text.
void parseParagraph(const std::vector<int> &poss, const std::vector<LayoutBlock> &blocks,
                    const std::vector<int> &order, std::vector<std::string> &ends) {
    if (poss.empty()) return;
    const double NaN = std::numeric_limits<double>::quiet_NaN();

    // group into paragraphs by left/right edge alignment and line spacing
    std::vector<std::vector<int> > paras;
    std::vector<double> parasLineSpace;
    {
        const LayoutBlock &b0 = blocks[order[poss[0]]];
        double paraL = b0.x0, paraTop = b0.y0, paraR = b0.x1, paraBottom = b0.y1;
        double paraLineH = paraBottom - paraTop;
        double paraLineS = NaN;
        std::vector<int> nowPara;
        nowPara.push_back(poss[0]);
        for (size_t i = 1; i < poss.size(); ++i) {
            const LayoutBlock &b = blocks[order[poss[i]]];
            double l = b.x0, top = b.y0, r = b.x1, bottom = b.y1;
            double h = bottom - top;
            double ls = top - paraBottom;
            bool same = std::fabs(paraL - l) <= paraLineH * PARA_TH &&
                        std::fabs(paraR - r) <= paraLineH * PARA_TH &&
                        (std::isnan(paraLineS) || ls < paraLineS + paraLineH * 0.5);
            if (same) {
                paraL = (paraL + l) / 2;
                paraR = (paraR + r) / 2;
                paraLineH = (paraLineH + h) / 2;
                paraLineS = std::isnan(paraLineS) ? ls : (paraLineS + ls) / 2;
                nowPara.push_back(poss[i]);
            } else {
                paras.push_back(nowPara);
                parasLineSpace.push_back(paraLineS);
                nowPara.clear();
                nowPara.push_back(poss[i]);
                paraL = l;
                paraR = r;
                paraLineH = h;
                paraLineS = NaN;
            }
            paraBottom = bottom;
        }
        paras.push_back(nowPara);
        parasLineSpace.push_back(paraLineS);
    }

    // merge single-line paragraphs into the paragraph above or below
    for (int i1 = (int) paras.size() - 1; i1 >= 0; --i1) {
        if (paras[i1].size() != 1) continue;
        const LayoutBlock &b = blocks[order[paras[i1][0]]];
        double l = b.x0, top = b.y0, r = b.x1, bottom = b.y1;
        bool upFlag = false, downFlag = false;
        double upBottom = 0, downTop = 0;
        if (i1 > 0) {
            const LayoutBlock &up = blocks[order[paras[i1 - 1].back()]];
            double upL = up.x0, upR = up.x1;
            upBottom = up.y1;
            double upH = upBottom - up.y0;
            double upDist = std::fabs(upL - l);
            upFlag = upDist <= upH * PARA_TH && r <= upR + upH * PARA_TH;
            double upS = parasLineSpace[i1 - 1];
            if (!std::isnan(upS) && top - upBottom > upS + upH * 0.5) upFlag = false;
        }
        if (i1 + 1 < (int) paras.size()) {
            const LayoutBlock &down = blocks[order[paras[i1 + 1].front()]];
            double downL = down.x0, downR = down.x1;
            downTop = down.y0;
            double downH = down.y1 - downTop;
            if (l >= downL - downH * PARA_TH && l <= downL + downH * (1 + PARA_TH)) {
                if (paras[i1 + 1].size() > 1) {
                    downFlag = std::fabs(downR - r) <= downH * PARA_TH;
                } else {
                    downFlag = downR - downH * PARA_TH < r;
                }
            }
            double downS = parasLineSpace[i1 + 1];
            if (!std::isnan(downS) && downTop - bottom > downS + downH * 0.5) downFlag = false;
        }
        bool merged = false;
        if (upFlag && downFlag) {
            if (top - upBottom < downTop - bottom) { // closer to the paragraph above
                paras[i1 - 1].push_back(paras[i1][0]);
            } else {
                paras[i1 + 1].insert(paras[i1 + 1].begin(), paras[i1][0]);
            }
            merged = true;
        } else if (upFlag) {
            paras[i1 - 1].push_back(paras[i1][0]);
            merged = true;
        } else if (downFlag) {
            paras[i1 + 1].insert(paras[i1 + 1].begin(), paras[i1][0]);
            merged = true;
        }
        if (merged) {
            paras.erase(paras.begin() + i1);
            parasLineSpace.erase(parasLineSpace.begin() + i1);
        }
    }

    // assign end separators
    for (size_t p = 0; p < paras.size(); ++p) {
        const std::vector<int> &para = paras[p];
        for (size_t i = 0; i + 1 < para.size(); ++i) {
            char32_t letter1 = utf8Last(blocks[order[para[i]]].text);
            char32_t letter2 = utf8First(blocks[order[para[i + 1]]].text);
            ends[para[i]] = wordSeparator(letter1, letter2);
        }
        ends[para.back()] = "\n";
    }
}

} // namespace

LayoutResult analyze(const std::vector<LayoutBlock> &blocks, const std::string &sortMode, bool para) {
    LayoutResult out;
    std::vector<int> kept;
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (!blocks[i].text.empty()) kept.push_back((int) i);
    }
    if (kept.empty()) return out;

    // regions: contiguous segments of the output order, one per layout node
    std::vector<std::pair<int, int> > segments;
    if (sortMode == "gaptree") {
        std::vector<GUnit> units;
        units.reserve(kept.size());
        for (size_t i = 0; i < kept.size(); ++i) {
            const LayoutBlock &b = blocks[kept[i]];
            GUnit u;
            u.x0 = b.x0;
            u.y0 = b.y0;
            u.x1 = b.x1;
            u.y1 = b.y1;
            u.idx = (int) i;
            u.pos = 0;
            units.push_back(u);
        }
        std::sort(units.begin(), units.end(), [](const GUnit &a, const GUnit &b) { return a.y0 < b.y0; });
        for (size_t i = 0; i < units.size(); ++i) units[i].pos = (int) i;
        double pageL = units[0].x0, pageR = units[0].x1;
        for (size_t i = 1; i < units.size(); ++i) {
            if (units[i].x0 < pageL) pageL = units[i].x0;
            if (units[i].x1 > pageR) pageR = units[i].x1;
        }
        std::vector<Cut> cuts;
        std::vector<std::vector<GUnit> > rows;
        getCutsRows(units, pageL, pageR, cuts, rows);
        std::vector<std::vector<int> > preorderUnits; // unit positions, y-sorted per node
        getLayoutTree(cuts, rows, units, preorderUnits);
        for (size_t n = 0; n < preorderUnits.size(); ++n) {
            int start = (int) out.order.size();
            for (size_t u = 0; u < preorderUnits[n].size(); ++u) {
                out.order.push_back(kept[units[preorderUnits[n][u]].idx]);
            }
            segments.push_back(std::make_pair(start, (int) out.order.size()));
        }
    } else {
        out.order = kept;
        segments.push_back(std::make_pair(0, (int) out.order.size()));
    }

    out.ends.assign(out.order.size(), "\n");
    if (para) {
        for (size_t s = 0; s < segments.size(); ++s) {
            std::vector<int> poss;
            for (int p = segments[s].first; p < segments[s].second; ++p) poss.push_back(p);
            parseParagraph(poss, blocks, out.order, out.ends);
        }
    }
    return out;
}

} // namespace textlayout
