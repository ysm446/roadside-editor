#pragma once

#include <string>

#include "../node_graph.h"

namespace rock
{
// RibbonSettings のデモ道路プロファイルをUVグリッドへベイクする。
// heights = φ = P_z + h・N_z、baseZ = P_z、normalZ = N_z。
// grid.terrainSizeMeters は resolution x texelSizeCentimeters から決まり、
// グローバルの Terrain Size 設定は使用しない (ワールド比例UVの強制)。
HeightfieldGrid BuildHeightfieldFromRibbon(const RibbonSettings& settings, int resolution, std::string* message);
} // namespace rock
