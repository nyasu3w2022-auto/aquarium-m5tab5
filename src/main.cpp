#include <M5Unified.h>
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
    float swim_phase;  // 泳ぎのアニメーション位相（0.0〜6.0）
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
LGFX_Device* display;

// 泳ぎアニメーション用の魚画像（左6フレーム + 右6フレーム）
M5Canvas fish_sprites_left[6];
M5Canvas fish_sprites_right[6];

// 方向転換用の画像（既存の5枚）
M5Canvas fish_sprite_left_90;
M5Canvas fish_sprite_left_45;
M5Canvas fish_sprite_front;
M5Canvas fish_sprite_right_45;
M5Canvas fish_sprite_right_90;

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
M5Canvas* getFishSprite(const NeonTetra& fish);

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
    display = &M5.Display;
    display->init();
    display->setRotation(1);
    
    screen_width = display->width();
    screen_height = display->height();
    
    // 背景色を設定（水色）
    bg_color = display->color565(30, 144, 255);  // DodgerBlue
    
    // 背景を塗りつぶし
    display->fillScreen(bg_color);
    
    // ダブルバッファの最大サイズを設定
    buffer_max_width = screen_width;
    buffer_max_height = screen_height;
    
    M5_LOGI("Display size: %d x %d", screen_width, screen_height);
    M5_LOGI("Dynamic buffer canvas enabled");
}

void loadFishImages() {
    // 一時キャンバスを作成
    M5Canvas temp_canvas;
    temp_canvas.setColorDepth(16);
    temp_canvas.createSprite(FISH_WIDTH, FISH_HEIGHT);
    
    // 左向き泳ぎアニメーション（6フレーム）
    const char* left_swim_files[6] = {
        "/images/swim/neon_tetra_left_swim1_optimized.png",
        "/images/swim/neon_tetra_left_swim2_optimized.png",
        "/images/swim/neon_tetra_left_swim3_optimized.png",
        "/images/swim/neon_tetra_left_swim4_optimized.png",
        "/images/swim/neon_tetra_left_swim5_optimized.png",
        "/images/swim/neon_tetra_left_swim6_optimized.png"
    };
    
    for (int i = 0; i < 6; i++) {
        fish_sprites_left[i].setColorDepth(16);
        fish_sprites_left[i].createSprite(FISH_WIDTH, FISH_HEIGHT);
        
        temp_canvas.fillSprite(TFT_GREEN);
        temp_canvas.drawPngFile(LittleFS, left_swim_files[i]);
        temp_canvas.pushSprite(&fish_sprites_left[i], 0, 0);
        M5_LOGI("Loaded: %s", left_swim_files[i]);
    }
    
    // 右向き泳ぎアニメーション（6フレーム）
    const char* right_swim_files[6] = {
        "/images/swim/neon_tetra_right_swim1_optimized.png",
        "/images/swim/neon_tetra_right_swim2_optimized.png",
        "/images/swim/neon_tetra_right_swim3_optimized.png",
        "/images/swim/neon_tetra_right_swim4_optimized.png",
        "/images/swim/neon_tetra_right_swim5_optimized.png",
        "/images/swim/neon_tetra_right_swim6_optimized.png"
    };
    
    for (int i = 0; i < 6; i++) {
        fish_sprites_right[i].setColorDepth(16);
        fish_sprites_right[i].createSprite(FISH_WIDTH, FISH_HEIGHT);
        
        temp_canvas.fillSprite(TFT_GREEN);
        temp_canvas.drawPngFile(LittleFS, right_swim_files[i]);
        temp_canvas.pushSprite(&fish_sprites_right[i], 0, 0);
        M5_LOGI("Loaded: %s", right_swim_files[i]);
    }
    
    // 方向転換用の画像（既存の5枚）
    fish_sprite_left_90.setColorDepth(16);
    fish_sprite_left_90.createSprite(FISH_WIDTH, FISH_HEIGHT);
    temp_canvas.fillSprite(TFT_GREEN);
    temp_canvas.drawPngFile(LittleFS, "/images/neon_tetra_left_optimized.png");
    temp_canvas.pushSprite(&fish_sprite_left_90, 0, 0);
    
    fish_sprite_left_45.setColorDepth(16);
    fish_sprite_left_45.createSprite(FISH_WIDTH, FISH_HEIGHT);
    temp_canvas.fillSprite(TFT_GREEN);
    temp_canvas.drawPngFile(LittleFS, "/images/neon_tetra_45left_optimized.png");
    temp_canvas.pushSprite(&fish_sprite_left_45, 0, 0);
    
    fish_sprite_front.setColorDepth(16);
    fish_sprite_front.createSprite(FISH_WIDTH, FISH_HEIGHT);
    temp_canvas.fillSprite(TFT_GREEN);
    temp_canvas.drawPngFile(LittleFS, "/images/neon_tetra_front_optimized.png");
    temp_canvas.pushSprite(&fish_sprite_front, 0, 0);
    
    fish_sprite_right_45.setColorDepth(16);
    fish_sprite_right_45.createSprite(FISH_WIDTH, FISH_HEIGHT);
    temp_canvas.fillSprite(TFT_GREEN);
    temp_canvas.drawPngFile(LittleFS, "/images/neon_tetra_45right_optimized.png");
    temp_canvas.pushSprite(&fish_sprite_right_45, 0, 0);
    
    fish_sprite_right_90.setColorDepth(16);
    fish_sprite_right_90.createSprite(FISH_WIDTH, FISH_HEIGHT);
    temp_canvas.fillSprite(TFT_GREEN);
    temp_canvas.drawPngFile(LittleFS, "/images/neon_tetra_right_optimized.png");
    temp_canvas.pushSprite(&fish_sprite_right_90, 0, 0);
    
    temp_canvas.deleteSprite();
    
    sprites_loaded = true;
}

