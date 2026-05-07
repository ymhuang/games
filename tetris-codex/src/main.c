#include "uefi.h"

#define BOARD_W 10
#define BOARD_H 20
#define EMPTY 0
#define TICK_US 16000U
#define PLAYER_COUNT 2

static EFI_SYSTEM_TABLE *g_st;
static EFI_BOOT_SERVICES *g_bs;
static EFI_GRAPHICS_OUTPUT_PROTOCOL *g_gop;
static UINT32 rng_state = 0x31415926U;
static int paused;

typedef struct {
    int type;
    int rot;
    int x;
    int y;
} Piece;

typedef struct {
    UINT32 board[BOARD_H][BOARD_W];
    Piece current;
    Piece next_piece;
    UINTN score;
    UINTN lines;
    UINTN frame;
    UINTN drop_frames;
    int game_over;
} Player;

static Player players[PLAYER_COUNT];

static const EFI_GUID gop_guid = {
    0x9042a9deU, 0x23dcU, 0x4a38U,
    {0x96U, 0xfbU, 0x7aU, 0xdeU, 0xd0U, 0x80U, 0x51U, 0x6aU}
};

static const UINT16 shapes[7][4] = {
    {0x0f00, 0x2222, 0x00f0, 0x4444},
    {0x8e00, 0x6440, 0x0e20, 0x44c0},
    {0x2e00, 0x4460, 0x0e80, 0xc440},
    {0x6600, 0x6600, 0x6600, 0x6600},
    {0x6c00, 0x4620, 0x06c0, 0x8c40},
    {0x4e00, 0x4640, 0x0e40, 0x4c40},
    {0xc600, 0x2640, 0x0c60, 0x4c80}
};

static const UINT32 colors[8] = {
    0x101820U, 0x00d7ffU, 0x1d6cffU, 0xff9f1cU,
    0xffdd00U, 0x23ce6bU, 0xb14cffU, 0xff3864U
};

static const UINT8 dummy_reloc[12] __attribute__((section(".reloc"), used)) = {
    0, 0, 0, 0,
    12, 0, 0, 0,
    0, 0, 0, 0
};

static void print(CHAR16 *s)
{
    if (g_st && g_st->ConOut) {
        g_st->ConOut->OutputString(g_st->ConOut, s);
    }
}

static void print_uint(UINTN value)
{
    CHAR16 buf[24];
    UINTN i = 22;

    buf[23] = 0;
    if (value == 0) {
        print(L"0");
        return;
    }

    while (value && i > 0) {
        buf[i--] = (CHAR16)(L'0' + (value % 10));
        value /= 10;
    }
    print(&buf[i + 1]);
}

static UINT32 rand_next(void)
{
    rng_state = rng_state * 1664525U + 1013904223U;
    return rng_state;
}

static int piece_cell(int type, int rot, int x, int y)
{
    UINT16 mask = shapes[type][rot & 3];
    return (mask >> (15 - (y * 4 + x))) & 1;
}

static void fill_rect(UINTN x, UINTN y, UINTN w, UINTN h, UINT32 rgb)
{
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = g_gop->Mode->Info;
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL pixel;

    if (x >= info->HorizontalResolution || y >= info->VerticalResolution) {
        return;
    }
    if (w > info->HorizontalResolution - x) {
        w = info->HorizontalResolution - x;
    }
    if (h > info->VerticalResolution - y) {
        h = info->VerticalResolution - y;
    }

    pixel.Red = (UINT8)((rgb >> 16) & 0xffU);
    pixel.Green = (UINT8)((rgb >> 8) & 0xffU);
    pixel.Blue = (UINT8)(rgb & 0xffU);
    pixel.Reserved = 0;

    g_gop->Blt(g_gop, &pixel, EfiBltVideoFill, 0, 0, x, y, w, h, 0);
}

static void clear_screen_graphics(void)
{
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = g_gop->Mode->Info;
    fill_rect(0, 0, info->HorizontalResolution, info->VerticalResolution, 0x0b1020U);
}

static int collides(Player *player, Piece p)
{
    int py;
    int px;

    for (py = 0; py < 4; py++) {
        for (px = 0; px < 4; px++) {
            int bx;
            int by;

            if (!piece_cell(p.type, p.rot, px, py)) {
                continue;
            }
            bx = p.x + px;
            by = p.y + py;
            if (bx < 0 || bx >= BOARD_W || by >= BOARD_H) {
                return 1;
            }
            if (by >= 0 && player->board[by][bx] != EMPTY) {
                return 1;
            }
        }
    }
    return 0;
}

