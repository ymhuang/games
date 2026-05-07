#include "uefi.h"

#define BOARD_W 10
#define BOARD_H 20
#define EMPTY 0
#define TICK_US 16000U

static EFI_SYSTEM_TABLE *g_st;
static EFI_BOOT_SERVICES *g_bs;
static EFI_GRAPHICS_OUTPUT_PROTOCOL *g_gop;
static UINT32 board[BOARD_H][BOARD_W];
static UINT32 rng_state = 0x31415926U;
static UINTN score;
static UINTN lines;
static int game_over;

typedef struct {
    int type;
    int rot;
    int x;
    int y;
} Piece;

static Piece current;
static Piece next_piece;

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

static int collides(Piece p)
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
            if (by >= 0 && board[by][bx] != EMPTY) {
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

static void spawn_piece(void)
{
    current = next_piece;
    current.x = 3;
    current.y = -1;
    current.rot = 0;
    next_piece = make_piece();
    if (collides(current)) {
        game_over = 1;
    }
}

static void lock_piece(void)
{
    int py;
    int px;

    for (py = 0; py < 4; py++) {
        for (px = 0; px < 4; px++) {
            int bx;
            int by;

            if (!piece_cell(current.type, current.rot, px, py)) {
                continue;
            }
            bx = current.x + px;
            by = current.y + py;
            if (by >= 0 && by < BOARD_H && bx >= 0 && bx < BOARD_W) {
                board[by][bx] = (UINT32)current.type + 1U;
            }
        }
    }
}

static void clear_lines(void)
{
    int y;

    for (y = BOARD_H - 1; y >= 0; y--) {
        int full = 1;
        int x;

        for (x = 0; x < BOARD_W; x++) {
            if (board[y][x] == EMPTY) {
                full = 0;
                break;
            }
        }

        if (full) {
            int yy;
            for (yy = y; yy > 0; yy--) {
                for (x = 0; x < BOARD_W; x++) {
                    board[yy][x] = board[yy - 1][x];
                }
            }
            for (x = 0; x < BOARD_W; x++) {
                board[0][x] = EMPTY;
            }
            lines++;
            score += 100 + (lines / 10) * 25;
            y++;
        }
    }
}

static int move_piece(int dx, int dy)
{
    Piece p = current;

    p.x += dx;
    p.y += dy;
    if (!collides(p)) {
        current = p;
        return 1;
    } else if (dy > 0) {
        lock_piece();
        clear_lines();
        spawn_piece();
    }
    return 0;
}

static void rotate_piece(void)
{
    Piece p = current;

    p.rot = (p.rot + 1) & 3;
    if (!collides(p)) {
        current = p;
        return;
    }
    p.x--;
    if (!collides(p)) {
        current = p;
        return;
    }
    p.x += 2;
    if (!collides(p)) {
        current = p;
    }
}

static void hard_drop(void)
{
    Piece p = current;
    UINTN dropped = 0;

    p.y++;
    while (!collides(p)) {
        current = p;
        p.y++;
        dropped++;
    }
    score += dropped;
    lock_piece();
    clear_lines();
    spawn_piece();
}

static UINTN cell_size(void)
{
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = g_gop->Mode->Info;
    UINTN by_height = (info->VerticalResolution - 40U) / BOARD_H;
    UINTN by_width = (info->HorizontalResolution - 40U) / (BOARD_W + 8U);
    UINTN cell = by_height < by_width ? by_height : by_width;

    if (cell < 8U) {
        cell = 8U;
    }
    if (cell > 32U) {
        cell = 32U;
    }
    return cell;
}

static void draw_cell(UINTN x, UINTN y, UINTN cell, UINT32 color)
{
    fill_rect(x, y, cell, cell, 0x25304aU);
    fill_rect(x + 1U, y + 1U, cell - 2U, cell - 2U, color);
    fill_rect(x + 3U, y + 3U, cell > 6U ? cell - 6U : 1U, cell / 5U + 1U, 0xffffffU);
}

static void draw_game(void)
{
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = g_gop->Mode->Info;
    UINTN cell = cell_size();
    UINTN board_px_w = BOARD_W * cell;
    UINTN board_px_h = BOARD_H * cell;
    UINTN origin_x = (info->HorizontalResolution - board_px_w) / 2U;
    UINTN origin_y = (info->VerticalResolution - board_px_h) / 2U;
    int y;
    int x;

    clear_screen_graphics();
    fill_rect(origin_x - 4U, origin_y - 4U, board_px_w + 8U, board_px_h + 8U, 0xd7dee8U);
    fill_rect(origin_x, origin_y, board_px_w, board_px_h, 0x12192aU);

    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++) {
            UINTN sx = origin_x + (UINTN)x * cell;
            UINTN sy = origin_y + (UINTN)y * cell;

            if (board[y][x]) {
                draw_cell(sx, sy, cell, colors[board[y][x]]);
            } else {
                fill_rect(sx + 1U, sy + 1U, cell - 2U, cell - 2U, 0x151f33U);
            }
        }
    }

    for (y = 0; y < 4; y++) {
        for (x = 0; x < 4; x++) {
            int bx = current.x + x;
            int by = current.y + y;
            if (piece_cell(current.type, current.rot, x, y) && by >= 0) {
                draw_cell(origin_x + (UINTN)bx * cell, origin_y + (UINTN)by * cell,
                          cell, colors[current.type + 1]);
            }
        }
    }

    if (g_st->ConOut && g_st->ConOut->SetCursorPosition) {
        g_st->ConOut->SetCursorPosition(g_st->ConOut, 0, 0);
    }
    print(L"\r\nUEFI Tetris  Score: ");
    print_uint(score);
    print(L"  Lines: ");
    print_uint(lines);
    print(L"                    \r\nArrows: move/rotate  Space: drop  P: pause  Q/Esc: quit                    \r\n");
    if (game_over) {
        print(L"Game over. Press R to restart or Q/Esc to quit.                    \r\n");
    } else {
        print(L"                                                            \r\n");
    }
}

