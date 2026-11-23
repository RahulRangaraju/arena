#pragma once
#include "Vec2.hpp"
#include <algorithm>
#include <cstddef>
#include <vector>

// Uniform-grid broadphase built with a counting sort into two flat arrays:
//   cellStart_[c] .. cellStart_[c+1]  is the range in items_ holding cell c's item indices.
// Two passes over the items, no allocation after the first build, memory read in order.
class SpatialGrid {
public:
    SpatialGrid(float width, float height, float cellSize)
        : cell_(cellSize),
          cols_(static_cast<int>(width / cellSize) + 1),
          rows_(static_cast<int>(height / cellSize) + 1),
          counts_(cols_ * rows_),
          cellStart_(cols_ * rows_ + 1),
          cursor_(cols_ * rows_) {}

    // posOf(i) returns the Vec2 position of item i.
    template <class PosFn>
    void build(std::size_t n, PosFn posOf) {
        std::fill(counts_.begin(), counts_.end(), 0);
        cellOf_.resize(n);
        for (std::size_t i = 0; i < n; ++i) {   // pass 1: count per cell
            int c = cellIndex(posOf(i));
            cellOf_[i] = c;
            ++counts_[c];
        }
        cellStart_[0] = 0;                       // prefix sum
        for (std::size_t c = 0; c < counts_.size(); ++c)
            cellStart_[c + 1] = cellStart_[c] + counts_[c];

        std::copy(cellStart_.begin(), cellStart_.end() - 1, cursor_.begin());
        items_.resize(n);
        for (std::size_t i = 0; i < n; ++i)      // pass 2: scatter
            items_[cursor_[cellOf_[i]]++] = static_cast<int>(i);
    }

    // Calls fn(index) for every item in cells overlapping the circle (p, r).
    // If fn returns true, the query stops early.
    template <class Fn>
    void query(Vec2 p, float r, Fn&& fn) const {
        int x0 = clampCol(static_cast<int>((p.x - r) / cell_));
        int x1 = clampCol(static_cast<int>((p.x + r) / cell_));
        int y0 = clampRow(static_cast<int>((p.y - r) / cell_));
        int y1 = clampRow(static_cast<int>((p.y + r) / cell_));
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                int c = y * cols_ + x;
                for (int k = cellStart_[c]; k < cellStart_[c + 1]; ++k)
                    if (fn(items_[k])) return;
            }
    }

    float cellSize() const { return cell_; }

private:
    int clampCol(int x) const { return std::clamp(x, 0, cols_ - 1); }
    int clampRow(int y) const { return std::clamp(y, 0, rows_ - 1); }
    int cellIndex(Vec2 p) const {
        // Entities slightly off-screen (spawning) get clamped into edge cells.
        int x = clampCol(static_cast<int>(p.x / cell_));
        int y = clampRow(static_cast<int>(p.y / cell_));
        return y * cols_ + x;
    }

    float cell_;
    int cols_, rows_;
    std::vector<int> counts_, cellStart_, cursor_, cellOf_, items_;
};
