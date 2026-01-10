#include <M5Unified.h>
#include <M5GFX.h>
#include <LittleFS.h>
#include <vector>
#include <cmath>

// ネオンテトラの構造体
struct NeonTetra {
    float x;           // X座標
    float y;           // Y座標
    float vx;          // X方向の速度
    float vy;          // Y方向の速度
    bool facing_right; // 右向きか左向きか
    int width;         // 画像幅
    int height;        // 画像高さ
    uint32_t last_direction_change; // 最後に方向が変わった時刻
};

// グローバル変数
std::vector<NeonTetra> fishes;
M5GFX gfx;
LGFX_Device* display;
M5Canvas fish_sprite_right;
M5Canvas fish_sprite_left;
bool sprites_loaded = false;

// 関数プロトタイプ
void initDisplay();
void loadFishImages();
void initFishes();
void updateFishes(uint32_t delta_ms);
void drawFishes();
void drawBackground();

void setup() {
    // M5Stackの初期化
    auto cfg = M5.config();
    M5.begin(cfg);
    
    // ディスプレイの初期化
    initDisplay();
    
    // LittleFSの初期化
    if (!LittleFS.begin(true)) {
        M5_LOGE("LittleFS Mount Failed");
        return;
    }
    
    // 魚の画像を読み込み
    loadFishImages();
    
    // 魚を初期化
    initFishes();
    
    M5_LOGI("Setup complete");
}

void loop() {
    static uint32_t last_time = millis();
    uint32_t current_time = millis();
    uint32_t delta_ms = current_time - last_time;
    last_time = current_time;
    
    // 背景を描画
    drawBackground();
    
    // 魚を更新
    updateFishes(delta_ms);
    
    // 魚を描画
    drawFishes();
    
    // フレームレート制御（約30FPS）
    delay(33);
}

void initDisplay() {
    // M5Unified経由でディスプレイを初期化
    display = &M5.Display;
    display->setRotation(1);  // 横向き（landscape）に設定
    display->fillScreen(TFT_BLACK);
}

void loadFishImages() {
    // スプライトを初期化
    fish_sprite_right.setColorDepth(16);
    fish_sprite_left.setColorDepth(16);
    
    // 右向きの魚の画像を読み込み
    File file_right = LittleFS.open("/images/neon_tetra_side_optimized.png", "r");
    if (file_right) {
        size_t file_size = file_right.size();
        uint8_t* buffer = (uint8_t*)malloc(file_size);
        if (buffer) {
            file_right.readBytes((char*)buffer, file_size);
            file_right.close();
            
            fish_sprite_right.createSprite(358, 200);
            fish_sprite_right.fillSprite(TFT_BLACK);  // 背景を黒で塗りつぶし
            if (fish_sprite_right.drawPng(buffer, file_size, 0, 0)) {
                M5_LOGI("Loaded fish image (right)");
                sprites_loaded = true;
            } else {
                M5_LOGE("Failed to draw fish image (right)");
            }
            free(buffer);
        } else {
            M5_LOGE("Memory allocation failed (right)");
            file_right.close();
        }
    } else {
        M5_LOGE("Failed to open fish image file (right)");
    }
    
    // 左向きの魚の画像を読み込み
    File file_left = LittleFS.open("/images/neon_tetra_left_optimized.png", "r");
    if (file_left) {
        size_t file_size = file_left.size();
        uint8_t* buffer = (uint8_t*)malloc(file_size);
        if (buffer) {
            file_left.readBytes((char*)buffer, file_size);
            file_left.close();
            
            fish_sprite_left.createSprite(358, 200);
            fish_sprite_left.fillSprite(TFT_BLACK);  // 背景を黒で塗りつぶし
            if (fish_sprite_left.drawPng(buffer, file_size, 0, 0)) {
                M5_LOGI("Loaded fish image (left)");
            } else {
                M5_LOGE("Failed to draw fish image (left)");
            }
            free(buffer);
        } else {
            M5_LOGE("Memory allocation failed (left)");
            file_left.close();
        }
    } else {
        M5_LOGE("Failed to open fish image file (left)");
    }
    
    // 画像の読み込みに失敗した場合のフォールバック
    if (!sprites_loaded) {
        M5_LOGW("Using fallback graphics");
        // 右向きの魚
        fish_sprite_right.createSprite(358, 200);
        fish_sprite_right.fillSprite(TFT_TRANSPARENT);
        fish_sprite_right.fillEllipse(179, 100, 120, 60, TFT_CYAN);
        fish_sprite_right.fillEllipse(240, 100, 80, 40, TFT_RED);
        fish_sprite_right.fillCircle(140, 90, 8, TFT_WHITE);
        fish_sprite_right.fillCircle(140, 90, 4, TFT_BLACK);
        
        // 左向きの魚
        fish_sprite_left.createSprite(358, 200);
        fish_sprite_left.fillSprite(TFT_TRANSPARENT);
        fish_sprite_left.fillEllipse(179, 100, 120, 60, TFT_CYAN);
        fish_sprite_left.fillEllipse(118, 100, 80, 40, TFT_RED);
        fish_sprite_left.fillCircle(218, 90, 8, TFT_WHITE);
        fish_sprite_left.fillCircle(218, 90, 4, TFT_BLACK);
    }
}

