# AQUAZONE M5Stack TAB5 Edition

M5Stack TAB5上で動作するネオンテトラ飼育シミュレーションアプリケーションです。

## 概要

このプロジェクトは、M5Stack TAB5のタッチスクリーン上にリアルなネオンテトラが泳ぐシミュレーションを実装しています。

## 機能

- **リアルな魚のアニメーション**: ネオンテトラが自然な泳ぎ方で画面内を移動
- **複数の魚**: 3匹のネオンテトラが同時に泳ぐ
- **物理シミュレーション**: 速度、加速度、画面端での反射を実装
- **ランダムな行動**: 魚がランダムに方向を変更
- **水色のグラデーション背景**: 水中環境を表現

## ハードウェア要件

- M5Stack TAB5
- USB-Cケーブル（プログラム書き込み用）

## セットアップ

### 1. PlatformIOのインストール

```bash
pip install platformio
```

### 2. プロジェクトのクローン

```bash
cd aquazone_m5tab5
```

### 3. 依存ライブラリのインストール

```bash
platformio lib install
```

### 4. M5Stack TAB5への書き込み

```bash
platformio run --target upload
```

## ファイル構成

```
aquazone_m5tab5/
├── platformio.ini           # PlatformIO設定ファイル
├── src/
│   └── main.cpp            # メインプログラム
├── data/
│   └── images/
│       ├── neon_tetra_side_optimized.png    # 右向きのネオンテトラ
│       └── neon_tetra_left_optimized.png    # 左向きのネオンテトラ
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
    int width;                  // 画像幅
    int height;                 // 画像高さ
    uint32_t last_direction_change;  // 最後に方向が変わった時刻
};
```

### 主要な関数

- `initDisplay()`: ディスプレイの初期化
- `loadFishImages()`: 魚の画像をSPIFFSから読み込み
- `initFishes()`: 魚を初期化
- `updateFishes()`: 魚の位置と速度を更新
- `drawFishes()`: 魚を描画

## 今後の拡張予定

- [ ] タッチ入力による魚への給餌機能
- [ ] 水質管理システム
- [ ] 複数の魚種の追加
- [ ] セーブ・ロード機能
- [ ] 吹き出しメッセージシステム
- [ ] SDカードへのエキスポート・インポート機能

## トラブルシューティング

### コンパイルエラーが出る場合

1. PlatformIOが正しくインストールされているか確認
2. M5Unified と M5GFX ライブラリが正しくインストールされているか確認
3. platformio.ini の設定が正しいか確認

### M5Stack TAB5が認識されない場合

1. USB-Cケーブルが正しく接続されているか確認
2. ドライバがインストールされているか確認
3. 別のUSBポートを試す

## ライセンス

MIT License

## 作成者

Manus AI Assistant
