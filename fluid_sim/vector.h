#pragma once
#include<stdlib.h>
#include<stdio.h>

#define VECTOR_DEFINE(T)                                                                                                \
typedef struct Vector_##T {T* data; size_t length; size_t capacity; } Vector_##T;                                       \
static inline void init_vector_##T(Vector_##T* v) { v->data = NULL; v->length = 0; v->capacity = 0;}                    \
static inline void free_vector_##T(Vector_##T* v) {free(v->data); v->data = NULL; v->length = 0; v->capacity = 0;}      \
                                                                                                                        \
static inline void append_vector_##T(Vector_##T* v, T value) {                                                          \
    if(v->length >= v->capacity) {                                                                                      \
        size_t new_cap = v->capacity ? v->capacity * 2 : 2;                                                             \
        T* p = realloc(v->data, new_cap * sizeof(T));                                                                   \
        assert(p && "realloc failed");                                                                                  \
        v->data = p;                                                                                                    \
        v->capacity = new_cap;                                                                                          \
    }                                                                                                                   \
                                                                                                                        \
    v->data[v->length++] = value;                                                                                       \
}                                                                                                                       \
static inline T pop_vector_##T(Vector_##T* v) {                                                                         \
    assert(v->length > 0 && "Vector must have elements to pop");                                                        \
    return v->data[--v->length];                                                                                        \
}                                                                                                                       \
                                                                                                                        \
static inline void shift_vector_##T(Vector_##T* v) {                                                                    \
    assert(v->length > 0 && "Vector must have elements to shift");                                                      \
    memmove(&v->data[0], &v->data[1], (v->length - 1) * sizeof(T));                                                     \
    v->length--;                                                                                                        \
}                                                                                                                       \
                                                                                                                        \
static inline void unshift_vector_##T(Vector_##T* v, T value) {                                                         \
    if(v->length >= v->capacity) {                                                                                      \
        size_t new_cap = v->capacity ? v->capacity * 2 : 2;                                                             \
        T* p = realloc(v->data, new_cap * sizeof(T));                                                                   \
        assert(p && "realloc failed");                                                                                  \
        v->data = p;                                                                                                    \
        v->capacity = new_cap;                                                                                          \
    }                                                                                                                   \
    memmove(&v->data[1], &v->data[0], v->length * sizeof(T));                                                           \
    v->data[0] = value;                                                                                                 \
    v->length++;                                                                                                        \
}
