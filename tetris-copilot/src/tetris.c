#include <efi.h>
#include <efilib.h>

#define BOARD_W 10
#define BOARD_H 20
#define TICK_MS 500 // gravity tick in milliseconds

static const int pieces[7][4][4][4] = {
    // I
    {{{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
     {{0,0,1,0},{0,0,1,0},{0,0,1,0},{0,0,1,0}},
     {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
     {{0,0,1,0},{0,0,1,0},{0,0,1,0},{0,0,1,0}}},
    // O
    {{{0,0,0,0},{0,1,1,0},{0,1,1,0},{0,0,0,0}},
     {{0,0,0,0},{0,1,1,0},{0,1,1,0},{0,0,0,0}},
     {{0,0,0,0},{0,1,1,0},{0,1,1,0},{0,0,0,0}},
     {{0,0,0,0},{0,1,1,0},{0,1,1,0},{0,0,0,0}}},
    // T
    {{{0,0,0,0},{1,1,1,0},{0,1,0,0},{0,0,0,0}},
     {{0,1,0,0},{1,1,0,0},{0,1,0,0},{0,0,0,0}},
     {{0,1,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
     {{0,1,0,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}}},
    // S
    {{{0,0,0,0},{0,1,1,0},{1,1,0,0},{0,0,0,0}},
     {{1,0,0,0},{1,1,0,0},{0,1,0,0},{0,0,0,0}},
     {{0,0,0,0},{0,1,1,0},{1,1,0,0},{0,0,0,0}},
     {{1,0,0,0},{1,1,0,0},{0,1,0,0},{0,0,0,0}}},
    // Z
    {{{0,0,0,0},{1,1,0,0},{0,1,1,0},{0,0,0,0}},
     {{0,1,0,0},{1,1,0,0},{1,0,0,0},{0,0,0,0}},
     {{0,0,0,0},{1,1,0,0},{0,1,1,0},{0,0,0,0}},
     {{0,1,0,0},{1,1,0,0},{1,0,0,0},{0,0,0,0}}},
    // J
    {{{0,0,0,0},{1,1,1,0},{0,0,1,0},{0,0,0,0}},
     {{0,1,0,0},{0,1,0,0},{1,1,0,0},{0,0,0,0}},
     {{1,0,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
     {{0,1,1,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}}},
    // L
    {{{0,0,0,0},{1,1,1,0},{1,0,0,0},{0,0,0,0}},
     {{1,1,0,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}},
     {{0,0,1,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
     {{0,1,0,0},{0,1,0,0},{0,1,1,0},{0,0,0,0}}}
};

static int board[BOARD_H][BOARD_W];

static void clear_board() { for (int y=0;y<BOARD_H;y++) for (int x=0;x<BOARD_W;x++) board[y][x]=0; }

static void draw_board(EFI_SYSTEM_TABLE *ST, int px,int py,int piece,int rot) {
    ST->ConOut->ClearScreen(ST->ConOut);
    CHAR16 line[64];
    for (int y=0;y<BOARD_H;y++) {
        int pos = 0;
        line[pos++]=L'|';
        for (int x=0;x<BOARD_W;x++) {
            int cell = board[y][x];
            // overlay current piece
            int relx = x - px + 1; // piece indices 0..3
            int rely = y - py + 1;
            if (relx>=0 && relx<4 && rely>=0 && rely<4) {
                if (pieces[piece][rot][rely][relx]) cell = 2;
            }
            line[pos++] = cell ? (cell==1?L'#':L'O') : L' ';
        }
        line[pos++]=L'|';
        line[pos++]=L'\r'; line[pos++]=L'\n';
        line[pos]=0;
        ST->ConOut->OutputString(ST->ConOut, line);
    }
    // footer
    ST->ConOut->OutputString(ST->ConOut, L"+----------+\r\n");
}

static int collides(int px,int py,int piece,int rot) {
    for (int pyi=0;pyi<4;pyi++) for (int pxi=0;pxi<4;pxi++) if (pieces[piece][rot][pyi][pxi]) {
        int x = px + pxi - 1;
        int y = py + pyi - 1;
        if (x<0 || x>=BOARD_W || y<0 || y>=BOARD_H) return 1;
        if (board[y][x]) return 1;
    }
    return 0;
}

static void lock_piece(int px,int py,int piece,int rot) {
    for (int pyi=0;pyi<4;pyi++) for (int pxi=0;pxi<4;pxi++) if (pieces[piece][rot][pyi][pxi]) {
        int x = px + pxi -1;
        int y = py + pyi -1;
        if (y>=0 && y<BOARD_H && x>=0 && x<BOARD_W) board[y][x]=1;
    }
}

static void clear_lines() {
    for (int y=BOARD_H-1;y>=0;y--) {
        int full=1; for (int x=0;x<BOARD_W;x++) if (!board[y][x]) { full=0; break; }
        if (full) {
            for (int yy=y;yy>0;yy--) for (int x=0;x<BOARD_W;x++) board[yy][x]=board[yy-1][x];
            for (int x=0;x<BOARD_W;x++) board[0][x]=0;
            y++; // recheck same line
        }
    }
}

EFI_STATUS EFIAPI efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"UEFI Tetris - arrows to move, up rotate, ESC quit\r\n");

    clear_board();
    // create timer
    EFI_EVENT timerEvent;
    EFI_STATUS Status = SystemTable->BootServices->CreateEvent(EVT_TIMER, TPL_CALLBACK, NULL, NULL, &timerEvent);
    if (EFI_ERROR(Status)) return Status;
    // set periodic timer: units are 100ns; 500ms = 5,000,000
    SystemTable->BootServices->SetTimer(timerEvent, TimerPeriodic, TICK_MS * 10000);

    EFI_EVENT events[2];
    events[0] = SystemTable->ConIn->WaitForKey;
    events[1] = timerEvent;

    int piece = 0;
    int rot = 0;
    int px = BOARD_W/2;
    int py = 1;
    unsigned long seed = (unsigned long)ImageHandle;

    auto_next:
    piece = (seed++ ) % 7;
    rot = 0; px = BOARD_W/2; py = 1;
    if (collides(px,py,piece,rot)) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Game Over\r\n");
        SystemTable->BootServices->CloseEvent(timerEvent);
        return EFI_SUCCESS;
    }

    draw_board(SystemTable, px, py, piece, rot);

    while (1) {
        UINTN index;
        Status = SystemTable->BootServices->WaitForEvent(2, events, &index);
        if (EFI_ERROR(Status)) break;
        if (index==0) {
            EFI_INPUT_KEY Key;
            SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &Key);
            if (Key.ScanCode == SCAN_ESC) break;
            if (Key.ScanCode == SCAN_LEFT) {
                if (!collides(px-1,py,piece,rot)) px--;
            } else if (Key.ScanCode == SCAN_RIGHT) {
                if (!collides(px+1,py,piece,rot)) px++;
            } else if (Key.ScanCode == SCAN_UP) {
                int nrot = (rot+1)&3; if (!collides(px,py,piece,nrot)) rot = nrot;
            } else if (Key.ScanCode == SCAN_DOWN) {
                if (!collides(px,py+1,piece,rot)) py++;
            }
            draw_board(SystemTable, px, py, piece, rot);
        } else if (index==1) {
            // timer tick: gravity
            if (!collides(px,py+1,piece,rot)) {
                py++;
            } else {
                lock_piece(px,py,piece,rot);
                clear_lines();
                goto auto_next;
            }
            draw_board(SystemTable, px, py, piece, rot);
        }
    }

    SystemTable->BootServices->CloseEvent(timerEvent);
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Exiting...\r\n");
    return EFI_SUCCESS;
}
