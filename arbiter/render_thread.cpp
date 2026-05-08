// ============================================================================
// arbiter/render_thread.cpp — Phase 16: Real-Time SFML Rendering Thread
// CS 2006 Operating Systems | Spring 2026 | Chrono Rift
// Roll Number: 24i-0847  |  Partner: 24i-0650
//
// WHAT THIS FILE IMPLEMENTS (Phase 16):
//   A dedicated pthread inside the Arbiter that renders the full game UI
//   using SFML at 30 FPS. The thread snapshots shared memory safely
//   (brief lock → memcpy → unlock → draw with no lock held) so it never
//   blocks or slows down the game loop.
//
// SFML WINDOW LAYOUT (1280 × 720):
//
//   ┌─────────────────────────────── CHRONO RIFT ─────────────────────────────┐
//   │  [PLAYER PARTY]               [ACTION LOG]              [ENEMIES]        │
//   │                                                                          │
//   │  ▶ Player 0        HP ████░░  > P0 STRIKES NPC2 for 17  NPC0 HP ███░░  │
//   │    HP:  241000/241500         > NPC2 STRIKES P0 for 14   NPC1 HP ██░░░  │
//   │    Stm: ████████░░ 80/100     > P0 SKIPs stamina→50      ...            │
//   │    Inv: [Solar][Halberd]      > ...                                     │
//   │                               > ...                      Kills: 3/10    │
//   │  ▶ Player 1  [STUNNED]        > ...                                     │
//   │    HP: ██████░░ 180000/241000 > ...               [ULTIMATE ACTIVE!]    │
//   │    ...                        > ...                                     │
//   └──────────────────────────────────────────────────────────────────────────┘
//
// NON-BLOCKING DESIGN:
//   1. Lock master_mutex → memcpy players/npcs/kills → unlock  (< 1ms)
//   2. Lock log_mutex    → memcpy action_log         → unlock  (< 1ms)
//   3. Draw entire frame from snapshots — NO locks held during SFML calls
//   4. sleep 33ms (≈ 30 FPS)
//
// SFML NOTE:
//   sf::RenderWindow MUST be created in the thread that calls draw()/display().
//   Do NOT create the window in main() and pass it to the thread.
//   We create it inside render_thread_func() itself.
// ============================================================================

#include <SFML/Graphics.hpp>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <pthread.h>
#include <unistd.h>
#include "shared_types.h"

// ── Forward declarations ──────────────────────────────────────────────────
void append_action_log(SharedGameState* shm, const char* message);

// ── Window dimensions ─────────────────────────────────────────────────────
#define WIN_W   1280
#define WIN_H    720
#define FPS       30

// ── Layout constants (all in pixels) ──────────────────────────────────────
#define PAD           16    // general padding
#define PANEL_PLAYER_X  10
#define PANEL_PLAYER_W 360
#define PANEL_LOG_X   380
#define PANEL_LOG_W   520
#define PANEL_ENEMY_X 910
#define PANEL_ENEMY_W 360
#define HEADER_H       50
#define BAR_H          14
#define BAR_W         180

// ── Color palette ─────────────────────────────────────────────────────────
static const sf::Color COL_BG         (10,  10,  20);
static const sf::Color COL_PANEL      (20,  20,  40);
static const sf::Color COL_BORDER     (60,  60, 100);
static const sf::Color COL_TITLE      (220, 180, 60);   // gold
static const sf::Color COL_PLAYER     (100, 200, 255);  // sky blue
static const sf::Color COL_ENEMY      (255, 100,  80);  // red-orange
static const sf::Color COL_HP_FILL    (220,  60,  60);  // red
static const sf::Color COL_HP_EMPTY   (60,   20,  20);
static const sf::Color COL_STM_FILL   (60,  180,  60);  // green
static const sf::Color COL_STM_EMPTY  (20,   50,  20);
static const sf::Color COL_LOG_TEXT   (200, 200, 200);
static const sf::Color COL_LOG_HEADER (255, 220,  80);
static const sf::Color COL_STUN       (255, 200,   0);  // yellow
static const sf::Color COL_DEAD       (80,   80,  80);
static const sf::Color COL_ULTIMATE   (180,  60, 255);  // purple
static const sf::Color COL_KILLS      (80,  220,  80);
static const sf::Color COL_WHITE      (255, 255, 255);