static Piece make_piece(void)
{
    Piece p;

    p.type = (int)(rand_next() % 7U);
    p.rot = 0;
    p.x = 3;
    p.y = -1;
    return p;
}

static void spawn_piece(Player *player)
{
    player->current = player->next_piece;
    player->current.x = 3;
    player->current.y = -1;
    player->current.rot = 0;
    player->next_piece = make_piece();
    if (collides(player, player->current)) {
        player->game_over = 1;
    }
}

static void lock_piece(Player *player)
{
    int py;
    int px;

    for (py = 0; py < 4; py++) {
        for (px = 0; px < 4; px++) {
            int bx;
            int by;

            if (!piece_cell(player->current.type, player->current.rot, px, py)) {
                continue;
            }
            bx = player->current.x + px;
            by = player->current.y + py;
            if (by >= 0 && by < BOARD_H && bx >= 0 && bx < BOARD_W) {
                player->board[by][bx] = (UINT32)player->current.type + 1U;
            }
        }
    }
}

static Player *opponent_of(Player *player)
{
    return player == &players[0] ? &players[1] : &players[0];
}

static UINTN clear_lines(Player *player)
{
    int y;
    UINTN cleared = 0;

    for (y = BOARD_H - 1; y >= 0; y--) {
        int full = 1;
        int x;

        for (x = 0; x < BOARD_W; x++) {
            if (player->board[y][x] == EMPTY) {
                full = 0;
                break;
            }
        }

        if (full) {
            int yy;
            for (yy = y; yy > 0; yy--) {
                for (x = 0; x < BOARD_W; x++) {
                    player->board[yy][x] = player->board[yy - 1][x];
                }
            }
            for (x = 0; x < BOARD_W; x++) {
                player->board[0][x] = EMPTY;
            }
            player->lines++;
            cleared++;
            player->score += 100 + (player->lines / 10) * 25;
            y++;
        }
    }
    return cleared;
}

static void append_garbage(Player *player, UINTN count)
{
    UINTN row;

    if (player->game_over) {
        return;
    }

    for (row = 0; row < count; row++) {
        int x;
        int y;
        int hole = (int)(rand_next() % BOARD_W);

        for (x = 0; x < BOARD_W; x++) {
            if (player->board[0][x] != EMPTY) {
                player->game_over = 1;
                return;
            }
        }

        for (y = 0; y < BOARD_H - 1; y++) {
            for (x = 0; x < BOARD_W; x++) {
                player->board[y][x] = player->board[y + 1][x];
            }
        }
        for (x = 0; x < BOARD_W; x++) {
            player->board[BOARD_H - 1][x] = x == hole ? EMPTY : 7U;
        }
    }

    if (collides(player, player->current)) {
        player->game_over = 1;
    }
}

static void update_speed(Player *player)
{
    if (player->drop_frames > 8 && player->lines > 0) {
        player->drop_frames = 35 - (player->lines > 27 ? 27 : player->lines);
    }
}

static int move_piece(Player *player, int dx, int dy)
{
    Piece p = player->current;

    p.x += dx;
    p.y += dy;
    if (!collides(player, p)) {
        player->current = p;
        return 1;
    } else if (dy > 0) {
        UINTN cleared;

        lock_piece(player);
        cleared = clear_lines(player);
        if (cleared >= 2U) {
            append_garbage(opponent_of(player), cleared);
        }
        update_speed(player);
        spawn_piece(player);
    }
    return 0;
}

static void rotate_piece(Player *player)
{
    Piece p = player->current;

    p.rot = (p.rot + 1) & 3;
    if (!collides(player, p)) {
        player->current = p;
        return;
    }
    p.x--;
    if (!collides(player, p)) {
        player->current = p;
        return;
    }
    p.x += 2;
    if (!collides(player, p)) {
        player->current = p;
    }
}

static void hard_drop(Player *player)
{
    Piece p = player->current;
    UINTN dropped = 0;

    p.y++;
    while (!collides(player, p)) {
        player->current = p;
        p.y++;
        dropped++;
    }
    player->score += dropped;
    lock_piece(player);
    dropped = clear_lines(player);
    if (dropped >= 2U) {
        append_garbage(opponent_of(player), dropped);
    }
    update_speed(player);
    spawn_piece(player);
}

