#include "picojpeg.h"

#include <stdio.h>
#include <stdlib.h>

//------------------------------------------------------------------------------
#ifndef max
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#endif
//------------------------------------------------------------------------------
#define MAX_DIGITS 10
// Number of pixels to evaluate for each segment (X * Y) - Must be uneven
#define NBR_PIXELS_PER_SEGMENT_X 3 
#define NBR_PIXELS_PER_SEGMENT_Y 3 
//------------------------------------------------------------------------------
enum
{
   PSSOCR_SEGMENT_TOP = 0,
   PSSOCR_SEGMENT_TOP_LEFT,
   PSSOCR_SEGMENT_TOP_RIGHT,
   PSSOCR_SEGMENT_MIDDLE,
   PSSOCR_SEGMENT_BOTTOM_LEFT,
   PSSOCR_SEGMENT_BOTTOM_RIGT,
   PSSOCR_SEGMENT_BOTTOM,
   PSSOCR_NBR_SEGMENTS_OF_DIGIT // Just a size marker
};
//------------------------------------------------------------------------------
typedef struct
{
    int x;
    int y;
} pssocr_segment_position_t;
//-----------------------------
//------------------------------------------------------------------------------
typedef struct
{
    pssocr_segment_pixel_colors_t segment_pixel_colors[PSSOCR_NBR_SEGMENTS_OF_DIGIT];
} pssocr_digit_segment_pixel_colors_t;
//------------------------------------------------------------------------------
static FILE *g_pFile;
static uint g_nFileSize;
static uint g_nFileOfs;
static pssocr_digit_segment_positions_t g_digit_segment_positions[MAX_DIGITS];
static pssocr_segment_pixel_colors_t g_digit_segment_pixel_colors[MAX_DIGITS];
//------------------------------------------------------------------------------
void store_pixel_color_if_needed(int x, int y, pssocr_pixel_color_t *color)
{
    int digit, segment, sx, sy;
    pssocr_segment_position_t *pos;

    // Find out boundaries
    const int X_MIN = max(0, x - (NBR_PIXELS_PER_SEGMENT_X/2));
    const int X_MAX = x + (NBR_PIXELS_PER_SEGMENT_X/2);
    const int Y_MIN = max(0, y - (NBR_PIXELS_PER_SEGMENT_Y/2));
    const int Y_MAX = y + (NBR_PIXELS_PER_SEGMENT_Y/2);

    // Find a pixel within the boundaries
    for (digit = 0 ; digit < MAX_DIGITS ; digit ++)
    {
        for (segment = 0 ; segment < PSSOCR_NBR_SEGMENTS_OF_DIGIT ; segment++)
        {
            pos = &g_digit_segment_positions[digit].segment_positions[segment];
            if (((pos->x >= X_MIN) && (pos->x <= X_MAX)) &&
                ((pos->y >= Y_MIN) && (pos->y <= Y_MAX)))
            {
                printf("Digit: %d - Segment: %d [%d, %d] [%d,%d,%d]\n", digit, segment, x, y, color->red, color->green, color->blue);
                // Figure out where to store pixel color
                sx = x - X_MIN;
                sy = y - Y_MIN;
                // Save pixel color
                g_digit_segment_pixel_colors[digit].pixel_colors[sx][sy] = *color;
            }
        }
    }
}
//------------------------------------------------------------------------------
unsigned char pjpeg_need_bytes_callback(unsigned char* pBuf, unsigned char buf_size, unsigned char *pBytes_actually_read, void *pCallback_data)
{
   uint n;
   pCallback_data;
   
   n = min(g_nFileSize - g_nFileOfs, buf_size);
   if (n && (fread(pBuf, 1, n, g_pFile) != n))
      return PJPG_STREAM_READ_ERROR;
   *pBytes_actually_read = (unsigned char)(n);
   g_nFileOfs += n;
   return 0;
}
//------------------------------------------------------------------------------
int main(int arg_c, char *arg_v[])
{
    char *scanTypeStr;
    pjpeg_image_info_t image_info;
    unsigned char status = 0;
    int x = 0, y = 0, x_max = 0, y_max = 0;
    int mcu_x = 0, mcu_y = 0, mcu_index = 0;
    int MCU_progress = 0, count = 0;
    pssocr_pixel_color_t color;

    // Fill in some test data
    g_digit_segment_positions[0].segment_positions[PSSOCR_SEGMENT_TOP].x = 2000;
    g_digit_segment_positions[0].segment_positions[PSSOCR_SEGMENT_TOP].y = 2000;

    if (arg_c != 2)
    {
        printf("Usage: %s IMAGE\n", arg_v[0]);
        return EXIT_FAILURE;
    }

    g_pFile = fopen(arg_v[1], "rb");
    if (g_pFile == NULL)
    {
        printf("Unable to open %s\n", arg_v[1]);
        return EXIT_FAILURE;
    }

    // Setup global variables
    g_nFileOfs = 0;
    fseek(g_pFile, 0, SEEK_END);
    g_nFileSize = ftell(g_pFile);
    fseek(g_pFile, 0, SEEK_SET);

    status = pjpeg_decode_init(&image_info, pjpeg_need_bytes_callback, NULL, 0);
        
    if (status != 0)
    {
        printf("pjpeg_decode_init() failed with status %u\n", status);
        if (status == PJPG_UNSUPPORTED_MODE)
        {
            printf("Progressive JPEG files are not supported.\n");
        }

        fclose(g_pFile);
        return EXIT_FAILURE;
    }

    switch(image_info.m_scanType)
    {
        case PJPG_GRAYSCALE:
            scanTypeStr = "GRAYSCALE";
            break;
        case PJPG_YH1V1:
            scanTypeStr = "H1V1";
            break;
        case PJPG_YH2V1:
            scanTypeStr = "H2V1";
            break;
        case PJPG_YH1V2:
            scanTypeStr = "H1V2";
            break;
        case PJPG_YH2V2:
            scanTypeStr = "H2V2";
            break;
    }

    // Print image info
    printf("width:  %d\n", image_info.m_width);
    printf("height: %d\n", image_info.m_height);
    printf("comps:  %d\n", image_info.m_comps);
    printf("MCU per row: %d\n", image_info.m_MCUSPerRow);
    printf("MCU per col: %d\n", image_info.m_MCUSPerCol);
    printf("Scan type:   %d (%s)\n", image_info.m_scanType, scanTypeStr);
    printf("MCU width:   %d\n", image_info.m_MCUWidth);
    printf("MCU height:  %d\n", image_info.m_MCUHeight);

    // Decode image
    printf("\n");
    printf("Decoding image\n");
    printf("|                    |");
    printf("\r|");
    fflush(stdout);

    MCU_progress = (image_info.m_MCUSPerRow * image_info.m_MCUSPerCol) / 20;
    while (pjpeg_decode_mcu() == 0)
    {
        // Print progress
        count++;
        if (count % MCU_progress == 0)
        {
            printf("-");
            fflush(stdout);
        }

        // Parse the MCU
        mcu_index = 0;
        x = mcu_x * image_info.m_MCUWidth;
        y = mcu_y * image_info.m_MCUHeight;
        x_max = min(x + image_info.m_MCUWidth, image_info.m_width);
        y_max = min(y + image_info.m_MCUHeight, image_info.m_height);
        for (y ; y < y_max ; y++)
        {
            for (x = mcu_x * image_info.m_MCUWidth ; x < x_max ; x++)
            {
                color.red   = image_info.m_pMCUBufR[mcu_index];
                color.green = image_info.m_pMCUBufG[mcu_index];
                color.blue  = image_info.m_pMCUBufB[mcu_index];
                store_pixel_color_if_needed(x, y, &color);
                mcu_index++;
            }
        }

        // Step MCU counters
        mcu_x++;
        if (mcu_x == image_info.m_MCUSPerRow)
        {
            mcu_x = 0;
            mcu_y++;
        }

    }
    printf("\n");
    printf("Image decoded successfully\n");

    fclose(g_pFile);

    return EXIT_SUCCESS;
}