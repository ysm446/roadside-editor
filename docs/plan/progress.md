# Progress

作成日時: 2026-07-22 22:27
更新日時: 2026-07-23 09:14

## 現在の状態

Phase 0〜1 完了、Phase 3 の簡易版パイプラインが貫通した。`Path`(S字スプライン)→ `Ribbon` → `Multi-Scale Erosion` のノードグラフが動作し、曲線センターラインに沿ったUVベイク、φ = P_z + h・N_z ポテンシャルでの侵食、曲線リボンのワールドメッシュ 3D プレビューまで確認済み。次の作業は Phase 2 の検証ツール(面積比較)、Phase 3 残り($s_u\,du$ 距離補正・$s_u$ クランプ・メートル値書き出し)。

## 完了済み

- 2026-07-22: リポジトリ初期化、初回コミット
- 2026-07-22: UV空間侵食の技術メモ [docs/uv-space-erosion.md](../uv-space-erosion.md) を整備
- 2026-07-22: AGENTS.md / CLAUDE.md を作成
- 2026-07-22: goals / plan / progress の初期内容を作成
- 2026-07-23: Phase 0 完了。terrain-editor (v0.24.0 時点) を fork 方式で取り込み(`src/`, `shaders/`, `assets/`, CMake + vcpkg 構成)。`RoadsideEditor v0.1.0` / `roadside_editor.exe` にリネームしてビルド確認
- 2026-07-23: `Ribbon` ソースノードを実装([src/evaluation/RibbonSource.cpp](../../src/evaluation/RibbonSource.cpp))。直線センターライン + 縦断勾配のデモ道路プロファイル(道路/路肩/法面)をワールド比例UV(1テクセル = 指定cm)でベイク。heights = φ = P_z + h・N_z、`HeightfieldGrid.baseZ / normalZ` に P_z / N_z を保持。初期変位 h として fBm ノイズの山を路肩・法面ゾーンに生成
- 2026-07-23: `Multi-Scale Erosion` に `Displacement` 出力ピンを追加。`HeightfieldPreviewField::Displacement` で h = (φ - P_z)/N_z を可視化・マスク書き出し(0.5 = 変位ゼロの符号付き正規化)
- 2026-07-23: ヘッドレスのスモークテストで Ribbon → MSE → h 復元の貫通を確認(512², 8cm/texel, N_z 0.86〜1.0, φ 正値域)
- 2026-07-23: 曲線センターライン対応(Phase 1 完了)。`Ribbon` に Path 入力ピンを追加し、既存 `Path` ノードのスプライン(Catmull-Rom / 直線)をチェーン化 → 弧長リサンプリングして掃引ベイク。曲率 κ(u) を計算し N_z の $s_u = 1 - \kappa v$ 補正に使用。`BuildRibbonWorldMesh` で曲線リボンのワールドメッシュを 3D プレビュー表示(World Preview 設定、カスプは $0.95/\kappa$ クランプ)。デモプロジェクトを S 字カーブ(80m、最小半径 16m、82m アトラス 16cm/texel)に更新し、俯瞰スプラット画像でスイープ形状・クラウン・縦断勾配を検証

## 未完了

- [plan.md](plan.md) の Phase 1 残り(Path 高さ入力の縦断)、Phase 2 残り(検証ツール: 面積比較・√(EG−F²) 可視化)、Phase 3 残り($s_u\,du$ 距離補正、$s_u$ クランプ、メートル値 displacement 書き出し)、Phase 4 以降すべて

## 注意点

- 侵食ポテンシャルは必ず $\phi = P_z + h \cdot N_z$ を使う(変位 $h$ 単独で侵食をかけると道路の傾きを無視した流れになる)。実装レビュー時の最重要チェックポイント。現在の実装はソルバ無改造で「φ を heights として渡す」方式(§8.1 簡易版)
- `Ribbon` はグローバルの Terrain Size / 解像度設定のうち **Terrain Size を無視** し、グリッド実寸 = 解像度 × Texel Size で決める(ワールド比例UVの強制)。下流ノードはグリッドの `terrainSizeMeters` からセルサイズを取るので整合する
- `HeightfieldGrid.baseZ / normalZ` は侵食オペレーションが触らないことで下流に素通し伝搬する設計。解像度を変えるオペレーションを追加する場合は要注意
- terrain-editor のローカルクローンは `D:\GitHub\terrain-editor`(CLAUDE.md 旧記載の C: ではない)
- MSE の thermal パスは境界 wraparound のため、縦断勾配による u 端の段差でエッジにアーティファクトが出る可能性がある(デモでは許容、要観察)
- 曲線リボンのワールドプレビューは CPU Mesh バックエンドのみ(GPU Displacement はUVグリッド前提)。侵食ソルバ自体は依然 UV 空間の均一グリッドで動いており、$s_u\,du$ 距離補正は未実装(Phase 3 残り)。カーブがきついと物理的には内側圧縮・外側伸長の誤差が出る
- Path 入力のセンターラインは複数エッジを端点から辿って一本のチェーンにする。分岐・ループは非対応(最初に辿れた枝のみ)