static UINTN cell_size(void)
{
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = g_gop->Mode->Info;
    UINTN by_height = (info->VerticalResolution - 48U) / BOARD_H;
    UINTN by_width = (info->HorizontalResolution - 64U) / ((BOARD_W * PLAYER_COUNT) + 6U);
    UINTN cell = by_height < by_width ? by_height : by_width;

    if (cell < 6U) {
        cell = 6U;
    }
    if (cell > 28U) {
        cell = 28U;
    }
    return cell;
}

static void draw_cell(UINTN x, UINTN y, UINTN cell, UINT32 color)
{
    fill_rect(x, y, cell, cell, 0x25304aU);
    fill_rect(x + 1U, y + 1U, cell - 2U, cell - 2U, color);
    fill_rect(x + 2U, y + 2U, cell > 5U ? cell - 4U : 1U, cell / 5U + 1U, 0xffffffU);
}

static void draw_board(Player *player, UINTN origin_x, UINTN origin_y, UINTN cell)
{
    UINTN board_px_w = BOARD_W * cell;
    UINTN board_px_h = BOARD_H * cell;
    int y;
    int x;

    fill_rect(origin_x - 4U, origin_y - 4U, board_px_w + 8U, board_px_h + 8U,
              player->game_over ? 0xff3864U : 0xd7dee8U);
    fill_rect(origin_x, origin_y, board_px_w, board_px_h, 0x12192aU);

    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++) {
            UINTN sx = origin_x + (UINTN)x * cell;
            UINTN sy = origin_y + (UINTN)y * cell;

            if (player->board[y][x]) {
                draw_cell(sx, sy, cell, colors[player->board[y][x]]);
            } else {
                fill_rect(sx + 1U, sy + 1U, cell - 2U, cell - 2U, 0x151f33U);
            }
        }
    }

    if (!player->game_over) {
        for (y = 0; y < 4; y++) {
            for (x = 0; x < 4; x++) {
                int bx = player->current.x + x;
                int by = player->current.y + y;
                if (piece_cell(player->current.type, player->current.rot, x, y) && by >= 0) {
                    draw_cell(origin_x + (UINTN)bx * cell, origin_y + (UINTN)by * cell,
                              cell, colors[player->current.type + 1]);
                }
            }
        }
    }
}

static void draw_game(void)
{
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = g_gop->Mode->Info;
    UINTN cell = cell_size();
    UINTN board_px_w = BOARD_W * cell;
    UINTN board_px_h = BOARD_H * cell;
    UINTN gap = cell * 4U;
    UINTN total_w = board_px_w * PLAYER_COUNT + gap;
    UINTN origin_x = info->HorizontalResolution > total_w ? (info->HorizontalResolution - total_w) / 2U : 8U;
    UINTN origin_y = info->VerticalResolution > board_px_h ? (info->VerticalResolution - board_px_h) / 2U : 8U;

    clear_screen_graphics();
    draw_board(&players[0], origin_x, origin_y, cell);
    draw_board(&players[1], origin_x + board_px_w + gap, origin_y, cell);

    if (g_st->ConOut && g_st->ConOut->SetCursorPosition) {
        g_st->ConOut->SetCursorPosition(g_st->ConOut, 0, 0);
    }
    print(L"\r\nP1 Score: ");
    print_uint(players[0].score);
    print(L" Lines: ");
    print_uint(players[0].lines);
    print(L"      P2 Score: ");
    print_uint(players[1].score);
    print(L" Lines: ");
    print_uint(players[1].lines);
    print(L"                    \r\nP1: Arrows + Space   P2: WASD + F   P: pause   R: restart   Q/Esc: quit                    \r\n");
    if (paused) {
        print(L"Paused. Press P to resume.                    \r\n");
    } else if (players[0].game_over && players[1].game_over) {
        print(L"Both players are out. Press R to restart or Q/Esc to quit.                    \r\n");
    } else if (players[0].game_over) {
        print(L"Player 1 is out. Player 2 can keep playing.                    \r\n");
    } else if (players[1].game_over) {
        print(L"Player 2 is out. Player 1 can keep playing.                    \r\n");
    } else {
        print(L"                                                            \r\n");
    }
}

static void reset_player(Player *player)
{
    int y;
    int x;

    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++) {
            player->board[y][x] = EMPTY;
        }
    }
    player->score = 0;
    player->lines = 0;
    player->frame = 0;
    player->drop_frames = 35;
    player->game_over = 0;
    player->next_piece = make_piece();
    spawn_piece(player);
}

