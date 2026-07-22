# Changelog

作成日時: 2026-07-23 01:00
更新日時: 2026-07-23 01:00

## 未リリース

- terrain-editor (v0.24.0) を fork してプロジェクト基盤を構築。`RoadsideEditor v0.1.0` / `roadside_editor.exe` として C++20 / CMake / vcpkg / Win32 + Direct3D 12 でビルド可能に
- `Ribbon` ソースノードを追加。直線センターライン + 縦断勾配の道路プロファイル(道路・路肩・法面)をワールド比例UV(1テクセル = 指定cm)のUVグリッドへベイクし、路肩・法面に初期ノイズの山を生成する。ハイトマップ出力は侵食ポテンシャル φ = P_z + h・N_z
- `Multi-Scale Erosion` に `Displacement` 出力ピンを追加。Ribbon 上流のとき法線方向変位 h = (φ - P_z)/N_z を可視化・マスク書き出しできる(0.5 が変位ゼロ)
- 起動引数にプロジェクトパスを渡すと開けるようにした(`roadside_editor.exe path.terrainproj`)
- デモプロジェクト [assets/sample/demo_ribbon_erosion.terrainproj](../assets/sample/demo_ribbon_erosion.terrainproj) を追加(Ribbon → Multi-Scale Erosion)
- 起動用バッチ `start.bat` を追加(引数省略時はデモプロジェクトを開く)
