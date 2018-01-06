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
    char *pngname = "test.png";
    char *tag = 0;

    char *tag_upper = 0;

    if(argc > 1)
    {
        pngname = argv[1];
    }

    if(argc > 2)
    {
        tag = argv[2];
        tag_upper = malloc(strlen(tag)+1);

        for(int i = 0; i < strlen(tag)+1; i++)
        {
            tag_upper[i] = toupper(tag[i]);
        }
    }

    if(load_png(pngname, &width, &height, &bytes_per_pixel, &data))
    {
        printf("%s: %d x %d, %d bytes per pixel\n", pngname, width, height, bytes_per_pixel);
        char filename_c[strlen(pngname) + 3];
        char filename_h[strlen(pngname) + 3];

        strcpy(filename_c, pngname);
        strcpy(filename_h, pngname);

        if(!strcmp(&pngname[strlen(pngname) - 4], ".png"))
        {
            strcpy(&filename_c[strlen(pngname) - 4], ".c");
            strcpy(&filename_h[strlen(pngname) - 4], ".h");
        } else {
            strcpy(&filename_h[strlen(pngname)], ".c");
            strcpy(&filename_c[strlen(pngname)], ".c");
        }

        printf("%s -> %s + %s\n", pngname, filename_c, filename_h);

        FILE *file_c = fopen(filename_c, "w");
        FILE *file_h = fopen(filename_h, "w");

        printf("\n");
        for(int i = 0; i < height ; i++)
        {
            for(int j = 0; j < width; j++)
            {
                if(data[bytes_per_pixel * (width * i + j)+0])
                {
                    printf("#");
                } else {
                    printf(" ");
                }
            }
            printf("\n");
        }

        if(tag)
        {
            fprintf(file_h, "#ifndef %s_H_\n", tag_upper);
            fprintf(file_h, "#define %s_H_\n", tag_upper);
            fprintf(file_h, "\n");
        }
        fprintf(file_h, "#include <stdint.h>\n");
        fprintf(file_h, "\n");
        if(tag)
        {
            fprintf(file_h, "#define %s_WIDTH %d\n", tag_upper, width);
            fprintf(file_h, "#define %s_HEIGHT %d\n", tag_upper, height);
            fprintf(file_h, "\n");
            fprintf(file_h, "extern const uint8_t %s[];", tag);
        } else {
            fprintf(file_h, "#define WIDTH %d\n", width);
            fprintf(file_h, "#define HEIGHT %d\n", height);
            fprintf(file_h, "\n");
            fprintf(file_h, "extern const uint8_t data[];");

        }
        fprintf(file_h, "\n");
        if(tag)
        {
            fprintf(file_h, "#endif\n");
        }

        fprintf(file_c, "#include \"%s\"\n", filename_h);
        fprintf(file_c, "\n");

        if(tag)
        {
            fprintf(file_c, "const uint8_t %s[] = {\n", tag);
        } else {
            fprintf(file_c, "const uint8_t data[] = {\n");
        }

        for(int j = 0; j < width; j++)
        {
            fprintf(file_c, "   ");
            for(int i = 0; i < 1 + (height-1)/8; i++)
            {
                uint8_t byte = 0;

                for(int k = 7; k >= 0; k--)
                {
                    if((8 * i + k) < height)
                    {
                        int n = ((8 * i + k) * width + j);
                        int r = data[bytes_per_pixel * n];
                        int g = data[bytes_per_pixel * n + 1];
                        int b = data[bytes_per_pixel * n + 2];

                        byte = (byte << 1) | (r ? 1 : 0);
                    } else {
                        byte <<= 1;
                    }
                }

//                    fwrite(&byte, 1, 1, file_c);
                fprintf(file_c, " 0x%02X,", byte);
//                if((j % 16) == 15)
//                    fprintf(file_c, "\n");
            }
            fprintf(file_c, "\n");
        }
        fprintf(file_c, "};\n");

        fclose(file_c);
    } else {
        fprintf(stderr, "Failed to load PNG file %s", pngname);
    }

    // No cleanup!
}