static void reset_game(void)
{
    UINTN i;

    paused = 0;
    for (i = 0; i < PLAYER_COUNT; i++) {
        reset_player(&players[i]);
    }
}

static int read_key(EFI_INPUT_KEY *key)
{
    EFI_STATUS status = g_st->ConIn->ReadKeyStroke(g_st->ConIn, key);
    return status == EFI_SUCCESS;
}

static void handle_player_input(Player *player, int action)
{
    if (player->game_over) {
        return;
    }
    if (action == 0) {
        move_piece(player, -1, 0);
    } else if (action == 1) {
        move_piece(player, 1, 0);
    } else if (action == 2) {
        rotate_piece(player);
    } else if (action == 3) {
        if (move_piece(player, 0, 1)) {
            player->score++;
        }
    } else if (action == 4) {
        hard_drop(player);
    }
}

static int handle_input(void)
{
    EFI_INPUT_KEY key;

    while (read_key(&key)) {
        if (key.ScanCode == 23 || key.UnicodeChar == L'q' || key.UnicodeChar == L'Q') {
            return 0;
        }
        if (key.UnicodeChar == L'r' || key.UnicodeChar == L'R') {
            reset_game();
            continue;
        }
        if (key.UnicodeChar == L'p' || key.UnicodeChar == L'P') {
            paused = !paused;
            continue;
        }
        if (paused) {
            continue;
        }

        if (key.ScanCode == 3) {
            handle_player_input(&players[0], 0);
        } else if (key.ScanCode == 4) {
            handle_player_input(&players[0], 1);
        } else if (key.ScanCode == 1) {
            handle_player_input(&players[0], 2);
        } else if (key.ScanCode == 2) {
            handle_player_input(&players[0], 3);
        } else if (key.UnicodeChar == L' ') {
            handle_player_input(&players[0], 4);
        } else if (key.UnicodeChar == L'a' || key.UnicodeChar == L'A') {
            handle_player_input(&players[1], 0);
        } else if (key.UnicodeChar == L'd' || key.UnicodeChar == L'D') {
            handle_player_input(&players[1], 1);
        } else if (key.UnicodeChar == L'w' || key.UnicodeChar == L'W') {
            handle_player_input(&players[1], 2);
        } else if (key.UnicodeChar == L's' || key.UnicodeChar == L'S') {
            handle_player_input(&players[1], 3);
        } else if (key.UnicodeChar == L'f' || key.UnicodeChar == L'F') {
            handle_player_input(&players[1], 4);
        }
    }
    return 1;
}

static void tick_player(Player *player)
{
    if (paused || player->game_over) {
        return;
    }

    if (player->frame >= player->drop_frames) {
        player->frame = 0;
        move_piece(player, 0, 1);
    }
    player->frame++;
}

EFI_STATUS EFIAPI EfiMain(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table)
{
    EFI_STATUS status;

    (void)image_handle;
    g_st = system_table;
    g_bs = system_table->BootServices;
    rng_state ^= system_table->Hdr.CRC32;

    if (g_st->ConOut) {
        g_st->ConOut->ClearScreen(g_st->ConOut);
    }
    if (g_st->ConIn) {
        g_st->ConIn->Reset(g_st->ConIn, 0);
    }

    status = g_bs->LocateProtocol((EFI_GUID *)&gop_guid, 0, (VOID **)&g_gop);
    if (status != EFI_SUCCESS || !g_gop || !g_gop->Mode || !g_gop->Mode->Info) {
        print(L"UEFI Tetris requires Graphics Output Protocol, but GOP was not found.\r\n");
        return status == EFI_SUCCESS ? EFI_UNSUPPORTED : status;
    }
    if (!g_gop->Blt) {
        print(L"UEFI Tetris requires GOP Blt drawing, but Blt is unavailable.\r\n");
        return EFI_UNSUPPORTED;
    }

    reset_game();

    while (handle_input()) {
        tick_player(&players[0]);
        tick_player(&players[1]);
        draw_game();
        g_bs->Stall(TICK_US);
    }

    clear_screen_graphics();
    if (g_st->ConOut) {
        g_st->ConOut->ClearScreen(g_st->ConOut);
    }
    print(L"Thanks for playing UEFI Tetris.\r\n");
    return EFI_SUCCESS;
}
