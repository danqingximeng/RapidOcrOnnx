#ifndef __TEXT_LAYOUT_H__
#define __TEXT_LAYOUT_H__

#include <string>
#include <vector>

// Layout post-processing for OCR text blocks: reading-order sort (GapTree)
// and paragraph end prediction. Ported from hiroi-sora's GapTree_Sort_Algorithm
// (https://github.com/hiroi-sora/GapTree_Sort_Algorithm, MIT) and Umi-OCR's
// tbpu paragraph parser. Operates on axis-aligned boxes only (no deskew).
namespace textlayout {

struct LayoutBlock {
    double x0;
    double y0;
    double x1;
    double y1;
    std::string text;
};

struct LayoutResult {
    // Input indices in reading order. Empty-text blocks are dropped.
    std::vector<int> order;
    // Parallel to order: the separator that follows each block's text.
    // "\n" ends a paragraph, "" continues a CJK line, " " continues otherwise.
    std::vector<std::string> ends;
};

// sortMode: "gaptree" (reading order) or "none" (keep input order).
// para=false assigns "\n" to every end (one line per block).
LayoutResult analyze(const std::vector<LayoutBlock> &blocks, const std::string &sortMode, bool para);

} // namespace textlayout

#endif //__TEXT_LAYOUT_H__
