#include <vector>
#include <cmath>
#include <iostream>
#include <limits>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"

using namespace std;

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red = TGAColor(255, 0, 0, 255);
const TGAColor blue = TGAColor(0, 0, 255, 255);

const int width = 10000;
const int height = 10000;

Model *model = NULL;

// Compute barycentric coordinates using cross product.
Vec3f barycentric(Vec2i *triangle, Vec2i p) {
    // Compute barycentric coordinate u, v in homogeneous coordinates.
    Vec3f uv_homo = Vec3f(triangle[2].x- triangle[0].x, triangle[1].x - triangle[0].x, triangle[0].x - p.x) ^ Vec3f(triangle[2].y - triangle[0].y, triangle[1].y - triangle[0].y, triangle[0].y - p.y);
    
    // If abs(uv_homo[2]) < 1, it means the triangle is degenerate, as the points all have integer value. 
    // In that case, return some arbitrary negative coordinates.
    if (abs(uv_homo.z) < 1) return Vec3f(-1, 1, 1);
    
    // Normalize the homogenous coordinates.
    return Vec3f(1. - (uv_homo.x - uv_homo.y) / uv_homo.z, uv_homo.x / uv_homo.z, uv_homo.y / uv_homo.z);
}

// Triangle drawing function.
void triangle(Vec3i *triangle, int *zbuffer, TGAImage &image, TGAColor color) {
    Vec2i triangle_2d[3];
    for (int i = 0; i < 3; i++) {
        triangle_2d[i] = Vec2i(triangle[i].x, triangle[i].y);
    }
    // Bounding box.
    int x_max = max(max(triangle[0].x, triangle[1].x), triangle[2].x);
    int x_min = min(min(triangle[0].x, triangle[1].x), triangle[2].x);
    int y_max = max(max(triangle[0].y, triangle[1].y), triangle[2].y);
    int y_min = min(min(triangle[0].y, triangle[1].y), triangle[2].y);

    for (int x = x_min; x <= x_max; x++) {
        for (int y = y_min; y <= y_max; y++) {
            Vec2i p = Vec2i(x, y);
            // If not inside the triangle, skip.
            Vec3f bc = barycentric(triangle_2d, p);
            if (bc.x < 0 || bc.y < 0 || bc.z < 0) continue;

            // z value is the interpolation of the 3 vertices using barycentric coordinates.
            float z = triangle[0].z * bc.x + triangle[1].z * bc.y + triangle[2].z * bc.z;
            if (zbuffer[x + y * width] < z) {
                zbuffer[x + y * width] = (int)z;
                image.set(x, y, color);
            }
        }
    }
}

int main(int argc, char** argv) {
    TGAImage image(width, height, TGAImage::RGB);

    model = new Model("obj/african_head.obj");
    Vec3f light_dir(0, 0, -1);

    // zbuffer to store depth info, initial value is set to minus infinity.
    // 1D-2D conversion: idx = x + y * width, x = idx % width, y = idx / width.
    int *zbuffer = new int[width * height];
    for (int i = 0; i < width * height; i++) {
        zbuffer[i] = numeric_limits<int>::min();
    }

    for (int i = 0; i < model->nfaces(); i++) {
        vector<int> face = model->face(i);
        Vec3i screen_coords[3];
        Vec3f world_coords[3];
        for (int j = 0; j < 3; j++) {
            Vec3f v = model->vert(face[j]);
            screen_coords[j] = Vec3i((v.x + 1.) * width / 2., (v.y + 1.) * height / 2., v.z);
            world_coords[j] = v;
        }
        Vec3f n = (world_coords[2] - world_coords[0]) ^ (world_coords[1] - world_coords[0]); 
        n.normalize();
        float intensity = n * light_dir;
        if (intensity > 0) {
            triangle(screen_coords, zbuffer, image, TGAColor(intensity * 255, intensity * 255, intensity * 255, 255));
        }   
    }

    // Origin at the left bottom corner of the image.
    image.flip_vertically();
    image.write_tga_file("output.tga");
    delete model;
    return 0;
}