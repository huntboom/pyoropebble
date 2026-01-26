#include <pebble.h>
#include <stdlib.h>
#include <math.h>

// Game constants
#define GAME_WIDTH 20
#define GAME_HEIGHT 20
#define BLOCK_SIZE 8
#define PYORO_SIZE 3
#define BEAN_SIZE 2
#define TONGUE_WIDTH 2
#define TONGUE_SPEED 15.0f
#define BEAN_SPEED 1.8f
#define PYORO_SPEED 25.0f
#define BEAN_SPAWN_FREQUENCY 2.0f
#define SPEED_ACCELERATION 0.01f
#define DEATH_DELAY 1.0f // Delay in seconds before showing game over screen
#define MOUTH_ANIMATION_FRAMES 3 // Number of animation frames (closed, halfway, open)
#define MOUTH_ANIMATION_SPEED 10 // Frames per animation cycle (higher = slower)

// Game state
typedef struct {
  float x, y;
  int direction; // 1 = right, -1 = left
  bool moving;
  bool dead;
  struct {
    bool active;
    float x, y;
    int direction;
    bool going_back;
    bool caught_bean;
  } tongue;
} Pyoro;

typedef struct {
  float x, y;
  float speed;
  bool active;
  bool caught;
} Bean;

typedef struct {
  bool exists;
} Block;

typedef enum {
  GAME_STATE_MENU,
  GAME_STATE_PLAYING,
  GAME_STATE_GAME_OVER
} GameState;

typedef struct {
  GameState state;
  Pyoro pyoro;
  Bean beans[5]; // Max 5 beans on screen
  Block blocks[GAME_WIDTH];
  int score;
  float game_speed;
  float bean_spawn_timer;
  float death_timer;
  bool game_paused;
} Game;

static Window *s_window;
static Layer *s_game_layer;
static TextLayer *s_score_layer;
static TextLayer *s_game_over_layer;
static AppTimer *s_game_timer;
static Game s_game;
static GBitmap *s_background_bitmap;
static GBitmap *s_pyoro_right_bitmap;
static GBitmap *s_pyoro_left_bitmap;
static GBitmap *s_pyoro_mouth_halfway_open_right_bitmap;
static GBitmap *s_pyoro_mouth_halfway_open_left_bitmap;
static GBitmap *s_pyoro_mouth_open_right_bitmap;
static GBitmap *s_pyoro_mouth_open_left_bitmap;
static GBitmap *s_pyoro_dead_left_bitmap;
static GBitmap *s_pyoro_dead_right_bitmap;
static GBitmap *s_block_bitmap;
static GBitmap *s_tongue_bitmap;
static GBitmap *s_tongue_left_bitmap;
static GBitmap *s_tongue_body_right_bitmap;
static GBitmap *s_tongue_body_left_bitmap;
static GBitmap *s_green_bean_left_bitmap;
static GBitmap *s_green_bean_middle_bitmap;
static GBitmap *s_green_bean_right_bitmap;
static uint32_t s_last_movement_press_frame = 0;
static uint32_t s_frame_count = 0;
#define BEAN_ANIMATION_SPEED 5 // Frames per animation frame (higher = slower)

// Forward declarations
static void game_update(void *data);
static void game_layer_update_callback(Layer *layer, GContext *ctx);
static void init_game(void);
static void reset_game(void);
static void spawn_bean(void);
static void update_game(float delta_time);
static bool check_collision(float x1, float y1, float w1, float h1,
                         float x2, float y2, float w2, float h2);

// Initialize game
static void init_game(void) {
  s_game.state = GAME_STATE_MENU;
  s_game.score = 0;
  s_game.game_speed = 1.0f;
  s_game.bean_spawn_timer = 0.0f;
  s_game.death_timer = 0.0f;
  s_game.game_paused = false;
  
  // Initialize Pyoro
  s_game.pyoro.x = GAME_WIDTH / 2.0f;
  s_game.pyoro.y = GAME_HEIGHT - 2.0f;
  s_game.pyoro.direction = 1;
  s_game.pyoro.moving = false;
  s_game.pyoro.dead = false;
  s_game.pyoro.tongue.active = false;
  
  // Initialize blocks
  for (int i = 0; i < GAME_WIDTH; i++) {
    s_game.blocks[i].exists = true;
  }
  
  // Initialize beans
  for (int i = 0; i < 5; i++) {
    s_game.beans[i].active = false;
  }
}

static void reset_game(void) {
  init_game();
  s_game.state = GAME_STATE_PLAYING;
  layer_mark_dirty(s_game_layer);
}