// ============================================================================
// SECTION 1 — SNAPSHOT STRUCT
// We copy all data we need in one locked region, then draw with no locks.
// ============================================================================
typedef struct {
    Entity  players[MAX_PLAYERS];
    Entity  npcs[MAX_NPCS];
    int     num_players;
    int     num_npcs_concurrent;
    int     total_kills;
    int     game_result;
    int     ultimate_active;
    // Action log snapshot
    char    action_log[ACTION_LOG_LINES][ACTION_LOG_WIDTH];
    int     log_head;
    int     log_count;
} RenderSnapshot;

static void take_snapshot(SharedGameState* shm, RenderSnapshot* snap) {
    pthread_mutex_lock(&shm->master_mutex);
    memcpy(snap->players,           shm->players,           sizeof(snap->players));
    memcpy(snap->npcs,              shm->npcs,              sizeof(snap->npcs));
    snap->num_players         = shm->num_players;
    snap->num_npcs_concurrent = shm->num_npcs_concurrent;
    snap->total_kills         = shm->total_kills;
    snap->game_result         = shm->game_result;
    snap->ultimate_active     = shm->ultimate_active;
    pthread_mutex_unlock(&shm->master_mutex);

    pthread_mutex_lock(&shm->log_mutex);
    memcpy(snap->action_log, shm->action_log, sizeof(snap->action_log));
    snap->log_head  = shm->log_head;
    snap->log_count = shm->log_count;
    pthread_mutex_unlock(&shm->log_mutex);
}

// ============================================================================
// SECTION 2 — DRAWING HELPERS
// ============================================================================

// draw_bar() — filled/empty horizontal bar
static void draw_bar(sf::RenderWindow& win,
                     float x, float y, float w, float h,
                     float fraction,
                     sf::Color fill_col, sf::Color empty_col) {
    // Empty background
    sf::RectangleShape bg(sf::Vector2f(w, h));
    bg.setPosition(x, y);
    bg.setFillColor(empty_col);
    win.draw(bg);

    // Filled portion
    float filled_w = w * std::max(0.0f, std::min(1.0f, fraction));
    if (filled_w > 0) {
        sf::RectangleShape bar(sf::Vector2f(filled_w, h));
        bar.setPosition(x, y);
        bar.setFillColor(fill_col);
        win.draw(bar);
    }
}

// draw_panel() — filled rounded rect panel background
static void draw_panel(sf::RenderWindow& win,
                       float x, float y, float w, float h) {
    sf::RectangleShape panel(sf::Vector2f(w, h));
    panel.setPosition(x, y);
    panel.setFillColor(COL_PANEL);
    panel.setOutlineColor(COL_BORDER);
    panel.setOutlineThickness(1.5f);
    win.draw(panel);
}

// ============================================================================
// SECTION 3 — PANEL DRAWING FUNCTIONS
// ============================================================================

