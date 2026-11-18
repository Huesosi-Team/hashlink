#define HL_NAME(n) fmt_##n
#include <hl.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <zlib.h>
#include <vorbis/vorbisfile.h>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT
#include <minimp3.h>

/* ------------------------------------------------- IMG --------------------------------------------------- */

typedef struct {
	unsigned char a,r,g,b;
} pixel;

static SDL_PixelFormat fmt_to_sdl_pixel_format(int format, int *bpp) 
{
	switch (format) 
	{
		case 0:  *bpp = 3; return SDL_PIXELFORMAT_RGB24;
		case 1:  *bpp = 3; return SDL_PIXELFORMAT_BGR24;
		case 2:  *bpp = 4; return SDL_PIXELFORMAT_RGBX32;
		case 3:  *bpp = 4; return SDL_PIXELFORMAT_BGRX32;
		case 4:  *bpp = 4; return SDL_PIXELFORMAT_XBGR32;
		case 5:  *bpp = 4; return SDL_PIXELFORMAT_XRGB32;
		case 6:
		case 12: *bpp = 1; return SDL_PIXELFORMAT_INDEX8;
		case 7:  *bpp = 4; return SDL_PIXELFORMAT_RGBA32;
		case 8:  *bpp = 4; return SDL_PIXELFORMAT_BGRA32;
		case 9:  *bpp = 4; return SDL_PIXELFORMAT_ABGR32;
		case 10: *bpp = 4; return SDL_PIXELFORMAT_ARGB32;
		default: *bpp = 0; return SDL_PIXELFORMAT_UNKNOWN;
	}
}

HL_PRIM bool HL_NAME(img_info)( vbyte *data, int dataLen, int *width, int *height ) 
{
	hl_blocking(true);
	SDL_IOStream *io = SDL_IOFromConstMem(data, (size_t)dataLen);
	if (!io) 
	{
		hl_blocking(false);
		return false;
	}

	SDL_Surface *surf = IMG_Load_IO(io, true);

	if (!surf) 
	{
		hl_blocking(false);
		return false;
	}
	*width = surf->w;
	*height = surf->h;
	SDL_DestroySurface(surf);
	hl_blocking(false);
	return true;
}

HL_PRIM bool HL_NAME(img_decode)( vbyte *data, int dataLen, vbyte *out, int width, int height, int stride, int format, int flags ) 
{
	int bpp = 0;
	SDL_PixelFormat targetFmt = fmt_to_sdl_pixel_format(format, &bpp);
	if (targetFmt == SDL_PIXELFORMAT_UNKNOWN || bpp == 0) 
	{
		hl_error("Unsupported format %d", format);
		return false;
	}

	hl_blocking(true);

	SDL_IOStream *io = SDL_IOFromConstMem(data, (size_t)dataLen);
	if (!io) 
	{
		hl_blocking(false);
		return false;
	}

	SDL_Surface *surf = IMG_Load_IO(io, true);
	if (!surf) 
	{
		hl_blocking(false);
		return false;
	}

	if (surf->w != width || surf->h != height) 
	{
		SDL_DestroySurface(surf);
		hl_blocking(false);
		return false;
	}

	if (surf->format != targetFmt) 
	{
		SDL_Surface *converted = SDL_ConvertSurface(surf, targetFmt);
		SDL_DestroySurface(surf);
		if (!converted) 
		{
			hl_blocking(false);
			return false;
		}
		surf = converted;
	}

	const int rowBytes = width * bpp;
	const bool bottomUp = (flags & 1) != 0;
	const uint8_t *src = (const uint8_t *)surf->pixels;

	for (int y = 0; y < height; y++) 
	{
		int dstY = bottomUp ? (height - 1 - y) : y;
		memcpy(out + dstY * stride, src + y * surf->pitch, rowBytes);
	}

	SDL_DestroySurface(surf);
	hl_blocking(false);
	return true;
}

HL_PRIM void HL_NAME(img_scale)( vbyte *out, int outPos, int outStride, int outWidth, int outHeight, vbyte *in, int inPos, int inStride, int inWidth, int inHeight, int flags ) 
{
	int x, y;

	float scaleX = outWidth <= 1 ? 0.0f : (float)((inWidth - 1.001f) / (outWidth - 1));
	float scaleY = outHeight <= 1 ? 0.0f : (float)((inHeight - 1.001f) / (outHeight - 1));
	out += outPos;
	in += inPos;

	hl_blocking(true);

	for(y=0; y<outHeight; y++) 
	{
		for(x=0; x<outWidth; x++) 
		{
			float fx = x * scaleX;
			float fy = y * scaleY;

			int ix = (int)fx;
			int iy = (int)fy;
			if( (flags & 1) == 0 ) {
				// nearest
				vbyte *rin = in + iy * inStride;
				*(pixel*)out = *(pixel*)(rin + (ix<<2));
				out += 4;
			} else {
				// bilinear
				float rx = fx - ix;
				float ry = fy - iy;
				float rx1 = 1.0f - rx;
				float ry1 = 1.0f - ry;
				int w1 = (int)(rx1 * ry1 * 256.0f);
				int w2 = (int)(rx * ry1 * 256.0f);
				int w3 = (int)(rx1 * ry * 256.0f);
				int w4 = (int)(rx * ry * 256.0f);
				vbyte *rin = in + iy * inStride;
				pixel p1 = *(pixel*)(rin + (ix<<2));
				pixel p2 = *(pixel*)(rin + ((ix + 1)<<2));
				pixel p3 = *(pixel*)(rin + inStride + (ix<<2));
				pixel p4 = *(pixel*)(rin + inStride + ((ix + 1)<<2));
				*out++ = (unsigned char)((p1.a * w1 + p2.a * w2 + p3.a * w3 + p4.a * w4 + 128)>>8);
				*out++ = (unsigned char)((p1.r * w1 + p2.r * w2 + p3.r * w3 + p4.r * w4 + 128)>>8);
				*out++ = (unsigned char)((p1.g * w1 + p2.g * w2 + p3.g * w3 + p4.g * w4 + 128)>>8);
				*out++ = (unsigned char)((p1.b * w1 + p2.b * w2 + p3.b * w3 + p4.b * w4 + 128)>>8);
			}
		}
		out += outStride - (outWidth << 2);
	}
	hl_blocking(false);
}

DEFINE_PRIM(_BOOL, img_info, _BYTES _I32 _REF(_I32) _REF(_I32));
DEFINE_PRIM(_BOOL, img_decode, _BYTES _I32 _BYTES _I32 _I32 _I32 _I32 _I32);
DEFINE_PRIM(_VOID, img_scale, _BYTES _I32 _I32 _I32 _I32 _BYTES _I32 _I32 _I32 _I32 _I32);