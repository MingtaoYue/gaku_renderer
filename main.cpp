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
Vec3f light_dir = Vec3f(1, -1, 1).normalize();

// Camera position, center of the scene, up vector.
// The camera is looking at the center of the scene, the length of the look-at vector is the focal length.
// up is perpendicular to the camera look-at vector.
Vec3f camera(1, 1, 3);
Vec3f center(0, 0, 0);
Vec3f up(3, 0, -3);

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

Matrix lookat(Vec3f camera, Vec3f center, Vec3f up) {
    // Camera look at minus z axis.
    Vec3f z = (camera - center).normalize();
    Vec3f y = up.normalize();
    Vec3f x = (y ^ z).normalize();
    Matrix m = Matrix::identity(4);
    // Inverse of rotation matrix is its transpose.
    for (int i = 0; i < 3; i++) {
        m[0][i] = x[i];
        m[1][i] = y[i];
        m[2][i] = z[i];
        m[i][3] = -center[i];
    }
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
        Vec3i a = t0 + Vec3f(t2 - t0) * alpha;
        // Caution about the int-float cast.
        Vec3i b = second_half ? t1 + Vec3f(t2 - t1) * beta : t0 + Vec3f(t1 - t0) * beta;
        Vec2i uv_a = uv0 + (uv2 - uv0) * alpha;
        Vec2i uv_b = second_half ? uv1 + (uv2 - uv1) * beta : uv0 + (uv1 - uv0) * beta;
        if (a.x > b.x) {
            std::swap(a, b);
            std::swap(uv_a, uv_b);
        }
        for (int j = a.x; j <= b.x; j++) {
            float phi = a.x == b.x ? 1.f : (float)(j - a.x) / (float)(b.x - a.x);
            Vec3i p = Vec3f(a) + Vec3f(b - a) * phi;
            Vec2i uv_p = uv_a + (uv_b - uv_a) * phi;
            int idx = p.x + p.y * WIDTH;
            // z-buffering.
            if (zbuffer[idx] < p.z) {
                zbuffer[idx] = p.z;
                TGAColor color = model->diffuse(uv_p);
                image.set(p.x, p.y, TGAColor(255 * intensity, 255 * intensity, 255 * intensity));
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

    // Model transformation matrix.
    Matrix model_trans = Matrix::identity(4);

    // View (camera) transformation matrix.
    Matrix view_trans = lookat(camera, center, up);

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
            screen_coords[j] = Vec3f(view_port * persp_proj * view_trans * model_trans * Matrix(v));
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