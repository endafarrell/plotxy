//
//  main.c
//  plotxy
//
//  Created by Enda Farrell on 22/08/2011.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include <stddef.h>
#include <time.h>
#include <float.h>

#define CR 13
#define LF 10


/* A coloured pixel. */
typedef struct {
    u_int8_t red;
    u_int8_t green;
    u_int8_t blue;
    double num;
} pixel_t;

/* A picture. */
typedef struct  {
    pixel_t *pixels;
    size_t width;
    size_t height;
    clock_t plotCreated;
    double plotInitialised;
    double inputFirstRead;
    double inputSecondRead;
    double pixelsDrawn;
    double outputWritten;
    long dataPoints;
    double minX;
    double maxX;
    double minY;
    double maxY;
    double maxNum;
} plot_t;

/* An XY coord */
typedef struct {
    double X;
    double Y;
} coord_t;

/* Given "plot", this returns the pixel of bitmap at the point
 ("x", "y"). */
static pixel_t * pixel_at (plot_t * plot, size_t x, size_t y){
    return plot->pixels + plot->width * y + x;
}

/* Write "bitmap" to a PNG file specified by "path"; returns 0 on
 success, non-zero on error. */
static int save_png_to_file (plot_t *plot, const char *path){
    FILE * fp;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    size_t x, y;
    png_byte ** row_pointers = NULL;
    /* "status" contains the return value of this function. At first
     it is set to a value which means 'failure'. When the routine
     has finished its work, it is set to a value which means
     'success'. */
    int status = -1;
    /* The following number is set by trial and error only. I cannot
     see where it it is documented in the libpng manual.
     */
    int pixel_size = 3;
    int depth = 8;
    
    fp = fopen (path, "wb");
    if (! fp) {
        goto fopen_failed;
    }
    
    png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        goto png_create_write_struct_failed;
    }
    
    info_ptr = png_create_info_struct (png_ptr);
    if (info_ptr == NULL) {
        goto png_create_info_struct_failed;
    }
    
    /* Set up error handling. */
    
    if (setjmp (png_jmpbuf (png_ptr))) {
        goto png_failure;
    }
    
    /* Set image attributes. */
    png_set_IHDR (png_ptr,
                  info_ptr,
                  plot->width,
                  plot->height,
                  depth,
                  PNG_COLOR_TYPE_RGB,
                  PNG_INTERLACE_NONE,
                  PNG_COMPRESSION_TYPE_DEFAULT,
                  PNG_FILTER_TYPE_DEFAULT);
    
    /* Initialize rows of PNG. */    
    row_pointers = png_malloc (png_ptr, plot->height * sizeof (png_byte *));
    for (y = 0; y < plot->height; ++y) {
        png_byte *row = png_malloc (png_ptr, sizeof (u_int8_t) * plot->width * pixel_size);
        row_pointers[y] = row;
        for (x = 0; x < plot->width; ++x) {
            pixel_t * pixel = pixel_at (plot, x, y);
            *row++ = pixel->red;
            *row++ = pixel->green;
            *row++ = pixel->blue;
        }
    }
    
    /* Write the image data to "fp". */
    png_init_io (png_ptr, fp);
    png_set_rows (png_ptr, info_ptr, row_pointers);
    png_write_png (png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
    
    /* The routine has successfully written the file, so we set
     "status" to a value which indicates success. */
    
    status = 0;    
    for (y = 0; y < plot->height; y++) {
        png_free (png_ptr, row_pointers[y]);
    }
    png_free (png_ptr, row_pointers);
    
png_failure:
png_create_info_struct_failed:
    png_destroy_write_struct (&png_ptr, &info_ptr);
png_create_write_struct_failed:
    fclose (fp);
fopen_failed:
    return status;
}

void chomp (char* s) {
    long end = strlen(s) - 1;
    if (end >= 0 && s[end] == '\n')
        s[end] = '\0';
}