void initFishes() {
    // 3匹のネオンテトラを初期化
    for (int i = 0; i < 3; i++) {
        NeonTetra fish;
        fish.x = random(100, screen_width - FISH_WIDTH - 100);
        fish.y = random(100, screen_height - FISH_HEIGHT - 100);
        fish.vx = (random(0, 2) == 0 ? -1.0f : 1.0f) * (random(50, 150) / 100.0f);
        fish.vy = (random(0, 2) == 0 ? -1.0f : 1.0f) * (random(50, 150) / 100.0f);
        fish.facing_right = fish.vx > 0;
        fish.width = FISH_WIDTH;
        fish.height = FISH_HEIGHT;
        fish.last_direction_change = millis();
        fish.swim_phase = random(0, 600) / 100.0f;  // 0〜6.0
        fish.swim_speed = random(80, 120) / 100.0f;  // 0.8〜1.2
        fish.is_turning = false;
        fish.turn_progress = 0.0f;
        fish.turn_target_right = fish.facing_right;
        fish.prev_draw_x = (int)fish.x;
        fish.prev_draw_y = (int)fish.y;
        fish.curr_draw_x = (int)fish.x;
        fish.curr_draw_y = (int)fish.y;
        fishes.push_back(fish);
    }
}

void updateFishes(uint32_t delta_ms) {
    float delta_sec = delta_ms / 1000.0f;
    
    for (auto& fish : fishes) {
        // 方向転換中の処理
        if (fish.is_turning) {
            fish.turn_progress += delta_sec / 1.0f;  // 1秒で完了
            if (fish.turn_progress >= 1.0f) {
                fish.turn_progress = 1.0f;
                fish.is_turning = false;
                fish.facing_right = fish.turn_target_right;
            }
        } else {
            // 泳ぎアニメーションの位相を更新（0.0〜6.0の範囲で循環）
            fish.swim_phase += delta_sec * 6.0f * fish.swim_speed;  // 1秒で1サイクル
            if (fish.swim_phase >= 6.0f) {
                fish.swim_phase -= 6.0f;
            }
        }
        
        // 位置を更新
        fish.x += fish.vx * delta_sec * 50;
        fish.y += fish.vy * delta_sec * 50;
        
        // 画面端での反射
        if (fish.x < 0 || fish.x + fish.width > screen_width) {
            fish.vx = -fish.vx;
            fish.x = constrain(fish.x, 0, screen_width - fish.width);
            
            // 方向転換開始
            if (!fish.is_turning) {
                fish.is_turning = true;
                fish.turn_progress = 0.0f;
                fish.turn_target_right = fish.vx > 0;
            }
        }
        
        if (fish.y < 0 || fish.y + fish.height > screen_height) {
            fish.vy = -fish.vy;
            fish.y = constrain(fish.y, 0, screen_height - fish.height);
        }
        
        // ランダムな速度変更（ごくまれに）
        if (random(0, 1000) < 10) {
            fish.vx += (random(0, 200) - 100) / 100.0f;
            fish.vy += (random(0, 200) - 100) / 100.0f;
            
            // 速度制限
            float speed = sqrt(fish.vx * fish.vx + fish.vy * fish.vy);
            if (speed > 2.0f) {
                fish.vx = fish.vx / speed * 2.0f;
                fish.vy = fish.vy / speed * 2.0f;
            }
        }
        
        // 今回の描画位置を計算
        fish.curr_draw_x = (int)fish.x;
        fish.curr_draw_y = (int)fish.y;
    }
}

