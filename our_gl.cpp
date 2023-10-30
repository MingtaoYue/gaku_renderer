#include <cmath>
#include <limits>
#include <cstdlib>
#include "our_gl.h"

Matrix ModelView;
Matrix Projection;
Matrix Viewport;

IShader::~IShader() {}

void lookat(Vec3f eye, Vec3f center, Vec3f up) {
    // camera looks at -z direction
    Vec3f z = (eye - center).normalize();
    Vec3f x = cross(up, z).normalize();
    // up may not be perpendicular to z
    Vec3f y = cross(z, x).normalize();
    Matrix rotation = Matrix::identity();
    Matrix translation = Matrix::identity();
    for (int i = 0; i < 3; i++) {
        // the rotation matrix from world to camera is the inverse of that from camera to world, for rotation matrix, inverse is equal to transpose
        rotation[0][i] = x[i];
        rotation[1][i] = y[i];
        rotation[2][i] = z[i];
        translation[i][3] = -eye[i];
    }
    ModelView = rotation * translation;
}

void projection(float coeff) {
    Projection = Matrix::identity();
    // coeff = -1/c, where c is focal length, 0 indicates orthographic projection
    Projection[3][2] = coeff;
}

// set from xyz: [-1, 1] to x: [x, x + w], y: [y, y + h] and z: [0, 1]
void viewport(int x, int y, int w, int h) {
    Viewport = Matrix::identity();
    // scaling
    Viewport[0][0] = w / 2.f;
    Viewport[1][1] = h / 2.f;
    Viewport[2][2] = 0.5;

    // translation
    Viewport[0][3] = x + w / 2.f;
    Viewport[1][3] = y + h / 2.f;
    Viewport[2][3] = 0.5;
}

Vec3f barycentric(Vec2f A, Vec2f B, Vec2f C, Vec2f P) {
    Vec3f s[2];
    for (int i = 0; i < 2; i++) {
        s[i][0] = C[i]-A[i];
        s[i][1] = B[i]-A[i];
        s[i][2] = A[i]-P[i];
    }
    Vec3f coeff = cross(s[0], s[1]);

    // if coeff[2] is 0 (note that it is a float), the triangle is degenerate, return a negative value and the rasterizer will ignore it
    if (std::abs(coeff[2]) < 1e-2) {
        return Vec3f(-1,1,1);
    }
    float u = coeff.y/coeff.z;
    float v = coeff.x/coeff.z;
    return Vec3f(1.f - u - v, u, v);
}

void triangle(mat<4,3,float> &tri_clip, IShader &shader, TGAImage &image, float *zbuffer) {
    // clip coordinates to screen coordinates, transpose for easy access
    mat<3,4,float> pts  = (Viewport * tri_clip).transpose();
    // convert 3d homogeneous coordinates to 2d cartesian coordinates
    mat<3,2,float> pts_xy;
    for (int i = 0; i < 3; i++) {
        pts_xy[i] = proj<2>(pts[i] / pts[i][3]);
    }

    // init bounding box
    Vec2f bboxmin(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    Vec2f bboxmax(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
    // image size
    Vec2f clamp(image.get_width() - 1, image.get_height() - 1);
    // set bounding box
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 2; j++) {
            bboxmin[j] = std::max(0.f, std::min(bboxmin[j], pts_xy[i][j]));
            bboxmax[j] = std::min(clamp[j], std::max(bboxmax[j], pts_xy[i][j]));
        }
    }

    // iterate over bounding box
    Vec2i P;
    TGAColor color;
    for (P.x = bboxmin.x; P.x <= bboxmax.x; P.x++) {
        for (P.y = bboxmin.y; P.y <= bboxmax.y; P.y++) {
            Vec3f bc = barycentric(pts_xy[0], pts_xy[1], pts_xy[2], P);
            float frag_depth = tri_clip[2] * bc;
            // if the fragment is outside the triangle, or the depth is larger than the current depth in zbuffer, ignore it
            if (bc.x<0 || bc.y<0 || bc.z<0 || zbuffer[P.x + P.y * image.get_width()] > frag_depth) {
                continue;
            }
            // the fragment shader determines the color of the fragment and whether to discard it
            bool discard = shader.fragment(bc, color);
            if (!discard) {
                zbuffer[P.x + P.y * image.get_width()] = frag_depth;
                image.set(P.x, P.y, color);
            }
        }
    }
}

