#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <GL/glew.h>

#include <GLFW/glfw3.h>

#include <cglm/cglm.h>

// Ye
#include "utils.h"
#include "vector.h"

// Constants or macros idk

#define MAX_TRIANGLES INT_MAX/2000
const int MAX_TRAIL_POINTS = INT_MAX/5000;

#define FLOAT2_TO_VEC2(dest, f) glm_vec2_copy((vec2){(f).x, (f).y}, dest) // gipiti
#define VEC2_TO_FLOAT2(v) ((float2){(v)[0], (v)[1]})

const GLuint WIDTH = 1280; const GLuint HEIGHT = 720;
const uint16_t CIRC_RES = 32;

GLuint triangleVAO, triangleVBO;
GLuint shaderProgram;

#define G 667.4f

typedef struct EngineSettings {
    bool enableBodyGravity;
    bool enableCollisions;
    bool enableWorldBoxGravity;
    bool enableWorldBoxPhysicsBox;
} EngineSettings;

/* Utils */

typedef vec2 pos2;
typedef vec3 pos3;

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

typedef struct float3 {
    float x;
    float y;
    float z;
} float3;

typedef struct float4 {
    float x;
    float y;
    float z;
    float w;
} float4;

typedef ivec2 int2;
typedef ivec3 int3;
typedef ivec4 int4;

typedef struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} Color;

typedef struct Circle {
    float r;
    float2 pos;
    Color col;
} Circle;

typedef struct Rectangle {
    float2 pos;
    float2 size;
    Color col;
} Rectangle;

typedef struct Line {
    float2 a;
    float2 b;
    float t;
    Color col;
} Line;

typedef struct PhysicBody {
    Circle circ;
    float2 vel;
    float mass;
    bool Static;
} PhysicBody;

VECTOR_DEFINE(PhysicBody)

typedef struct Trail {
    Vector_float2 points;
    Color col;
} Trail;

void initTrail(Trail* trail) {
    init_vector_float2(&trail->points);
}

void freeTrail(Trail* trail) {
    free_vector_float2(&trail->points);
}

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

const float2 gravity_acc = {0.0f, 981.0f};

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

