#include <vector>
#include <cmath>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"

using namespace std;

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red = TGAColor(255, 0, 0, 255);
const int width = 1000;
const int height = 1000;
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

int main(int argc, char** argv) {
    model = new Model("obj/african_head.obj");
    TGAImage image(width, height, TGAImage::RGB);
    
    // Draw the model.
    for (int i = 0; i < model->nfaces(); i++) {
        vector<int> face = model->face(i);
        for (int j = 0; j < 3; j++) {
            Vec3f v0 = model->vert(face[j]);
            Vec3f v1 = model->vert(face[(j + 1) % 3]);
            // Original range is [-1, 1], scale to [0, width], [0, height].
            int x0 = (v0.x + 1) * width / 2;
            int y0 = (v0.y + 1) * height / 2;
            int x1 = (v1.x + 1) * width / 2;
            int y1 = (v1.y + 1) * height / 2;
            line(x0, y0, x1, y1, image, white);
        }
    }

    // Origin at the left bottom corner of the image.
    image.flip_vertically();
    image.write_tga_file("output.tga");
    delete model;
    return 0;
}