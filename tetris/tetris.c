#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
//#include <conio.h>

#define WIDTH 10
#define HEIGHT 20
#define SHAPE_SIZE 4
#define NUM_SHAPES 7

int board[WIDTH][HEIGHT];
int currentShape[SHAPE_SIZE][SHAPE_SIZE];
int currentX, currentY, currentShapeType;
int score;
struct termios orig_termios;

void resetTerminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    printf("\033[?25h"); // Show cursor
}

void setNonCanonicalMode() {
    struct termios new_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(resetTerminal);
    new_termios = orig_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
    printf("\033[?25l"); // Hide cursor
}

int shapes[NUM_SHAPES][SHAPE_SIZE][SHAPE_SIZE] = {
    // Square shape
    {
        {0, 0, 0, 0},
        {0, 1, 1, 0},
        {0, 1, 1, 0},
        {0, 0, 0, 0}
    },
    // Line shape
    {
        {0, 0, 0, 0},
        {1, 1, 1, 1},
        {0, 0, 0, 0},
        {0, 0, 0, 0}
    },
    // L shape
    {
        {0, 0, 0, 0},
        {1, 1, 1, 0},
        {1, 0, 0, 0},
        {0, 0, 0, 0}
    },
    // Reverse L shape
    {
        {0, 0, 0, 0},
        {1, 1, 1, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 0}
    },
    // Z shape
    {
        {0, 0, 0, 0},
        {1, 1, 0, 0},
        {0, 1, 1, 0},
        {0, 0, 0, 0}
    },
    // Reverse Z shape
    {
        {0, 0, 0, 0},
        {0, 1, 1, 0},
        {1, 1, 0, 0},
        {0, 0, 0, 0}
    },
    // T shape
    {
        {0, 0, 0, 0},
        {1, 1, 1, 0},
        {0, 1, 0, 0},
        {0, 0, 0, 0}
    }
};

void initBoard() {
    int i, j;
    for (i = 0; i < WIDTH; i++) {
        for (j = 0; j < HEIGHT; j++) {
            board[i][j] = 0;
        }
    }
}

void initShape() {
    int i, j;
    currentShapeType = rand() % NUM_SHAPES;
    for (i = 0; i < SHAPE_SIZE; i++) {
        for (j = 0; j < SHAPE_SIZE; j++) {
            currentShape[i][j] = shapes[currentShapeType][i][j];
        }
    }
    currentX = WIDTH / 2 - SHAPE_SIZE / 2;
    currentY = 0;
}

bool isCollision(int x, int y, int shape[][SHAPE_SIZE]) {
    int i, j;
    for (i = 0; i < SHAPE_SIZE; i++) {
        for (j = 0; j < SHAPE_SIZE; j++) {
            if (shape[i][j] != 0 && (x + j < 0 || x + j >= WIDTH || y + i >= HEIGHT || board[x+j][y+i] != 0)) {
                return true;
            }
        }
    }
    return false;
}

void placeShape(int x, int y, int shape[][SHAPE_SIZE]) {
    int i, j;
    for (i = 0; i < SHAPE_SIZE; i++) {
        for (j = 0; j < SHAPE_SIZE; j++) {
            if (shape[i][j] != 0) {
                board[x+j][y+i] = shape[i][j];
            }
        }
    }
}

void removeShape(int x, int y, int shape[][SHAPE_SIZE]) {
    int i, j;
    for (i = 0; i < SHAPE_SIZE; i++) {
        for (j = 0; j < SHAPE_SIZE; j++) {
            if (shape[i][j] != 0) {
                board[x+j][y+i] = 0;
            }
        }
    }
}

void clearLines() {
    int i, j, k;
    for (i = HEIGHT - 1; i >= 0; i--) {
        bool full = true;
        for (j = 0; j < WIDTH; j++) {
            if (board[j][i] == 0) {
                full = false;
                break;
            }
        }
        if (full) {
            score += 100;
            for (k = i; k > 0; k--) {
                for (j = 0; j < WIDTH; j++) {
                    board[j][k] = board[j][k-1];
                }
            }
            for (j = 0; j < WIDTH; j++) {
                board[j][0] = 0;
            }
            i++; // Check the same line again
        }
    }
}

void drawBoard() {
    int i, j;
    printf("\033[H"); // Move cursor to top-left
    printf("Score: %d\n\n", score);
    for (i = 0; i < HEIGHT; i++) {
        printf("<!"); // Left border
        for (j = 0; j < WIDTH; j++) {
            if (board[j][i] != 0) {
                printf("[]");
            } else {
                if (currentX <= j && j < currentX + SHAPE_SIZE &&
                    currentY <= i && i < currentY + SHAPE_SIZE &&
                    currentShape[i-currentY][j-currentX] != 0) {
                    printf("[]");
                } else {
                    printf(" .");
                }
            }
        }
        printf("!>\n"); // Right border
    }
    printf("<!");
    for (j = 0; j < WIDTH; j++) printf("==");
    printf("!>\n");
}

bool update() {
    int i, j;
    if (isCollision(currentX, currentY+1, currentShape)) {
        placeShape(currentX, currentY, currentShape);
        clearLines();
        initShape();
        for (i = 0; i < WIDTH; i++) {
            if (board[i][0] != 0) {
                return false;
            }
        }
        return true;
    } else {
        removeShape(currentX, currentY, currentShape);
        currentY++;
        placeShape(currentX, currentY, currentShape);
        return true;
    }
}

void moveShape(int dx) {
    if (!isCollision(currentX+dx, currentY, currentShape)) {
        removeShape(currentX, currentY, currentShape);
        currentX += dx;
        placeShape(currentX, currentY, currentShape);
    }
}

void rotateShape() {
    int temp[SHAPE_SIZE][SHAPE_SIZE];
    int i, j;
    for (i = 0; i < SHAPE_SIZE; i++) {
        for (j = 0; j < SHAPE_SIZE; j++) {
            temp[i][j] = currentShape[i][j];
        }
    }
    for (i = 0; i < SHAPE_SIZE; i++) {
        for (j = 0; j < SHAPE_SIZE; j++) {
            currentShape[i][j] = temp[SHAPE_SIZE-j-1][i];
        }
    }
    if (isCollision(currentX, currentY, currentShape)) {
        for (i = 0; i < SHAPE_SIZE; i++) {
            for (j = 0; j < SHAPE_SIZE; j++) {
                currentShape[i][j] = temp[i][j];
            }
        }
    }
}

void drop() {
    while (update()) {
        // do nothing
    }
}

void playGame() {
    initBoard();
    initShape(); // Ensure first shape is initialized
    setNonCanonicalMode();
    
    // Clear screen once
    printf("\033[2J");

    while (1) {
        drawBoard();
        
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 500000; // 0.5 seconds gravity

        int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);

        if (ret > 0) {
            char input = getchar();
            switch(input) {
                case 'a': moveShape(-1); break;
                case 'd': moveShape(1); break;
                case 'w': rotateShape(); break;
                case 's': drop(); break;
                case 'q': return;
            }
        } else {
            // Timeout - Gravity
            if (!update()) {
                printf("\nGame Over!\n");
                return;
            }
        }
    }
}

int main() {
    srand(time(NULL));
    playGame();
    return 0;
}