static void draw_players_panel(sf::RenderWindow& win,
                                const sf::Font& font,
                                const RenderSnapshot& snap) {
    float px = PANEL_PLAYER_X;
    float py = HEADER_H + PAD;
    float pw = PANEL_PLAYER_W;
    float ph = WIN_H - HEADER_H - PAD * 2;

    draw_panel(win, px, py, pw, ph);

    // Header
    sf::Text header("◈ PLAYER PARTY", font, 16);
    header.setFillColor(COL_PLAYER);
    header.setStyle(sf::Text::Bold);
    header.setPosition(px + PAD, py + 6);
    win.draw(header);

    float row = py + 32;

    for (int i = 0; i < snap.num_players && i < MAX_PLAYERS; i++) {
        const Entity& p = snap.players[i];
        if (row > py + ph - 10) break;

        // Name + status
        char name_buf[64];
        if (!p.is_alive) {
            snprintf(name_buf, sizeof(name_buf), "Player %d  [DEAD]", i);
            sf::Text name_t(name_buf, font, 14);
            name_t.setFillColor(COL_DEAD);
            name_t.setPosition(px + PAD, row);
            win.draw(name_t);
            row += 20;
            continue;
        }

        if (p.is_stunned)
            snprintf(name_buf, sizeof(name_buf), "Player %d  ⚡STUNNED", i);
        else
            snprintf(name_buf, sizeof(name_buf), "Player %d", i);

        sf::Text name_t(name_buf, font, 14);
        name_t.setFillColor(p.is_stunned ? COL_STUN : COL_PLAYER);
        name_t.setStyle(sf::Text::Bold);
        name_t.setPosition(px + PAD, row);
        win.draw(name_t);
        row += 18;

        // HP bar + text
        float hp_frac = (p.max_hp > 0) ? (float)p.hp / p.max_hp : 0.f;
        draw_bar(win, px + PAD, row, BAR_W, BAR_H, hp_frac,
                 COL_HP_FILL, COL_HP_EMPTY);
        char hp_buf[48];
        snprintf(hp_buf, sizeof(hp_buf), " HP %d", p.hp);
        sf::Text hp_t(hp_buf, font, 11);
        hp_t.setFillColor(COL_WHITE);
        hp_t.setPosition(px + PAD + BAR_W + 2, row);
        win.draw(hp_t);
        row += BAR_H + 3;

        // Stamina bar + text
        float stm_frac = (p.max_stamina > 0)
                         ? (float)p.stamina / p.max_stamina : 0.f;
        draw_bar(win, px + PAD, row, BAR_W, BAR_H, stm_frac,
                 COL_STM_FILL, COL_STM_EMPTY);
        char stm_buf[48];
        snprintf(stm_buf, sizeof(stm_buf), " Stm %.0f", p.stamina);
        sf::Text stm_t(stm_buf, font, 11);
        stm_t.setFillColor(COL_WHITE);
        stm_t.setPosition(px + PAD + BAR_W + 2, row);
        win.draw(stm_t);
        row += BAR_H + 4;

        // Inventory summary (first line, abbreviated)
        char inv_buf[128];
        inv_buf[0] = '\0';
        strncat(inv_buf, "Inv: ", 6);
        bool inv_shown = false;
        for (int s = 0; s < INVENTORY_SLOTS; s++) {
            int wid = p.inventory[s];
            if (wid != WEAPON_NONE && (s == 0 || p.inventory[s-1] != wid)) {
                if (strlen(inv_buf) + strlen(weapon_name[wid]) + 3 < sizeof(inv_buf)) {
                    strncat(inv_buf, "[", 2);
                    strncat(inv_buf, weapon_name[wid], 20);
                    strncat(inv_buf, "] ", 3);
                    inv_shown = true;
                }
            }
        }
        if (!inv_shown) strncat(inv_buf, "(empty)", 8);

        sf::Text inv_t(inv_buf, font, 11);
        inv_t.setFillColor(COL_TITLE);
        inv_t.setPosition(px + PAD, row);
        win.draw(inv_t);
        row += 18;

        // Separator line
        sf::RectangleShape sep(sf::Vector2f(pw - PAD * 2, 1));
        sep.setPosition(px + PAD, row);
        sep.setFillColor(COL_BORDER);
        win.draw(sep);
        row += 8;
    }
}

