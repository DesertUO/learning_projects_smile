#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#include <GL/glew.h>

#include <GLFW/glfw3.h>

#include <cglm/cglm.h>

// Ye
#include "utils.h"
#include "vector.h"

#define MAX_TRIANGLES INT_MAX/2000

#define FLOAT2_TO_VEC2(dest, f) glm_vec2_copy((vec2){(f).x, (f).y}, dest) // gipiti
#define VEC2_TO_FLOAT2(v) ((float2){(v)[0], (v)[1]})

const GLuint WIDTH = 1280; const GLuint HEIGHT = 720;
const uint16_t CIRC_RES = 32;

GLuint triangleVAO, triangleVBO;
GLuint shaderProgram;


typedef vec2 pos2;

//typedef vec2 float2;
// typedef vec3 float3;
// typedef vec4 float4;

typedef struct float2 {
    float x;
    float y;
} float2;

float2 float2_sub(float2 a, float2 b) { return (float2){a.x - b.x, a.y - b.y}; }
float2 float2_add(float2 a, float2 b) { return (float2){a.x + b.x, a.y + b.y}; }
float2 float2_mul(float2 v, float s) { return (float2){v.x * s, v.y * s}; }

float float2_length(float2 v) { return sqrtf(v.x*v.x + v.y*v.y); }

float2 float2_normalize(float2 v) {
    float len = float2_length(v);
    if(len == 0) return (float2){0,0};
    return float2_mul(v, 1.0f/len);
}

VECTOR_DEFINE(float2)


typedef struct int2 {
    int x;
    int y;
} int2;

VECTOR_DEFINE(int2);

typedef ivec3 int3;
typedef ivec4 int4;

typedef struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} Color;

typedef struct Rectangle {
    float2 pos;
    float2 size;
    Color col;
} Rectangle;


typedef struct Vertex {
    float2 pos;
    Color col;
} Vertex;

typedef struct Triangle {
    Vertex vertices[3];
} Triangle;

typedef struct TriangleBuffer {
    Triangle triangles[MAX_TRIANGLES];
    int count;
} TriangleBuffer;

// Global
TriangleBuffer triangleBuffer = {{}, 0};

// Some CONSTANTS
const float2 screenCenter = {WIDTH / 2.0f, HEIGHT / 2.0f};
const Rectangle screenBox = {{0.0f, 0.0f}, {WIDTH, HEIGHT}};


float2 toNDC(float2 screenPos) {
    return (float2){
        2.0f * screenPos.x / WIDTH  - 1.0f,
        1.0f - 2.0f * screenPos.y / HEIGHT
    };
}

/* Initialization(s) */
void initTriangleRenderer(GLuint* triangleVAO, GLuint* triangleVBO) {
    glGenVertexArrays(1, triangleVAO);
    glGenBuffers(1, triangleVBO);

    glBindVertexArray(*triangleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, *triangleVBO);

    glBufferData(GL_ARRAY_BUFFER, sizeof(Triangle) * MAX_TRIANGLES, NULL, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex), (void*)(offsetof(Vertex, pos)));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                          sizeof(Vertex), (void*)(offsetof(Vertex, col)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

/* Functions to compile and link shaders */

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        fprintf(stderr, "Shader compile error:\n%s\n", log);
        exit(EXIT_FAILURE);
    }

    return shader;
}

GLuint createShaderProgram(const char* vertSrc, const char* fragSrc) {
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, 512, NULL, log);
        fprintf(stderr, "Program link error:\n%s\n", log);
        exit(EXIT_FAILURE);
    }

    glDeleteShader(vert);
    glDeleteShader(frag);

    return program;
}

/* Used*/
void sendTrianglesToGPU() {
    glUseProgram(shaderProgram);
    glBindVertexArray(triangleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, triangleVBO);

    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Triangle) * triangleBuffer.count, triangleBuffer.triangles);
    glDrawArrays(GL_TRIANGLES, 0, triangleBuffer.count * 3);

    glBindVertexArray(0);

}

// Prob gonna change it to GPU side rendering, like \/ gonna disappear (probably)
void drawTriangle(Triangle* triangle) {
    if (triangleBuffer.count >= MAX_TRIANGLES) return;

    Triangle ndcTriangle = *triangle;
    for(int i = 0; i < 3; i++) {
        ndcTriangle.vertices[i].pos = toNDC(triangle->vertices[i].pos);
    }

    if (triangleBuffer.count >= MAX_TRIANGLES) return; // safety
    triangleBuffer.triangles[triangleBuffer.count++] = ndcTriangle;
}

