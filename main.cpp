#include <vector>
#include <cmath>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"

using namespace std;

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red = TGAColor(255, 0, 0, 255);
const TGAColor blue = TGAColor(0, 0, 255, 255);
const int width = 500;
const int height = 500;
Model *model = NULL;

// Line drawing function (Bresenham's line drawing algorithm).
void line(int x0, int y0, int x1, int y1, TGAImage &image, TGAColor color) {
    // If the line is steep, we swap x and y.
    // This is to ensure that the sampling frequency of x is higher than y.
    bool steep = false;
    if (abs(x0 - x1) < abs(y0 - y1)) {
        steep = true;
        swap(x0, y0);
        swap(x1, y1);
    }
    // To ensure x0 < x1.
    if (x0 > x1) {
        swap(x0, x1);
        swap(y0, y1);
    }
    int dx = x1 - x0;
    int dy = y1 - y0;
    // Normalize the slope and error by multiplying (2 * dx) to avoid floating point calculation.
    int slope_norm = abs(dy) *2;
    int error_norm = 0;
    int y = y0;
    for (int x = x0; x <= x1; x++) {
        if (!steep) {
            image.set(x, y, color);
        } else {
            image.set(y, x, color);
        }
        error_norm += slope_norm;
        if (error_norm > dx) {
            y += (y1 > y0 ? 1 : -1);
            error_norm -= dx * 2;
        }
    }
}

void triangle(Vec2i t0, Vec2i t1, Vec2i t2, TGAImage &image, TGAColor color) {
    // Sort the vertices by y.
    if (t0.y > t1.y) {
        swap(t0, t1);
    }
    if (t0.y > t2.y) {
        swap(t0, t2);
    }
    if (t1.y > t2.y) {
        swap(t1, t2);
    }
    // Line sweeping.
    int total_height = t2.y - t0.y;
    int lower_height = t1.y - t0.y;
    int upper_height = t2.y - t1.y;
    // Draw lower half;
    for (int y = t0.y; y <= t1.y; y++) {
        float alpha = (y - t0.y) / (float)lower_height;
        float beta = (y - t0.y) / (float)total_height;
        Vec2i A = t0 + (t1 - t0) * alpha;
        Vec2i B = t0 + (t2 - t0) * beta;
        if (A.x > B.x) {
            swap(A, B);
        }
        for (int x = A.x; x <= B.x; x++) {
            image.set(x, y, color);
        }
    }
    // Draw upper half;
    for (int y = t1.y; y <= t2.y; y++) {
        float alpha = (y - t1.y) / (float)upper_height;
        float beta = (y - t0.y) / (float)total_height;
        Vec2i A = t1 + (t2 - t1) * alpha;
        Vec2i B = t0 + (t2 - t0) * beta;
        if (A.x > B.x) {
            swap(A, B);
        }
        for (int x = A.x; x <= B.x; x++) {
            image.set(x, y, color);
        }
    }
}

int main(int argc, char** argv) {
    TGAImage image(width, height, TGAImage::RGB);
    Vec2i triangle0[3] = {Vec2i(10, 70), Vec2i(50, 160), Vec2i(70, 80)};
    Vec2i triangle1[3] = {Vec2i(180, 50), Vec2i(150, 1), Vec2i(70, 180)};
    Vec2i triangle2[3] = {Vec2i(180, 150), Vec2i(120, 160), Vec2i(130, 180)};
    triangle(triangle0[0], triangle0[1], triangle0[2], image, red);
    triangle(triangle1[0], triangle1[1], triangle1[2], image, white);
    triangle(triangle2[0], triangle2[1], triangle2[2], image, blue);
    // Draw the model.
    // model = new Model("obj/diablo3_pose.obj");
    // for (int i = 0; i < model->nfaces(); i++) {
    //     vector<int> face = model->face(i);
    //     for (int j = 0; j < 3; j++) {
    //         Vec3f v0 = model->vert(face[j]);
    //         Vec3f v1 = model->vert(face[(j + 1) % 3]);
    //         // Original range is [-1, 1], scale to [0, width], [0, height].
    //         int x0 = (v0.x + 1) * width / 2;
    //         int y0 = (v0.y + 1) * height / 2;
    //         int x1 = (v1.x + 1) * width / 2;
    //         int y1 = (v1.y + 1) * height / 2;
    //         line(x0, y0, x1, y1, image, white);
    //     }
    // }

    // Origin at the left bottom corner of the image.
    image.flip_vertically();
    image.write_tga_file("output.tga");
    delete model;
    return 0;
}