#ifndef LCD_COVER_IMAGE_H
#define LCD_COVER_IMAGE_H

#include "../../../../picture/tafei/picture_tafei.h"

#define LCD_COVER_IMAGE_WIDTH 247U
#define LCD_COVER_IMAGE_HEIGHT 240U
/* Img2Lcd prefixes the RGB565 pixels with an 8-byte descriptor. */
#define LCD_COVER_IMAGE_HEADER_SIZE 8U

#define LCD_COVER_IMAGE_DATA \
  (&gImage_picture[LCD_COVER_IMAGE_HEADER_SIZE])

#endif

