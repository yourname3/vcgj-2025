#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

void
usage() {
    puts("usage: shader2c <inputs...> <output.c> <output.h>");
}

bool
is_invalid(char c) {
    if (c >= 'a' && c <= 'z') return false;
    if (c >= 'A' && c <= 'Z') return false;
    if (c >= '0' && c <= '9') return false; // Just assume these aren't at the front..
    if (c == '_' || c == '$') return false;
    return true;
}

char*
mangle_path(char *path) {
    char *name = NULL;
    if((name = strrchr(path, '/')) != NULL) {
        name++; // One past the last slash in the path.
    }
    else {
        // No slash, keep the path name.
        name = path;
    }

    char *ext = strrchr(path, '.');
    if(ext != NULL) {
        // Delete the extension from the path.
        *ext = '\0';
    }

    // Finally, replace any un-variable-le character with an underscore.
    for(size_t i = 0; name[i]; ++i) {
        if(is_invalid(name[i])) {
            name[i] = '_';
        }
    }

    return name;
}

void
convert(FILE *input, FILE *out_c, FILE *out_h, char *path) {
    // First, modify the "path" so that we get a valid C name for the shader text.
    char *mangle = mangle_path(path);

    fprintf(out_c, "const char *%s_src = \n", mangle);
    fprintf(out_c, "\"");
    for(;;) {
        int next = fgetc(input);
        if(next == EOF) {
            break;
        }

        if(next == '\n') {
            // terminate this line and start a new one.
            fprintf(out_c, "\\n\"\n\"");
            continue;
        }

        // TODO: Handle other special characters, \r, \0??, etc?
        fprintf(out_c, "%c", (char)next);
    }

    // Terminate the last string.
    fprintf(out_c, "\";\n\n");

    // Generate the header file stuff.
    fprintf(out_h, "extern const char *%s_src;\n", mangle);
}

int
main(int argc, char **argv) {
    if(argc < 4) {
        usage();
        return 0;
    }

    char *out_c_path = argv[argc - 2];
    char *out_h_path = argv[argc - 1];
    FILE *out_c = fopen(out_c_path, "w");
    FILE *out_h = fopen(out_h_path, "w");
    if(!out_c) {
        printf("error: failed to open output file: %s\n", out_c_path);
        return 1;
    }
    if(!out_h) {
        printf("error: failed to open output file: %s\n", out_c_path);
        return 1;
    }

    fprintf(out_h, "#ifndef SHADER_SRC_H\n");
    fprintf(out_h, "#define SHADER_SRC_H\n\n");

    for(int i = 1; i < argc - 2; ++i) {
        FILE *in = fopen(argv[i], "r");
        if(!in) {
            printf("warning: failed to open input file: %s\n", argv[i]);
            continue;
        }

        // NOTE: This modifies the argv[i] string
        convert(in, out_c, out_h, argv[i]);
        fclose(in);
    }

    fprintf(out_h, "#endif\n");

    fclose(out_c);
    fclose(out_h);
    return 0;
}