static void reset_game(void)
{
    int y;
    int x;

    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++) {
            board[y][x] = EMPTY;
        }
    }
    score = 0;
    lines = 0;
    game_over = 0;
    next_piece = make_piece();
    spawn_piece();
}

static int read_key(EFI_INPUT_KEY *key)
{
    EFI_STATUS status = g_st->ConIn->ReadKeyStroke(g_st->ConIn, key);
    return status == EFI_SUCCESS;
}

static int handle_input(void)
{
    EFI_INPUT_KEY key;

    while (read_key(&key)) {
        if (key.ScanCode == 23 || key.UnicodeChar == L'q' || key.UnicodeChar == L'Q') {
            return 0;
        }
        if (game_over) {
            if (key.UnicodeChar == L'r' || key.UnicodeChar == L'R') {
                reset_game();
            }
            continue;
        }
        if (key.ScanCode == 3) {
            move_piece(-1, 0);
        } else if (key.ScanCode == 4) {
            move_piece(1, 0);
        } else if (key.ScanCode == 1) {
            rotate_piece();
        } else if (key.ScanCode == 2) {
            if (move_piece(0, 1)) {
                score++;
            }
        } else if (key.UnicodeChar == L' ') {
            hard_drop();
        } else if (key.UnicodeChar == L'p' || key.UnicodeChar == L'P') {
            print(L"\r\nPaused. Press any key.\r\n");
            while (!read_key(&key)) {
                g_bs->Stall(50000U);
            }
        }
    }
    return 1;
}

EFI_STATUS EFIAPI EfiMain(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table)
{
    EFI_STATUS status;
    UINTN frame;
    UINTN drop_frames;

    print(L"this is a test line\r\n");

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
    frame = 0;
    drop_frames = 35;

    while (handle_input()) {
        if (!game_over && frame >= drop_frames) {
            frame = 0;
            move_piece(0, 1);
            if (drop_frames > 8 && lines > 0) {
                drop_frames = 35 - (lines > 27 ? 27 : lines);
            }
        }

        draw_game();
        g_bs->Stall(TICK_US);
        frame++;
    }

    clear_screen_graphics();
    if (g_st->ConOut) {
        g_st->ConOut->ClearScreen(g_st->ConOut);
    }
    print(L"Thanks for playing UEFI Tetris.\r\n");
    return EFI_SUCCESS;
}