// Spawn a new bean
static void spawn_bean(void) {
  for (int i = 0; i < 5; i++) {
    if (!s_game.beans[i].active) {
      s_game.beans[i].x = (rand() % GAME_WIDTH) + 0.5f;
      s_game.beans[i].y = 0.0f;
      s_game.beans[i].speed = (rand() % 100) / 100.0f * 1.0f + 0.5f;
      s_game.beans[i].active = true;
      s_game.beans[i].caught = false;
      break;
    }
  }
}

// Collision detection
static bool check_collision(float x1, float y1, float w1, float h1,
                            float x2, float y2, float w2, float h2) {
  return (x1 - w1/2 < x2 + w2/2) &&
         (x1 + w1/2 > x2 - w2/2) &&
         (y1 - h1/2 < y2 + h2/2) &&
         (y1 + h1/2 > y2 - h2/2);
}

// Update game logic
static void update_game(float delta_time) {
  if (s_game.state != GAME_STATE_PLAYING || s_game.game_paused) {
    return;
  }
  
  // Handle death timer
  if (s_game.pyoro.dead) {
    s_game.death_timer -= delta_time;
    if (s_game.death_timer <= 0.0f) {
      s_game.state = GAME_STATE_GAME_OVER;
    }
    // Don't update game logic while dead, but keep rendering
    layer_mark_dirty(s_game_layer);
    return;
  }
  
  float dt = delta_time * s_game.game_speed;
  
  // Update game speed
  s_game.game_speed += dt * SPEED_ACCELERATION;
  
  // Update Pyoro movement (stop if tongue is active)
  if (s_game.pyoro.tongue.active) {
    s_game.pyoro.moving = false;
  }
  
  // Increment frame counter
  s_frame_count++;
  
  // Stop moving if button hasn't been pressed recently (button released)
  // Check if it's been more than ~9 frames (150ms at 60fps) since last press
  if (s_game.pyoro.moving) {
    if (s_frame_count - s_last_movement_press_frame > 9) {
      s_game.pyoro.moving = false;
    }
  }
  
  if (s_game.pyoro.moving && !s_game.pyoro.tongue.active) {
    float move_speed = PYORO_SPEED * dt;
    float new_x = s_game.pyoro.x + s_game.pyoro.direction * move_speed;
    
    // Check boundaries
    if (new_x < PYORO_SIZE / 2.0f) {
      new_x = PYORO_SIZE / 2.0f;
    } else if (new_x > GAME_WIDTH - PYORO_SIZE / 2.0f) {
      new_x = GAME_WIDTH - PYORO_SIZE / 2.0f;
    }
    
    // Check for holes in blocks
    int block_left = (int)(new_x - PYORO_SIZE / 2.0f);
    int block_right = (int)(new_x + PYORO_SIZE / 2.0f);
    bool can_move = true;
    
    for (int i = block_left; i <= block_right && i < GAME_WIDTH; i++) {
      if (i >= 0 && !s_game.blocks[i].exists) {
        can_move = false;
        break;
      }
    }
    
    if (can_move) {
      s_game.pyoro.x = new_x;
    }
  }
  
  // Update tongue
  if (s_game.pyoro.tongue.active) {
    if (s_game.pyoro.tongue.going_back) {
      // Tongue retracting
      float retract_speed = TONGUE_SPEED * 2.0f * dt;
      s_game.pyoro.tongue.x -= s_game.pyoro.tongue.direction * retract_speed;
      s_game.pyoro.tongue.y += retract_speed;
      
      if (s_game.pyoro.tongue.caught_bean) {
        // Move caught bean with tongue
        for (int i = 0; i < 5; i++) {
          if (s_game.beans[i].active && s_game.beans[i].caught) {
            s_game.beans[i].x = s_game.pyoro.tongue.x;
            s_game.beans[i].y = s_game.pyoro.tongue.y;
            break;
          }
        }
      }
      
      // Check if tongue is back
      if (s_game.pyoro.tongue.y >= s_game.pyoro.y) {
        if (s_game.pyoro.tongue.caught_bean) {
          // Calculate score based on height
          int score_add = 10;
          if (s_game.pyoro.tongue.y < GAME_HEIGHT * 0.2f) {
            score_add = 1000;
          } else if (s_game.pyoro.tongue.y < GAME_HEIGHT * 0.4f) {
            score_add = 300;
          } else if (s_game.pyoro.tongue.y < GAME_HEIGHT * 0.6f) {
            score_add = 100;
          } else if (s_game.pyoro.tongue.y < GAME_HEIGHT * 0.8f) {
            score_add = 50;
          }
          s_game.score += score_add;
          
          // Remove caught bean
          for (int i = 0; i < 5; i++) {
            if (s_game.beans[i].active && s_game.beans[i].caught) {
              s_game.beans[i].active = false;
              break;
            }
          }
        }
        s_game.pyoro.tongue.active = false;
      }
    } else {
      // Tongue extending
      float extend_speed = TONGUE_SPEED * dt;
      s_game.pyoro.tongue.x += s_game.pyoro.tongue.direction * extend_speed;
      s_game.pyoro.tongue.y -= extend_speed;
      
      // Check for bean collision
      for (int i = 0; i < 5; i++) {
        if (s_game.beans[i].active && !s_game.beans[i].caught) {
          if (check_collision(s_game.pyoro.tongue.x, s_game.pyoro.tongue.y,
                             TONGUE_WIDTH, TONGUE_WIDTH,
                             s_game.beans[i].x, s_game.beans[i].y,
                             BEAN_SIZE, BEAN_SIZE)) {
            s_game.beans[i].caught = true;
            s_game.pyoro.tongue.caught_bean = true;
            s_game.pyoro.tongue.going_back = true;
            break;
          }
        }
      }
      
      // Check if tongue is out of bounds
      if (s_game.pyoro.tongue.x < 0 || s_game.pyoro.tongue.x > GAME_WIDTH ||
          s_game.pyoro.tongue.y < 0) {
        s_game.pyoro.tongue.going_back = true;
      }
    }
  }
  
  // Update beans
  for (int i = 0; i < 5; i++) {
    if (s_game.beans[i].active && !s_game.beans[i].caught) {
      s_game.beans[i].y += BEAN_SPEED * s_game.beans[i].speed * dt * s_game.game_speed;
      
      // Check collision with Pyoro
      if (!s_game.pyoro.dead && !s_game.pyoro.tongue.active) {
        if (check_collision(s_game.pyoro.x, s_game.pyoro.y,
                           PYORO_SIZE, PYORO_SIZE,
                           s_game.beans[i].x, s_game.beans[i].y,
                           BEAN_SIZE, BEAN_SIZE)) {
          // Pyoro dies - start death timer
          s_game.pyoro.dead = true;
          s_game.death_timer = DEATH_DELAY;
          break;
        }
      }
      
      // Check collision with ground/blocks
      if (s_game.beans[i].y >= GAME_HEIGHT - 1.0f) {
        int block_index = (int)s_game.beans[i].x;
        if (block_index >= 0 && block_index < GAME_WIDTH) {
          if (s_game.blocks[block_index].exists) {
            s_game.blocks[block_index].exists = false;
          }
        }
        s_game.beans[i].active = false;
      }
    }
  }
  
  // Spawn new beans
  s_game.bean_spawn_timer += dt;
  if (s_game.bean_spawn_timer >= BEAN_SPAWN_FREQUENCY / s_game.game_speed) {
    spawn_bean();
    s_game.bean_spawn_timer = 0.0f;
  }
  
  // Update score display
  static char score_text[20];
  snprintf(score_text, sizeof(score_text), "Score: %d", s_game.score);
  text_layer_set_text(s_score_layer, score_text);
  
  layer_mark_dirty(s_game_layer);
}

