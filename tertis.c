#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <unistd.h>
#include <time.h>

#define ROWS 20
#define COLS 10
#define NUM_TETROMINOES 7
#define HIGHSCORE_FILE "highscore.txt"

int board[ROWS][COLS] = {0};
int colorBoard[ROWS][COLS] = {0};
int score = 0, highScore = 0, linesCleared = 0, level = 1;

int active[4][4], nextPiece[4][4];
int activeColor = 1, nextColor = 1;
int px = COLS / 2 - 2, py = 0;

int tetrominoes[NUM_TETROMINOES][4][4] = {
    {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},  // I
    {{0,0,0,0},{0,1,1,0},{0,1,1,0},{0,0,0,0}},  // O
    {{0,0,0,0},{1,1,1,0},{0,1,0,0},{0,0,0,0}},  // T
    {{0,0,0,0},{1,1,1,0},{1,0,0,0},{0,0,0,0}},  // L
    {{0,0,0,0},{1,1,1,0},{0,0,1,0},{0,0,0,0}},  // J
    {{0,0,0,0},{0,1,1,0},{1,1,0,0},{0,0,0,0}},  // S
    {{0,0,0,0},{1,1,0,0},{0,1,1,0},{0,0,0,0}}   // Z
};

// ---------- Helper Functions ----------
void copy_piece(int dst[4][4], int src[4][4]) {
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            dst[r][c] = src[r][c];
}

void rotate_clockwise(int m[4][4]) {
    int t[4][4];
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            t[c][3 - r] = m[r][c];
    copy_piece(m, t);
}

int can_place(int ny, int nx, int p[4][4]) {
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (p[r][c]) {
                int gy = ny + r, gx = nx + c;
                if (gx < 0 || gx >= COLS || gy >= ROWS) return 0;
                if (gy >= 0 && board[gy][gx]) return 0;
            }
    return 1;
}

void lock_piece(int p[4][4], int color) {
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (p[r][c]) {
                int gy = py + r, gx = px + c;
                if (gy >= 0 && gy < ROWS && gx >= 0 && gx < COLS) {
                    board[gy][gx] = 1;
                    colorBoard[gy][gx] = color;
                }
            }
}

void clear_full_lines() {
    int cleared = 0;
    for (int r = ROWS - 1; r >= 0; r--) {
        int full = 1;
        for (int c = 0; c < COLS; c++)
            if (!board[r][c]) { full = 0; break; }
        if (full) {
            cleared++;
            for (int rr = r; rr > 0; rr--)
                for (int cc = 0; cc < COLS; cc++) {
                    board[rr][cc] = board[rr - 1][cc];
                    colorBoard[rr][cc] = colorBoard[rr - 1][cc];
                }
            for (int cc = 0; cc < COLS; cc++)
                board[0][cc] = colorBoard[0][cc] = 0;
            r++;
        }
    }
    if (cleared > 0) {
        linesCleared += cleared;
        score += 100 * cleared;
        if (score > highScore) highScore = score;
        if (linesCleared / 10 + 1 > level) level++;
    }
}

int is_game_over() {
    for (int c = 0; c < COLS; c++)
        if (board[0][c]) return 1;
    return 0;
}

void reset_board() {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            board[r][c] = colorBoard[r][c] = 0;
    score = linesCleared = level = 0;
}

// ---------- File I/O ----------
void load_high_score() {
    FILE *f = fopen(HIGHSCORE_FILE, "r");
    if (f) {
        fscanf(f, "%d", &highScore);
        fclose(f);
    } else {
        highScore = 0;
    }
}

void save_high_score() {
    FILE *f = fopen(HIGHSCORE_FILE, "w");
    if (f) {
        fprintf(f, "%d", highScore);
        fclose(f);
    }
}

// ---------- Rendering ----------
void draw_border(int sy, int sx) {
    for (int r = 0; r <= ROWS; r++) {
        mvprintw(sy + r, sx - 2, "|");
        mvprintw(sy + r, sx + COLS * 2, "|");
    }
    for (int c = -2; c <= COLS * 2; c++)
        mvprintw(sy + ROWS, sx + c, "-");
}

void draw_board(int sy, int sx) {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            if (board[r][c]) {
                attron(COLOR_PAIR(colorBoard[r][c]));
                mvprintw(sy + r, sx + c * 2, "[]");
                attroff(COLOR_PAIR(colorBoard[r][c]));
            } else mvprintw(sy + r, sx + c * 2, "  ");
}

void draw_ghost(int sy, int sx) {
    int gy = py;
    while (can_place(gy + 1, px, active)) gy++;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (active[r][c]) {
                int yy = gy + r, xx = px + c;
                if (yy >= 0 && yy < ROWS && xx >= 0 && xx < COLS)
                    mvprintw(sy + yy, sx + xx * 2, "::");
            }
}

void draw_active(int sy, int sx) {
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (active[r][c]) {
                int gy = py + r, gx = px + c;
                if (gy >= 0 && gy < ROWS && gx >= 0 && gx < COLS) {
                    attron(COLOR_PAIR(activeColor));
                    mvprintw(sy + gy, sx + gx * 2, "[]");
                    attroff(COLOR_PAIR(activeColor));
                }
            }
}

void draw_piece(int sy, int sx, int piece[4][4], int color) {
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (piece[r][c]) {
                attron(COLOR_PAIR(color));
                mvprintw(sy + r, sx + c * 2, "[]");
                attroff(COLOR_PAIR(color));
            } else mvprintw(sy + r, sx + c * 2, "  ");
}