static void draw_enemies_panel(sf::RenderWindow& win,
                                const sf::Font& font,
                                const RenderSnapshot& snap) {
    float px = PANEL_ENEMY_X;
    float py = HEADER_H + PAD;
    float pw = PANEL_ENEMY_W;
    float ph = WIN_H - HEADER_H - PAD * 2;

    draw_panel(win, px, py, pw, ph);

    // Header
    sf::Text header("◈ ENEMIES", font, 16);
    header.setFillColor(COL_ENEMY);
    header.setStyle(sf::Text::Bold);
    header.setPosition(px + PAD, py + 6);
    win.draw(header);

    // Kill counter
    char kills_buf[32];
    snprintf(kills_buf, sizeof(kills_buf), "Kills: %d/10", snap.total_kills);
    sf::Text kills_t(kills_buf, font, 14);
    kills_t.setFillColor(COL_KILLS);
    kills_t.setStyle(sf::Text::Bold);
    kills_t.setPosition(px + PAD, py + 26);
    win.draw(kills_t);

    float row = py + 52;

    for (int i = 0; i < snap.num_npcs_concurrent && i < MAX_NPCS; i++) {
        const Entity& n = snap.npcs[i];
        if (row > py + ph - 10) break;
        if (!n.is_alive) continue;

        // Name + stun
        char name_buf[48];
        if (n.is_stunned)
            snprintf(name_buf, sizeof(name_buf), "NPC %d  ⚡STUNNED", n.id);
        else
            snprintf(name_buf, sizeof(name_buf), "NPC %d", n.id);

        sf::Text name_t(name_buf, font, 14);
        name_t.setFillColor(n.is_stunned ? COL_STUN : COL_ENEMY);
        name_t.setStyle(sf::Text::Bold);
        name_t.setPosition(px + PAD, row);
        win.draw(name_t);
        row += 18;

        // HP bar
        float hp_frac = (n.max_hp > 0) ? (float)n.hp / n.max_hp : 0.f;
        draw_bar(win, px + PAD, row, BAR_W, BAR_H, hp_frac,
                 COL_HP_FILL, COL_HP_EMPTY);
        char hp_buf[32];
        snprintf(hp_buf, sizeof(hp_buf), " %d", n.hp);
        sf::Text hp_t(hp_buf, font, 11);
        hp_t.setFillColor(COL_WHITE);
        hp_t.setPosition(px + PAD + BAR_W + 2, row);
        win.draw(hp_t);
        row += BAR_H + 3;

        // Stamina bar
        float stm_frac = (n.max_stamina > 0) ? n.stamina / n.max_stamina : 0.f;
        draw_bar(win, px + PAD, row, BAR_W, BAR_H, stm_frac,
                 COL_STM_FILL, COL_STM_EMPTY);
        char stm_buf[32];
        snprintf(stm_buf, sizeof(stm_buf), " %.0f", n.stamina);
        sf::Text stm_t(stm_buf, font, 11);
        stm_t.setFillColor(COL_WHITE);
        stm_t.setPosition(px + PAD + BAR_W + 2, row);
        win.draw(stm_t);
        row += BAR_H + 10;
    }

    // Ultimate active overlay
    if (snap.ultimate_active) {
        sf::RectangleShape ult_bg(sf::Vector2f(pw - PAD * 2, 30));
        ult_bg.setPosition(px + PAD, py + ph - 40);
        ult_bg.setFillColor(sf::Color(60, 0, 80, 200));
        ult_bg.setOutlineColor(COL_ULTIMATE);
        ult_bg.setOutlineThickness(2.f);
        win.draw(ult_bg);

        sf::Text ult_t("★ ULTIMATE ACTIVE — NPCS SUSPENDED ★", font, 12);
        ult_t.setFillColor(COL_ULTIMATE);
        ult_t.setStyle(sf::Text::Bold);
        ult_t.setPosition(px + PAD + 4, py + ph - 34);
        win.draw(ult_t);
    }
}

