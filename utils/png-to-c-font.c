// LibPNG example
// A.Greensted
// http://www.labbookpages.co.uk

// Version 2.0
// With some minor corrections to Mandlebrot code (thanks to Jan-Oliver)

// Version 1.0 - Initial release

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <png.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

struct jumptab_entry {
    uint16_t offset;
    uint8_t size;
    uint8_t width;
};


int load_png(char *name, int *out_width, int *out_height, int *bytes_per_pixel, uint8_t **out_data) {
    png_structp png_ptr;
    png_infop info_ptr;
    unsigned int sig_read = 0;
    int color_type, interlace_type;
    FILE *fp;

    if ((fp = fopen(name, "rb")) == NULL)
        return 0;

    /* Create and initialize the png_struct with the desired error
     * handler functions.  If you want to use the default stderr and
     * longjump method, you can supply NULL for the last three
     * parameters.  We also supply the the compiler header file
     * version, so that we know if the application was compiled with a
     * compatible version of the library.  REQUIRED
     */
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (png_ptr == NULL) {
        fclose(fp);
        return 0;
    }

    /* Allocate/initialize the memory for image information.
     * REQUIRED. */
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        fclose(fp);
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return 0;
    }

    /* Set error handling if you are using the setjmp/longjmp method
     * (this is the normal method of doing things with libpng).
     * REQUIRED unless you set up your own error handlers in the
     * png_create_read_struct() earlier.
     */
    if (setjmp(png_jmpbuf(png_ptr))) {
        /* Free all of the memory associated with the png_ptr and
         * info_ptr */
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        /* If we get here, we had a problem reading the file */
        return 0;
    }

    /* Set up the output control if
     * you are using standard C streams */
    png_init_io(png_ptr, fp);

    /* If we have already read some of the signature */
    png_set_sig_bytes(png_ptr, sig_read);

    /*
     * If you have enough memory to read in the entire image at once,
     * and you need to specify only transforms that can be controlled
     * with one of the PNG_TRANSFORM_* bits (this presently excludes
     * dithering, filling, setting background, and doing gamma
     * adjustment), then you can read the entire image (including
     * pixels) into the info structure with this call
     *
     * PNG_TRANSFORM_STRIP_16 |
     * PNG_TRANSFORM_PACKING  forces 8 bit
     * PNG_TRANSFORM_EXPAND forces to
     *  expand a palette into RGB
     */
    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND, NULL);

    png_uint_32 width, height;
    int bit_depth;
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
                 &interlace_type, NULL, NULL);
    *out_width = width;
    *out_height = height;


    unsigned int row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    *out_data = (unsigned char*) malloc(row_bytes * height);

    *bytes_per_pixel = row_bytes/width;

    png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);

    for (int i = 0; i < height; i++) {
        memcpy(*out_data+(row_bytes * i), row_pointers[i], row_bytes);
    }

    /* Clean up after the read,
     * and free any memory allocated */
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    /* Close the file */
    fclose(fp);

    /* That's it */
    return 1;
}

int main(int argc, char *argv[])
{
   int width, height, bytes_per_pixel;
    uint8_t *data;
    char *pngname = "font-3x5.png";
    char *tag = 0;

    int glyph_width = 4;

    if(argc > 1)
    {
        pngname = argv[1];
    }

    if(argc > 2)
    {
        glyph_width = strtol(argv[2], 0, 10);
    }

    if(load_png(pngname, &width, &height, &bytes_per_pixel, &data))
    {
        printf("%s: %d x %d, %d bytes per pixel\n", pngname, width, height, bytes_per_pixel);
        char filename[strlen(pngname) + 3];

        strcpy(filename, pngname);

        if(!strcmp(&pngname[strlen(pngname) - 4], ".png"))
        {
            strcpy(&filename[strlen(pngname) - 4], ".c");
        } else {
            strcpy(&filename[strlen(pngname)], ".c");
        }

        printf("%s -> %s\n", pngname, filename);

        int num_glyphs = width / (glyph_width);

        char first_char = ' ';

        uint8_t font_pixel_data[num_glyphs][glyph_width * height];

        struct jumptab_entry jumptab[num_glyphs];
        uint8_t *font_data[num_glyphs];

        int offset = 0;

        printf("%d glyphs\n", num_glyphs);

        int HEIGHT = height;
        HEIGHT = (1 + (HEIGHT - 1) / 8) * 8;

        for(int n = 0; n < num_glyphs; n++) {

            jumptab[n].width = glyph_width;
            jumptab[n].size = glyph_width * HEIGHT / 8;

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


            for(int i = 0; i < height ; i++) {
                for(int j = 0; j < glyph_width; j++) {
                    if(data[bytes_per_pixel * (width * i + (glyph_width) * n + j)]) {
                        font_pixel_data[n][i * glyph_width + j] = 1;
                        printf("#");
                    } else {
                        font_pixel_data[n][i * glyph_width + j] = 0;
                        printf(" ");
                    }
                }
                printf("\n");
            }
            printf("\n");

            for(int j = 0; j < glyph_width; j++) {
                for(int i = 0; i < HEIGHT; i++) {
                    for(int k = 0; k < 8; k++) {

                        uint8_t d;
                        if(8*i+k < height) {
                            d = data[bytes_per_pixel * (width * (8*i+k) + (glyph_width) * n + j)];
                        } else {
                            d = 0;
                        }

                        if(d) {
                            font_data[n][j * (HEIGHT/8) + i] |= 1 << k;
                            printf("#");
                        } else {
                            printf(" ");
                        }
                    }
                }
                printf("\n");
            }
            printf("\n");
        }

        FILE *f = fopen("out.c", "w");
        fprintf(f, "#include <stdint.h>\n");
        fprintf(f, "\n");
        fprintf(f, "const uint8_t font[] PROGMEM = {\n");
        fprintf(f, "  0x%02X, // Width: %d\n", glyph_width, glyph_width);
        fprintf(f, "  0x%02X, // Height: %d\n", height, height);
        fprintf(f, "  0x%02X, // First Char: %d\n", first_char, first_char);
        fprintf(f, "  0x%02X, // Number of Chars: %d\n", num_glyphs, num_glyphs);
        fprintf(f, "\n");
        fprintf(f, "  // Jump Table:\n");

        for(int c = first_char, n = 0; c < first_char + num_glyphs; c++, n++)
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
        for(int n = 0; n < num_glyphs; n++)
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
    } else {
        fprintf(stderr, "Failed to load PNG file %s", pngname);
    }

    // No cleanup!
}
