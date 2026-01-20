#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

void crash(const char* s);

const char* load_file_as_string(const char* filename);

void set_bit(uint8_t* byte, uint8_t pos);

void clear_bit(uint8_t* byte, uint8_t pos);

void toggle_bit(uint8_t* byte, uint8_t pos);

bool check_bit(uint8_t byte, uint8_t pos);