void drawLine(Line line) {
    float2 dir = {line.b.x - line.a.x, line.b.y - line.a.y};
    float len = sqrtf(dir.x*dir.x + dir.y*dir.y);
    if(len == 0.0f) return;
    dir.x /= len;
    dir.y /= len;

    float2 perp = {-dir.y, dir.x};

    float2 offset = {perp.x * line.t / 2.0f, perp.y * line.t / 2.0f};

    float2 fA1 = {line.a.x + offset.x, line.a.y + offset.y};
    float2 fA2 = {line.a.x - offset.x, line.a.y - offset.y};
    float2 fB1 = {line.b.x + offset.x, line.b.y + offset.y};
    float2 fB2 = {line.b.x - offset.x, line.b.y - offset.y};

    Triangle tri1 = {{{fA1, line.col}, {fB1, line.col}, {fA2, line.col}}};
    Triangle tri2 = {{{fA2, line.col}, {fB1, line.col}, {fB2, line.col}}};

    drawTriangle(&tri1);
    drawTriangle(&tri2);
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

void updateTrail(Trail* trail, float2 pos) {
    append_vector_float2(&trail->points, pos);

    while(trail->points.length > MAX_TRAIL_POINTS) {
        shift_vector_float2(&trail->points);
    }
}

void drawTrail(Trail* trail, float thickness) {
    for(int i = 0; i < trail->points.length - 1; i++) {
        Line line = {trail->points.data[i], trail->points.data[i+1], thickness, trail->col};
        drawLine(line);
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

bool checkAABB(float2 point, Rectangle rect) {
    float halfW = rect.size.x / 2.0f;
    float halfH = rect.size.y / 2.0f;

    if (point.x < rect.pos.x - halfW || point.x > rect.pos.x + halfW)
        return false;
    if (point.y < rect.pos.y - halfH || point.y > rect.pos.y + halfH)
        return false;

    return true;
}

bool checkAABBCircRect(Circle circ, Rectangle rect) {
    float closestX = fmax(rect.pos.x, fmin(circ.pos.x, rect.pos.x + rect.size.x));
    float closestY = fmax(rect.pos.y, fmin(circ.pos.y, rect.pos.y + rect.size.y));

    float dx = circ.pos.x - closestX;
    float dy = circ.pos.y - closestY;

    return (dx*dx + dy*dy) <= (circ.r * circ.r);
}

bool checkAABBCircCirc(Circle a, Circle b) {
    float dx = b.pos.x - a.pos.x;
    float dy = b.pos.y - a.pos.y;
    float distSqr = dx*dx + dy*dy;
    float radiusSum = a.r + b.r;
    return distSqr <= radiusSum * radiusSum;
}

bool checkAABBCircAllInRect(Circle circ, Rectangle rect) {
    if (circ.pos.x - circ.r < rect.pos.x) return false;                 // left edge
    if (circ.pos.x + circ.r > rect.pos.x + rect.size.x) return false;   // right edge
    if (circ.pos.y - circ.r < rect.pos.y) return false;                 // top edge
    if (circ.pos.y + circ.r > rect.pos.y + rect.size.y) return false;   // bottom edge

    return true;
}

void resolveCircleCollision(PhysicBody* a, PhysicBody* b) {
    float2 delta = float2_sub(b->circ.pos, a->circ.pos);
    float dist = sqrtf(delta.x*delta.x + delta.y*delta.y);
    float penetration = (a->circ.r + b->circ.r) - dist;
    if(penetration > 0 && dist > 1e-6f) {
        float2 correction = float2_mul(delta, penetration / dist / 2.0f);
        if(!a->Static) a->circ.pos = float2_sub(a->circ.pos, correction);
        if(!b->Static) b->circ.pos = float2_add(b->circ.pos, correction);

        float2 normal = float2_normalize(delta);
        float2 relativeVel = float2_sub(b->vel, a->vel);
        float velAlongNormal = relativeVel.x*normal.x + relativeVel.y*normal.y;
        if(velAlongNormal > 0) return;

        float e = 1.0f;
        float j = -(1 + e) * velAlongNormal;
        j /= (a->Static ? 0 : 1/a->mass) + (b->Static ? 0 : 1/b->mass);

        float2 impulse = float2_mul(normal, j);
        if(!a->Static) a->vel = float2_sub(a->vel, float2_mul(impulse, 1/a->mass));
        if(!b->Static) b->vel = float2_add(b->vel, float2_mul(impulse, 1/b->mass));
    }
}

void applyGravityToBodies(Vector_PhysicBody* bodies, double deltaTime, Vector_float2 accelerations) {
    for(int i = 0; i < bodies->length; i++) {
        if(bodies->data[i].Static) continue;
        for(int j = 0; j < bodies->length; j++) {
            if(i == j) continue;
            float2 r = float2_sub(bodies->data[j].circ.pos, bodies->data[i].circ.pos);
            float distSqr = r.x*r.x + r.y*r.y;
            float dist = sqrtf(distSqr);
            if(dist < 1e-3) continue;

            float forceMag = (G * bodies->data[j].mass) / distSqr;
            float2 forceDir = float2_normalize(r);
            accelerations.data[i] = float2_add(accelerations.data[i], float2_mul(forceDir, forceMag));
        }
    }
}

void resolveWorldBoxCollisions(Vector_PhysicBody* bodies) {
    for(int i = 0; i < bodies->length; i++) {
        Circle* circ = &bodies->data[i].circ;

        if(!checkAABBCircAllInRect(*circ, screenBox)) {
            // Left wall
            if(circ->pos.x - circ->r < screenBox.pos.x) {
                circ->pos.x = screenBox.pos.x + circ->r;
                bodies->data[i].vel.x *= -0.7f;
            }
            // Right wall
            if(circ->pos.x + circ->r > screenBox.pos.x + screenBox.size.x) {
                circ->pos.x = screenBox.pos.x + screenBox.size.x - circ->r;
                bodies->data[i].vel.x *= -0.7f;
            }
            // Top wall
            if(circ->pos.y - circ->r < screenBox.pos.y) {
                circ->pos.y = screenBox.pos.y + circ->r;
                bodies->data[i].vel.y *= -0.7f;
            }
            // Bottom wall
            if(circ->pos.y + circ->r > screenBox.pos.y + screenBox.size.y) {
                circ->pos.y = screenBox.pos.y + screenBox.size.y - circ->r;
                bodies->data[i].vel.y *= -0.7f;
            }
        }
    }
}

void updateBodiesPosition(Vector_PhysicBody* bodies, double deltaTime, EngineSettings* engineSettings) {
    Vector_float2 accelerations;
    init_vector_float2(&accelerations);

    for(int i = 0; i < bodies->length; i++) {
        append_vector_float2(&accelerations, (float2){0,0});
    }


    // Apply gravity between bodies
    if(engineSettings->enableBodyGravity)
        applyGravityToBodies(bodies, deltaTime, accelerations);

    // Apply downward gravity to all non-static bodies
    if(engineSettings->enableWorldBoxGravity) {
        for(int i = 0; i < bodies->length; i++) {
            if(bodies->data[i].Static) continue;
            accelerations.data[i] = float2_add(accelerations.data[i], gravity_acc);
        }
    }

    // Update positions and velocities
    for(int i = 0; i < bodies->length; i++) {
        if(bodies->data[i].Static) continue;

        bodies->data[i].vel = float2_add(bodies->data[i].vel, float2_mul(accelerations.data[i], deltaTime));

        float2 newPos = float2_add(bodies->data[i].circ.pos, float2_mul(bodies->data[i].vel, deltaTime));
        bodies->data[i].circ.pos = newPos;
    }

    // Collision detection and resolution
    if(engineSettings->enableCollisions) {
        for(int i = 0; i < bodies->length; i++) {
            for(int j = i+1; j < bodies->length; j++) {
                resolveCircleCollision(&bodies->data[i], &bodies->data[j]);
            }
        }
    }

    // World box collision
    if(engineSettings->enableWorldBoxPhysicsBox) {
        resolveWorldBoxCollisions(bodies);
    }

    free_vector_float2(&accelerations);
}

/* Main Functions */
void updateScene(double deltaTime, Vector_PhysicBody* bodies, EngineSettings* engineSettings) {

    /*
    for(int i= 0 ; i < bodies->length; i++) {
        updateBodyPosition(&bodies->data[i], deltaTime);
    }
    */
    updateBodiesPosition(bodies, deltaTime, engineSettings);
}

// Exmplae to make a trail for a body
// Probably gonna change to a easier way to make trails for bodies
// Trail bodyTrail;

void renderScene(GLFWwindow* window, Vector_PhysicBody* bodies) {
    //glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    triangleBuffer.count = 0;

    // To update and render the trail, prob gonna change it too
    // updateTrail(&bodyTrail, bodies->data[0].circ.pos);
    // drawTrail(&bodyTrail, 2.0f);

    for(int i = 0 ; i < bodies->length; i++) {
        drawCircle(bodies->data[i].circ.r, bodies->data[i].circ.pos, bodies->data[i].circ.col);
    }

    // Test
    // Rectangle a = {{200.0f, 204.0f}, {50.0f, 100.0f}, {255, 255, 0, 255}};
    // drawRectangle(a);

    sendTrianglesToGPU();

    glfwSwapBuffers(window);
}

void gameLoop(GLFWwindow* window, Vector_PhysicBody* bodies, EngineSettings* engineSettings) {
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        float deltaTime = (float)(currentTime - lastTime);
        lastTime = currentTime;

        glfwPollEvents();

        updateScene(deltaTime, bodies, engineSettings);
        renderScene(window, bodies);
    }
}

void generateBodies(Vector_PhysicBody* bodies, int count) {
    float mass = (float)(rand() % 500 + 100);
    float radius = sqrtf(mass) * 0.5f;

    Vector_float2 positions;
    init_vector_float2(&positions);

    for(int i = 0; i < count; i++) {
        float2 pos = {
            (float)(rand() % WIDTH),
            (float)(rand() % HEIGHT)
        };

        if(positions.length > 0) {
            bool overlapping;
            do {
                overlapping = false;
                for(int j = 0; j < positions.length; j++) {
                    float2 existingPos = positions.data[j];
                    float dx = pos.x - existingPos.x;
                    float dy = pos.y - existingPos.y;
                    float distSqr = dx*dx + dy*dy;
                    float minDist = radius * 2.0f;
                    if(distSqr < minDist * minDist) {
                        overlapping = true;
                        pos = (float2){
                            (float)(rand() % WIDTH),
                            (float)(rand() % HEIGHT)
                        };
                        break;
                    }
                }
            } while(overlapping);
        }

        append_vector_float2(&positions, pos);

        if(pos.x > WIDTH - radius) pos.x = WIDTH - radius;
        if(pos.x < radius) pos.x = radius;

        if(pos.y > HEIGHT - radius) pos.y = HEIGHT - radius;
        if(pos.y < radius) pos.y = radius;

        Color col = {
            (uint8_t)(rand() % 256),
            (uint8_t)(rand() % 256),
            (uint8_t)(rand() % 256),
            255
        };

        PhysicBody body = {
            { radius, pos, col },
            { 0.0f, 0.0f },
            mass,
            false
        };

        append_vector_PhysicBody(bodies, body);
    }
}

int main(int argc, const char * argv[]) {
    EngineSettings engineSettings = {
        false,
        false,
        false,
        true
    };

    GLFWwindow* window = initialize();
    initTriangleRenderer(&triangleVAO, &triangleVBO);

	const char* vertSrc = load_file_as_string("Shaders/triangle_shader.vert");
	const char* fragSrc = load_file_as_string("Shaders/triangle_shader.frag");

	shaderProgram = createShaderProgram(vertSrc, fragSrc);

    free((void*)vertSrc);
    free((void*)fragSrc);

    // Example to make a body
    /*
    float mass = 900.0f;
    float radius = 15.0f;

    const Color body_color = {252, 229, 112, 255};
    float2 body_pos = {640.0f, 260.0f};
    PhysicBody body = {
        (Circle){
            radius,
            body_pos,
            body_color
        },
        {0.0f, 0.0f},
        mass,
        false
    };

    initTrail(&bodyTrail);
    bodyTrail.col = (Color){100, 200, 120, 100};
    */


    Vector_PhysicBody Sim_Bodies;
    init_vector_PhysicBody(&Sim_Bodies);

    // append_vector_PhysicBody(&Sim_Bodies, body_1);

    // Do not try more than like
    generateBodies(&Sim_Bodies, 1000);

    gameLoop(window, &Sim_Bodies, &engineSettings);
    glfwTerminate();

    free_vector_PhysicBody(&Sim_Bodies);

    return EXIT_SUCCESS;
}
