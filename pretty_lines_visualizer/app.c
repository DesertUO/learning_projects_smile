#include <stdlib.h>
#include <stdint.h>
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

// Needs to be odd for it to have space to walk and edges on all directions kinda
#define MAZE_WITDH 11
#define MAZE_HEIGHT 11

const float  GRID_SIZE = 100.0f;

const GLuint WIDTH = MAZE_WITDH * GRID_SIZE + 10.0f; const GLuint HEIGHT = MAZE_HEIGHT * GRID_SIZE + 10.0f;
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

typedef struct int_box {
    int2 tl;
    int2 br;
} int_box;

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

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Pretty lines generator", NULL, NULL);
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

typedef enum Direction {
    top,
    bottom,
    left,
    right
} Direction;

typedef struct cell {
    // Horizontal line
    uint8_t wall;  // 0 0 0 0 0 0 0 0
                   //         r l b t
                   //         i e o o
                   //         g f t p
                   //         h t t
                   //         t   o
                   //             m
} cell;

// Represents the grid of a maze, including the edges
typedef struct maze{
    cell grid[MAZE_WITDH][MAZE_HEIGHT];
} maze;

void initialize_maze(maze* maze) {
    for(int i = 0; i < MAZE_WITDH; i++) {
        for(int j = 0; j < MAZE_HEIGHT; j++) {
            set_bit(&maze->grid[i][j].wall, top);
            set_bit(&maze->grid[i][j].wall, bottom);
            set_bit(&maze->grid[i][j].wall, left);
            set_bit(&maze->grid[i][j].wall, right);
        }
    }
}

// Empty spaces are on even coordinates
// Edges are on odd coordinates

void render_maze(maze* maze) {
    Color line_color = {50, 0, 60, 255};
    Color no_line_color = {255, 255, 255, 100};

    float h_g = GRID_SIZE / 2.0f;
    float line_thick = 5.0f;
    float h_l_t = line_thick / 2.0f;

    for(int i = 0; i < MAZE_WITDH; i++) {
        for(int j = 0; j < MAZE_HEIGHT; j++) {
            int2 cell_offset = (int2){i * GRID_SIZE + h_l_t, j * GRID_SIZE + h_l_t};

            Rectangle cell;
            cell.size = (float2){GRID_SIZE, GRID_SIZE};
            cell.pos = (float2){cell_offset.x + h_g, cell_offset.y + h_g};
            cell.col = (Color){100, 100, 150, 255};
            drawRectangle(cell);

        }
    }
    for(int i = 0; i < MAZE_WITDH; i++) {
        for(int j = 0; j < MAZE_HEIGHT; j++) {
            int2 cell_offset = (int2){i * GRID_SIZE + h_l_t, j * GRID_SIZE + h_l_t};

            // Lines
            Rectangle line;

            // Top
            line.size = (float2){GRID_SIZE, 5.0f};
            line.pos = (float2){cell_offset.x + (GRID_SIZE / 2.0f), cell_offset.y + h_l_t};
            if(check_bit(maze->grid[i][j].wall, top)) {
                line.col = line_color;
            } else {
                line.col = no_line_color;
            }
            drawRectangle(line);

            // Bottom
            line.size = (float2){GRID_SIZE, 5.0f};
            line.pos = (float2){cell_offset.x + (GRID_SIZE / 2.0f), cell_offset.y + GRID_SIZE - h_l_t};
            if(check_bit(maze->grid[i][j].wall, bottom)) {
                line.col = line_color;
            } else {
                line.col = no_line_color;
            }
            drawRectangle(line);

            // Left
            line.size = (float2){5.0f, GRID_SIZE};
            line.pos = (float2){cell_offset.x + h_l_t, cell_offset.y + (GRID_SIZE / 2.0f)};
            if(check_bit(maze->grid[i][j].wall, left)) {
                line.col = line_color;
            } else {
                line.col = no_line_color;
            }
            drawRectangle(line);

            // Right
            line.size = (float2){5.0f, GRID_SIZE};
            line.pos = (float2){cell_offset.x + GRID_SIZE - h_l_t, cell_offset.y + (GRID_SIZE / 2.0f)};
            if(check_bit(maze->grid[i][j].wall, right)) {
                line.col = line_color;
            } else {
                line.col = no_line_color;
            }
            drawRectangle(line);

        }
    }
}