static void draw_log_panel(sf::RenderWindow& win,
                            const sf::Font& font,
                            const RenderSnapshot& snap) {
    float px = PANEL_LOG_X;
    float py = HEADER_H + PAD;
    float pw = PANEL_LOG_W;
    float ph = WIN_H - HEADER_H - PAD * 2;

    draw_panel(win, px, py, pw, ph);

    // Header
    sf::Text header("◈ ACTION LOG", font, 16);
    header.setFillColor(COL_LOG_HEADER);
    header.setStyle(sf::Text::Bold);
    header.setPosition(px + PAD, py + 6);
    win.draw(header);

    // Log entries — show most recent at BOTTOM
    float line_h = 16.f;
    int max_lines = (int)((ph - 36) / line_h);
    if (max_lines > ACTION_LOG_LINES) max_lines = ACTION_LOG_LINES;

    int count = snap.log_count < max_lines ? snap.log_count : max_lines;
    float log_y = py + 36 + (max_lines - count) * line_h;

    for (int i = 0; i < count; i++) {
        // Read from oldest to newest
        int idx = (snap.log_head + (snap.log_count - count + i)) % ACTION_LOG_LINES;
        if (idx < 0) idx += ACTION_LOG_LINES;

        const char* msg = snap.action_log[idx];
        if (strlen(msg) == 0) continue;

        // Truncate to fit panel width
        char line[ACTION_LOG_WIDTH];
        snprintf(line, sizeof(line), "> %s", msg);
        // Clip to ~60 chars to avoid overflow
        if (strlen(line) > 70) {
            line[70] = '.'; line[71] = '.'; line[72] = '.'; line[73] = '\0';
        }

        // Color-code by content
        sf::Color col = COL_LOG_TEXT;
        if (strstr(msg, "KILLED") || strstr(msg, "VICTORY") || strstr(msg, "DEFEAT"))
            col = sf::Color(255, 120, 80);
        else if (strstr(msg, "STUNNED") || strstr(msg, "stun"))
            col = COL_STUN;
        else if (strstr(msg, "ULTIMATE") || strstr(msg, "SIGSTOP"))
            col = COL_ULTIMATE;
        else if (strstr(msg, "DEADLOCK"))
            col = sf::Color(255, 80, 255);
        else if (strstr(msg, "HEALS") || strstr(msg, "appeared"))
            col = COL_KILLS;

        sf::Text log_t(line, font, 12);
        log_t.setFillColor(col);
        log_t.setPosition(px + PAD, log_y + i * line_h);
        win.draw(log_t);
    }
}

static void draw_header(sf::RenderWindow& win,
                         const sf::Font& font,
                         const RenderSnapshot& snap) {
    // Background bar
    sf::RectangleShape hdr(sf::Vector2f(WIN_W, HEADER_H));
    hdr.setPosition(0, 0);
    hdr.setFillColor(sf::Color(15, 10, 30));
    hdr.setOutlineColor(COL_BORDER);
    hdr.setOutlineThickness(1.f);
    win.draw(hdr);

    // Title
    sf::Text title("⚔  CHRONO RIFT  ⚔", font, 22);
    title.setFillColor(COL_TITLE);
    title.setStyle(sf::Text::Bold);
    title.setPosition(WIN_W / 2.f - 110, 10);
    win.draw(title);

    // Kills
    char kills_buf[48];
    snprintf(kills_buf, sizeof(kills_buf), "Kills: %d / 10", snap.total_kills);
    sf::Text kills_t(kills_buf, font, 14);
    kills_t.setFillColor(COL_KILLS);
    kills_t.setPosition(WIN_W - 160, 16);
    win.draw(kills_t);

    // Subtitle
    sf::Text sub("CS 2006 OS Project  |  Roll: 24i-0847", font, 12);
    sub.setFillColor(sf::Color(120, 120, 180));
    sub.setPosition(10, 30);
    win.draw(sub);
}