void initFishes() {
    // 複数の魚を初期化
    for (int i = 0; i < 3; i++) {
        NeonTetra fish;
        fish.x = random(0, display->width() - 358);
        fish.y = random(100, display->height() - 300);
        fish.vx = (random(0, 2) == 0 ? 1 : -1) * (0.5f + random(0, 100) / 200.0f);
        fish.vy = (random(0, 2) == 0 ? 1 : -1) * (0.1f + random(0, 50) / 500.0f);
        fish.facing_right = fish.vx > 0;
        fish.width = 358;   // 最適化された画像の幅
        fish.height = 200;  // 最適化された画像の高さ
        fish.last_direction_change = millis();
        
        fishes.push_back(fish);
    }
}

void updateFishes(uint32_t delta_ms) {
    float delta_sec = delta_ms / 1000.0f;
    
    for (auto& fish : fishes) {
        // 位置を更新
        fish.x += fish.vx * delta_sec * 50; // スケール調整
        fish.y += fish.vy * delta_sec * 50;
        
        // 画面の端で反射
        if (fish.x < 0 || fish.x + fish.width > display->width()) {
            fish.vx = -fish.vx;
            fish.facing_right = fish.vx > 0;
            fish.x = constrain(fish.x, 0, display->width() - fish.width);
        }
        
        if (fish.y < 0 || fish.y + fish.height > display->height()) {
            fish.vy = -fish.vy;
            fish.y = constrain(fish.y, 0, display->height() - fish.height);
        }
        
        // ランダムに方向を変更
        uint32_t current_time = millis();
        if (current_time - fish.last_direction_change > random(3000, 8000)) {
            fish.vx += (random(0, 2) == 0 ? 1 : -1) * (0.1f + random(0, 50) / 500.0f);
            fish.vy += (random(0, 2) == 0 ? 1 : -1) * (0.05f + random(0, 30) / 600.0f);
            
            // 速度の制限
            float speed = sqrt(fish.vx * fish.vx + fish.vy * fish.vy);
            if (speed > 2.0f) {
                fish.vx = (fish.vx / speed) * 2.0f;
                fish.vy = (fish.vy / speed) * 2.0f;
            }
            
            fish.facing_right = fish.vx > 0;
            fish.last_direction_change = current_time;
        }
    }
}

void drawBackground() {
    // 水色のグラデーション背景
    for (int y = 0; y < display->height(); y++) {
        // 上から下へ濃くなるグラデーション
        uint8_t r = map(y, 0, display->height(), 100, 50);
        uint8_t g = map(y, 0, display->height(), 180, 120);
        uint8_t b = map(y, 0, display->height(), 220, 180);
        uint32_t color = display->color565(r, g, b);
        display->drawFastHLine(0, y, display->width(), color);
    }
}

void drawFishes() {
    // スプライトを使用して魚を描画
    for (const auto& fish : fishes) {
        if (fish.facing_right) {
            fish_sprite_right.pushSprite(display, fish.x, fish.y, TFT_BLACK);
        } else {
            fish_sprite_left.pushSprite(display, fish.x, fish.y, TFT_BLACK);
        }
    }
}
