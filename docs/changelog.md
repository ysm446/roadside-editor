# Changelog

作成日時: 2026-07-23 01:00
更新日時: 2026-07-23 09:48

## 未リリース

- UV確認表示を追加(表示パネル > UVグリッド)。2D ビューにワールド比例のUVグリッド線(指定間隔、5本ごとに明るい線)とリボンのプロファイル境界線(センターライン / 道路端 / 路肩端 / 法尻)をオーバーレイ表示。3D ビューではリボンのワールドメッシュのワイヤーフレームが同じ間隔の iso-u / iso-v 線になり、カーブ内側の詰まり(メトリック歪み)を目視確認できる

- `Ribbon` ノードに Path 入力ピンを追加し、曲線センターラインに対応。`Path` ノードのスプライン(Catmull-Rom / 直線)を弧長パラメータでリサンプリングして掃引ベイクし、曲率 κ(u) による $s_u = 1 - \kappa v$ 補正を N_z 計算に反映。未接続時は従来どおり直線デモセンターライン
- Ribbon の World Preview 設定を追加。3D ビューにワールド空間の掃引リボンメッシュを表示(CPU Mesh バックエンドのみ。カーブ内側のカスプは曲率半径手前でクランプ)。2D ビューは従来どおり UV 空間
- デモプロジェクトを S 字カーブ(約80m、最小半径 16m)+ 82m アトラス(16cm/texel)に更新

- terrain-editor (v0.24.0) を fork してプロジェクト基盤を構築。`RoadsideEditor v0.1.0` / `roadside_editor.exe` として C++20 / CMake / vcpkg / Win32 + Direct3D 12 でビルド可能に
- `Ribbon` ソースノードを追加。直線センターライン + 縦断勾配の道路プロファイル(道路・路肩・法面)をワールド比例UV(1テクセル = 指定cm)のUVグリッドへベイクし、路肩・法面に初期ノイズの山を生成する。ハイトマップ出力は侵食ポテンシャル φ = P_z + h・N_z
- `Multi-Scale Erosion` に `Displacement` 出力ピンを追加。Ribbon 上流のとき法線方向変位 h = (φ - P_z)/N_z を可視化・マスク書き出しできる(0.5 が変位ゼロ)
- 起動引数にプロジェクトパスを渡すと開けるようにした(`roadside_editor.exe path.terrainproj`)
- デモプロジェクト [assets/sample/demo_ribbon_erosion.terrainproj](../assets/sample/demo_ribbon_erosion.terrainproj) を追加(Ribbon → Multi-Scale Erosion)
- 起動用バッチ `start.bat` を追加(引数省略時はデモプロジェクトを開く)