// Render game
static void game_layer_update_callback(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int screen_width = bounds.size.w;
  int screen_height = bounds.size.h;
  
  // Calculate scaling
  int game_pixel_width = screen_width;
  int game_pixel_height = screen_height - 20; // Reserve space for score
  float scale_x = (float)game_pixel_width / GAME_WIDTH;
  float scale_y = (float)game_pixel_height / GAME_HEIGHT;
  
  // Draw background
  if (s_background_bitmap) {
    graphics_draw_bitmap_in_rect(ctx, s_background_bitmap, bounds);
  } else {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  }
  
  if (s_game.state == GAME_STATE_MENU) {
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "PYORO", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                      GRect(0, screen_height/2 - 20, screen_width, 30),
                      GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, "Press SELECT", fonts_get_system_font(FONT_KEY_GOTHIC_18),
                      GRect(0, screen_height/2 + 10, screen_width, 20),
                      GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    return;
  }
  
  if (s_game.state == GAME_STATE_GAME_OVER) {
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "GAME OVER", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                      GRect(0, screen_height/2 - 20, screen_width, 30),
                      GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    static char final_score[30];
    snprintf(final_score, sizeof(final_score), "Score: %d", s_game.score);
    graphics_draw_text(ctx, final_score, fonts_get_system_font(FONT_KEY_GOTHIC_18),
                      GRect(0, screen_height/2 + 10, screen_width, 20),
                      GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    return;
  }
  
  // Draw blocks
  for (int i = 0; i < GAME_WIDTH; i++) {
    if (s_game.blocks[i].exists) {
      int x = (int)(i * scale_x);
      // Draw blocks at the bottom of the game area (accounting for score layer)
      int y = 20 + (int)((GAME_HEIGHT - 1) * scale_y);
      int w = (int)scale_x;
      int h = (int)scale_y;
      GRect block_rect = GRect(x, y, w, h);
      if (s_block_bitmap) {
        graphics_draw_bitmap_in_rect(ctx, s_block_bitmap, block_rect);
      } else {
        // Fallback to gray rectangle if bitmap not loaded
        graphics_context_set_fill_color(ctx, GColorDarkGray);
        graphics_fill_rect(ctx, block_rect, 0, GCornerNone);
      }
    }
  }
  
  // Draw Pyoro
  if (!s_game.pyoro.dead) {
    // Select sprite based on tongue state and direction
    // If tongue is active, show fully open mouth; otherwise show default closed sprite
    GBitmap *pyoro_bitmap = NULL;
    if (s_game.pyoro.tongue.active) {
      // Tongue is out - show fully open mouth
      if (s_game.pyoro.direction == -1) {
        pyoro_bitmap = s_pyoro_mouth_open_left_bitmap;
      } else {
        pyoro_bitmap = s_pyoro_mouth_open_right_bitmap;
      }
    } else {
      // Tongue is not active - show default closed sprite
      if (s_game.pyoro.direction == -1) {
        pyoro_bitmap = s_pyoro_left_bitmap;
      } else {
        pyoro_bitmap = s_pyoro_right_bitmap;
      }
    }
    
    if (pyoro_bitmap) {
      // Calculate desired center position
      int pyoro_center_x = (int)(s_game.pyoro.x * scale_x);
      int pyoro_center_y = 20 + (int)(s_game.pyoro.y * scale_y);
      
      // Get bitmap size
      GRect bitmap_bounds = gbitmap_get_bounds(pyoro_bitmap);
      
      // Position bitmap so its center aligns with desired position
      // The bitmap will be drawn at its original size, but centered correctly
      int bitmap_x = pyoro_center_x - bitmap_bounds.size.w / 2;
      int bitmap_y = pyoro_center_y - bitmap_bounds.size.h / 2;
      GRect bitmap_rect = GRect(bitmap_x, bitmap_y, bitmap_bounds.size.w, bitmap_bounds.size.h);
      
      // Set compositing mode to respect alpha channel/transparency
      graphics_context_set_compositing_mode(ctx, GCompOpSet);
      // Draw bitmap - it will be larger than desired but centered correctly
      graphics_draw_bitmap_in_rect(ctx, pyoro_bitmap, bitmap_rect);
      // Reset compositing mode to default
      graphics_context_set_compositing_mode(ctx, GCompOpAssign);
    }
  } else if (s_game.pyoro.dead) {
    // Draw death sprite based on direction
    GBitmap *death_bitmap = NULL;
    if (s_game.pyoro.direction == -1) {
      death_bitmap = s_pyoro_dead_left_bitmap;
    } else {
      death_bitmap = s_pyoro_dead_right_bitmap;
    }
    
    if (death_bitmap) {
      // Calculate desired center position
      int pyoro_center_x = (int)(s_game.pyoro.x * scale_x);
      int pyoro_center_y = 20 + (int)(s_game.pyoro.y * scale_y);
      
      // Get bitmap size
      GRect bitmap_bounds = gbitmap_get_bounds(death_bitmap);
      
      // Position bitmap so its center aligns with desired position
      int bitmap_x = pyoro_center_x - bitmap_bounds.size.w / 2;
      int bitmap_y = pyoro_center_y - bitmap_bounds.size.h / 2;
      GRect bitmap_rect = GRect(bitmap_x, bitmap_y, bitmap_bounds.size.w, bitmap_bounds.size.h);
      
      // Set compositing mode to respect alpha channel/transparency
      graphics_context_set_compositing_mode(ctx, GCompOpSet);
      // Draw death sprite
      graphics_draw_bitmap_in_rect(ctx, death_bitmap, bitmap_rect);
      // Reset compositing mode to default
      graphics_context_set_compositing_mode(ctx, GCompOpAssign);
    } else {
      // Fallback to red rectangle if death bitmap not loaded
      graphics_context_set_fill_color(ctx, GColorRed);
      int pyoro_x = (int)((s_game.pyoro.x - PYORO_SIZE/2.0f) * scale_x);
      int pyoro_y = 20 + (int)((s_game.pyoro.y - PYORO_SIZE/2.0f) * scale_y);
      int pyoro_w = (int)(PYORO_SIZE * scale_x);
      int pyoro_h = (int)(PYORO_SIZE * scale_y);
      graphics_fill_rect(ctx, GRect(pyoro_x, pyoro_y, pyoro_w, pyoro_h), 0, GCornerNone);
    }
  } else {
    // Fallback to red rectangle if bitmap not loaded
    graphics_context_set_fill_color(ctx, GColorRed);
    int pyoro_x = (int)((s_game.pyoro.x - PYORO_SIZE/2.0f) * scale_x);
    int pyoro_y = 20 + (int)((s_game.pyoro.y - PYORO_SIZE/2.0f) * scale_y);
    int pyoro_w = (int)(PYORO_SIZE * scale_x);
    int pyoro_h = (int)(PYORO_SIZE * scale_y);
    graphics_fill_rect(ctx, GRect(pyoro_x, pyoro_y, pyoro_w, pyoro_h), 0, GCornerNone);
  }
  
  // Draw tongue
  if (s_game.pyoro.tongue.active) {
    // Calculate tongue start position (where it leaves the bird)
    float tongue_start_x = s_game.pyoro.x + (PYORO_SIZE/2.0f + 0.6f) * s_game.pyoro.direction;
    float tongue_start_y = s_game.pyoro.y - PYORO_SIZE/2.0f + 0.6f;
    
    // Calculate tip position
    float tongue_tip_x = s_game.pyoro.tongue.x;
    float tongue_tip_y = s_game.pyoro.tongue.y;
    
    // Calculate distance and direction from start to tip
    float dx = tongue_tip_x - tongue_start_x;
    float dy = tongue_tip_y - tongue_start_y;
    float distance_sq = dx * dx + dy * dy;
    float distance = 0.0f;
    if (distance_sq > 0.01f) {
      // Simple approximation: distance ≈ max(|dx|, |dy|) + 0.4 * min(|dx|, |dy|)
      // This avoids sqrtf which has linker issues
      float adx = dx < 0 ? -dx : dx;
      float ady = dy < 0 ? -dy : dy;
      float max_abs = adx > ady ? adx : ady;
      float min_abs = adx < ady ? adx : ady;
      distance = max_abs + 0.4f * min_abs;
    }
    
    // Select body and tip bitmaps based on direction (1 = right, -1 = left)
    GBitmap *tongue_body_bitmap = NULL;
    GBitmap *tongue_tip_bitmap = NULL;
    if (s_game.pyoro.tongue.direction == 1) {
      tongue_body_bitmap = s_tongue_body_right_bitmap;
      tongue_tip_bitmap = s_tongue_bitmap;
    } else {
      tongue_body_bitmap = s_tongue_body_left_bitmap;
      tongue_tip_bitmap = s_tongue_left_bitmap;
    }
    
    // Set compositing mode to respect alpha channel/transparency
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    
    if (tongue_body_bitmap && distance > 0.05f) {
      // Get body bitmap size (in pixels)
      GRect body_bounds = gbitmap_get_bounds(tongue_body_bitmap);
      int body_width_px = body_bounds.size.w;
      int body_height_px = body_bounds.size.h;
      
      // Get tip bitmap size to know how much space to leave
      int tip_width_px = 0;
      if (tongue_tip_bitmap) {
        GRect tip_bounds = gbitmap_get_bounds(tongue_tip_bitmap);
        tip_width_px = tip_bounds.size.w;
      }
      
      // Convert to game coordinates for spacing calculations
      float body_width_game = (float)body_width_px / scale_x;
      float tip_width_game = (float)tip_width_px / scale_x;
      
      // Calculate how many body segments we need
      // Leave a small gap before the tip (about 1/3 of tip width)
      float body_distance = distance - tip_width_game * 0.33f;
      if (body_distance < body_width_game * 0.5f) {
        // If distance is very small, still draw at least one segment if we have room
        body_distance = distance * 0.7f; // Use 70% of distance for body
      }
      
      // Always draw at least one segment if distance is significant
      int num_segments = (int)(body_distance / body_width_game);
      if (num_segments < 1 && distance > body_width_game * 0.3f) {
        num_segments = 1;
      }
      num_segments += 1; // Add one more to ensure full coverage
      
      // Normalize direction vector
      if (distance > 0.01f) {
        float inv_distance = 1.0f / distance;
        float dir_x = dx * inv_distance;
        float dir_y = dy * inv_distance;
        
        // Draw body segments from start towards tip
        for (int i = 0; i < num_segments; i++) {
          float segment_pos = (float)i * body_width_game;
          
          // Stop before we would overlap the tip
          if (segment_pos >= body_distance) {
            break;
          }
          
          float seg_x = tongue_start_x + dir_x * segment_pos;
          float seg_y = tongue_start_y + dir_y * segment_pos;
          
          // Convert to screen coordinates and center the bitmap
          int seg_screen_x = (int)(seg_x * scale_x) - body_width_px / 2;
          int seg_screen_y = 20 + (int)(seg_y * scale_y) - body_height_px / 2;
          
          GRect body_rect = GRect(seg_screen_x, seg_screen_y, body_width_px, body_height_px);
          graphics_draw_bitmap_in_rect(ctx, tongue_body_bitmap, body_rect);
        }
      }
    }
    
    // Draw tongue tip at the end
    if (tongue_tip_bitmap) {
      GRect tip_bounds = gbitmap_get_bounds(tongue_tip_bitmap);
      int tip_center_x = (int)(tongue_tip_x * scale_x);
      int tip_center_y = 20 + (int)(tongue_tip_y * scale_y);
      int tip_x = tip_center_x - tip_bounds.size.w / 2;
      int tip_y = tip_center_y - tip_bounds.size.h / 2;
      GRect tip_rect = GRect(tip_x, tip_y, tip_bounds.size.w, tip_bounds.size.h);
      graphics_draw_bitmap_in_rect(ctx, tongue_tip_bitmap, tip_rect);
    }
    
    // Reset compositing mode to default
    graphics_context_set_compositing_mode(ctx, GCompOpAssign);
    
    // Fallback if bitmaps not loaded
    if (!tongue_body_bitmap && !tongue_tip_bitmap) {
      // Fallback to yellow rectangle if bitmap not loaded
      graphics_context_set_fill_color(ctx, GColorYellow);
      int tongue_x = (int)((s_game.pyoro.tongue.x - TONGUE_WIDTH/2.0f) * scale_x);
      int tongue_y = 20 + (int)((s_game.pyoro.tongue.y - TONGUE_WIDTH/2.0f) * scale_y);
      int tongue_w = (int)(TONGUE_WIDTH * scale_x);
      int tongue_h = (int)(TONGUE_WIDTH * scale_y);
      graphics_fill_rect(ctx, GRect(tongue_x, tongue_y, tongue_w, tongue_h), 0, GCornerNone);
    }
  }
  
  // Draw beans
  for (int i = 0; i < 5; i++) {
    if (s_game.beans[i].active) {
      // Calculate animation frame based on frame count and bean index
      // This creates a staggered animation effect for multiple beans
      int animation_frame = (s_frame_count / BEAN_ANIMATION_SPEED + i) % 3;
      
      GBitmap *bean_bitmap = NULL;
      switch (animation_frame) {
        case 0:
          bean_bitmap = s_green_bean_left_bitmap;
          break;
        case 1:
          bean_bitmap = s_green_bean_middle_bitmap;
          break;
        case 2:
          bean_bitmap = s_green_bean_right_bitmap;
          break;
      }
      
      if (bean_bitmap) {
        // Calculate bean center position
        int bean_center_x = (int)(s_game.beans[i].x * scale_x);
        int bean_center_y = 20 + (int)(s_game.beans[i].y * scale_y);
        
        // Get bitmap size
        GRect bitmap_bounds = gbitmap_get_bounds(bean_bitmap);
        
        // Position bitmap so its center aligns with bean position
        int bitmap_x = bean_center_x - bitmap_bounds.size.w / 2;
        int bitmap_y = bean_center_y - bitmap_bounds.size.h / 2;
        GRect bitmap_rect = GRect(bitmap_x, bitmap_y, bitmap_bounds.size.w, bitmap_bounds.size.h);
        
        // Set compositing mode to respect alpha channel/transparency
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        graphics_draw_bitmap_in_rect(ctx, bean_bitmap, bitmap_rect);
        graphics_context_set_compositing_mode(ctx, GCompOpAssign);
      } else {
        // Fallback to green rectangle if bitmap not loaded
        graphics_context_set_fill_color(ctx, GColorGreen);
        int bean_x = (int)((s_game.beans[i].x - BEAN_SIZE/2.0f) * scale_x);
        int bean_y = 20 + (int)((s_game.beans[i].y - BEAN_SIZE/2.0f) * scale_y);
        int bean_w = (int)(BEAN_SIZE * scale_x);
        int bean_h = (int)(BEAN_SIZE * scale_y);
        graphics_fill_rect(ctx, GRect(bean_x, bean_y, bean_w, bean_h), 0, GCornerNone);
      }
    }
  }
}

// Game timer callback
static void game_update(void *data) {
  update_game(0.016f); // ~60 FPS
  s_game_timer = app_timer_register(16, game_update, NULL);
}

// Button handlers
static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_game.state == GAME_STATE_MENU) {
    reset_game();
    s_game_timer = app_timer_register(16, game_update, NULL);
  } else if (s_game.state == GAME_STATE_GAME_OVER) {
    s_game.state = GAME_STATE_MENU;
    layer_mark_dirty(s_game_layer);
  } else if (s_game.state == GAME_STATE_PLAYING && !s_game.pyoro.dead) {
    // Extend tongue (stops movement)
    if (!s_game.pyoro.tongue.active) {
      s_game.pyoro.moving = false;
      s_game.pyoro.tongue.active = true;
      s_game.pyoro.tongue.x = s_game.pyoro.x + (PYORO_SIZE/2.0f + 0.6f) * s_game.pyoro.direction;
      s_game.pyoro.tongue.y = s_game.pyoro.y - PYORO_SIZE/2.0f + 0.6f;
      s_game.pyoro.tongue.direction = s_game.pyoro.direction;
      s_game.pyoro.tongue.going_back = false;
      s_game.pyoro.tongue.caught_bean = false;
    }
  }
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_game.state == GAME_STATE_PLAYING && !s_game.pyoro.dead && !s_game.pyoro.tongue.active) {
    s_game.pyoro.direction = -1;
    s_game.pyoro.moving = true;
    s_last_movement_press_frame = s_frame_count;
  }
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_game.state == GAME_STATE_PLAYING && !s_game.pyoro.dead && !s_game.pyoro.tongue.active) {
    s_game.pyoro.direction = 1;
    s_game.pyoro.moving = true;
    s_last_movement_press_frame = s_frame_count;
  }
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, prv_down_click_handler);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Create game layer
  s_game_layer = layer_create(bounds);
  layer_set_update_proc(s_game_layer, game_layer_update_callback);
  layer_add_child(window_layer, s_game_layer);
  
  // Create score layer
  s_score_layer = text_layer_create(GRect(0, 0, bounds.size.w, 20));
  text_layer_set_text(s_score_layer, "Score: 0");
  text_layer_set_text_alignment(s_score_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_score_layer, GColorClear);
  text_layer_set_text_color(s_score_layer, GColorWhite);
  layer_add_child(window_layer, text_layer_get_layer(s_score_layer));
  
  // Create game over layer (initially hidden)
  s_game_over_layer = text_layer_create(GRect(0, bounds.size.h/2 - 20, bounds.size.w, 40));
  text_layer_set_text_alignment(s_game_over_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_game_over_layer, GColorClear);
  text_layer_set_text_color(s_game_over_layer, GColorWhite);
  layer_add_child(window_layer, text_layer_get_layer(s_game_over_layer));
  
  // Load background bitmap
  s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BACKGROUND_0);
  
  // Load Pyoro bitmaps
  s_pyoro_right_bitmap = gbitmap_create_with_resource(RESOURCE_ID_PYORO_RIGHT);
  s_pyoro_left_bitmap = gbitmap_create_with_resource(RESOURCE_ID_PYORO_LEFT);
  s_pyoro_mouth_halfway_open_right_bitmap = gbitmap_create_with_resource(RESOURCE_ID_PYORO_MOUTH_HALFWAY_OPEN_RIGHT);
  s_pyoro_mouth_halfway_open_left_bitmap = gbitmap_create_with_resource(RESOURCE_ID_PYORO_MOUTH_HALFWAY_OPEN_LEFT);
  s_pyoro_mouth_open_right_bitmap = gbitmap_create_with_resource(RESOURCE_ID_PYORO_MOUTH_OPEN_RIGHT);
  s_pyoro_mouth_open_left_bitmap = gbitmap_create_with_resource(RESOURCE_ID_PYORO_MOUTH_OPEN_LEFT);
  s_pyoro_dead_left_bitmap = gbitmap_create_with_resource(RESOURCE_ID_PYORO_DEAD_LEFT);
  s_pyoro_dead_right_bitmap = gbitmap_create_with_resource(RESOURCE_ID_PYORO_DEAD_RIGHT);
  
  // Load block bitmap
  s_block_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BLOCK);
  
  // Load tongue bitmaps
  s_tongue_bitmap = gbitmap_create_with_resource(RESOURCE_ID_TONGUE);
  s_tongue_left_bitmap = gbitmap_create_with_resource(RESOURCE_ID_TONGUE_LEFT);
  s_tongue_body_right_bitmap = gbitmap_create_with_resource(RESOURCE_ID_TONGUE_BODY_RIGHT);
  s_tongue_body_left_bitmap = gbitmap_create_with_resource(RESOURCE_ID_TONGUE_BODY_LEFT);
  
  // Load bean bitmaps
  s_green_bean_left_bitmap = gbitmap_create_with_resource(RESOURCE_ID_GREEN_BEAN_LEFT);
  s_green_bean_middle_bitmap = gbitmap_create_with_resource(RESOURCE_ID_GREEN_BEAN_MIDDLE);
  s_green_bean_right_bitmap = gbitmap_create_with_resource(RESOURCE_ID_GREEN_BEAN_RIGHT);
  
  init_game();
}

