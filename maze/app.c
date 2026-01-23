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

bool contains_vector_int2(Vector_int2* vector, int2 value) {
    for(int i = 0; i < vector->length; i++) {
        if(vector->data[i].x == value.x && vector->data[i].y == value.y) { return true; }
    }
    return false;
}

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

// Sets the bounding box for the mazes points
int_box valid_box = {(int2){0, 0}, (int2){MAZE_WITDH - 1, MAZE_HEIGHT - 1}};

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

void drawCircle(float r, float2 pos, Color col) {
    float theta = (2 * M_PI) / CIRC_RES;

    for(int i = 0; i < CIRC_RES; i++) {
        float theta_i = theta*i;
        float theta_i_2 = theta * (i+1);

        float2 pos_theta_i = {r * cos(theta_i), r * sin(theta_i)};
        float2 pos_theta_i2 = {r * cos(theta_i_2), r * sin(theta_i_2)};

        float2 final_pos_i = {pos.x + pos_theta_i.x, pos.y + pos_theta_i.y};
        float2 final_pos_i2 = {pos.x + pos_theta_i2.x, pos.y + pos_theta_i2.y};

        Triangle fragCircle = {
            {
                {pos, col},
                {final_pos_i, col},
                {final_pos_i2, col}
            }
        };

        drawTriangle(&fragCircle);
    }
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
void gen_valid_rand_num(int* num, int min, int max) {
    *num = rand() % (max + 1);
    while(*num < min) {
        *num = rand() % (max + 1);
    }
}

void choose_rand_num(int* num, int a, int b) {
    int choice;
    gen_valid_rand_num(&choice, 0, 1);
    *num = choice ? a : b;
}

// Is inside inclusive with edges
bool is_point_inside_box(int2 point, int2 box_tl, int2 box_br) {
    return (point.x >= box_tl.x && point.x <= box_br.x) && (point.y >= box_tl.y && point.y <= box_br.y);
}

typedef enum Direction {
    top,
    bottom,
    left,
    right,
    none
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
    bool visited;
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
            maze->grid[i][j].visited = false;
        }
    }
}

// Empty spaces are on even coordinates
// Edges are on odd coordinates

const float h_g = GRID_SIZE / 2.0f;
const float line_thick = 5.0f;
const float h_l_t = line_thick / 2.0f;

bool is_point_in_maze(int2 point) {
    return is_point_inside_box(point, valid_box.tl, valid_box.br);
}