plot_t readPlot(char * xyFileName, long width, long height, long colorA, long colorB, long colorC, long colorD){
    plot_t plot;
    int x, y = 0;
    int MAX_REC_LEN = 128;  /* ought to be enought for each line */
    char line[MAX_REC_LEN]; // plenty big for Enda's xy plots
    
    char delimiters[] = ", #\n";
    char * lineCopy;
    char * strX;
    char * strY;
    double dX;
    double dY;
    char **ignore = NULL;
    /* Care is needed with these initial values! When drawing a plot of Germany
     the nin value was 0.0 - which often means that the Equator south of 
     Greenwich is always added to the map :-( */
    double maxX = DBL_MIN;
    double minX = DBL_MAX;
    double maxY = DBL_MIN;
    double minY = DBL_MAX;
    double maxNum = DBL_MIN;
    long lTotalLines = -1;
    coord_t **coords;
    coord_t *coord;
    long i = 0;
    
    plot.width = width;
    plot.height = height;
    plot.plotCreated = clock();
    
    plot.pixels = calloc (sizeof (pixel_t), plot.width * plot.height);
    
    // Initialise everything to Black //Nokia Blue    
    for (y = 0; y < plot.height; y++) {
        for (x = 0; x < plot.width; x++) {
            pixel_t *pixel = pixel_at(& plot, x, y);
            pixel->red = 0;   //18;
            pixel->green = 0; //65;
            pixel->blue = 0;  //145;
            pixel->num = 0;
        }
    }
    plot.plotInitialised = (double) clock() - plot.plotCreated;
    
    /**
     * On this first pass, we get the number of lines, and while
     * assuming that the file layout is EXACTLY what is needed,
     * we get the total number of lines and the max/min for the
     * X and Y coords.
     */
    FILE *input;
    input = fopen(xyFileName, "r");
    if (input != NULL){
        while (fgets(line, MAX_REC_LEN, input) != NULL){
            lTotalLines++;
            lineCopy = strdup(line);
            chomp(lineCopy);
            strX = strtok (lineCopy, delimiters);
            strY = strtok(NULL, delimiters);
            dX = strtod(strX, ignore);
            dY = strtod(strY, ignore);
            // Set the max/min for X and Y
            if (lTotalLines == 0) {
                maxX = dX;
                minX = dX;
                maxY = dY;
                minY = dY;
                printf("maxX %f, minX %f, maxY %f, minY %f init\n", maxX, minX, maxY, minY);
            }
            if (dX > maxX) {maxX = dX; printf("maxX now set to %f on line %ld\n", maxX, lTotalLines); }
            if (dX < minX) {minX = dX; printf("minX now set to %f on line %ld\n", minX, lTotalLines); }
            if (dY > maxY) {maxY = dY; printf("maxY now set to %f on line %ld\n", maxY, lTotalLines); }
            if (dY < minY) {minY = dY; printf("minY now set to %f on line %ld\n", minY, lTotalLines); }
            free(lineCopy);
        }
        fclose(input);
    } else {
        perror(xyFileName);
    }
    printf("First read done.\n");
    
    plot.minX = minX;
    plot.maxX = maxX;
    plot.minY = minY;
    plot.maxY = maxY;
    plot.dataPoints = lTotalLines;
    
    coords = (coord_t **) calloc(lTotalLines,  sizeof(coord_t *));
    if (coords == NULL) {
        perror("Insufficient memory to store coords.\n");
        return plot;
    }
    plot.inputFirstRead = (double) clock() - plot.plotCreated;
    
    /**
     * On this second pass, move the coords from the file to 
     * the array of coords (sized earlier by counting the
     * lines
     */
    i = 0;
    input = fopen(xyFileName, "r");
    while(fgets(line, MAX_REC_LEN, input) != NULL){        
        lineCopy = strdup(line);
        chomp(lineCopy);        
        strX = strtok (lineCopy, delimiters);
        strY = strtok(NULL, delimiters);
        dX = strtod(strX, ignore);
        dY = strtod(strY, ignore);
        coord = (coord_t *) calloc(1, sizeof(coord_t));
        coord->X = dX;
        coord->Y = dY;
        coords[i] = coord;
        free(lineCopy);
        i++;
    }
    fclose(input);
    printf("Second read done.\n");
    
    plot.inputSecondRead = (double) clock() - plot.plotCreated;
    
    for (i = 0; i < lTotalLines; i++) {
        coord = coords[i];
        x = (coord->X - minX)*plot.width/(maxX - minX) - 1;
        // Remember, (x=0,y=0) is not the bottom-left corner,
        // it's the top-left corner.
        y = (maxY - coord->Y)*plot.height/(maxY - minY) - 1;
        pixel_t *pixel = pixel_at(& plot, x , y);
        
        pixel->num = pixel->num + 1;
        if (pixel->num > maxNum) maxNum = pixel->num;
    }   
    
    
    // Initialise everything to Black //Nokia Blue    
    for (y = 0; y < plot.height; y++) {
        for (x = 0; x < plot.width; x++) {
            pixel_t *pixel = pixel_at(& plot, x, y);
            if (pixel->num > 0) {
                if (pixel->num >= colorA) {
                    pixel->red = 255;
                    pixel->green = 255;
                    pixel->blue = 255;
                } else if (pixel->num > colorB) {
                    pixel->red = 255;
                    pixel->green = 100;
                    pixel->blue = 100;            
                } else if (pixel->num > colorC) {
                    pixel->red = 255;
                    pixel->green = 165;
                    pixel->blue = 0;            
                } else if (pixel->num > colorD) {
                    pixel->red = 255;
                    pixel->green = 255;
                    pixel->blue = 0;            
                } else {
                    pixel->red = 100;
                    pixel->green = 100;
                    pixel->blue = 255;
                }
            }
        }
    }
    
    free(coords);
    plot.pixelsDrawn = (double) clock() - plot.plotCreated;
    plot.maxNum = maxNum;
    return plot;
}



