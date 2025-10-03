#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>

#define WIDTH 40
#define HEIGHT 20
#define TICK_RATE 0.05
#define MAX_BOMBS 50

typedef struct {
    int x;
    int y;
    int active;
} Bomb;

Bomb bombs[MAX_BOMBS];
int score = 0;
int bombSpawnCounter = 0;

double getTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

int kbhit(void) {
    struct termios oldt, newt;
    int ch;
    int oldf;
    
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    
    ch = getchar();
    
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    
    if(ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    
    return 0;
}

void clearScreen() {
    printf("\033[2J\033[H");
    fflush(stdout);
}

void initBombs() {
    for(int i = 0; i < MAX_BOMBS; i++) {
        bombs[i].active = 0;
    }
}

int getBombsPerWave() {
    int waves = score / 10;
    int bombCount = 1 + waves;
    int maxBombs = WIDTH - 2;
    return (bombCount > maxBombs) ? maxBombs : bombCount;
}

int isSafePosition(int x, int futureRows) {
    for(int futureY = 0; futureY < futureRows; futureY++) {
        int collision = 0;
        for(int i = 0; i < MAX_BOMBS; i++) {
            if(bombs[i].active) {
                int bombFutureY = bombs[i].y + futureY;
                if(bombFutureY == HEIGHT - 2 && (bombs[i].x == x || bombs[i].x == x + 1)) {
                    collision = 1;
                    break;
                }
            }
        }
        if(!collision) {
            return 1;
        }
    }
    return 0;
}

void spawnBombWave() {
    int bombsToSpawn = getBombsPerWave();
    int availablePositions[WIDTH];
    int posCount = 0;
    
    for(int x = 0; x < WIDTH - 1; x++) {
        int occupied = 0;
        for(int i = 0; i < MAX_BOMBS; i++) {
            if(bombs[i].active && bombs[i].y == 0 && bombs[i].x == x) {
                occupied = 1;
                break;
            }
        }
        if(!occupied) {
            availablePositions[posCount++] = x;
        }
    }
    
    if(posCount == 0) return;
    
    for(int attempt = 0; attempt < 100 && bombsToSpawn > 0; attempt++) {
        int selectedPositions[WIDTH];
        int selectedCount = 0;
        
        for(int i = 0; i < posCount; i++) {
            selectedPositions[i] = availablePositions[i];
        }
        int tempCount = posCount;
        
        for(int b = 0; b < bombsToSpawn && tempCount > 0; b++) {
            int idx = rand() % tempCount;
            int chosenX = selectedPositions[idx];
            
            selectedPositions[idx] = selectedPositions[tempCount - 1];
            tempCount--;
            
            for(int i = 0; i < MAX_BOMBS; i++) {
                if(!bombs[i].active) {
                    bombs[i].x = chosenX;
                    bombs[i].y = 0;
                    bombs[i].active = 1;
                    selectedCount++;
                    break;
                }
            }
        }
        
        int safePathExists = 0;
        for(int x = 0; x <= WIDTH - 2; x++) {
            if(isSafePosition(x, HEIGHT - 2)) {
                safePathExists = 1;
                break;
            }
        }
        
        if(safePathExists) {
            break;
        } else {
            for(int i = 0; i < MAX_BOMBS; i++) {
                if(bombs[i].active && bombs[i].y == 0) {
                    bombs[i].active = 0;
                }
            }
        }
    }
}

void updateBombs() {
    for(int i = 0; i < MAX_BOMBS; i++) {
        if(bombs[i].active) {
            bombs[i].y++;
            
            if(bombs[i].y >= HEIGHT) {
                bombs[i].active = 0;
                score++;
            }
        }
    }
}

int checkCollision(int planeX, int planeY) {
    for(int i = 0; i < MAX_BOMBS; i++) {
        if(bombs[i].active) {
            if(bombs[i].y == planeY && (bombs[i].x == planeX || bombs[i].x == planeX + 1)) {
                return 1;
            }
        }
    }
    return 0;
}

void drawGame(int planeX, int planeY) {
    for(int i = 0; i < WIDTH + 2; i++) printf("#");
    printf("\n");
    
    for(int i = 0; i < HEIGHT; i++) {
        printf("#");
        for(int j = 0; j < WIDTH; j++) {
            int drewBomb = 0;
            
            for(int b = 0; b < MAX_BOMBS; b++) {
                if(bombs[b].active && bombs[b].y == i && bombs[b].x == j) {
                    printf("&");
                    drewBomb = 1;
                    break;
                }
            }
            
            if(!drewBomb) {
                if(i == planeY && j == planeX) {
                    printf("/\\");
                    j++;
                } else {
                    printf(" ");
                }
            }
        }
        printf("#\n");
    }
    
    for(int i = 0; i < WIDTH + 2; i++) printf("#");
    printf("\n");
}

void drawExplosion(int planeX, int planeY, int frame) {
    for(int i = 0; i < WIDTH + 2; i++) printf("#");
    printf("\n");
    
    for(int i = 0; i < HEIGHT; i++) {
        printf("#");
        for(int j = 0; j < WIDTH; j++) {
            if(frame == 0) {
                if(i == planeY && (j == planeX || j == planeX + 1)) {
                    printf("*");
                } else {
                    printf(" ");
                }
            } else if(frame == 1) {
                int dist = abs(i - planeY) + abs(j - planeX);
                if(dist <= 2) {
                    printf("*");
                } else {
                    printf(" ");
                }
            } else {
                int dist = abs(i - planeY) + abs(j - planeX);
                if(dist <= 3) {
                    printf(".");
                } else {
                    printf(" ");
                }
            }
        }
        printf("#\n");
    }
    
    for(int i = 0; i < WIDTH + 2; i++) printf("#");
    printf("\n");
}

void showExplosionAnimation(int planeX, int planeY) {
    for(int frame = 0; frame < 3; frame++) {
        clearScreen();
        drawExplosion(planeX, planeY, frame);
        printf("\n*** BOOM! ***\n");
        fflush(stdout);
        usleep(200000);
    }
}

int main() {
    int planeX = WIDTH / 2;
    int planeY = HEIGHT - 2;
    char key;
    int running = 1;
    int tickCount = 0;
    double lastTickTime = 0;
    double currentTime = 0;
    
    srand(time(NULL));
    initBombs();
    
    printf("=== C HYDROGEN BOMBS, RUN! ===\n");
    printf("Dodge the Hydrogen Bombs (&)!\n");
    printf("Difficulty increases every 10 points!\n");
    printf("Use 'a' and 'd' to move left and right\n");
    printf("Press 'q' to quit\n");
    printf("Press ENTER to start...\n");
    getchar();
    
    lastTickTime = getTime();
    
    while(running) {
        currentTime = getTime();
        
        if(currentTime - lastTickTime >= TICK_RATE) {
            tickCount++;
            lastTickTime = currentTime;
            
            if(kbhit()) {
                key = getchar();
                
                if(key == 'a' || key == 'A') {
                    if(planeX > 0) planeX--;
                }
                else if(key == 'd' || key == 'D') {
                    if(planeX < WIDTH - 2) planeX++;
                }
                else if(key == 'q' || key == 'Q') {
                    running = 0;
                    continue;
                }
            }
            
            bombSpawnCounter++;
            if(bombSpawnCounter >= 20) {
                spawnBombWave();
                bombSpawnCounter = 0;
            }
            
            updateBombs();
            
            if(checkCollision(planeX, planeY)) {
                showExplosionAnimation(planeX, planeY);
                clearScreen();
                printf("\n");
                printf("╔════════════════════════════╗\n");
                printf("║       GAME OVER!           ║\n");
                printf("╠════════════════════════════╣\n");
                printf("║                            ║\n");
                printf("║   Your Score: %-4d        ║\n", score);
                printf("║   Difficulty: %d bombs     ║\n", getBombsPerWave());
                printf("║                            ║\n");
                printf("╚════════════════════════════╝\n");
                printf("\n");
                break;
            }
            
            clearScreen();
            drawGame(planeX, planeY);
            printf("Score: %d | Bombs/Wave: %d | Controls: a=Left, d=Right, q=Quit\n", 
                   score, getBombsPerWave());
            fflush(stdout);
        }
        
        usleep(1000);
    }
    
    return 0;
}