# Progress

作成日時: 2026-07-22 22:27
更新日時: 2026-07-23 01:00

## 現在の状態

Phase 0 完了、Phase 1〜3 のデモパイプラインが貫通した。terrain-editor を fork したコードベース上で、`Ribbon` ソースノード → `Multi-Scale Erosion` のノードグラフが動作し、φ = P_z + h・N_z ポテンシャルでの侵食と変位 h の復元まで確認済み。次の作業はアプリ起動しての目視確認・パラメータ調整と、曲線センターライン対応(Phase 1 残り)。

## 完了済み

- 2026-07-22: リポジトリ初期化、初回コミット
- 2026-07-22: UV空間侵食の技術メモ [docs/uv-space-erosion.md](../uv-space-erosion.md) を整備
- 2026-07-22: AGENTS.md / CLAUDE.md を作成
- 2026-07-22: goals / plan / progress の初期内容を作成
- 2026-07-23: Phase 0 完了。terrain-editor (v0.24.0 時点) を fork 方式で取り込み(`src/`, `shaders/`, `assets/`, CMake + vcpkg 構成)。`RoadsideEditor v0.1.0` / `roadside_editor.exe` にリネームしてビルド確認
- 2026-07-23: `Ribbon` ソースノードを実装([src/evaluation/RibbonSource.cpp](../../src/evaluation/RibbonSource.cpp))。直線センターライン + 縦断勾配のデモ道路プロファイル(道路/路肩/法面)をワールド比例UV(1テクセル = 指定cm)でベイク。heights = φ = P_z + h・N_z、`HeightfieldGrid.baseZ / normalZ` に P_z / N_z を保持。初期変位 h として fBm ノイズの山を路肩・法面ゾーンに生成
- 2026-07-23: `Multi-Scale Erosion` に `Displacement` 出力ピンを追加。`HeightfieldPreviewField::Displacement` で h = (φ - P_z)/N_z を可視化・マスク書き出し(0.5 = 変位ゼロの符号付き正規化)
- 2026-07-23: ヘッドレスのスモークテストで Ribbon → MSE → h 復元の貫通を確認(512², 8cm/texel, N_z 0.86〜1.0, φ 正値域)

## 未完了

- [plan.md](plan.md) の Phase 1 残り(スプライン入力、曲線リボンの3Dプレビュー)、Phase 2 残り(P のXYベイク、検証ツール)、Phase 3 残り($s_u\,du$ 距離補正、$s_u$ クランプ、メートル値 displacement 書き出し)、Phase 4 以降すべて

## 注意点

- 侵食ポテンシャルは必ず $\phi = P_z + h \cdot N_z$ を使う(変位 $h$ 単独で侵食をかけると道路の傾きを無視した流れになる)。実装レビュー時の最重要チェックポイント。現在の実装はソルバ無改造で「φ を heights として渡す」方式(§8.1 簡易版)
- `Ribbon` はグローバルの Terrain Size / 解像度設定のうち **Terrain Size を無視** し、グリッド実寸 = 解像度 × Texel Size で決める(ワールド比例UVの強制)。下流ノードはグリッドの `terrainSizeMeters` からセルサイズを取るので整合する
- `HeightfieldGrid.baseZ / normalZ` は侵食オペレーションが触らないことで下流に素通し伝搬する設計。解像度を変えるオペレーションを追加する場合は要注意
- terrain-editor のローカルクローンは `D:\GitHub\terrain-editor`(CLAUDE.md 旧記載の C: ではない)
- MSE の thermal パスは境界 wraparound のため、縦断勾配による u 端の段差でエッジにアーティファクトが出る可能性がある(デモでは許容、要観察)