void draw_side_panel(int sy, int sx) {
    mvprintw(sy, sx, "Score: %d", score);
    mvprintw(sy + 1, sx, "Lines: %d", linesCleared);
    mvprintw(sy + 2, sx, "Level: %d", level);
    mvprintw(sy + 3, sx, "High : %d", highScore);

    mvprintw(sy + 5, sx, "Next:");
    draw_piece(sy + 6, sx, nextPiece, nextColor);

    mvprintw(sy + 12, sx, "Controls:");
    mvprintw(sy + 13, sx, "<-/-> move");
    mvprintw(sy + 14, sx, "Up : rotate");
    mvprintw(sy + 15, sx, "Down : soft drop");
    mvprintw(sy + 16, sx, "SPACE hard drop");
    mvprintw(sy + 17, sx, "P pause");
    mvprintw(sy + 18, sx, "Q quit");
}

static void flush_input(void) {
    int ch;
    nodelay(stdscr, TRUE);
    while ((ch = getch()) != ERR) {}
}

// ---------- Main ----------
int main() {
    initscr(); noecho(); cbreak(); keypad(stdscr, TRUE);
    curs_set(0); nodelay(stdscr, TRUE); start_color();

    srand(time(NULL));
    for (int i = 1; i <= 7; i++) init_pair(i, i, COLOR_BLACK);

    load_high_score();

    int th, tw; getmaxyx(stdscr, th, tw);
    int sy = (th - ROWS) / 2, sx = (tw - (COLS * 2 + 16)) / 2;

    int running = 1, paused = 0, game_over = 0;
    int ch, frame = 0, input_cd = 0;

    int currId = rand() % 7, nextId = rand() % 7;
    copy_piece(active, tetrominoes[currId]);
    copy_piece(nextPiece, tetrominoes[nextId]);
    activeColor = (currId % 7) + 1; nextColor = (nextId % 7) + 1;
    px = COLS / 2 - 2; py = -1;

    while (running) {
        clear();

        if (paused) {
            mvprintw(th / 2 - 1, (tw - 14) / 2, "|| GAME PAUSED");
            mvprintw(th / 2 + 1, (tw - 24) / 2, "Press 'P' to Resume");
            refresh();
            usleep(100000);
            ch = getch();
            if (ch == 'p' || ch == 'P') paused = 0;
            else if (ch == 'q' || ch == 'Q') running = 0;
            continue;
        }

        if (game_over) {
            mvprintw(th / 2 - 2, (tw - 16) / 2, "__ GAME OVER __");
            mvprintw(th / 2, (tw - 12) / 2, "Score: %d", score);
            mvprintw(th / 2 + 2, (tw - 34) / 2, "Press 'R' to Restart or 'Q' to Quit");
            refresh();
            usleep(100000);
            ch = getch();
            if (ch == 'r' || ch == 'R') {
                reset_board(); game_over = 0;
                currId = rand() % 7; nextId = rand() % 7;
                copy_piece(active, tetrominoes[currId]);
                copy_piece(nextPiece, tetrominoes[nextId]);
                activeColor = (currId % 7) + 1; nextColor = (nextId % 7) + 1;
                px = COLS / 2 - 2; py = -1;
                flush_input();
            } else if (ch == 'q' || ch == 'Q') running = 0;
            continue;
        }

        draw_border(sy, sx);
        draw_board(sy, sx);
        draw_ghost(sy, sx);
        draw_active(sy, sx);
        draw_side_panel(sy, sx + COLS * 2 + 6);
        refresh();

        if (input_cd > 0) input_cd--;

        ch = getch();
        switch (ch) {
            case KEY_LEFT:
                if (!input_cd && can_place(py, px - 1, active)) { px--; input_cd = 2; }
                break;
            case KEY_RIGHT:
                if (!input_cd && can_place(py, px + 1, active)) { px++; input_cd = 2; }
                break;
            case KEY_DOWN:
                if (can_place(py + 1, px, active)) py++;
                break;
            case KEY_UP: {
                int tmp[4][4]; copy_piece(tmp, active);
                rotate_clockwise(tmp);
                if (can_place(py, px, tmp)) copy_piece(active, tmp);
                else if (can_place(py, px - 1, tmp)) { px--; copy_piece(active, tmp); }
                else if (can_place(py, px + 1, tmp)) { px++; copy_piece(active, tmp); }
            } break;
            case ' ':
                while (can_place(py + 1, px, active)) py++;
                lock_piece(active, activeColor);
                clear_full_lines();
                if (is_game_over()) { game_over = 1; break; }
                currId = nextId; nextId = rand() % 7;
                copy_piece(active, tetrominoes[currId]);
                copy_piece(nextPiece, tetrominoes[nextId]);
                activeColor = (currId % 7) + 1; nextColor = (nextId % 7) + 1;
                px = COLS / 2 - 2; py = -1;
                flush_input();
                break;
            case 'p': case 'P': paused = 1; break;
            case 'q': case 'Q': running = 0; break;
        }

        int speed = 10 - (level / 2);
        if (speed < 2) speed = 2;
        frame++;
        if (frame % speed == 0) {
            if (can_place(py + 1, px, active)) py++;
            else {
                lock_piece(active, activeColor);
                clear_full_lines();
                if (is_game_over()) { game_over = 1; continue; }
                currId = nextId; nextId = rand() % 7;
                copy_piece(active, tetrominoes[currId]);
                copy_piece(nextPiece, tetrominoes[nextId]);
                activeColor = (currId % 7) + 1; nextColor = (nextId % 7) + 1;
                px = COLS / 2 - 2; py = -1;
                flush_input();
                if (!can_place(py, px, active)) game_over = 1;
            }
        }

        usleep(80000);
    }

    save_high_score();
    endwin();
    return 0;
}