bool check_wall(maze* maze, int2 pos, Direction dir) {
    uint8_t* cell = &maze->grid[pos.x][pos.y].wall;
    return check_bit(*cell, dir);
}

void set_wall_value(maze* maze, int2 pos, Direction dir, bool value) {
    int_box valid_box = {(int2){0, 0}, (int2){MAZE_WITDH - 1, MAZE_HEIGHT - 1}};
    if(!is_point_inside_box(pos, valid_box.tl, valid_box.br)) {
        printf("Invalid position passed: x:%d, y:%d\n", pos.x, pos.y);
        printf("the position must not have the coords: width: %d, height: %d\n", MAZE_WITDH, MAZE_HEIGHT);
        return;
    }

    uint8_t* cell = &maze->grid[pos.x][pos.y].wall;
    Direction op_dir;

    int2 new_pos = pos;
    switch(dir) {
        case top:
            new_pos.y -= 1;
            op_dir = bottom;
            break;
        case bottom:
            new_pos.y += 1;
            op_dir = top;
            break;
        case left:
            new_pos.x -= 1;
            op_dir = right;
            break;
        case right:
            new_pos.x += 1;
            op_dir = left;
            break;
        default:
            printf("Invalid direction passed: %d", dir);
    }

    if(value) { set_bit(cell, dir); } else { clear_bit(cell, dir); }
    if(is_point_inside_box(new_pos, valid_box.tl, valid_box.br)) {
        uint8_t* adyacent_cell = &maze->grid[new_pos.x][new_pos.y].wall;
        if(value) { set_bit(adyacent_cell, op_dir); } else { clear_bit(adyacent_cell, op_dir); }
    }
}

void toggle_wall(maze* maze, int2 pos, Direction dir) {
    set_wall_value(maze, pos, dir, !check_wall(maze, pos, dir));
}

void set_wall(maze* maze, int2 pos, Direction dir) {
    set_wall_value(maze, pos, dir, true);
}

void clear_wall(maze* maze, int2 pos, Direction dir) {
    set_wall_value(maze, pos, dir, false);
}


// Carve maze
void gen_maze(maze* maze, int* step) {
    int2 pos;

    srand(time(NULL));

    uint8_t* cell;
    Direction rand_dir;

    gen_valid_rand(&pos.x, 0, MAZE_WITDH - 1);
    gen_valid_rand(&pos.y, 0, MAZE_WITDH - 1);

    rand_dir = rand() % 4;

    toggle_wall(maze, pos, rand_dir);
    step++;
}


/* Main Functions */
void updateScene(double deltaTime, double* time_elapsed, maze* maze, int* step) {
    if(*time_elapsed >= 0.05) {
        gen_maze(maze, step);
        *time_elapsed = 0.0;
    }
    return;
}

// Exmplae to make a trail for a body
// Probably gonna change to a easier way to make trails for bodies
// Trail bodyTrail;

void renderScene(GLFWwindow* window, maze* maze) {
    //glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    triangleBuffer.count = 0;

    // Test
    // Rectangle a = {{200.0f, 204.0f}, {50.0f, 100.0f}, {255, 255, 0, 255}};
    // drawRectangle(a);
    render_maze(maze);

    sendTrianglesToGPU();

    glfwSwapBuffers(window);
}

void gameLoop(GLFWwindow* window, maze* maze) {
    double lastTime = glfwGetTime();

    double time_elapsed = 0;
    int step = 0;

    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        float deltaTime = (float)(currentTime - lastTime);
        lastTime = currentTime;

        time_elapsed += deltaTime;
        printf("Time elapsed: %lf\n", time_elapsed);

        glfwPollEvents();

        updateScene(deltaTime, &time_elapsed, maze, &step);
        renderScene(window, maze);
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

    maze actual_maze;
    initialize_maze(&actual_maze);

    gameLoop(window, &actual_maze);
    glfwTerminate();


    return EXIT_SUCCESS;
}

