#include <M5Unified.h>
#include <LittleFS.h>
#include <vector>
#include <cmath>
#include <algorithm>

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
    bool turn_start_facing_right; // 方向転換開始時の向き
    bool turn_via_tail; // 尾経由で回転するか（false=正面経由）
    // 奥行き（depth）: 0.0=最も奥、1.0=最も手前
    float depth;
    float depth_target;  // 奥行きの目標値
    // 前回の描画位置（部分更新用）
    int prev_draw_x;
    int prev_draw_y;
    int prev_draw_w;
    int prev_draw_h;
    // 今回の描画位置
    int curr_draw_x;
    int curr_draw_y;
    int curr_draw_w;
    int curr_draw_h;
};

// グローバル変数
std::vector<NeonTetra> fishes;
LGFX_Device* display;

// 泳ぎアニメーション用の魚画像（左6フレーム + 右6フレーム）
M5Canvas fish_sprites_left[6];
M5Canvas fish_sprites_right[6];

// 方向転換用の画像（正面経由）
M5Canvas fish_sprite_left_90;
M5Canvas fish_sprite_left_45;
M5Canvas fish_sprite_front;
M5Canvas fish_sprite_right_45;
M5Canvas fish_sprite_right_90;

// 方向転換用の画像（尾経由）
M5Canvas fish_sprite_tail;
M5Canvas fish_sprite_tail_left_45;
M5Canvas fish_sprite_tail_right_45;

M5Canvas buffer_canvas;  // ダブルバッファ用キャンバス
M5Canvas background_canvas;  // 背景画像用キャンバス
bool sprites_loaded = false;
bool background_loaded = false;
int screen_width = 0;
int screen_height = 0;
uint16_t bg_color;  // 背景色（フォールバック用）

const int FISH_WIDTH = 358;
const int FISH_HEIGHT = 200;
const int NUM_FISHES = 3;
const float MAX_SPEED = 2.0f;
const uint32_t DIRECTION_CHANGE_INTERVAL = 3000;  // 3秒
const float TURN_DURATION = 1.0f;  // 方向転換にかかる時間（秒）
const float DEPTH_SCALE_MIN = 0.7f;  // 最も奥のスケール（70%）
const float DEPTH_SCALE_MAX = 1.0f;  // 最も手前のスケール（100%）
const float DEPTH_CHANGE_SPEED = 0.1f;  // 奥行き変化速度（秒あたり）
const float DEPTH_TARGET_INTERVAL = 5.0f;  // 奥行き目標変更間隔（秒）

int buffer_max_width = 0;
int buffer_max_height = 0;

// 関数プロトタイプ
void initDisplay();
void loadBackgroundImage();
void loadFishImages();
void initFishes();
void updateFishes(uint32_t delta_ms);
void drawScene();
M5Canvas* getFishSprite(const NeonTetra& fish);
void handleTouch();
void triggerFishTurn(NeonTetra& fish);
float getDepthScale(float depth);

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
    
    // 背景画像を読み込み
    loadBackgroundImage();
    
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
    
    // M5の状態を更新（タッチ情報を取得）
    M5.update();
    
    // タッチ処理
    handleTouch();
    
    // 魚を更新
    updateFishes(delta_ms);
    
    // シーンを描画（最小矩形ダブルバッファ）
    drawScene();
}

