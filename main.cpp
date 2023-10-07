#include <vector>
#include <cmath>
#include <limits>
#include <iostream>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"

// In pixels.
const int WIDTH = 800;
const int HEIGHT = 800;
const int DEPTH = 255;

Model *model = NULL;
int *zbuffer = NULL;
Vec3f light_dir(0, 0, -1);
// Camera is towards negative z-axis.
Vec3f camera(0, 0, 3);

// Matrix of 4x1 (homogeneous coordinates) to vector of 3 (cartesian coordinates).
Vec3f m2v(Matrix m) {
    return Vec3f(m[0][0] / m[3][0], m[1][0] / m[3][0], m[2][0] / m[3][0]);
}

// Vector of 3 (cartesian coordinates) to matrix of 4x1 (homogeneous coordinates).
Matrix v2m(Vec3f v) {
    Matrix m(4, 1);
    m[0][0] = v.x;
    m[1][0] = v.y;
    m[2][0] = v.z;
    m[3][0] = 1.f;
    return m;
}

// View port transformation.
Matrix viewport(int x, int y, int w, int h) {
    Matrix m = Matrix::identity(4);
    // Scaling.
    m[0][0] = w / 2.f;
    m[1][1] = h / 2.f;
    m[2][2] = DEPTH / 2.f;

    // Translation.
    m[0][3] = x + w / 2.f;
    m[1][3] = y + h / 2.f;
    m[2][3] = DEPTH / 2.f;
    return m;
}

// Compute barycentric coordinates.
// p = (1 - u - v) * a + u * b + v * c.
Vec3f barycentric(Vec3i a, Vec3i b, Vec3i c, Vec3i p) {
    // Set vectors for cross product.
    // Note that only x, y value is used for interpolation.
    Vec3f s[2];
    for (int i = 0; i < 2; i++) {
        s[i][0] = c[i] - a[i];
        s[i][1] = b[i] - a[i];
        s[i][2] = a[i] - p[i];
    }

    // Compute u, v in homogeneous coordinates.
    Vec3f uv_homo = s[0] ^ s[1];

    // If uv_homo[2] is 0, then the triangle is degenerate, return an arbitrary negative result.
    // Note uv_homo[2] is float type, so we cannot use uv_homo[2] == 0.
    if (abs(uv_homo[2]) < 1e-2) {
        return Vec3f(-1, 1, 1);
    }

    // Caution for the order.
    float u = uv_homo.y / uv_homo.z;
    float v = uv_homo.x / uv_homo.z;

    return Vec3f(1.f - u - v, u, v);
}

// Draw triangle.
void triangle(Vec3i *triangle, Vec2i *texture, TGAImage &image, float intensity, int *zbuffer) {
    // Compute the bounding box of the triangle.
    Vec2i bbox_min(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    Vec2i bbox_max(std::numeric_limits<int>::min(), std::numeric_limits<int>::min());
    Vec2i image_max(WIDTH - 1, HEIGHT - 1);
    for (int i = 0; i < 3; i++) {
        bbox_min.x = std::max(0, std::min(bbox_min.x, triangle[i].x));
        bbox_min.y = std::max(0, std::min(bbox_min.y, triangle[i].y));
        bbox_max.x = std::min(image_max.x, std::max(bbox_max.x, triangle[i].x));
        bbox_max.y = std::min(image_max.y, std::max(bbox_max.y, triangle[i].y));
    }


    // Iterate each pixel inside the bounding box.
    Vec3i p;
    for (p.x = bbox_min.x; p.x <= bbox_max.x; p.x++) {
        for (p.y = bbox_min.y; p.y <= bbox_max.y; p.y++) {
            // If either of the barycentric coordinatesi less than 0, the pixel is not inside the triangle.
            Vec3f bc = barycentric(triangle[0], triangle[1], triangle[2], p);
            if (bc.x < 0 || bc.y < 0 || bc.z < 0) {
                continue;
            }
            // Interpolate z value.
            p.z = triangle[0].z * bc.x + triangle[1].z * bc.y + triangle[2].z * bc.z;
            // z-buffering.
            if (zbuffer[p.x + p.y * WIDTH] < p.z) {
                zbuffer[p.x + p.y * WIDTH] = p.z;
                image.set(p.x, p.y, TGAColor((int)(255 * intensity), (int)(255 * intensity), (int)(255 * intensity), 255));
            }
        }
    }
}

int main(int argc, char** argv) {
    if (argc == 2) {
        model = new Model(argv[1]);
    } else {
        model = new Model("obj/african_head.obj");
    }

    // Init zbuffer to minus infinity.
    zbuffer = new int[WIDTH * HEIGHT];
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        zbuffer[i] = std::numeric_limits<int>::min();
    }

    // Perspective projection matrix.
    Matrix persp_proj = Matrix::identity(4);
    persp_proj[3][2] = -1.f / camera.z;

    // View port transformation matrix.
    Matrix view_port = viewport(WIDTH / 8, HEIGHT / 8, WIDTH * 3 / 4, HEIGHT * 3 / 4);

    // Init output image.
    TGAImage image(WIDTH, HEIGHT, TGAImage::RGB);

    // Draw the model.
    for (int i = 0; i < model->nfaces(); i++) {
        std::vector<int> face = model->face(i);
        Vec3i screen_coords[3];
        Vec3f world_coords[3];
        for (int j = 0; j < 3; j++) {
            Vec3f v = model->vert(face[j]);
            world_coords[j] = v;
            screen_coords[j] = m2v(view_port * persp_proj * v2m(v));
        }

        // Normal vector of the triangle.
        Vec3f n = (world_coords[2] - world_coords[0]) ^ (world_coords[1] - world_coords[0]);
        n.normalize();

        // Intensity of the light.
        float intensity = n * light_dir;

        // Backface culling.
        if (intensity > 0) {
            // Get texture coordinates.
            Vec2i uv[3];
            for (int k = 0; k < 3; k++) {
                uv[k] = model->uv(i, k);
            }
            triangle(screen_coords, uv, image, intensity, zbuffer);
        }
    }

    image.flip_vertically();
    image.write_tga_file("output.tga");

    delete model;
    delete [] zbuffer;
    return 0;
}