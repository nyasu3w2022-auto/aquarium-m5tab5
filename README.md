# Aquarium M5Stack TAB5 Edition

M5Stack TAB5上で動作するネオンテトラ飼育シミュレーションアプリケーションです。

## 概要

このプロジェクトは、M5Stack TAB5の5インチタッチスクリーン（1280×720）上にリアルなネオンテトラが泳ぐシミュレーションを実装しています。

## 機能

- **リアルな魚のアニメーション**: AI生成されたネオンテトラ画像（358×200px）を使用
- **複数の魚**: 3匹のネオンテトラが同時に泳ぐ
- **速度ベースの移動**: 等速直線運動、画面端での反射、速度制限を実装
- **スムーズな方向転換**: 横方向のスケール変換による自然な方向転換アニメーション（約1秒）
- **泳ぎのアニメーション**: sin波による上下の揺れで泳ぎを表現
- **ランダムな行動**: 魚がランダムに方向を変更（3～8秒間隔）
- **最適化された描画**: 動的リサイズバッファによる部分転送で高速描画

## ハードウェア要件

- M5Stack TAB5（ESP32-P4搭載）
- USB-Cケーブル（プログラム書き込み用）

## セットアップ

### 1. PlatformIOのインストール

```bash
pip install platformio
```

### 2. プロジェクトのクローン

```bash
git clone https://github.com/nyasu3w2022-auto/aquarium-m5tab5.git
cd aquarium-m5tab5
```

### 3. 依存ライブラリのインストール

```bash
platformio lib install
```

### 4. 画像ファイルのアップロード

```bash
platformio run --target uploadfs
```

### 5. M5Stack TAB5への書き込み

```bash
platformio run --target upload
```

## ファイル構成

```
aquarium-m5tab5/
├── platformio.ini           # PlatformIO設定ファイル
├── src/
│   └── main.cpp            # メインプログラム
├── data/
│   └── images/
│       ├── neon_tetra_right_optimized.png    # 右向きのネオンテトラ（358×200px）
│       └── neon_tetra_left_optimized.png     # 左向きのネオンテトラ（358×200px）
└── README.md               # このファイル
```

## 実装詳細

### 魚のデータ構造

```cpp
struct NeonTetra {
    float x;                    // X座標
    float y;                    // Y座標
    float vx;                   // X方向の速度
    float vy;                   // Y方向の速度
    bool facing_right;          // 右向きか左向きか
    int width;                  // 画像幅（358px）
    int height;                 // 画像高さ（200px）
    uint32_t last_direction_change;  // 最後に方向が変わった時刻
    float swim_phase;           // 泳ぎのアニメーション位相
    float swim_speed;           // 泳ぎの速度（個体差）
    bool is_turning;            // 方向転換中かどうか
    float turn_progress;        // 方向転換の進行度（0.0〜1.0）
    bool turn_target_right;     // 方向転換後の向き
    int prev_draw_x;            // 前回の描画位置X
    int prev_draw_y;            // 前回の描画位置Y
    int curr_draw_x;            // 今回の描画位置X
    int curr_draw_y;            // 今回の描画位置Y
};
```

### 主要な関数

- `initDisplay()`: ディスプレイの初期化（横向き、1280×720）
- `loadFishImages()`: 魚の画像をLittleFSから読み込み
- `initFishes()`: 魚を初期化（3匹、ランダムな位置と速度）
- `updateFishes()`: 魚の位置と速度を更新（等速直線運動、反射、ランダム変化）
- `drawScene()`: 最小矩形ダブルバッファで魚を描画

### 描画最適化

**動的リサイズバッファ方式：**

1. 全魚の前回位置と今回位置を含む最小矩形を計算
2. バッファキャンバスを矩形サイズにリサイズ（サイズ変更時のみ）
3. バッファに背景色と魚を描画
4. バッファを画面の矩形位置に転送

この方式により、全画面転送（約1.8MB/フレーム）と比較して、魚が近くにいる場合は約76%のデータ転送量削減を実現しています。

### 技術仕様

| 項目 | 仕様 |
|------|------|
| プラットフォーム | ESP32-P4（M5Stack TAB5） |
| ディスプレイ | 5インチ、1280×720、横向き |
| ファイルシステム | LittleFS |
| ライブラリ | M5Unified、M5GFX |
| 画像フォーマット | PNG（透過あり） |
| メモリ | PSRAM使用（スプライト、バッファ） |

## 今後の拡張予定

- [ ] タッチ入力による魚への給餌機能
- [ ] 魚の数を動的に変更
- [ ] 複数の魚種の追加
- [ ] 背景のバリエーション（水草、岩など）
- [ ] FPSカウンター表示

## トラブルシューティング

### コンパイルエラーが出る場合

1. PlatformIOが正しくインストールされているか確認
2. M5Unified と M5GFX ライブラリが正しくインストールされているか確認
3. platformio.ini の設定が正しいか確認

### M5Stack TAB5が認識されない場合

1. USB-Cケーブルが正しく接続されているか確認（データ転送対応ケーブルを使用）
2. ドライバがインストールされているか確認
3. 別のUSBポートを試す

### 画像が表示されない場合

1. `platformio run --target uploadfs` で画像ファイルをアップロードしたか確認
2. LittleFSが正しく初期化されているか確認（シリアルモニタでログを確認）
3. 画像ファイルが `data/images/` ディレクトリに存在するか確認

## ライセンス

MIT License

## 作成者

Manus AI Assistant

## リポジトリ

https://github.com/nyasu3w2022-auto/aquarium-m5tab5
