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
uint8_t* fish_image_right = nullptr;
uint8_t* fish_image_left = nullptr;
uint32_t fish_image_size = 0;

// 関数プロトタイプ
void initDisplay();
void loadFishImages();
void initFishes();
void updateFishes(uint32_t delta_ms);
void drawFishes();
void drawBackground();
bool loadPNGFromLittleFS(const char* path, uint8_t*& buffer, uint32_t& size);

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
    // 右向きの魚の画像を読み込み
    if (!loadPNGFromLittleFS("/images/neon_tetra_side_optimized.png", fish_image_right, fish_image_size)) {
        M5_LOGE("Failed to load fish image (right)");
    }
    
    // 左向きの魚の画像を読み込み
    if (!loadPNGFromLittleFS("/images/neon_tetra_left_optimized.png", fish_image_left, fish_image_size)) {
        M5_LOGE("Failed to load fish image (left)");
    }
}

bool loadPNGFromLittleFS(const char* path, uint8_t*& buffer, uint32_t& size) {
    // 簡易的な実装：ファイルサイズを取得して、バッファに読み込む
    File file = LittleFS.open(path, "r");
    if (!file) {
        M5_LOGE("File not found: %s", path);
        return false;
    }
    
    size = file.size();
    buffer = (uint8_t*)malloc(size);
    if (!buffer) {
        M5_LOGE("Memory allocation failed");
        file.close();
        return false;
    }
    
    file.readBytes((char*)buffer, size);
    file.close();
    
    M5_LOGI("Loaded image: %s (size: %d bytes)", path, size);
    return true;
}

void initFishes() {
    // 複数の魚を初期化
    for (int i = 0; i < 3; i++) {
        NeonTetra fish;
        fish.x = random(0, display->width());
        fish.y = random(100, display->height() - 100);
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
    // 注：実際のPNG描画にはM5GFXのpngDrawメソッドを使用する必要があります
    // ここでは簡略化のため、矩形で魚を表現します
    
    for (const auto& fish : fishes) {
        // 魚の体を描画（簡略版）
        uint32_t color = display->color565(0, 255, 150); // ネオンテトラの色
        
        // 魚の体
        display->fillRect(fish.x, fish.y, fish.width / 4, fish.height, color);
        
        // 目
        display->fillCircle(fish.x + (fish.facing_right ? fish.width / 4 - 5 : 5), 
                           fish.y + fish.height / 2 - 10, 3, TFT_WHITE);
        
        // 赤い部分
        uint32_t red_color = display->color565(255, 50, 50);
        display->fillRect(fish.x + fish.width / 4, fish.y + fish.height / 3, 
                         fish.width / 2, fish.height / 3, red_color);
    }
}
