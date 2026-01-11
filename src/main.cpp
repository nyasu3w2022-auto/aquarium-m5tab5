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
    // 今回の描画位置
    int curr_draw_x;
    int curr_draw_y;
};

// グローバル変数
std::vector<NeonTetra> fishes;
M5GFX gfx;
LGFX_Device* display;

// 5つの角度の魚画像
M5Canvas fish_sprite_left_90;   // 左向き90度（真横）
M5Canvas fish_sprite_left_45;   // 左向き45度
M5Canvas fish_sprite_front;     // 正面0度
M5Canvas fish_sprite_right_45;  // 右向き45度
M5Canvas fish_sprite_right_90;  // 右向き90度（真横）

M5Canvas buffer_canvas;  // ダブルバッファ用キャンバス
bool sprites_loaded = false;
int screen_width = 0;
int screen_height = 0;
uint16_t bg_color;  // 背景色

// 魚画像のサイズ
const int FISH_WIDTH = 358;
const int FISH_HEIGHT = 200;

// ダブルバッファの最大サイズ（PSRAMに確保）
int buffer_max_width = 0;
int buffer_max_height = 0;

// 関数プロトタイプ
void initDisplay();
void loadFishImages();
void initFishes();
void updateFishes(uint32_t delta_ms);
void drawScene();
M5Canvas* getFishSprite(bool facing_right, float turn_progress, bool is_turning);

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
    
    // シーンを描画（最小矩形ダブルバッファ）
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
    
    // ダブルバッファ用キャンバスの設定（動的に作成される）
    buffer_max_width = screen_width;   // 画面幅
    buffer_max_height = screen_height; // 画面高さ
    
    buffer_canvas.setColorDepth(16);
    buffer_canvas.setPsram(true);
    // createSpriteは最初の描画時に動的に実行される
    
    M5_LOGI("Display size: %d x %d", screen_width, screen_height);
    M5_LOGI("Dynamic buffer canvas enabled");
}

void loadFishImages() {
    // スプライトを初期化（PSRAMを使用）
    fish_sprite_left_90.setColorDepth(16);
    fish_sprite_left_90.setPsram(true);
    fish_sprite_left_45.setColorDepth(16);
    fish_sprite_left_45.setPsram(true);
    fish_sprite_front.setColorDepth(16);
    fish_sprite_front.setPsram(true);
    fish_sprite_right_45.setColorDepth(16);
    fish_sprite_right_45.setPsram(true);
    fish_sprite_right_90.setColorDepth(16);
    fish_sprite_right_90.setPsram(true);
    
    // 画像ファイルのリスト
    struct ImageInfo {
        const char* path;
        M5Canvas* canvas;
        const char* name;
    };
    
    ImageInfo images[] = {
        {"/images/neon_tetra_left_optimized.png", &fish_sprite_left_90, "left_90"},
        {"/images/neon_tetra_45left_optimized.png", &fish_sprite_left_45, "left_45"},
        {"/images/neon_tetra_front_optimized.png", &fish_sprite_front, "front"},
        {"/images/neon_tetra_45right_optimized.png", &fish_sprite_right_45, "right_45"},
        {"/images/neon_tetra_right_optimized.png", &fish_sprite_right_90, "right_90"}
    };
    
    // 各画像を読み込み
    for (const auto& img : images) {
        File file = LittleFS.open(img.path, "r");
        if (file) {
            size_t file_size = file.size();
            uint8_t* buffer = (uint8_t*)malloc(file_size);
            if (buffer) {
                file.readBytes((char*)buffer, file_size);
                file.close();
                
                img.canvas->createSprite(FISH_WIDTH, FISH_HEIGHT);
                img.canvas->fillSprite(TFT_BLACK);
                if (img.canvas->drawPng(buffer, file_size, 0, 0)) {
                    M5_LOGI("Loaded fish image: %s", img.name);
                    sprites_loaded = true;
                } else {
                    M5_LOGE("Failed to draw fish image: %s", img.name);
                }
                free(buffer);
            } else {
                M5_LOGE("Memory allocation failed: %s", img.name);
                file.close();
            }
        } else {
            M5_LOGE("Failed to open fish image file: %s", img.path);
        }
    }
    
    // 画像の読み込みに失敗した場合のフォールバック
    if (!sprites_loaded) {
        M5_LOGW("Using fallback graphics");
        // 簡易的な魚の形を描画
        fish_sprite_right_90.createSprite(FISH_WIDTH, FISH_HEIGHT);
        fish_sprite_right_90.fillSprite(TFT_BLACK);
        fish_sprite_right_90.fillEllipse(179, 100, 120, 60, TFT_CYAN);
        fish_sprite_right_90.fillEllipse(240, 100, 80, 40, TFT_RED);
        fish_sprite_right_90.fillCircle(140, 90, 8, TFT_WHITE);
        fish_sprite_right_90.fillCircle(140, 90, 4, TFT_BLACK);
        
        fish_sprite_left_90.createSprite(FISH_WIDTH, FISH_HEIGHT);
        fish_sprite_left_90.fillSprite(TFT_BLACK);
        fish_sprite_left_90.fillEllipse(179, 100, 120, 60, TFT_CYAN);
        fish_sprite_left_90.fillEllipse(118, 100, 80, 40, TFT_RED);
        fish_sprite_left_90.fillCircle(218, 90, 8, TFT_WHITE);
        fish_sprite_left_90.fillCircle(218, 90, 4, TFT_BLACK);
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
        fish.swim_phase = random(0, 628) / 100.0f;
        fish.swim_speed = 3.0f + random(0, 200) / 100.0f;
        fish.is_turning = false;
        fish.turn_progress = 0.0f;
        fish.turn_target_right = fish.facing_right;
        fish.prev_draw_x = (int)fish.x;
        fish.prev_draw_y = (int)fish.y;
        fish.curr_draw_x = fish.prev_draw_x;
        fish.curr_draw_y = fish.prev_draw_y;
        
        fishes.push_back(fish);
    }
}