M5Canvas* getFishSprite(const NeonTetra& fish) {
    // 方向転換中は方向転換用の画像を使用
    if (fish.is_turning) {
        if (fish.turn_progress < 0.25f) {
            // 最初の25%: 真横
            return fish.turn_target_right ? &fish_sprite_left_90 : &fish_sprite_right_90;
        } else if (fish.turn_progress < 0.5f) {
            // 25〜50%: 斜め45度
            return fish.turn_target_right ? &fish_sprite_left_45 : &fish_sprite_right_45;
        } else if (fish.turn_progress < 0.75f) {
            // 50〜75%: 正面
            return &fish_sprite_front;
        } else {
            // 75〜100%: 反対側の斜め45度
            return fish.turn_target_right ? &fish_sprite_right_45 : &fish_sprite_left_45;
        }
    }
    
    // 通常の泳ぎアニメーション（6フレーム）
    int frame_index = (int)fish.swim_phase;  // 0〜5
    if (frame_index < 0) frame_index = 0;
    if (frame_index > 5) frame_index = 5;
    
    if (fish.facing_right) {
        return &fish_sprites_right[frame_index];
    } else {
        return &fish_sprites_left[frame_index];
    }
}

void drawScene() {
    if (!sprites_loaded) return;
    
    // 全魚の前回位置と今回位置を含む最小矩形を計算
    int min_x = screen_width;
    int max_x = 0;
    int min_y = screen_height;
    int max_y = 0;
    
    for (const auto& fish : fishes) {
        // 前回の位置
        min_x = min(min_x, fish.prev_draw_x);
        max_x = max(max_x, fish.prev_draw_x + FISH_WIDTH);
        min_y = min(min_y, fish.prev_draw_y);
        max_y = max(max_y, fish.prev_draw_y + FISH_HEIGHT);
        
        // 今回の位置
        min_x = min(min_x, fish.curr_draw_x);
        max_x = max(max_x, fish.curr_draw_x + FISH_WIDTH);
        min_y = min(min_y, fish.curr_draw_y);
        max_y = max(max_y, fish.curr_draw_y + FISH_HEIGHT);
    }
    
    // 矩形のサイズを計算
    int rect_width = max_x - min_x;
    int rect_height = max_y - min_y;
    
    // 画面外にはみ出さないようにクリップ
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x > screen_width) max_x = screen_width;
    if (max_y > screen_height) max_y = screen_height;
    rect_width = max_x - min_x;
    rect_height = max_y - min_y;
    
    // バッファのサイズが変わった場合のみリサイズ
    static int prev_rect_width = 0;
    static int prev_rect_height = 0;
    if (rect_width != prev_rect_width || rect_height != prev_rect_height) {
        buffer_canvas.deleteSprite();
        buffer_canvas.setColorDepth(16);
        buffer_canvas.createSprite(rect_width, rect_height);
        prev_rect_width = rect_width;
        prev_rect_height = rect_height;
        M5_LOGI("Buffer resized: %d x %d", rect_width, rect_height);
    }
    
    // バッファに背景色を塗る
    buffer_canvas.fillSprite(bg_color);
    
    // バッファに全ての魚を描画
    for (const auto& fish : fishes) {
        // バッファ内の相対位置を計算
        int rel_x = fish.curr_draw_x - min_x;
        int rel_y = fish.curr_draw_y - min_y;
        
        // 適切なフレームの画像を取得
        M5Canvas* sprite = getFishSprite(fish);
        
        // 魚を描画（緑色を透過）
        sprite->pushSprite(&buffer_canvas, rel_x, rel_y, TFT_GREEN);
    }
    
    // バッファを画面に転送
    buffer_canvas.pushSprite(display, min_x, min_y);
    
    // 前回の描画位置を更新
    for (auto& fish : fishes) {
        fish.prev_draw_x = fish.curr_draw_x;
        fish.prev_draw_y = fish.curr_draw_y;
    }
}
