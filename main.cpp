#include <vector>
#include <cmath>
#include <limits>
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

// Draw triangle.
void triangle(Vec3i *triangle, Vec2i *texture, TGAImage &image, float intensity, int *zbuffer) {
    Vec3i t0 = triangle[0];
    Vec3i t1 = triangle[1];
    Vec3i t2 = triangle[2];
    Vec2i uv0 = texture[0];
    Vec2i uv1 = texture[1];
    Vec2i uv2 = texture[2];

    // Ignore degenerate triangles.
    if (t0.y == t1.y && t0.y == t2.y) {
        return;
    }

    // Sort vertices by y coordinate.
    if (t0.y > t1.y) {
        std::swap(t0, t1);
        std::swap(uv0, uv1);
    }
    if (t0.y > t2.y) {
        std::swap(t0, t2);
        std::swap(uv0, uv2);
    }
    if (t1.y > t2.y) {
        std::swap(t1, t2);
        std::swap(uv1, uv2);
    }

    int total_height = t2.y - t0.y;
    for (int i = 0; i < total_height; i++) {
        bool second_half = i > (t1.y - t0.y) || t0.y == t1.y;
        int segment_height = second_half ? t2.y - t1.y : t1.y - t0.y;
        // Bilinear interpolation.
        float alpha = (float)i / total_height;
        float beta = second_half ? (float)(i - (t1.y - t0.y)) / segment_height : (float)i / segment_height;
        Vec3i a = t0 + (t2 - t0) * alpha;
        Vec3i b = second_half ? t1 + (t2 - t1) * beta : t0 + (t1 - t0) * beta;
        Vec2i uv_a = uv0 + (uv2 - uv0) * alpha;
        Vec2i uv_b = second_half ? uv1 + (uv2 - uv1) * beta : uv0 + (uv1 - uv0) * beta;
        if (a.x > b.x) {
            std::swap(a, b);
            std::swap(uv_a, uv_b);
        }
        for (int j = a.x; j <= b.x; j++) {
            float phi = a.x == b.x ? 1.f : (float)(j - a.x) / (b.x - a.x);
            Vec3i p = Vec3f(a) + Vec3f(b - a) * phi;
            Vec2i uv_p = uv_a + (uv_b - uv_a) * phi;
            int idx = p.x + p.y * WIDTH;
            // z-buffering.
            if (zbuffer[idx] < p.z) {
                zbuffer[idx] = p.z;
                TGAColor color = model->diffuse(uv_p);
                image.set(p.x, p.y, TGAColor(color.r * intensity, color.g * intensity, color.b * intensity));
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