static void draw_game_over(sf::RenderWindow& win,
                            const sf::Font& font,
                            int game_result) {
    // Dim overlay
    sf::RectangleShape overlay(sf::Vector2f(WIN_W, WIN_H));
    overlay.setFillColor(sf::Color(0, 0, 0, 180));
    win.draw(overlay);

    const char* msg1 = "";
    const char* msg2 = "";
    sf::Color   col  = COL_WHITE;

    if (game_result == GAME_WIN) {
        msg1 = "★  VICTORY!  ★";
        msg2 = "10 Enemies Defeated — The Rift is Sealed!";
        col  = sf::Color(255, 215, 0);   // gold
    } else if (game_result == GAME_LOSE) {
        msg1 = "✖  DEFEAT  ✖";
        msg2 = "All Players Have Fallen...";
        col  = sf::Color(220, 60, 60);
    } else {
        msg1 = "Game Exited";
        msg2 = "Thanks for playing Chrono Rift.";
        col  = sf::Color(180, 180, 180);
    }

    sf::Text t1(msg1, font, 48);
    t1.setFillColor(col);
    t1.setStyle(sf::Text::Bold);
    sf::FloatRect b1 = t1.getLocalBounds();
    t1.setPosition((WIN_W - b1.width) / 2.f, WIN_H / 2.f - 80);
    win.draw(t1);

    sf::Text t2(msg2, font, 24);
    t2.setFillColor(COL_WHITE);
    sf::FloatRect b2 = t2.getLocalBounds();
    t2.setPosition((WIN_W - b2.width) / 2.f, WIN_H / 2.f);
    win.draw(t2);

    sf::Text t3("(Closing in 4 seconds...)", font, 16);
    t3.setFillColor(sf::Color(160, 160, 160));
    sf::FloatRect b3 = t3.getLocalBounds();
    t3.setPosition((WIN_W - b3.width) / 2.f, WIN_H / 2.f + 60);
    win.draw(t3);
}

// ============================================================================
// SECTION 4 — RENDER THREAD MAIN FUNCTION
// ============================================================================

void* render_thread_func(void* arg) {
    SharedGameState* shm = (SharedGameState*)arg;

    // ── Create SFML window IN THIS THREAD (required by SFML) ─────────────
    sf::RenderWindow window(
        sf::VideoMode(WIN_W, WIN_H),
        "Chrono Rift — CS 2006 OS Project",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setFramerateLimit(FPS);

    // ── Load font ─────────────────────────────────────────────────────────
    // Tries system fonts in order. Falls back to a built-in sf::Font if none found.
    sf::Font font;
    bool font_loaded = false;
    const char* font_paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        nullptr
    };
    for (int i = 0; font_paths[i]; i++) {
        if (font.loadFromFile(font_paths[i])) {
            font_loaded = true;
            std::cout << "[RENDER] Font loaded: " << font_paths[i] << std::endl;
            break;
        }
    }
    if (!font_loaded) {
        std::cerr << "[RENDER] WARNING: Could not load font — text will not display.\n"
                  << "[RENDER] Install fonts: apt-get install fonts-dejavu-core\n";
        // Continue anyway — SFML won't crash, it just won't show text
    }

    append_action_log(shm, "[RENDER] SFML window open — rendering active");
    std::cout << "[RENDER] Thread started. Window: " << WIN_W << "x" << WIN_H << std::endl;

    RenderSnapshot snap;

    // ── Main render loop ──────────────────────────────────────────────────
    while (window.isOpen()) {
        // Handle window close button (X)
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
        }

        // ── Snapshot shared memory (brief lock) ───────────────────────────
        take_snapshot(shm, &snap);

        // ── Draw frame ────────────────────────────────────────────────────
        window.clear(COL_BG);

        draw_header(window, font, snap);
        draw_players_panel(window, font, snap);
        draw_enemies_panel(window, font, snap);
        draw_log_panel(window, font, snap);

        // Game over overlay — drawn on top of everything
        if (snap.game_result != GAME_ONGOING) {
            draw_game_over(window, font, snap.game_result);
            window.display();
            // Hold game-over screen for 4 seconds then close
            sf::sleep(sf::seconds(4));
            window.close();
            break;
        }

        window.display();
        // framerateLimit(30) handles the sleep automatically
    }

    std::cout << "[RENDER] Thread exiting cleanly." << std::endl;
    return nullptr;
}

// ============================================================================
// launch_render_thread()
// Called from main_game_loop() after game_started = 1.
// ============================================================================
void launch_render_thread(SharedGameState* shm, pthread_t* out_thread) {
    int rc = pthread_create(out_thread, nullptr, render_thread_func, shm);
    if (rc != 0) {
        std::cerr << "[ARBITER] ERROR: Could not create render thread (rc="
                  << rc << ")" << std::endl;
    } else {
        std::cout << "[ARBITER] Render thread launched (SFML " << WIN_W
                  << "x" << WIN_H << " @ " << FPS << " FPS)" << std::endl;
    }
}