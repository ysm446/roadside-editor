#pragma once

namespace rock
{
// UV確認用のチェッカーテクスチャをプロシージャルに返す。
// (uMeters, vMeters) はワールド比例UV座標、cellMeters はマクロセル一辺、
// texelMeters は線幅・ラベルサイズのスケール基準。
// マクロセルごとに 8 色パレット + 市松の明暗、4x4 のサブグリッド線、
// セル中央に「列数字 + 行英字」(例: 3B) のラベルを描く。
// ラベルはセルが小さいとき (12 テクセル未満) は省略される。
void UvCheckerColor(float uMeters, float vMeters, float cellMeters, float texelMeters, float& r, float& g, float& b);
} // namespace rock
