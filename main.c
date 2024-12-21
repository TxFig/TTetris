#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <time.h>


const int WIDTH = 10;
const int HEIGHT = 10;

void disable_cursor()          { printf("\033[?25l");       }
void enable_cursor()           { printf("\033[?25h");       }
void clear_after_cursor()      { printf("\033[0J");         }
void move_cursor_up(int lines) { printf("\033[%dA", lines); }

void configure_terminal() {
    struct termios t;

    // Get current terminal attributes
    if (tcgetattr(STDIN_FILENO, &t) == -1) {
        perror("tcgetattr");
        exit(EXIT_FAILURE);
    }

    t.c_lflag &= ~(ICANON | ECHO); // Turn off echo and canonical mode
    t.c_cc[VMIN] = 1;              // Minimum number of bytes for read
    t.c_cc[VTIME] = 0;             // No timeout for read

    // Set the new attributes
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &t) == -1) {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }

    // Set stdin to non-blocking mode
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return;
    }
    if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        return;
    }
}

void restore_terminal() {
    struct termios original;

    // Get current terminal attributes
    if (tcgetattr(STDIN_FILENO, &original) == -1) {
        perror("tcgetattr");
        exit(EXIT_FAILURE);
    }

    // Restore the terminal mode
    original.c_lflag |= (ECHO | ICANON);

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &original) == -1) {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }
}

int read_input(char (*buffer)[3], ssize_t* bytesRead) {
    *bytesRead = read(STDIN_FILENO, buffer, 3);

    if (*bytesRead == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("read");
            return 1;
        }
    }

    return 0;
}

const int pieces[7][6] = {
    {4, 1, 0, 1, 2, 3},
    {3, 2, 0, 3, 4, 5},
    {3, 2, 2, 3, 4, 5},
    {2, 2, 0, 1, 2, 3},
    {3, 2, 1, 2, 3, 4},
    {3, 2, 0, 1, 2, 4},
    {3, 2, 0, 1, 4, 5},
};

int calc_piece_index(int *data, int index, int x, int y) {
    int width = data[0];
    int height = data[1];

    int global_index = data[index + 2];
    int inner_y = global_index / width;
    global_index += inner_y * WIDTH;
    global_index -= inner_y * width;
    global_index += x;
    global_index += y * WIDTH;

    return global_index;
}

int random_number(int min, int max) {
    return (rand() % (max - min + 1)) + min;
}

int* rotate_1d_map(int map[6]) {
    int width = map[0];
    int height = map[1];

    int map_2d[height][width];
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            map_2d[y][x] = 0;
        }
    }
    for (int i = 0; i < 4; i++) {
        int index = map[i + 2];
        int x = index % width;
        int y = index / width;
        map_2d[y][x] = 1;
    }

    int rotated_map_2d[width][height];
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            int value = map_2d[y][x];
            int reversed_y = height - y - 1;
            rotated_map_2d[x][reversed_y] = value;
        }
    }

    static int rotated_map[6];
    rotated_map[0] = height;
    rotated_map[1] = width;
    int place_index = 2;
    for (int y = 0; y < width; y++) {
        for (int x = 0; x < height; x++) {
            if (rotated_map_2d[y][x]) {
                int index = y * height + x;
                rotated_map[place_index] = index;
                place_index++;
            }
        }
    }
    return rotated_map;
}