int main (int argc, const char * argv[]) {
    size_t x = 4000;
    size_t y = 2000;
    long colorA = 1000;
    long colorB = 500;
    long colorC = 100;
    long colorD = 20;
    char * xyFileName = "/Users/enda/Documents/PlaceMaps/xy.csv";
    char * pngFileName;
    char * tail;
    int CLOCKS_PER_MILLISEC =  CLOCKS_PER_SEC / 1000;
    
    /* When doing a test Run inside the IDE, the above initialised
     * value is used for the input file name. Here we (carefully)
     * re-set the value to the first of the parameters. */
    if (argc > 1) {
        xyFileName = malloc(strlen(argv[1]) + 1);
        strcpy(xyFileName, argv[1]);
    }
    /* +5 = +4 for ".png" + 1 for the NUL terminator */
    pngFileName = malloc(strlen(xyFileName) + 5);
    strcpy(pngFileName, xyFileName);
    strcat(pngFileName, ".png");
    
    if (argc > 2) {
        x = strtol(argv[2], &tail, 0);
        y = (long) x/2;
    }
    if (argc > 3) {
        y = strtol(argv[3], &tail, 0);
    }
    
    if (argc > 4) {
        colorA = strtol(argv[4], &tail, 0);
        colorB = strtol(argv[5], &tail, 0);
        colorC = strtol(argv[6], &tail, 0);
        colorD = strtol(argv[7], &tail, 0);
    }
    
    plot_t plot = readPlot(xyFileName, x, y, colorA, colorB, colorC, colorD);
    save_png_to_file (&plot, pngFileName);
    plot.outputWritten = (double) clock() - plot.plotCreated;
    printf("%s (%lux%lu) created from %.0lu data points. %8.2f X %8.2f, %8.2f Y %8.2f and maxNum %8.0f\n",
           pngFileName, plot.width, plot.height, plot.dataPoints, 
           plot.minX, plot.maxX, plot.minY, plot.maxY, plot.maxNum);
    printf("Plot timings (ms):\n");
    printf("* plot initialisation: %8.0f\n", plot.plotInitialised / CLOCKS_PER_MILLISEC);
    printf("*    input first read: %8.0f\n", plot.inputFirstRead / CLOCKS_PER_MILLISEC);    
    printf("*   input second read: %8.0f\n", plot.inputSecondRead / CLOCKS_PER_MILLISEC);
    printf("*        pixels drawn: %8.0f\n", plot.pixelsDrawn / CLOCKS_PER_MILLISEC);
    printf("*      output written: %8.0f\n", plot.outputWritten / CLOCKS_PER_MILLISEC);
    if (argc > 1) free(xyFileName);
    free(pngFileName);
    return 0;
}