void initDisplay() {
    display = &M5.Display;
    display->init();
    display->setRotation(3);  // 横向き（180度回転）
    
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

void loadBackgroundImage() {
    M5_LOGI("=== Starting loadBackgroundImage() ===");
    M5_LOGI("Free heap: %d bytes", ESP.getFreeHeap());
    M5_LOGI("Free PSRAM: %d bytes", ESP.getFreePsram());
    
    const char* bg_path = "/images/aquarium_background.png";
    File file = LittleFS.open(bg_path, "r");
    
    if (file) {
        size_t file_size = file.size();
        uint8_t* buffer = (uint8_t*)malloc(file_size);
        
        if (buffer) {
            file.readBytes((char*)buffer, file_size);
            file.close();
            
            background_canvas.setPsram(true);  // PSRAMを使用
            background_canvas.setColorDepth(16);
            background_canvas.createSprite(screen_width, screen_height);
            
            if (background_canvas.width() == 0 || background_canvas.height() == 0) {
                M5_LOGE("Failed to create background sprite (Free heap: %d, Free PSRAM: %d)", 
                        ESP.getFreeHeap(), ESP.getFreePsram());
                free(buffer);
                return;
            }
            
            background_canvas.fillSprite(bg_color);
            bool png_drawn = background_canvas.drawPng(buffer, file_size, 0, 0);
            if (png_drawn) {
                M5_LOGI("Loaded background image: size=%dx%d, depth=%d", 
                        background_canvas.width(), background_canvas.height(), 
                        background_canvas.getColorDepth());
                background_loaded = true;
                
                // 背景画像を画面に描画
                background_canvas.pushSprite(display, 0, 0);
                M5_LOGI("Background image drawn to display");
            } else {
                M5_LOGE("Failed to draw background PNG");
            }
            free(buffer);
        } else {
            M5_LOGE("Memory allocation failed for background image");
            file.close();
        }
    } else {
        M5_LOGE("Failed to open background image file: %s", bg_path);
    }
    
    M5_LOGI("=== Finished loadBackgroundImage() ===");
    M5_LOGI("Final free heap: %d bytes", ESP.getFreeHeap());
    M5_LOGI("Final free PSRAM: %d bytes", ESP.getFreePsram());
}

void loadFishImages() {
    M5_LOGI("=== Starting loadFishImages() ===");
    M5_LOGI("Free heap: %d bytes", ESP.getFreeHeap());
    M5_LOGI("Free PSRAM: %d bytes", ESP.getFreePsram());
    
    // 画像情報の構造体
    struct ImageInfo {
        const char* path;
        M5Canvas* canvas;
        const char* name;
    };
    
    // 左向き泳ぎアニメーション（6フレーム）
    ImageInfo left_swim_images[6] = {
        {"/images/swim/neon_tetra_left_swim1_optimized.png", &fish_sprites_left[0], "left_swim1"},
        {"/images/swim/neon_tetra_left_swim2_optimized.png", &fish_sprites_left[1], "left_swim2"},
        {"/images/swim/neon_tetra_left_swim3_optimized.png", &fish_sprites_left[2], "left_swim3"},
        {"/images/swim/neon_tetra_left_swim4_optimized.png", &fish_sprites_left[3], "left_swim4"},
        {"/images/swim/neon_tetra_left_swim5_optimized.png", &fish_sprites_left[4], "left_swim5"},
        {"/images/swim/neon_tetra_left_swim6_optimized.png", &fish_sprites_left[5], "left_swim6"}
    };
    
    // 右向き泳ぎアニメーション（6フレーム）
    ImageInfo right_swim_images[6] = {
        {"/images/swim/neon_tetra_right_swim1_optimized.png", &fish_sprites_right[0], "right_swim1"},
        {"/images/swim/neon_tetra_right_swim2_optimized.png", &fish_sprites_right[1], "right_swim2"},
        {"/images/swim/neon_tetra_right_swim3_optimized.png", &fish_sprites_right[2], "right_swim3"},
        {"/images/swim/neon_tetra_right_swim4_optimized.png", &fish_sprites_right[3], "right_swim4"},
        {"/images/swim/neon_tetra_right_swim5_optimized.png", &fish_sprites_right[4], "right_swim5"},
        {"/images/swim/neon_tetra_right_swim6_optimized.png", &fish_sprites_right[5], "right_swim6"}
    };
    
    // 方向転換用の画像（正面経由）
    ImageInfo turn_images[5] = {
        {"/images/neon_tetra_left_optimized.png", &fish_sprite_left_90, "left_90"},
        {"/images/neon_tetra_45left_optimized.png", &fish_sprite_left_45, "left_45"},
        {"/images/neon_tetra_front_optimized.png", &fish_sprite_front, "front"},
        {"/images/neon_tetra_45right_optimized.png", &fish_sprite_right_45, "right_45"},
        {"/images/neon_tetra_right_optimized.png", &fish_sprite_right_90, "right_90"}
    };
    
    // 方向転換用の画像（尾経由）
    ImageInfo tail_turn_images[3] = {
        {"/images/neon_tetra_tail_optimized.png", &fish_sprite_tail, "tail"},
        {"/images/neon_tetra_tail_left_45_optimized.png", &fish_sprite_tail_left_45, "tail_left_45"},
        {"/images/neon_tetra_tail_right_45_optimized.png", &fish_sprite_tail_right_45, "tail_right_45"}
    };
    
    // 全ての画像を読み込み
    ImageInfo all_images[20];
    for (int i = 0; i < 6; i++) {
        all_images[i] = left_swim_images[i];
        all_images[i + 6] = right_swim_images[i];
    }
    for (int i = 0; i < 5; i++) {
        all_images[i + 12] = turn_images[i];
    }
    for (int i = 0; i < 3; i++) {
        all_images[i + 17] = tail_turn_images[i];
    }
    
    for (int i = 0; i < 20; i++) {
        const auto& img = all_images[i];
        File file = LittleFS.open(img.path, "r");
        
        if (file) {
            size_t file_size = file.size();
            uint8_t* buffer = (uint8_t*)malloc(file_size);
            
            if (buffer) {
                file.readBytes((char*)buffer, file_size);
                file.close();
                
                img.canvas->setPsram(true);  // PSRAMを使用
                img.canvas->setColorDepth(16);
                img.canvas->createSprite(FISH_WIDTH, FISH_HEIGHT);
                
                // スプライトが正しく作成されたか確認
                if (img.canvas->width() == 0 || img.canvas->height() == 0) {
                    M5_LOGE("Failed to create sprite for: %s (Free heap: %d, Free PSRAM: %d)", 
                            img.name, ESP.getFreeHeap(), ESP.getFreePsram());
                    free(buffer);
                    continue;
                }
                
                img.canvas->fillSprite(TFT_BLACK);
                bool png_drawn = img.canvas->drawPng(buffer, file_size, 0, 0);
                if (png_drawn) {
                    M5_LOGI("Loaded fish image: %s (size: %dx%d, depth: %d)", 
                            img.name, img.canvas->width(), img.canvas->height(), img.canvas->getColorDepth());
                    sprites_loaded = true;
                } else {
                    M5_LOGE("Failed to draw PNG for: %s", img.name);
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
    
    M5_LOGI("=== Finished loadFishImages() ===");
    M5_LOGI("Final free heap: %d bytes", ESP.getFreeHeap());
    M5_LOGI("Final free PSRAM: %d bytes", ESP.getFreePsram());
}

void initFishes() {
    fishes.clear();
    
    for (int i = 0; i < NUM_FISHES; i++) {
        NeonTetra fish;
        fish.x = random(0, screen_width - FISH_WIDTH);
        fish.y = random(0, screen_height - FISH_HEIGHT);
        fish.vx = (random(50, 150) / 100.0f) * (random(0, 2) == 0 ? -1 : 1);
        fish.vy = (random(50, 150) / 100.0f) * (random(0, 2) == 0 ? -1 : 1);
        fish.facing_right = fish.vx > 0;
        fish.width = FISH_WIDTH;
        fish.height = FISH_HEIGHT;
        fish.last_direction_change = millis();
        fish.swim_phase = random(0, 600) / 100.0f;
        fish.swim_speed = random(80, 120) / 100.0f;
        fish.is_turning = false;
        fish.turn_progress = 0.0f;
        fish.turn_target_right = fish.facing_right;
        fish.turn_start_facing_right = fish.facing_right;
        fish.turn_via_tail = false;
        fish.depth = random(0, 100) / 100.0f;  // ランダムな奥行き
        fish.depth_target = random(0, 100) / 100.0f;
        float init_scale = getDepthScale(fish.depth);
        int scaled_w = (int)(FISH_WIDTH * init_scale);
        int scaled_h = (int)(FISH_HEIGHT * init_scale);
        fish.prev_draw_x = (int)fish.x;
        fish.prev_draw_y = (int)fish.y;
        fish.prev_draw_w = scaled_w;
        fish.prev_draw_h = scaled_h;
        fish.curr_draw_x = (int)fish.x;
        fish.curr_draw_y = (int)fish.y;
        fish.curr_draw_w = scaled_w;
        fish.curr_draw_h = scaled_h;
        
        fishes.push_back(fish);
    }
}

void updateFishes(uint32_t delta_ms) {
    float delta_sec = delta_ms / 1000.0f;
    
    for (auto& fish : fishes) {
        // 泳ぎのアニメーション位相を更新
        fish.swim_phase += delta_sec * 6.0f * fish.swim_speed;
        if (fish.swim_phase >= 6.0f) {
            fish.swim_phase -= 6.0f;
        }
        
        // 方向転換中の処理
        if (fish.is_turning) {
            fish.turn_progress += delta_sec / TURN_DURATION;
            if (fish.turn_progress >= 1.0f) {
                fish.turn_progress = 1.0f;
                fish.is_turning = false;
                fish.facing_right = fish.turn_target_right;
            }
        }
        
        // 位置を更新
        fish.x += fish.vx * delta_sec * 50;
        fish.y += fish.vy * delta_sec * 50;
        
        // 画面端での反射（スケールを考慮）
        float scale = getDepthScale(fish.depth);
        int scaled_w = (int)(FISH_WIDTH * scale);
        int scaled_h = (int)(FISH_HEIGHT * scale);
        if (fish.x < 0) {
            fish.x = 0;
            fish.vx = -fish.vx;
        }
        if (fish.x + scaled_w > screen_width) {
            fish.x = screen_width - scaled_w;
            fish.vx = -fish.vx;
        }
        if (fish.y < 0) {
            fish.y = 0;
            fish.vy = -fish.vy;
        }
        if (fish.y + scaled_h > screen_height) {
            fish.y = screen_height - scaled_h;
            fish.vy = -fish.vy;
        }
        
        // ランダムな速度変更
        fish.vx += (random(-10, 11) / 100.0f);
        fish.vy += (random(-10, 11) / 100.0f);
        
        // 速度制限
        float speed = sqrt(fish.vx * fish.vx + fish.vy * fish.vy);
        if (speed > MAX_SPEED) {
            fish.vx = fish.vx / speed * MAX_SPEED;
            fish.vy = fish.vy / speed * MAX_SPEED;
        }
        
        // 速度の符号が変わったか確認（方向転換が必要か）
        if (!fish.is_turning) {
            bool new_facing_right = fish.vx > 0;
            if (new_facing_right != fish.facing_right) {
                // 方向が変わった → 方向転換アニメーション開始
                fish.turn_start_facing_right = fish.facing_right;  // 古い方向を保存
                fish.is_turning = true;
                fish.turn_progress = 0.0f;
                fish.turn_target_right = new_facing_right;
                fish.facing_right = new_facing_right;  // 即座に更新
                fish.turn_via_tail = random(0, 2) == 0;  // ランダムに回転方向を選択
            }
        }
        
        // 奥行きの更新（横方向の移動時に少しずつ変化）
        if (fabs(fish.vx) > 0.1f) {
            // 奥行きを目標値に向かって少しずつ変化
            float depth_diff = fish.depth_target - fish.depth;
            if (fabs(depth_diff) > 0.01f) {
                float depth_step = DEPTH_CHANGE_SPEED * delta_sec;
                if (fabs(depth_diff) < depth_step) {
                    fish.depth = fish.depth_target;
                } else {
                    fish.depth += (depth_diff > 0 ? depth_step : -depth_step);
                }
            } else {
                // 目標に到達したら新しい目標を設定
                fish.depth_target = random(0, 100) / 100.0f;
            }
        }
        fish.depth = max(0.0f, min(1.0f, fish.depth));
        
        // 前回の描画位置・サイズを保存
        fish.prev_draw_x = fish.curr_draw_x;
        fish.prev_draw_y = fish.curr_draw_y;
        fish.prev_draw_w = fish.curr_draw_w;
        fish.prev_draw_h = fish.curr_draw_h;
        // 現在の描画位置・サイズを計算
        scale = getDepthScale(fish.depth);
        fish.curr_draw_w = (int)(FISH_WIDTH * scale);
        fish.curr_draw_h = (int)(FISH_HEIGHT * scale);
        fish.curr_draw_x = (int)fish.x;
        fish.curr_draw_y = (int)fish.y;
    }
}

M5Canvas* getFishSprite(const NeonTetra& fish) {
    if (fish.is_turning) {
        // 方向転換中：5段階の画像を使用
        if (fish.turn_via_tail) {
            // 尾経由で回転
            if (fish.turn_progress < 0.2f) {
                return fish.turn_start_facing_right ? &fish_sprite_right_90 : &fish_sprite_left_90;
            } else if (fish.turn_progress < 0.4f) {
                return fish.turn_start_facing_right ? &fish_sprite_tail_right_45 : &fish_sprite_tail_left_45;
            } else if (fish.turn_progress < 0.6f) {
                return &fish_sprite_tail;
            } else if (fish.turn_progress < 0.8f) {
                return fish.facing_right ? &fish_sprite_tail_right_45 : &fish_sprite_tail_left_45;
            } else {
                return fish.facing_right ? &fish_sprite_right_90 : &fish_sprite_left_90;
            }
        } else {
            // 正面経由で回転
            if (fish.turn_progress < 0.2f) {
                return fish.turn_start_facing_right ? &fish_sprite_right_90 : &fish_sprite_left_90;
            } else if (fish.turn_progress < 0.4f) {
                return fish.turn_start_facing_right ? &fish_sprite_right_45 : &fish_sprite_left_45;
            } else if (fish.turn_progress < 0.6f) {
                return &fish_sprite_front;
            } else if (fish.turn_progress < 0.8f) {
                return fish.facing_right ? &fish_sprite_right_45 : &fish_sprite_left_45;
            } else {
                return fish.facing_right ? &fish_sprite_right_90 : &fish_sprite_left_90;
            }
        }
    } else {
        // 通常の泳ぎ：6フレームアニメーション
        int frame_index = (int)fish.swim_phase;
        if (frame_index >= 6) frame_index = 5;
        
        // 特定フレームの出現頻度を減らす（80%スキップ）
        if (fish.facing_right && frame_index == 4) {  // right_swim5
            if (random(0, 100) < 80) {
                frame_index = 3;  // swim4を代わりに表示
            }
        } else if (!fish.facing_right && frame_index == 5) {  // left_swim6
            if (random(0, 100) < 80) {
                frame_index = 4;  // swim5を代わりに表示
            }
        }
        
        if (fish.facing_right) {
            return &fish_sprites_right[frame_index];
        } else {
            return &fish_sprites_left[frame_index];
        }
    }
}

void drawScene() {
    static uint32_t frame_count = 0;
    bool debug_log = (frame_count % 60 == 0);  // 60フレームごとにログ出力
    
    if (debug_log) {
        M5_LOGI("=== drawScene() frame %d, fishes count: %d ===", frame_count, fishes.size());
    }
    
    // 全魚の前回位置と今回位置を含む最小矩形を計算（スケール考慮）
    int min_x = screen_width;
    int max_x = 0;
    int min_y = screen_height;
    int max_y = 0;
    
    for (const auto& fish : fishes) {
        if (debug_log) {
            M5_LOGI("Fish: pos=(%d,%d), depth=%.2f, scale=%.2f, size=(%dx%d)",
                    fish.curr_draw_x, fish.curr_draw_y, fish.depth, 
                    getDepthScale(fish.depth), fish.curr_draw_w, fish.curr_draw_h);
        }
        min_x = min(min_x, min(fish.prev_draw_x, fish.curr_draw_x));
        max_x = max(max_x, max(fish.prev_draw_x + fish.prev_draw_w, fish.curr_draw_x + fish.curr_draw_w));
        min_y = min(min_y, min(fish.prev_draw_y, fish.curr_draw_y));
        max_y = max(max_y, max(fish.prev_draw_y + fish.prev_draw_h, fish.curr_draw_y + fish.curr_draw_h));
    }
    
    // 余白を追加
    min_x = max(0, min_x - 10);
    min_y = max(0, min_y - 10);
    max_x = min(screen_width, max_x + 10);
    max_y = min(screen_height, max_y + 10);
    
    int rect_width = max_x - min_x;
    int rect_height = max_y - min_y;
    
    if (debug_log) {
        M5_LOGI("Rect: min=(%d,%d), max=(%d,%d), size=(%dx%d)",
                min_x, min_y, max_x, max_y, rect_width, rect_height);
    }
    
    // バッファのサイズを変更（必要な場合のみ）
    static int prev_rect_width = 0;
    static int prev_rect_height = 0;
    if (rect_width != prev_rect_width || rect_height != prev_rect_height) {
        buffer_canvas.deleteSprite();
        buffer_canvas.setPsram(true);  // PSRAMを使用
        buffer_canvas.setColorDepth(16);
        buffer_canvas.createSprite(rect_width, rect_height);
        prev_rect_width = rect_width;
        prev_rect_height = rect_height;
    }
    
    // バッファに背景を描画
    if (background_loaded) {
        buffer_canvas.fillRect(0, 0, rect_width, rect_height, bg_color);
        background_canvas.pushSprite(&buffer_canvas, -min_x, -min_y);
    } else {
        buffer_canvas.fillRect(0, 0, rect_width, rect_height, bg_color);
    }
    
    // 魚を奥行き順にソート（depthが小さい=奥から先に描画）
    std::vector<int> draw_order(fishes.size());
    for (int i = 0; i < (int)fishes.size(); i++) draw_order[i] = i;
    std::sort(draw_order.begin(), draw_order.end(), [](int a, int b) {
        return fishes[a].depth < fishes[b].depth;
    });
    
    // バッファに全ての魚を奥行き順に描画
    for (int idx : draw_order) {
        const auto& fish = fishes[idx];
        int rel_x = fish.curr_draw_x - min_x;
        int rel_y = fish.curr_draw_y - min_y;
        int draw_w = fish.curr_draw_w;
        int draw_h = fish.curr_draw_h;
        
        // 適切なフレームの画像を取得
        M5Canvas* sprite = getFishSprite(fish);
        
        if (debug_log) {
            M5_LOGI("Fish[%d]: depth=%.2f, scale=%.2f, draw_size=(%dx%d), rel_pos=(%d,%d)",
                    idx, fish.depth, getDepthScale(fish.depth), draw_w, draw_h, rel_x, rel_y);
        }
        
        // スケールが元サイズと異なる場合は拡大縮小して描画
        if (draw_w != FISH_WIDTH || draw_h != FISH_HEIGHT) {
            sprite->pushRotateZoomWithAA(&buffer_canvas, 
                rel_x + draw_w / 2, rel_y + draw_h / 2,  // 描画先の中心座標
                0.0f,  // 回転なし
                (float)draw_w / FISH_WIDTH,   // Xスケール
                (float)draw_h / FISH_HEIGHT,  // Yスケール
                TFT_BLACK);  // 透過色
        } else {
            sprite->pushSprite(&buffer_canvas, rel_x, rel_y, TFT_BLACK);
        }
    }
    
    // バッファを画面に転送
    if (debug_log) {
        M5_LOGI("Pushing buffer (%dx%d) to display at (%d,%d)",
                rect_width, rect_height, min_x, min_y);
    }
    buffer_canvas.pushSprite(display, min_x, min_y);
    
    frame_count++;
}

void handleTouch() {
    // タッチされたかチェック
    if (M5.Touch.getCount() > 0) {
        auto touch = M5.Touch.getDetail();
        if (touch.wasPressed()) {
            int touch_x = touch.x;
            int touch_y = touch.y;
            
            M5_LOGI("Touch detected at (%d, %d)", touch_x, touch_y);
            
            // タッチ位置にいる魚を探す
            for (auto& fish : fishes) {
                int fish_x = fish.curr_draw_x;
                int fish_y = fish.curr_draw_y;
                
                // 魚の矩形範囲内かチェック（スケール考慮）
                if (touch_x >= fish_x && touch_x <= fish_x + fish.curr_draw_w &&
                    touch_y >= fish_y && touch_y <= fish_y + fish.curr_draw_h) {
                    M5_LOGI("Fish tapped! Triggering turn.");
                    triggerFishTurn(fish);
                    break;  // 最初にヒットした魚だけを処理
                }
            }
        }
    }
}

float getDepthScale(float depth) {
    // depth: 0.0=最も奥、1.0=最も手前
    // スケール: DEPTH_SCALE_MIN(0.7) 〜 DEPTH_SCALE_MAX(1.3)
    return DEPTH_SCALE_MIN + (DEPTH_SCALE_MAX - DEPTH_SCALE_MIN) * depth;
}

void triggerFishTurn(NeonTetra& fish) {
    // 既に方向転換中の場合は無視
    if (fish.is_turning) {
        return;
    }
    
    // 速度の方向を反転
    fish.vx = -fish.vx;
    fish.vy = -fish.vy;
    
    // 方向転換アニメーションを開始
    bool new_facing_right = fish.vx > 0;
    fish.turn_start_facing_right = fish.facing_right;
    fish.is_turning = true;
    fish.turn_progress = 0.0f;
    fish.turn_target_right = new_facing_right;
    fish.facing_right = new_facing_right;
    fish.turn_via_tail = random(0, 2) == 0;  // ランダムに回転方向を選択
}
