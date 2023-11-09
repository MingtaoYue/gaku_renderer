#include "tgaimage.h"
#include "geometry.h"

// model + view, projection, viewport transform
void lookat(const vec3 eye, const vec3 center, const vec3 up);
void projection(const double coeff=0);
void viewport(const int x, const int y, const int w, const int h);

struct IShader {
    // get color from texture image
    static TGAColor sample2D(const TGAImage &img, vec2 &uvf) {
        return img.get(uvf[0] * img.width(), uvf[1] * img.height());
    }
    // set up for one vertex (texture coordinate, normal vector, transformed coordinate)
    virtual void vertex(const int iface, const int nthvert, vec4 &gl_Position) = 0;
    // shade for one fragment (pixel) inside triangle
    virtual bool fragment(const vec3 bar, TGAColor &color) = 0;
};

// draw triangle
void triangle(const vec4 clip_verts[3], IShader &shader, TGAImage &image, std::vector<double> &zbuffer);