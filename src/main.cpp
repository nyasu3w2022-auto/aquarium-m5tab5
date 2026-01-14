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

// 10段階の角度の魚画像（左向き0度〜右向き180度）
M5Canvas fish_sprites[10];

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
int getFishSpriteIndex(bool facing_right, float turn_progress, bool is_turning);

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
    display = &gfx;
    display->init();
    display->setRotation(1);
    
    screen_width = display->width();
    screen_height = display->height();
    
    M5_LOGI("Display size: %d x %d", screen_width, screen_height);
    M5_LOGI("Dynamic buffer canvas enabled");
    
    // 背景色を設定（濃い青）
    bg_color = display->color565(0, 102, 204);
    
    // 背景を一度だけ塗る
    display->fillScreen(bg_color);
}

void loadFishImages() {
    // 10段階の角度画像を読み込み
    const char* angle_files[10] = {
        "/images/angles/neon_tetra_angle_000_optimized.png",  // 0度（左向き真横）
        "/images/angles/neon_tetra_angle_020_optimized.png",  // 20度
        "/images/angles/neon_tetra_angle_040_optimized.png",  // 40度
        "/images/angles/neon_tetra_angle_060_optimized.png",  // 60度
        "/images/angles/neon_tetra_angle_090_optimized.png",  // 90度（正面）
        "/images/angles/neon_tetra_angle_100_optimized.png",  // 100度
        "/images/angles/neon_tetra_angle_120_optimized.png",  // 120度
        "/images/angles/neon_tetra_angle_140_optimized.png",  // 140度
        "/images/angles/neon_tetra_angle_160_optimized.png",  // 160度
        "/images/angles/neon_tetra_angle_180_optimized.png"   // 180度（右向き真横）
    };
    
    for (int i = 0; i < 10; i++) {
        fish_sprites[i].setColorDepth(16);
        fish_sprites[i].createSprite(FISH_WIDTH, FISH_HEIGHT);
        fish_sprites[i].fillSprite(TFT_BLACK);
        
        if (fish_sprites[i].drawPngFile(LittleFS, angle_files[i])) {
            M5_LOGI("Loaded: %s", angle_files[i]);
        } else {
            M5_LOGE("Failed to load: %s", angle_files[i]);
        }
    }
    
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
        fish.swim_phase = random(0, 314) / 100.0f;  // 0〜3.14
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
        }
        
        // 位置を更新
        fish.x += fish.vx * delta_sec * 50;
        fish.y += fish.vy * delta_sec * 50;
        
        // 画面端での反射
        if (fish.x < 0) {
            fish.x = 0;
            fish.vx = -fish.vx;
            if (!fish.is_turning && fish.vx > 0 && !fish.facing_right) {
                fish.is_turning = true;
                fish.turn_progress = 0.0f;
                fish.turn_target_right = true;
            }
        }
        if (fish.x + fish.width > screen_width) {
            fish.x = screen_width - fish.width;
            fish.vx = -fish.vx;
            if (!fish.is_turning && fish.vx < 0 && fish.facing_right) {
                fish.is_turning = true;
                fish.turn_progress = 0.0f;
                fish.turn_target_right = false;
            }
        }
        if (fish.y < 0) {
            fish.y = 0;
            fish.vy = -fish.vy;
        }
        if (fish.y + fish.height > screen_height) {
            fish.y = screen_height - fish.height;
            fish.vy = -fish.vy;
        }
        
        // ランダムに速度を変更
        if (millis() - fish.last_direction_change > 2000) {
            if (random(0, 100) < 5) {  // 5%の確率
                fish.vx += (random(0, 200) - 100) / 100.0f;
                fish.vy += (random(0, 200) - 100) / 100.0f;
                
                // 速度を制限
                float speed = sqrt(fish.vx * fish.vx + fish.vy * fish.vy);
                if (speed > 2.0f) {
                    fish.vx = fish.vx / speed * 2.0f;
                    fish.vy = fish.vy / speed * 2.0f;
                }
                
                // 方向転換が必要か判定
                bool should_face_right = fish.vx > 0;
                if (!fish.is_turning && should_face_right != fish.facing_right) {
                    fish.is_turning = true;
                    fish.turn_progress = 0.0f;
                    fish.turn_target_right = should_face_right;
                }
                
                fish.last_direction_change = millis();
            }
        }
        
        // 泳ぎのアニメーション位相を更新
        fish.swim_phase += delta_sec * 3.14f * fish.swim_speed;
        if (fish.swim_phase > 6.28f) {
            fish.swim_phase -= 6.28f;
        }
    }
}

