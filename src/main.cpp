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
    float swim_phase;  // 泳ぎのアニメーション位相
    float swim_speed;  // 泳ぎの速度（個体差）
    bool is_turning;   // 方向転換中かどうか
    float turn_progress; // 方向転換の進行度（0.0〜1.0）
    bool turn_target_right; // 方向転換後の向き
    // 前回の描画位置（部分更新用）
    int prev_draw_x;
    int prev_draw_y;
};

// グローバル変数
std::vector<NeonTetra> fishes;
M5GFX gfx;
LGFX_Device* display;
M5Canvas fish_sprite_right;
M5Canvas fish_sprite_left;
M5Canvas fish_canvas;  // 魚描画用の小キャンバス（背景と合成用）
bool sprites_loaded = false;
int screen_width = 0;
int screen_height = 0;
uint16_t bg_color;  // 背景色

// 魚画像のサイズ
const int FISH_WIDTH = 358;
const int FISH_HEIGHT = 200;

// 関数プロトタイプ
void initDisplay();
void loadFishImages();
void initFishes();
void updateFishes(uint32_t delta_ms);
void drawScene();

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
    
    // 魚を更新
    updateFishes(delta_ms);
    
    // シーンを描画（部分更新）
    drawScene();
}

void initDisplay() {
    // M5Unified経由でディスプレイを初期化
    display = &M5.Display;
    display->setRotation(3);  // 横向き（landscape、180度回転）に設定
    
    // 画面サイズを取得
    screen_width = display->width();
    screen_height = display->height();
    
    // 背景色を設定
    bg_color = display->color565(50, 120, 180);
    
    // 最初に背景を一度だけ描画
    display->fillScreen(bg_color);
    
    // 魚描画用の小キャンバスを作成（PSRAMを使用）
    fish_canvas.setColorDepth(16);
    fish_canvas.setPsram(true);
    fish_canvas.createSprite(FISH_WIDTH, FISH_HEIGHT);
    
    M5_LOGI("Display size: %d x %d", screen_width, screen_height);
    M5_LOGI("Partial update with compositing enabled");
}

