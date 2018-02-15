#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


struct jumptab_entry {
    uint16_t offset;
    uint8_t size;
    uint8_t width;
};


int main(int argc, char *argv[])
{
    const char *filename = "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf";

    if(argc > 1)
    {
        filename = argv[1];
    }

    int size = 24;

    if(argc > 2)
    {
        size = strtol(argv[2], 0, 10);
    }

    FT_Library  library;
    FT_Face     face;
    int error = FT_Init_FreeType( &library );
    if ( error )
    {
        printf("... an error occurred during library initialization ... %d\n", error);
    }

    error = FT_Init_FreeType( &library );
    if ( error ) { printf("FT_Init_FreeType failed %d\n", error); }

    error = FT_New_Face( library,
                         filename,

                         0,
                         &face );
    if ( error == FT_Err_Unknown_File_Format )
    {
        printf("... the font file could be opened and read, but it appears\n... that its font format is unsupported\n");
    } else if ( error ) {
        printf("... another error code means that the font file could not\n... be opened or read, or that it is broken...\n");
    }

    printf("%s\n%s\n%d\n", face->family_name, face->style_name, size);

    error = FT_Set_Pixel_Sizes(face, 0, size);

    FT_GlyphSlot  slot = face->glyph;

    int x = 0;

    int total_width = 0;
    int max_width = 0;
    int max_height = 0;
    int max_top = 0;

    char first_char = 0x20;
    int num_chars = 0x5F;

    for(int c = first_char; c < first_char + num_chars; c++)
    {
        error = FT_Load_Char( face, c, FT_LOAD_MONOCHROME|FT_LOAD_RENDER);

        total_width += slot->advance.x >> 6;

        if(slot->advance.x >> 6 > max_width)
        {
            max_width = slot->advance.x >> 6;
        }

        if(slot->bitmap_top > max_top)
        {
            max_top = slot->bitmap_top;
        }
    }


    for(int c = first_char; c < first_char + num_chars; c++)
    {
        error = FT_Load_Char( face, c, FT_LOAD_MONOCHROME|FT_LOAD_RENDER);

        int h = max_top - slot->bitmap_top + slot->bitmap.rows;

        if(h > max_height)
        {
            max_height = h;
        }
    }

    int WIDTH = total_width;
    int HEIGHT = max_height;

    HEIGHT = (1 + (HEIGHT - 1) / 8) * 8;
    uint8_t *image =  malloc(WIDTH * HEIGHT);
    memset(image, 0, WIDTH * HEIGHT);

    struct jumptab_entry jumptab[num_chars];

    uint8_t *font_data[num_chars];

    int offset = 0;

    for(int c = first_char, n = 0; c < first_char + num_chars; c++, n++)
    {
        error = FT_Load_Char( face, c, FT_LOAD_MONOCHROME|FT_LOAD_RENDER );

        int left = slot->bitmap_left;
        if(left < 0)
        {
            left = 0;
        }

        int width = ((slot->metrics.width >> 6) + left);

        jumptab[n].width = slot->advance.x >> 6;
        jumptab[n].size = width * HEIGHT / 8;

        if(width > 0)
        {
            jumptab[n].offset = offset;
            offset += jumptab[n].size;

            font_data[n] = malloc(jumptab[n].size);
            memset(font_data[n],0,jumptab[n].size);
        } else {
            jumptab[n].offset = 0xFFFF;
            font_data[n] = 0;
        }

        for(int i = 0; i < max_height; i++)
        {
            for(int j = 0; j < width; j++)
            {
                image[i  * WIDTH + j + x] =  (c & 0x01) ? 0x44 : 0x66;
            }
        }


        for(int i = 0; i < slot->bitmap.rows; i++)
        {
            for(int j = 0; j < slot->bitmap.pitch; j++)
            {
                for(int k = 0; k < 8; k++)
                {
                    image[(i + max_top - slot->bitmap_top) * WIDTH + 8*j + k + x + left] ^= (slot->bitmap.buffer[i * slot->bitmap.pitch + j] & (1 << (7-k))) ? 0xFF : 0x00;
                }
            }
        }

        for(int j = 0; j < width; j++)
        {
             for(int i = 0; i < HEIGHT/8; i++)
             {
                 for(int k = 0; k < 8; k++)
                 {
                     uint8_t d = image[(8*i + k) * WIDTH + x + j];

                     if(d > 0x80)
                     {
                         font_data[n][j * (HEIGHT/8) + i] |= 1 << k;
                     }
                 }
             }
        }

        // printf("\n");

        x += slot->advance.x >> 6;
    }

    FILE *f = fopen("out.c", "w");
    fprintf(f, "#include <stdint.h>\n");
    fprintf(f, "\n");
    fprintf(f, "const uint8_t font[] PROGMEM = {\n");
    fprintf(f, "  0x%02X, // Width: %d\n", max_width, max_width);
    fprintf(f, "  0x%02X, // Height: %d\n", HEIGHT, HEIGHT);
    fprintf(f, "  0x%02X, // First Char: %d\n", first_char, first_char);
    fprintf(f, "  0x%02X, // Number of Chars: %d\n", num_chars, num_chars);
    fprintf(f, "\n");
    fprintf(f, "  // Jump Table:\n");

    for(int c = first_char, n = 0; c < first_char + num_chars; c++, n++)
    {
        fprintf(f, "  0x%02X, 0x%02X, 0x%02X, 0x%02X,  // %d:%d\n",
                (jumptab[n].offset >> 8) & 0xFF,
                jumptab[n].offset & 0xFF,
                jumptab[n].size,
                jumptab[n].width,
                c,
                jumptab[n].offset
            );
    }

    fprintf(f, "\n");
    fprintf(f, "  // Font Data:\n");
    for(int n = 0; n < num_chars; n++)
    {
        if(jumptab[n].size > 0)
        {
            fprintf(f, "  ");

            for(int m = 0; m < jumptab[n].size; m++)
            {
                fprintf(f, "0x%02X,", font_data[n][m]);
            }

            fprintf(f, "  // %d\n", first_char + n);
        }
    }


    fprintf(f, "};\n");

    fclose(f);


    stbi_write_png("out.png", WIDTH, HEIGHT, 1, image, WIDTH);
    return 0;
}
