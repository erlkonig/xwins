/* readtex.h */

/*
 * Read an SGI .rgb image file and generate a mipmap texture set.
 * Much of this code was borrowed from SGI's tk OpenGL toolkit.
 */

#ifndef HAVE_TK_RGBIMAGELOAD
#define HAVE_TK_RGBIMAGELOAD 1
#endif

typedef struct _TK_RGBImageRec {
   GLint sizeX, sizeY;
   GLint components;
   unsigned char *data;
} TK_RGBImageRec;

#if 0  /* hide the statics */
extern static void ConvertShort(unsigned short *array, long length);
extern static void ConvertLong(GLuint *array, long length);
extern static rawImageRec *RawImageOpen(const char *fileName);
extern static void RawImageClose(rawImageRec *raw);
extern static void RawImageGetRow(rawImageRec *raw, unsigned char *buf, int y, int z);
extern static void RawImageGetData(rawImageRec *raw, TK_RGBImageRec *final);
extern static void FreeImage( TK_RGBImageRec *image );
#endif
extern TK_RGBImageRec *tkRGBImageLoad(const char *fileName);
extern GLboolean LoadRGBMipmaps( const char *imageFile, GLint intFormat );

extern int RgbTexture2dLoad(unsigned int *texidaddr, const char *filename); /*erlkonig*/

/*---------*/