void loadFishImages() {
    // スプライトを初期化（PSRAMを使用）
    fish_sprite_right.setColorDepth(16);
    fish_sprite_right.setPsram(true);
    fish_sprite_left.setColorDepth(16);
    fish_sprite_left.setPsram(true);
    
    // 右向きの魚の画像を読み込み（左右反転させた画像）
    File file_right = LittleFS.open("/images/neon_tetra_right_optimized.png", "r");
    if (file_right) {
        size_t file_size = file_right.size();
        uint8_t* buffer = (uint8_t*)malloc(file_size);
        if (buffer) {
            file_right.readBytes((char*)buffer, file_size);
            file_right.close();
            
            fish_sprite_right.createSprite(FISH_WIDTH, FISH_HEIGHT);
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
            
            fish_sprite_left.createSprite(FISH_WIDTH, FISH_HEIGHT);
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
        fish_sprite_right.createSprite(FISH_WIDTH, FISH_HEIGHT);
        fish_sprite_right.fillSprite(TFT_BLACK);
        fish_sprite_right.fillEllipse(179, 100, 120, 60, TFT_CYAN);
        fish_sprite_right.fillEllipse(240, 100, 80, 40, TFT_RED);
        fish_sprite_right.fillCircle(140, 90, 8, TFT_WHITE);
        fish_sprite_right.fillCircle(140, 90, 4, TFT_BLACK);
        
        // 左向きの魚
        fish_sprite_left.createSprite(FISH_WIDTH, FISH_HEIGHT);
        fish_sprite_left.fillSprite(TFT_BLACK);
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
        fish.x = random(0, screen_width - FISH_WIDTH);
        fish.y = random(100, screen_height - 300);
        fish.vx = (random(0, 2) == 0 ? 1 : -1) * (0.5f + random(0, 100) / 200.0f);
        fish.vy = (random(0, 2) == 0 ? 1 : -1) * (0.1f + random(0, 50) / 500.0f);
        fish.facing_right = fish.vx > 0;
        fish.width = FISH_WIDTH;
        fish.height = FISH_HEIGHT;
        fish.last_direction_change = millis();
        fish.swim_phase = random(0, 628) / 100.0f;  // 0〜2πのランダム位相
        fish.swim_speed = 3.0f + random(0, 200) / 100.0f;  // 3〜5の個体差
        fish.is_turning = false;  // 方向転換中ではない
        fish.turn_progress = 0.0f;  // 方向転換の進行度
        fish.turn_target_right = fish.facing_right;  // 現在の向き
        // 前回の描画位置を初期化
        fish.prev_draw_x = (int)fish.x;
        fish.prev_draw_y = (int)fish.y;
        
        fishes.push_back(fish);
    }
}

void updateFishes(uint32_t delta_ms) {
    float delta_sec = delta_ms / 1000.0f;
    
    for (auto& fish : fishes) {
        // 位置を更新
        fish.x += fish.vx * delta_sec * 50; // スケール調整
        fish.y += fish.vy * delta_sec * 50;
        
        // 泳ぎのアニメーション位相を更新
        fish.swim_phase += delta_sec * fish.swim_speed;
        if (fish.swim_phase > 2 * M_PI) {
            fish.swim_phase -= 2 * M_PI;
        }
        
        // 画面の端で反射
        if (fish.x < 0 || fish.x + fish.width > screen_width) {
            fish.vx = -fish.vx;
            bool new_facing = fish.vx > 0;
            if (new_facing != fish.facing_right && !fish.is_turning) {
                // 方向転換アニメーションを開始
                fish.is_turning = true;
                fish.turn_progress = 0.0f;
                fish.turn_target_right = new_facing;
            }
            fish.x = constrain(fish.x, 0, screen_width - fish.width);
        }
        
        if (fish.y < 0 || fish.y + fish.height > screen_height) {
            fish.vy = -fish.vy;
            fish.y = constrain(fish.y, 0, screen_height - fish.height);
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
            
            bool new_facing = fish.vx > 0;
            if (new_facing != fish.facing_right && !fish.is_turning) {
                // 方向転換アニメーションを開始
                fish.is_turning = true;
                fish.turn_progress = 0.0f;
                fish.turn_target_right = new_facing;
            }
            fish.last_direction_change = current_time;
        }
        
        // 方向転換アニメーションの更新
        if (fish.is_turning) {
            fish.turn_progress += delta_sec * 1.0f;  // 約1秒で完了
            if (fish.turn_progress >= 1.0f) {
                fish.turn_progress = 1.0f;
                fish.is_turning = false;
                fish.facing_right = fish.turn_target_right;
            }
        }
    }
}

void drawScene() {
    // 各魚を個別に処理（消去→合成→描画をセットで行う）
    for (auto& fish : fishes) {
        // 泳ぎのアニメーション効果：上下の揺れ
        float y_offset = sin(fish.swim_phase) * 5.0f;
        
        // 描画位置
        int draw_x = (int)fish.x;
        int draw_y = (int)(fish.y + y_offset);
        
        // 1. 前回の位置を背景色で消去
        display->fillRect(fish.prev_draw_x, fish.prev_draw_y, FISH_WIDTH, FISH_HEIGHT, bg_color);
        
        // 2. 小キャンバスに背景色を塗る
        fish_canvas.fillSprite(bg_color);
        
        // 3. 小キャンバスに魚を描画（透過色を指定）
        if (fish.is_turning) {
            // 方向転換アニメーション：横幅のスケール変化
            float scale_x = 1.0f;
            M5Canvas* source_sprite;
            
            if (fish.turn_progress < 0.5f) {
                // 0.0 → 0.5: 横幅が 1.0 → 0.0 （縮小）
                scale_x = 1.0f - (fish.turn_progress * 2.0f);
                source_sprite = fish.facing_right ? &fish_sprite_right : &fish_sprite_left;
            } else {
                // 0.5 → 1.0: 横幅が 0.0 → 1.0 （拡大）
                scale_x = (fish.turn_progress - 0.5f) * 2.0f;
                source_sprite = fish.turn_target_right ? &fish_sprite_right : &fish_sprite_left;
            }
            
            // スケール変化を適用
            if (scale_x > 0.05f) {
                // キャンバスの中心に描画
                source_sprite->pushRotateZoom(&fish_canvas, FISH_WIDTH/2, FISH_HEIGHT/2, 0, scale_x, 1.0f, TFT_BLACK);
            }
        } else {
            // 通常描画
            if (fish.facing_right) {
                fish_sprite_right.pushSprite(&fish_canvas, 0, 0, TFT_BLACK);
            } else {
                fish_sprite_left.pushSprite(&fish_canvas, 0, 0, TFT_BLACK);
            }
        }
        
        // 4. 小キャンバスを画面に転送（透過なし、完全に上書き）
        fish_canvas.pushSprite(display, draw_x, draw_y);
        
        // 今回の描画位置を保存（次回の消去用）
        fish.prev_draw_x = draw_x;
        fish.prev_draw_y = draw_y;
    }
}