int main() {
    srand(time(0));
    disable_cursor();
    configure_terminal();
    char buffer[3];
    ssize_t bytesRead;

    int map[WIDTH * HEIGHT];
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        map[i] = 0;
    }
    int piece_index = random_number(0, 6);
    int piece_x = 0;
    int piece_y = -1;

    char width_dash[WIDTH + 1];
    for (int i = 0; i < WIDTH; i++) {
        width_dash[i] = '-';
    }
    width_dash[WIDTH] = '\0';

    struct timespec req, rem;
    req.tv_sec = 0;
    req.tv_nsec = 100000000;

    int move_delay_count = 0;
    int move_delay = 10;

    int rotation = 0; // 0 - 3 (0, 90, 180, 270)

    int score = 0;
    int game_over = 0;

    while (1) {
        if (read_input(&buffer, &bytesRead)) break;

        int *raw_piece_data = (int*)pieces[piece_index];
        int *piece_data = raw_piece_data;
        for (int i = 0; i < rotation; i++) {
            piece_data = rotate_1d_map(piece_data);
        }
        int width = piece_data[0];

        //* Handle Input
        if (bytesRead > 0) {
            if (buffer[0] == '\033') { // Escape sequence
                if (bytesRead == 1) {
                    break; // ESC key
                }
                if (buffer[1] == '[') {
                    switch (buffer[2]) {
                        case 'A': { // Arrow Up
                            rotation++;
                            rotation %= 4;
                            break;
                        }
                        case 'B': { // Arrow Down
                            piece_y++;
                            break;
                        }
                        case 'C': { // Arrow Right
                            if (piece_x + width < WIDTH) {
                                piece_x++;
                            }
                            break;
                        }
                        case 'D': { // Arrow Left
                            if (piece_x > 0) {
                                piece_x--;
                            }
                            break;
                        }
                    }
                }
            }
            else if (buffer[0] == 32) { // SPACE
                if (game_over) {
                    game_over = 0;
                    for (int i = 0; i < WIDTH * HEIGHT; i++) {
                        map[i] = 0;
                    }
                    piece_index = random_number(0, 6);
                    piece_x = 0;
                    piece_y = -2;
                    score = 0;
                }
            }
            // else {
            //     printf("You entered: %c, (ASCII: %d)\n", buffer[0], buffer[0]);
            // }
        }

        if (move_delay_count >= move_delay) {
            piece_y++;
            move_delay_count = 0;
        } else {
            move_delay_count++;
        }

        //* Check collision
        int collision = 0;
        for (int i = 0; i < 4; i++) {
            int index = calc_piece_index(piece_data, i, piece_x, piece_y);
            int index_below = index + WIDTH;
            if (index_below >= WIDTH * HEIGHT || map[index_below] == 1) {
                collision = 1;
            }
        }
        if (collision) {
            if (piece_y < 0) {
                game_over = 1;
            }

            for (int i = 0; i < 4; i++) {
                int index = calc_piece_index(piece_data, i, piece_x, piece_y);
                map[index] = 1;
            }
            piece_index = random_number(0, 6);
            piece_x = 0;
            piece_y = -2;
        }

        //* Handle complete lines
        for (int y = 0; y < HEIGHT; y++) {
            int empty_tile = 0;
            for (int x = 0; x < WIDTH; x++) {
                int index = y * WIDTH + x;
                if (!map[index]) {
                    empty_tile = 1;
                }
            }

            if (!empty_tile) {
                for (int x = 0; x < WIDTH; x++) {
                    int index = y * WIDTH + x;
                    map[index] = 0;
                }

                for (int y2 = y - 1; y2 >= 0; y2--) {
                    for (int x = 0; x < WIDTH; x++) {
                        int index = y2 * WIDTH + x;
                        map[index + WIDTH] = map[index];
                    }
                }
                score++;
            }
        }

        //* Update map
        int render_map[WIDTH * HEIGHT];
        for (int i = 0; i < WIDTH * HEIGHT; i++) {
            render_map[i] = map[i];
        }

        for (int i = 0; i < 4; i++) {
            int index = calc_piece_index(piece_data, i, piece_x, piece_y);
            render_map[index] = 1;
        }

        //* Draw
        clear_after_cursor();
        if (game_over) {
            printf("Game Over!\n");
            printf("Score: %d\n", score);
            printf("Press SPACE to restart\n");
            move_cursor_up(3);
        } else {
            printf(" %s\n", width_dash);
            for (int y = 0; y < HEIGHT; y++) {
                printf("|");
                for (int x = 0; x < WIDTH; x++) {
                    int index = y * WIDTH + x;
                    if (render_map[index]) {
                        printf("#");
                    } else {
                        printf(" ");
                    }
                }
                printf("|\n");
            }
            printf(" %s\n", width_dash);
            printf("Score: %d\n", score);
            move_cursor_up(HEIGHT + 3);
        }

        if (nanosleep(&req, &rem) == -1) {
            perror("nanosleep");
            exit(EXIT_FAILURE);
        }
    }

    clear_after_cursor();
    enable_cursor();
    restore_terminal();

    return 0;
}
