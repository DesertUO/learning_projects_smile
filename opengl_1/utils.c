#include "utils.h"

void crash(const char* s) {
    fprintf(stderr, s);

    printf("Press a Enter to continue...");
    getchar();

    exit(EXIT_FAILURE);
}

const char* load_file_as_string(const char* filename) {
    FILE *file;
    long length;
    char *buffer;

    file = fopen(filename, "rb");

    if (!file) {
        perror("Error opening file");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (length == -1) {
        perror("Error getting file size");
        fclose(file);
        return NULL;
    }

    buffer = (char*)malloc(length + 1);

    if (!buffer) {
        perror("Error allocating memory");
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(buffer, 1, length, file);
    fclose(file);

    if (read_size != length) {
        fprintf(stderr, "Error reading file content\n");
        free(buffer);
        return NULL;
    }

    buffer[length] = '\0';

	return (const char*)buffer;
}