void drawScene() {
    if (!sprites_loaded) return;
    
    // 全魚の前回位置と今回位置を含む最小矩形を計算
    int min_x = screen_width;
    int max_x = 0;
    int min_y = screen_height;
    int max_y = 0;
    
    for (auto& fish : fishes) {
        fish.curr_draw_x = (int)fish.x;
        fish.curr_draw_y = (int)fish.y;
        
        // 前回の位置を含める
        min_x = min(min_x, fish.prev_draw_x);
        max_x = max(max_x, fish.prev_draw_x + FISH_WIDTH);
        min_y = min(min_y, fish.prev_draw_y);
        max_y = max(max_y, fish.prev_draw_y + FISH_HEIGHT);
        
        // 今回の位置を含める
        min_x = min(min_x, fish.curr_draw_x);
        max_x = max(max_x, fish.curr_draw_x + FISH_WIDTH);
        min_y = min(min_y, fish.curr_draw_y);
        max_y = max(max_y, fish.curr_draw_y + FISH_HEIGHT);
    }
    
    // 矩形のサイズを計算
    int rect_width = max_x - min_x;
    int rect_height = max_y - min_y;
    
    // バッファをリサイズ（サイズが変わった場合のみ）
    static int prev_rect_width = 0;
    static int prev_rect_height = 0;
    if (rect_width != prev_rect_width || rect_height != prev_rect_height) {
        buffer_canvas.deleteSprite();
        buffer_canvas.setColorDepth(16);
        buffer_canvas.createSprite(rect_width, rect_height);
        prev_rect_width = rect_width;
        prev_rect_height = rect_height;
    }
    
    // バッファに背景色を塗る
    buffer_canvas.fillSprite(bg_color);
    
    // バッファに全ての魚を描画
    for (const auto& fish : fishes) {
        int rel_x = fish.curr_draw_x - min_x;
        int rel_y = fish.curr_draw_y - min_y;
        
        // 適切な角度の画像を選択
        int sprite_index = getFishSpriteIndex(fish.facing_right, fish.turn_progress, fish.is_turning);
        
        // 魚を描画（黒を透過）
        fish_sprites[sprite_index].pushSprite(&buffer_canvas, rel_x, rel_y, TFT_BLACK);
    }
    
    // バッファを画面に転送
    buffer_canvas.pushSprite(display, min_x, min_y);
    
    // 前回位置を更新
    for (auto& fish : fishes) {
        fish.prev_draw_x = fish.curr_draw_x;
        fish.prev_draw_y = fish.curr_draw_y;
    }
}

int getFishSpriteIndex(bool facing_right, float turn_progress, bool is_turning) {
    if (!is_turning) {
        // 方向転換していない場合
        return facing_right ? 9 : 0;  // 右向き真横 or 左向き真横
    }
    
    // 方向転換中：進行度に応じて10段階の画像を選択
    // 左向き→右向き: 0→1→2→3→4→5→6→7→8→9
    // 右向き→左向き: 9→8→7→6→5→4→3→2→1→0
    
    float angle_progress;
    if (facing_right) {
        // 現在左向き、右向きに転換中
        angle_progress = turn_progress;
    } else {
        // 現在右向き、左向きに転換中
        angle_progress = 1.0f - turn_progress;
    }
    
    // 0.0〜1.0を0〜9のインデックスに変換
    int index = (int)(angle_progress * 9.0f);
    if (index > 9) index = 9;
    if (index < 0) index = 0;
    
    return index;
}