static void prv_window_unload(Window *window) {
  if (s_game_timer) {
    app_timer_cancel(s_game_timer);
    s_game_timer = NULL;
  }
  if (s_background_bitmap) {
    gbitmap_destroy(s_background_bitmap);
    s_background_bitmap = NULL;
  }
  if (s_pyoro_right_bitmap) {
    gbitmap_destroy(s_pyoro_right_bitmap);
    s_pyoro_right_bitmap = NULL;
  }
  if (s_pyoro_left_bitmap) {
    gbitmap_destroy(s_pyoro_left_bitmap);
    s_pyoro_left_bitmap = NULL;
  }
  if (s_pyoro_mouth_halfway_open_right_bitmap) {
    gbitmap_destroy(s_pyoro_mouth_halfway_open_right_bitmap);
    s_pyoro_mouth_halfway_open_right_bitmap = NULL;
  }
  if (s_pyoro_mouth_halfway_open_left_bitmap) {
    gbitmap_destroy(s_pyoro_mouth_halfway_open_left_bitmap);
    s_pyoro_mouth_halfway_open_left_bitmap = NULL;
  }
  if (s_pyoro_mouth_open_right_bitmap) {
    gbitmap_destroy(s_pyoro_mouth_open_right_bitmap);
    s_pyoro_mouth_open_right_bitmap = NULL;
  }
  if (s_pyoro_mouth_open_left_bitmap) {
    gbitmap_destroy(s_pyoro_mouth_open_left_bitmap);
    s_pyoro_mouth_open_left_bitmap = NULL;
  }
  if (s_pyoro_dead_left_bitmap) {
    gbitmap_destroy(s_pyoro_dead_left_bitmap);
    s_pyoro_dead_left_bitmap = NULL;
  }
  if (s_pyoro_dead_right_bitmap) {
    gbitmap_destroy(s_pyoro_dead_right_bitmap);
    s_pyoro_dead_right_bitmap = NULL;
  }
  if (s_block_bitmap) {
    gbitmap_destroy(s_block_bitmap);
    s_block_bitmap = NULL;
  }
  if (s_tongue_bitmap) {
    gbitmap_destroy(s_tongue_bitmap);
    s_tongue_bitmap = NULL;
  }
  if (s_tongue_left_bitmap) {
    gbitmap_destroy(s_tongue_left_bitmap);
    s_tongue_left_bitmap = NULL;
  }
  if (s_tongue_body_right_bitmap) {
    gbitmap_destroy(s_tongue_body_right_bitmap);
    s_tongue_body_right_bitmap = NULL;
  }
  if (s_tongue_body_left_bitmap) {
    gbitmap_destroy(s_tongue_body_left_bitmap);
    s_tongue_body_left_bitmap = NULL;
  }
  if (s_green_bean_left_bitmap) {
    gbitmap_destroy(s_green_bean_left_bitmap);
    s_green_bean_left_bitmap = NULL;
  }
  if (s_green_bean_middle_bitmap) {
    gbitmap_destroy(s_green_bean_middle_bitmap);
    s_green_bean_middle_bitmap = NULL;
  }
  if (s_green_bean_right_bitmap) {
    gbitmap_destroy(s_green_bean_right_bitmap);
    s_green_bean_right_bitmap = NULL;
  }
  layer_destroy(s_game_layer);
  text_layer_destroy(s_score_layer);
  text_layer_destroy(s_game_over_layer);
}

static void prv_init(void) {
  s_window = window_create();
  window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);
}

static void prv_deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Pyoro game initialized");
  app_event_loop();
  prv_deinit();
}