void updateFishes(uint32_t delta_ms) {
    float delta_sec = delta_ms / 1000.0f;
    
    for (auto& fish : fishes) {
        // 位置を更新
        fish.x += fish.vx * delta_sec * 50;
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
            
            float speed = sqrt(fish.vx * fish.vx + fish.vy * fish.vy);
            if (speed > 2.0f) {
                fish.vx = (fish.vx / speed) * 2.0f;
                fish.vy = (fish.vy / speed) * 2.0f;
            }
            
            bool new_facing = fish.vx > 0;
            if (new_facing != fish.facing_right && !fish.is_turning) {
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
        
        // 今回の描画位置を計算
        float y_offset = sin(fish.swim_phase) * 5.0f;
        fish.curr_draw_x = (int)fish.x;
        fish.curr_draw_y = (int)(fish.y + y_offset);
    }
}

// 方向転換の進行度に応じて適切な魚の画像を返す
M5Canvas* getFishSprite(bool facing_right, float turn_progress, bool is_turning) {
    if (!is_turning) {
        // 方向転換していない場合は真横の画像
        return facing_right ? &fish_sprite_right_90 : &fish_sprite_left_90;
    }
    
    // 方向転換中：5段階の画像を使用
    // 0.0 → 0.25 → 0.5 → 0.75 → 1.0
    // 左90 → 左45 → 正面 → 右45 → 右90
    
    if (!facing_right) {
        // 左向き → 右向きへの転換
        if (turn_progress < 0.25f) {
            return &fish_sprite_left_90;
        } else if (turn_progress < 0.5f) {
            return &fish_sprite_left_45;
        } else if (turn_progress < 0.75f) {
            return &fish_sprite_front;
        } else {
            return &fish_sprite_right_45;
        }
    } else {
        // 右向き → 左向きへの転換
        if (turn_progress < 0.25f) {
            return &fish_sprite_right_90;
        } else if (turn_progress < 0.5f) {
            return &fish_sprite_right_45;
        } else if (turn_progress < 0.75f) {
            return &fish_sprite_front;
        } else {
            return &fish_sprite_left_45;
        }
    }
}

void drawScene() {
    // 1. 全ての魚の前回位置と今回位置を含む最小矩形を計算
    int min_x = screen_width;
    int min_y = screen_height;
    int max_x = 0;
    int max_y = 0;
    
    for (const auto& fish : fishes) {
        // 前回位置
        min_x = min(min_x, fish.prev_draw_x);
        min_y = min(min_y, fish.prev_draw_y);
        max_x = max(max_x, fish.prev_draw_x + FISH_WIDTH);
        max_y = max(max_y, fish.prev_draw_y + FISH_HEIGHT);
        
        // 今回位置
        min_x = min(min_x, fish.curr_draw_x);
        min_y = min(min_y, fish.curr_draw_y);
        max_x = max(max_x, fish.curr_draw_x + FISH_WIDTH);
        max_y = max(max_y, fish.curr_draw_y + FISH_HEIGHT);
    }
    
    // 画面内にクリップ
    min_x = max(0, min_x);
    min_y = max(0, min_y);
    max_x = min(screen_width, max_x);
    max_y = min(screen_height, max_y);
    
    int rect_width = max_x - min_x;
    int rect_height = max_y - min_y;
    
    // バッファサイズを超える場合は制限
    if (rect_width > buffer_max_width) rect_width = buffer_max_width;
    if (rect_height > buffer_max_height) rect_height = buffer_max_height;
    
    if (rect_width <= 0 || rect_height <= 0) return;
    
    // 2. バッファキャンバスを矩形サイズにリサイズ（効率化）
    // 前回とサイズが異なる場合のみ再作成
    static int prev_rect_width = 0;
    static int prev_rect_height = 0;
    if (rect_width != prev_rect_width || rect_height != prev_rect_height) {
        buffer_canvas.deleteSprite();
        buffer_canvas.createSprite(rect_width, rect_height);
        prev_rect_width = rect_width;
        prev_rect_height = rect_height;
    }
    
    // バッファに背景色を塗る
    buffer_canvas.fillSprite(bg_color);
    
    // 3. バッファキャンバスに全ての魚を描画
    for (const auto& fish : fishes) {
        // バッファ内での相対位置
        int rel_x = fish.curr_draw_x - min_x;
        int rel_y = fish.curr_draw_y - min_y;
        
        // 適切な角度の画像を取得
        M5Canvas* sprite = getFishSprite(fish.facing_right, fish.turn_progress, fish.is_turning);
        
        // 魚を描画
        sprite->pushSprite(&buffer_canvas, rel_x, rel_y, TFT_BLACK);
    }
    
    // 4. バッファキャンバスを画面に転送（部分転送）
    // キャンバスは矩形サイズなので、pushSpriteでそのまま転送
    buffer_canvas.pushSprite(display, min_x, min_y);
    
    // 5. 前回の描画位置を更新
    for (auto& fish : fishes) {
        fish.prev_draw_x = fish.curr_draw_x;
        fish.prev_draw_y = fish.curr_draw_y;
    }
}