void render_maze(maze* maze, Vector_int2* visited_pos) {
    Color line_color = {50, 0, 60, 255};
    Color no_line_color = {255, 255, 255, 000};
    Color unvisited_cell_col = {0, 0, 0, 255};
    Color visited_cell_col = {50, 50, 150, 255};
    Color final_cell_col = {100, 100, 150, 255};


    for(int i = 0; i < MAZE_WITDH; i++) {
        for(int j = 0; j < MAZE_HEIGHT; j++) {
            int2 pos = {i, j};
            int2 cell_offset = (int2){i * GRID_SIZE + h_l_t, j * GRID_SIZE + h_l_t};

            Rectangle cell;
            cell.size = (float2){GRID_SIZE, GRID_SIZE};
            cell.pos = (float2){cell_offset.x + h_g, cell_offset.y + h_g};
            if(contains_vector_int2(visited_pos, pos)) {
                cell.col = final_cell_col;
            } else if(maze->grid[i][j].visited == true) {
                cell.col = visited_cell_col;
            } else {
                cell.col = unvisited_cell_col;
            }

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

void set_wall_value(maze* maze, int2 pos, Direction dir, bool value) {
    if(!is_point_in_maze(pos)) {
        printf("Invalid position passed: x:%d, y:%d\n", pos.x, pos.y);
        printf("the position must not have the coords: width: %d, height: %d\n", MAZE_WITDH, MAZE_HEIGHT);
        return;
    }

    uint8_t* cell = &maze->grid[pos.x][pos.y].wall;
    Direction op_dir;

    int2 adyacent_pos = pos;
    switch(dir) {
        case top:
            adyacent_pos.y -= 1;
            op_dir = bottom;
            break;
        case bottom:
            adyacent_pos.y += 1;
            op_dir = top;
            break;
        case left:
            adyacent_pos.x -= 1;
            op_dir = right;
            break;
        case right:
            adyacent_pos.x += 1;
            op_dir = left;
            break;
        default:
            printf("Invalid direction passed: %d", dir);
    }

    if(value) { set_bit(cell, dir); } else { clear_bit(cell, dir); }
    if(is_point_in_maze(adyacent_pos)) {
        uint8_t* adyacent_cell = &maze->grid[adyacent_pos.x][adyacent_pos.y].wall;
        if(value) { set_bit(adyacent_cell, op_dir); } else { clear_bit(adyacent_cell, op_dir); }
    }
}

void set_wall(maze* maze, int2 pos, Direction dir) {
    set_wall_value(maze, pos, dir, true);
}

void clear_wall(maze* maze, int2 pos, Direction dir) {
    set_wall_value(maze, pos, dir, false);
}

bool check_wall(maze* maze, int2 pos, Direction dir) {
    uint8_t* cell = &maze->grid[pos.x][pos.y].wall;
    return check_bit(*cell, dir);
}

void choose_pos(int2* walker_pos) {
    int2 pos;
    gen_valid_rand_num(&pos.x, 0, MAZE_WITDH - 1);
    gen_valid_rand_num(&pos.y, 0, MAZE_WITDH - 1);
    *walker_pos = pos;
}


void choose_dir(Direction* dir) {
    *dir = rand() % 4;
}

void get_oppositide_dir(Direction dir, Direction* op) {
    if(dir == top) { *op = bottom; return;  }
    if(dir == bottom) { *op = top; return; }
    if(dir == left) { *op = left; return; }
    if(dir == right) { *op = right; return; }
}

void choose_point_outside_ring(int2* pos) {
    int2 point;

    bool axis = rand() % 2;

    if(axis) {
        choose_rand_num(&point.x, 0, MAZE_WITDH - 1);
        gen_valid_rand_num(&point.y, 0, MAZE_HEIGHT - 1);
    } else {
        gen_valid_rand_num(&point.x, 0, MAZE_WITDH - 1);
        choose_rand_num(&point.y, 0, MAZE_HEIGHT - 1);
    }

    *pos = point;
}

void get_outside_wall_from_outside_ring_point(int2 pos, Direction* wall) {
    Direction side;
    if(pos.x == 0) { side = left; }
    if(pos.x == (MAZE_WITDH - 1)) { side = right; }
    if(pos.y == 0) { side = top; }
    if(pos.y == (MAZE_HEIGHT - 1)) { side = bottom; }

    *wall = side;
}

bool is_outside_ring_wall(maze* maze, int2 pos, Direction dir) {
    if(!is_point_in_maze(pos)) {
        printf("Invalid position has been passed to is_border__wall(...)\n");
        return false;
    }

    if(pos.y == 0 && dir == top) { return true; }
    if(pos.y == (MAZE_HEIGHT - 1) && dir == bottom) { return true; }
    if(pos.x == 0 && dir == left) { return true; }
    if(pos.x == (MAZE_WITDH - 1) && dir == right) { return true; }

    return false;
}


bool is_point_outside_ring(int2 pos) {
    if(!is_point_in_maze(pos)) {
        printf("Invalid position has been passed to is_border__wall(...)\n");
        return false;
    }

    if(pos.y == 0) { return true; }
    if(pos.y == (MAZE_HEIGHT - 1)) { return true; }
    if(pos.x == 0) { return true; }
    if(pos.x == (MAZE_WITDH - 1)) { return true; }

    return false;
}

typedef struct Walker {
    int2 head;
    Direction dir;
} Walker;


void dir_from_adyacent_ab(int2 a, int2 b, Direction* dir) {
    int2 diff = (int2){b.x - a.x, b.y - a.y};
    Direction dir_;
    if(diff.x == 0 && diff.y != 0) {
        if(diff.y < 0) { *dir = top; }
        else { *dir = bottom; }
    } else if(diff.x != 0 && diff.y == 0) {
        if(diff.x < 0) { *dir = left; }
        else { *dir = right; }
    } else { *dir =  none; }
}

void dir_from_walker_to_walker(Walker walker_a, Walker walker_b, Direction* dir) {
    dir_from_adyacent_ab(walker_a.head, walker_b.head, dir);
}

void predict_new_pos(Walker walker, int2* prediction) {
    int2 new_pos = walker.head;
    switch(walker.dir) {
        case top:
            new_pos.y -= 1;
            break;
        case bottom:
            new_pos.y += 1;
            break;
        case left:
            new_pos.x -= 1;
            break;
        case right:
            new_pos.x += 1;
            break;
        default:
            printf("hmmm.... no direction for walker given...\n");
            break;
    }

    *prediction =new_pos;
}

void predict_new_pos_in_dir(Walker walker, Direction dir, int2* prediction) {
    int2 new_pos = walker.head;
    switch(dir) {
        case top:
            new_pos.y -= 1;
            break;
        case bottom:
            new_pos.y += 1;
            break;
        case left:
            new_pos.x -= 1;
            break;
        case right:
            new_pos.x += 1;
            break;
        default:
            printf("hmmm.... no direction for walker given...\n");
            break;
    }

    *prediction =new_pos;
}

bool move_walker_in_dir(Walker* walker) {
    int2 new_pos = walker->head;
    switch(walker->dir) {
        case top:
            new_pos.y -= 1;
            break;
        case bottom:
            new_pos.y += 1;
            break;
        case left:
            new_pos.x -= 1;
            break;
        case right:
            new_pos.x += 1;
            break;
        default:
            printf("hmmm.... no direction for walker given...\n");
            break;
    }
    if(!is_point_in_maze(new_pos)) { return false; }

    walker->head = new_pos;
    return true;
}

bool is_pointing_wall(maze* maze, Walker walker) {
    return check_wall(maze, walker.head, walker.dir);
}

bool can_move_walker_in_dir(maze* maze, Walker* walker) {
    int2 new_pos = walker->head;
    switch(walker->dir) {
        case top:
            new_pos.y -= 1;
            break;
        case bottom:
            new_pos.y += 1;
            break;
        case left:
            new_pos.x -= 1;
            break;
        case right:
            new_pos.x += 1;
            break;
        default:
            printf("hmmm.... no direction for walker given...\n");
            break;
    }
    if(!is_point_in_maze(new_pos)) { return false; }
    if(maze->grid[new_pos.x][new_pos.y].visited == true) { return false; }

    return true;
}

bool can_move_walker_in_visited(maze* maze, Walker* walker, Vector_int2* visited_pos) {
    int2 new_pos = walker->head;
    switch(walker->dir) {
        case top:
            new_pos.y -= 1;
            break;
        case bottom:
            new_pos.y += 1;
            break;
        case left:
            new_pos.x -= 1;
            break;
        case right:
            new_pos.x += 1;
            break;
        default:
            printf("hmmm.... no direction for walker given...\n");
            break;
    }
    if(!is_point_in_maze(new_pos) ||
       maze->grid[new_pos.x][new_pos.y].visited == false ||
       check_wall(maze, walker->head, walker->dir) ||
       contains_vector_int2(visited_pos, new_pos)) {
        return false;
    }

    return true;
}

bool is_enclosed_by_visited_pos(maze* maze, Walker walker) {
    int2 predicted_pos;
    Walker tmp_walker = walker;
    for(int i = 0; i < 4; i++) {
        tmp_walker.dir = i;
        predict_new_pos(tmp_walker, &predicted_pos);
        if(is_point_in_maze(predicted_pos)) {
            if(maze->grid[predicted_pos.x][predicted_pos.y].visited == false) {
                return false;
            }
        }
    }
    return true;
}

void backtrack_walker(maze* maze, Walker* walker, Walker* prev_walker, Vector_int2* visited_pos) {
    int2 predicted_pos;
    predict_new_pos(*walker, &predicted_pos);

    int choosed_num = 0;
    while(!can_move_walker_in_visited(maze, walker, visited_pos)) {
        if(choosed_num >= 4) { return; }
        printf("Choosing new dir to backtrack..., prev: %d\n", walker->dir);
        choose_dir(&walker->dir);
        choosed_num++;
    }
    if(maze->grid[predicted_pos.x][predicted_pos.y].visited == true)
        append_vector_int2(visited_pos, walker->head);
    move_walker_in_dir(walker);
}

// Recursive backtracking
void gen_maze(maze* maze, Walker* walker, Walker* prev_walker, int step, Vector_int2* visited_pos, bool* backtrack_nearest, bool* end_gen) {
    int2 tmp_pos;
    Direction tmp_dir;
    Direction tmp_dir_2;

    srand(time(NULL));

    printf("backtrack_nearest: %d\n", *backtrack_nearest);
    if(*backtrack_nearest) {
        printf("backtracking to nearest...\n");
        backtrack_walker(maze, walker, prev_walker, visited_pos);
        return;
    }


    // If the step if the 0th or the 1st then add the entrances and put the
    // walker on the maze
    if(step < 2) {
        do {
            choose_point_outside_ring(&tmp_pos);
            get_outside_wall_from_outside_ring_point(tmp_pos, &tmp_dir);
        } while(!check_wall(maze, tmp_pos, tmp_dir));

        clear_wall(maze, tmp_pos, tmp_dir);

        if(step < 1) {
            walker->head = tmp_pos;
            maze->grid[tmp_pos.x][tmp_pos.y].visited = true;
        }
        return;
    }

    // If this is an actual step then make the walker walk and open walls
    Walker tmp_walker = *walker;
    int2 predicted_pos;
    do {
        if(is_enclosed_by_visited_pos(maze, *walker)) {
            *backtrack_nearest = true;
            return;
        }
        do {
            choose_dir(&tmp_dir);
            dir_from_walker_to_walker(*walker, *prev_walker, &tmp_dir_2);
        } while(tmp_dir_2 == tmp_dir);


        walker->dir = tmp_dir;
        predict_new_pos(*walker, &predicted_pos);

    } while(!can_move_walker_in_dir(maze, walker));
    move_walker_in_dir(walker);
    maze->grid[predicted_pos.x][predicted_pos.y].visited = true;


    *prev_walker = tmp_walker;
    clear_wall(maze, prev_walker->head, tmp_dir);
}


/* Main Functions */
void updateScene(double deltaTime, double* time_elapsed, maze* maze, int* step, Walker* walker, Walker* prev_walker, Vector_int2* visited_pos, bool* backtrack_nearest, bool* end_gen) {
    if(*time_elapsed >= 0.05 && !*end_gen) {
        printf("Step: %d\n", *step);
        gen_maze(maze, walker, prev_walker, *step, visited_pos, backtrack_nearest, end_gen);
        *time_elapsed = 0.0;
        *step = *step + 1;
    }
    if(*end_gen) {
        printf("Maze generation stopped...\n");
    }
    return;
}

// Exmplae to make a trail for a body
// Probably gonna change to a easier way to make trails for bodies
// Trail bodyTrail;

void renderScene(GLFWwindow* window, maze* maze, Walker walker, Walker prev_walker, Vector_int2* visited_pos) {
    //glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    triangleBuffer.count = 0;

    // Test
    // Rectangle a = {{200.0f, 204.0f}, {50.0f, 100.0f}, {255, 255, 0, 255}};
    // drawRectangle(a);
    render_maze(maze, visited_pos);

    float walker_r = 25.0f;
    float2 walker_pos = {walker.head.x * GRID_SIZE + h_g + h_l_t, walker.head.y * GRID_SIZE + h_g + h_l_t};
    float2 prev_walker_pos = {prev_walker.head.x * GRID_SIZE + h_g + h_l_t, prev_walker.head.y * GRID_SIZE + h_g + h_l_t};

    drawCircle(walker_r, prev_walker_pos, (Color){200, 200, 100, 255});
    drawCircle(walker_r, walker_pos, (Color){255, 0, 0, 255});

    sendTrianglesToGPU();

    glfwSwapBuffers(window);
}

void gameLoop(GLFWwindow* window, maze* maze) {
    double lastTime = glfwGetTime();

    double time_elapsed = 0;
    int step = 0;

    int2 head = {-1, -1};
    Walker walker = {head};
    Walker prev_walker = {head};

    Vector_int2 visited_pos;
    init_vector_int2(&visited_pos);

    bool backtrack_nearest = false;
    bool end_gen = false;

    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        float deltaTime = (float)(currentTime - lastTime);
        lastTime = currentTime;

        time_elapsed += deltaTime;

        glfwPollEvents();

        updateScene(deltaTime, &time_elapsed, maze, &step, &walker, &prev_walker, &visited_pos, &backtrack_nearest, &end_gen);
        renderScene(window, maze, walker, prev_walker, &visited_pos);
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

