#ifndef __OUR_GL_H__
#define __OUR_GL_H__
#include "tgaimage.h"
#include "geometry.h"

extern Matrix ModelView;
extern Matrix Projection;
extern Matrix Viewport;
const float depth = 2000.f;

void lookat(Vec3f eye, Vec3f center, Vec3f up);
void viewport(int x, int y, int w, int h);
// coeff = -1/c, where c is focal length, 0 indicates orthographic projection
void projection(float coeff=0.f);

struct IShader {
    virtual ~IShader();
    virtual Vec4f vertex(int iface, int nthvert) = 0;
    virtual bool fragment(Vec3f bar, TGAColor &color) = 0;
};

void triangle(Vec4f *pts, IShader &shader, TGAImage &image, float *zbuffer);
#endif