// Used to draw the cell
void drawRectangle(Rectangle rect) {
    float hx = rect.size.x * 0.5f;
    float hy = rect.size.y * 0.5f;

    Vertex v[4] = {
                        {
                            { rect.pos.x + hx, rect.pos.y + hy },
                            rect.col
                        },
                        {
                            { rect.pos.x - hx, rect.pos.y + hy },
                            rect.col
                        },
                        {
                            { rect.pos.x - hx, rect.pos.y - hy },
                            rect.col
                        },
                        {
                            { rect.pos.x + hx, rect.pos.y - hy },
                            rect.col
                        }
                  };

    Triangle t1 = { v[0], v[2], v[1] };
    Triangle t2 = { v[0], v[2], v[3] };

    drawTriangle(&t1);
    drawTriangle(&t2);
}

GLFWwindow* initialize() {
    if (!glfwInit()) {
        crash("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "OPENGL_1", NULL, NULL);
    if (window == NULL) {
        glfwTerminate();
        crash("Failed to create window");
    }
    glfwMakeContextCurrent(window);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        crash("Failed to initialize GLEW");
    }

    glViewport(0, 0, WIDTH, HEIGHT);
    return window;
}

// Needs to be odd for it to have space to walk and edges on all directions kinda
#define MAZE_WITDH 11
#define MAZE_HEIGHT 11

float GRID_SIZE = 60.0f;

// Represents the grid of a maze, including the edges
typedef struct maze_grid{
    char grid[MAZE_WITDH][MAZE_HEIGHT]; // -> cell_ij <-> cell_xy :) matrix row-major order
    /* 0 0 0 0 0 0 0
     * 0 0 0 0 0 0 0
     * 0 0 0 0 1 0 0
     * 0 0 0 0 0 0 0
     * */
    // 1 is at i=3,j=5 and x=5.y=3 :) could be 3*7 + 5 if linear, nvm
} maze_grid;

// Empty spaces are on even coordinates
// Edges are on odd coordinates

maze_grid actual_maze = { 0 };

void render_maze(maze_grid maze) {
    for(int j = 0; j < MAZE_HEIGHT; j++) {
        for(int i = 0; i < MAZE_WITDH; i++) {
            char cell = maze.grid[i][j];

            Rectangle cell_rect;
            cell_rect.col = (cell == 0 ) ? (Color){0, 0, 0, 255} : (Color){255, 255, 255, 255};
            cell_rect.pos = (float2){i * GRID_SIZE + (GRID_SIZE / 2.0f), j * GRID_SIZE + (GRID_SIZE / 2.0f)};
            cell_rect.size = (float2){GRID_SIZE, GRID_SIZE};

            drawRectangle(cell_rect);
        }
    }

    Color line_color = {255, 0, 0, 100};

    for(int i = 1; i < MAZE_HEIGHT; i++) {
        Rectangle hor;
        hor.size = (float2){MAZE_WITDH * GRID_SIZE, 5.0f};
        hor.col = line_color;
        hor.pos = (float2){MAZE_WITDH * GRID_SIZE / 2.0f, i * GRID_SIZE};
        drawRectangle(hor);
    }
    for(int i = 1; i < MAZE_WITDH; i++) {
        Rectangle ver;
        ver.size = (float2){5.0f, MAZE_HEIGHT * GRID_SIZE};
        ver.col = line_color;
        ver.pos = (float2){i * GRID_SIZE, MAZE_WITDH * GRID_SIZE / 2.0f};
        drawRectangle(ver);
    }
}


void generate_box_borders(maze_grid* maze) {
    // Basic:
    for(int j = 0; j < MAZE_HEIGHT; j++) {
        for(int i = 0; i < MAZE_WITDH; i++) {
            if(i == 0 || i == (MAZE_WITDH - 1) || j == 0 || j == (MAZE_HEIGHT - 1)) {
                maze->grid[i][j] = 1;
            } else {
                maze->grid[i][j] = 0;
            }
        }
    }
}

// Generates a random number bewteen min and max inclusive
void gen_valid_rand(int* num, int min, int max) {
    *num = rand() % (max + 1);
    while(*num < min) {
        *num = rand() % (max + 1);
    }
}

// Is inside inclusive with edges
bool is_point_inside_box(int2 point, int2 box_tl, int2 box_br) {
    return (point.x >= box_tl.x && point.x <= box_br.x) && (point.y >= box_tl.y && point.y <= box_br.y);
}

int2 generate_point_int_ring() {
    int2 start;
    start.x = 0;
    start.y = 0;

    gen_valid_rand(&start.x, 1, (MAZE_WITDH - 2));
    gen_valid_rand(&start.y, 1, (MAZE_HEIGHT - 2));

    int2 invalid_top_left = {2, 2};
    int2 invalid_bottom_right = {MAZE_WITDH - 3, MAZE_HEIGHT - 3};

    while(is_point_inside_box(start, invalid_top_left, invalid_bottom_right)){
        gen_valid_rand(&start.x, 1, (MAZE_WITDH - 2));
        gen_valid_rand(&start.y, 1, (MAZE_HEIGHT - 2));
    }
    return start;
}

int2 generate_valid_point_int_ring(int in, int out) {
    int2 start = generate_point_int_ring();

    while((start.x == in && start.y == 1) || /* If it's blocking in and out*/
          (start.x == out && start.y == MAZE_HEIGHT - 2) ||
          (start.x == start.y) || /* If it's in one of the corners */
          (start.x == MAZE_WITDH - 2 && start.y == 1) ||
          (start.x == 1 && start.y == MAZE_HEIGHT - 2) || /* Check if it's next to other start points or so*/
          (0)) {
        start = generate_point_int_ring();
    }

    return start;
}

void add_entrances(maze_grid* maze, int* maze_in, int* maze_out) {
    gen_valid_rand(maze_in, 1, MAZE_WITDH - 2);
    gen_valid_rand(maze_out, 1, MAZE_WITDH - 2);


    maze->grid[*maze_in][0] = 0;
    maze->grid[*maze_out][MAZE_HEIGHT - 1] = 0;
}

typedef enum Direciton {
    up,
    down,
    left,
    right,
    none
} Direciton;

int2 choose_new_cell(maze_grid* maze, int2 current_cell, Direciton* dir, Direciton* prev) {
    *prev = *dir;
    *dir = rand() % 4;
    int2 new_cell = current_cell;

    switch(*dir) {
        case up:
            new_cell = (int2){new_cell.x, new_cell.y + 1};
            break;
        case down:
            new_cell = (int2){new_cell.x, new_cell.y - 1};
            break;
        case left:
            new_cell = (int2){new_cell.x - 1, new_cell.y};
            break;
        case right:
            new_cell = (int2){new_cell.x + 1, new_cell.y};
            break;
        default:
            crash("Error wtf\n");
    }
    return new_cell;
}

bool valid_pos(int2 cell) {
    return cell.x >= 0 && cell.x <= (MAZE_WITDH - 1) && cell.y >= 0 && cell.y <= (MAZE_HEIGHT - 1);
}

bool is_adyacent_to_at_least_one(maze_grid* maze, int2 cell) {
    int2 up = (int2){cell.x, cell.y + 1};
    int2 down = (int2){cell.x, cell.y - 1};
    int2 left = (int2){cell.x - 1, cell.y};
    int2 right = (int2){cell.x + 1, cell.y};

    bool is_up = false;
    if(valid_pos(up)) {
        is_up = (maze->grid[up.x][up.y] == 1) ? true : false;
    } else {
        is_up = true;
    }

    bool is_down = false;
    if(valid_pos(down)) {
        is_down = (maze->grid[down.x][down.y] == 1) ? true : false;
    } else {
        is_down = true;
    }

    bool is_left = false;
    if(valid_pos(left)) {
        is_left = (maze->grid[left.x][left.y] == 1) ? true : false;
    } else {
        is_left = true;
    }

    bool is_right = false;
    if(valid_pos(right)) {
        is_right = (maze->grid[right.x][right.y] == 1) ? true : false;
    } else {
        is_right = true;
    }

    return is_up || is_down || is_left || is_right;
}

bool is_enclosed(maze_grid* maze, int2 cell) {
    int2 up = (int2){cell.x, cell.y + 1};
    int2 down = (int2){cell.x, cell.y - 1};
    int2 left = (int2){cell.x - 1, cell.y};
    int2 right = (int2){cell.x + 1, cell.y};

    bool is_up = false;
    if(valid_pos(up)) {
        is_up = (maze->grid[up.x][up.y] == 1) ? true : false;
    } else {
        is_up = true;
    }

    bool is_down = false;
    if(valid_pos(down)) {
        is_down = (maze->grid[down.x][down.y] == 1) ? true : false;
    } else {
        is_down = true;
    }

    bool is_left = false;
    if(valid_pos(left)) {
        is_left = (maze->grid[left.x][left.y] == 1) ? true : false;
    } else {
        is_left = true;
    }

    bool is_right = false;
    if(valid_pos(right)) {
        is_right = (maze->grid[right.x][right.y] == 1) ? true : false;
    } else {
        is_right = true;
    }

    return is_up && is_down && is_left && is_right;
}

bool can_be_occupied(maze_grid* maze, int2 cell) {
    int count = 0;

    int2 cell_to_check;

    bool can_be = false;

    if(maze->grid[cell.x][cell.y] == 0)
        can_be = true;

    if((cell.x < 1 ||
        cell.x > (MAZE_WITDH - 2) ||
        cell.y < 1 ||
        cell.y > (MAZE_HEIGHT - 2)
        )) {
        can_be = false;
    }

    return can_be;
}

int2 choose_new_cell_valid(maze_grid* maze, int2* current_cell, Direciton* dir, Direciton* prev, bool* break_curr) {
    if(is_enclosed(maze, *current_cell)) {
        *break_curr = true;
        return (int2){0, 0};
    }

    // Current cell is still the same
    printf("choosing next cell...\n");
    int2 new_cell = (int2){-1, -1};
    // Check if can walk there
    int try = 1;
    do {
        printf("%d. cell x: %d, y:%d cannot be occupied, generating new one...\n", try, new_cell.x, new_cell.y);
        *dir = *prev; // If didn't go then don't forget the real prev dir
        new_cell = choose_new_cell(maze, *current_cell, dir, prev);
        try++;
    } while(!can_be_occupied(maze, new_cell));

    return new_cell;

}

void generate_maze(maze_grid* maze, Vector_int2* cells_in_order) {
    int maze_in;
    int maze_out;

    add_entrances(maze, &maze_in, &maze_out);
    printf("maze_in: %d, maze_out: %d\n", maze_in, maze_out);


    //Asfdgdfhg
    // Rund DFS from starting point: start
    // to a depth of: to_walk (that regens when walked hits to_walk) or until it cannot more
    int2 start = generate_valid_point_int_ring(maze_in, maze_out);
    append_vector_int2(cells_in_order, start);

    int to_walk;
    gen_valid_rand(&to_walk, 1, (MAZE_WITDH - 2) * (MAZE_HEIGHT - 2));

    Direciton dir, prev;
    dir = none;
    prev = none;

    int walked = 0;

    bool break_curr = false;
    while(walked < to_walk) {
        printf("Step: %d out of %d\n", walked, to_walk);
        printf("Assigning: x=%d, y=%d\n\n", start.x, start.y);

        maze->grid[start.x][start.y] = 1;

        printf("choosing new cell..\n");
        start = choose_new_cell_valid(maze, &start, &dir, &prev, &break_curr);

        printf("checking if break...\n");
        if(break_curr) {
            printf("Breaking...");
            break_curr = false;
            walked = to_walk + 1;
        } else {
            printf("Not breaking...");
            printf("appending valid cell...\n");
            append_vector_int2(cells_in_order, start);
        }

        walked++;
    }
}

/* Main Functions */
void updateScene(double deltaTime) {
    return;
}

// Exmplae to make a trail for a body
// Probably gonna change to a easier way to make trails for bodies
// Trail bodyTrail;

void renderScene(GLFWwindow* window, Vector_int2* cells_in_order) {
    //glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    triangleBuffer.count = 0;

    // Test
    // Rectangle a = {{200.0f, 204.0f}, {50.0f, 100.0f}, {255, 255, 0, 255}};
    // drawRectangle(a);
    render_maze(actual_maze);

    float base_trail_cell_size = GRID_SIZE * 0.5f;

    for(int i = 0; i < (cells_in_order->length - 1); i++) {
        Rectangle cell;
        cell.size = (float2){base_trail_cell_size, base_trail_cell_size};
        if(i == 0)
            cell.col = (Color){255, 255, 0, 255};
        else if(i == (cells_in_order->length - 2))
            cell.col = (Color){255, 0, 0, 255};
        else
            cell.col = (Color){0, 255, 0, (int)(((float)i / cells_in_order->length) * 255)};
        cell.pos = (float2){cells_in_order->data[i].x * GRID_SIZE + base_trail_cell_size,
                            cells_in_order->data[i].y * GRID_SIZE + base_trail_cell_size};
        drawRectangle(cell);
    }

    sendTrianglesToGPU();

    glfwSwapBuffers(window);
}

void gameLoop(GLFWwindow* window, Vector_int2* cells_in_order) {
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        float deltaTime = (float)(currentTime - lastTime);
        lastTime = currentTime;

        glfwPollEvents();

        updateScene(deltaTime);
        renderScene(window, cells_in_order);
    }
}


int main(int argc, const char * argv[]) {
    srand(time(NULL));


    GLFWwindow* window = initialize();
    initTriangleRenderer(&triangleVAO, &triangleVBO);

    const char* vertSrc = load_file_as_string("Shaders/triangle_shader.vert");
    const char* fragSrc = load_file_as_string("Shaders/triangle_shader.frag");

    shaderProgram = createShaderProgram(vertSrc, fragSrc);

    free((void*)vertSrc);
    free((void*)fragSrc);

    Vector_int2 created_cells;
    init_vector_int2(&created_cells);

    generate_box_borders(&actual_maze);
    generate_maze(&actual_maze, &created_cells);

    gameLoop(window, &created_cells);
    glfwTerminate();


    return EXIT_SUCCESS;
}

