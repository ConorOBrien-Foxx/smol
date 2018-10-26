#include "file.h"

char* read_file(char* name, size_t* out_size) {
    // from https://stackoverflow.com/a/3747128/4119004
    FILE* file = fopen(name, "rb");
    
    if(!file) {
        fprintf(stderr, "Error opening file `%s'.\n", name);
        perror(name);
        return NULL;
    }
    
    fseek(file, 0L, SEEK_END);
    size_t size = ftell(file);
    rewind(file);
    
    char* res = malloc((size + 1) * sizeof(char));
    if(!res) {
        fclose(file);
        fprintf(stderr, "Error allocating memory for file `%s'.\n", name);
        return NULL;
    }
    if(fread(res, size, sizeof(char), file) != 1) {
        fclose(file);
        free(res);
        fprintf(stderr, "Error reading file `%s'.\n", name);
        return NULL;
    }
    
    fclose(file);
    
    *out_size = size;
    res[size] = '\0';
    
    return res;
}