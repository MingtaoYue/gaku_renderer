#include <vector>
#include <cmath>
#include <iostream>
#include <limits>
#include <algorithm>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"

using namespace std;

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red = TGAColor(255, 0, 0, 255);
const TGAColor blue = TGAColor(0, 0, 255, 255);

const int width = 1000;
const int height = 1000;

Model *model = NULL;

// Compute barycentric coordinates.
Vec3f barycentric(Vec3f a, Vec3f b, Vec3f c, Vec3f p) {
    // Set cross product vectors.
    // Note that only x, y value is used for interpolation.
    Vec3f s[2];
    for (int i = 0; i < 2; i++) {
        s[i][0] = c[i] - a[i];
        s[i][1] = b[i] - a[i];
        s[i][2] = a[i] - p[i];
    }

    // Compute u, v in homogeneous coordinates.
    Vec3f uv_homo = cross(s[0], s[1]);

    // If uv_homo[2] is 0, then the triangle is degenerate, return an arbitrary negative result.
    // Note that uv_homo[2] is float type.
    if (abs(uv_homo[2]) < 1e-2) return Vec3f(-1, 1, 1);
    
    // Return the normalized (1 - u - v), u, v.
    return Vec3f(1.f - (uv_homo.x + uv_homo.y) / uv_homo.z, uv_homo.y / uv_homo.z, uv_homo.x / uv_homo.z);
}

// Draw triangle.
void triangle(Vec3f *triangle, float *zbuffer, TGAImage &image, TGAColor color) {
    // Compute the bounding box of the triangle.
    Vec2f bbox_min(numeric_limits<float>::max(), numeric_limits<float>::max());
    Vec2f bbox_max(-numeric_limits<float>::max(), -numeric_limits<float>::max());
    Vec2f image_max(image.get_width() - 1, image.get_height() - 1);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 2; j++) {
            bbox_min[j] = max(0.f, min(bbox_min[j], triangle[i][j]));
            bbox_max[j] = min(image_max[j], max(bbox_max[j], triangle[i][j]));
        }
    }

    // Iterate each pixel inside the bounding box.
    Vec3f p;
    for (p.x = bbox_min.x; p.x <= bbox_max.x; p.x++) {
        for (p.y = bbox_min.y; p.y <= bbox_max.y; p.y++) {
            // If either of the barycentric coordinates is less than 0, it means the pixel is not inside the triangle, then skip.
            Vec3f bc = barycentric(triangle[0], triangle[1], triangle[2], p);
            if (bc.x < 0 || bc.y < 0 || bc.z < 0) continue;

            // Interpolate the z value by the barycentric coordinates.
            p.z = triangle[0].z * bc.x + triangle[1].z * bc.y + triangle[2].z * bc.z;

            // Z-buffering.
            if (zbuffer[int(p.x + p.y * width)] < p.z) {
                zbuffer[int(p.x + p.y * width)] = p.z;
                image.set(p.x, p.y, color);
            }
        }
    }
}

// Convert the (normalized) world coordinate to the screen coordinate.
// Note that the expected range of the world coordinate is [-1, 1];
Vec3f world2screen(Vec3f v) {
    return Vec3f((int)((v.x + 1.) * width / 2. + .5), (int)((v.y + 1.) * height / 2. + .5), v.z);
}

int main(int argc, char** argv) {
    // Set image.
    TGAImage image(width, height, TGAImage::RGB);

    // Load model.
    model = new Model("obj/african_head.obj");

    // Set light direction.
    Vec3f light_dir(0, 0, -1);

    // Init z-buffer to minus infinity.
    // 1D-2D conversion: idx = x + y * width, x = idx % width, y = idx / width.
    float *zbuffer = new float[width * height];
    for (int i = 0; i < width * height; i++) {
        zbuffer[i] = -numeric_limits<float>::max();
    }
    
    // Draw each face from the model.
    for (int i = 0; i < model->nfaces(); i++) {
        vector<int> face = model->face(i);
        
        // Set world and screen coordinates.
        Vec3f screen_coords[3];
        Vec3f world_coords[3];
        for (int j = 0; j < 3; j++) {
            world_coords[j] = model->vert(face[j]);
            screen_coords[j] = world2screen(world_coords[j]);
        }

        // Compute the normal vector.
        Vec3f n = cross(world_coords[2] - world_coords[0], world_coords[1] - world_coords[0]); 
        n.normalize();

        // Compute the intensity of the light.
        float intensity = n * light_dir;

        // Backface culling.
        if (intensity > 0) {
            triangle(screen_coords, zbuffer, image, TGAColor(intensity * 255, intensity * 255, intensity * 255, 255));
        }   
    }

    // Origin at the left bottom corner of the image.
    image.flip_vertically();
    image.write_tga_file("output.tga");

    // Also draw the related depth map.
    TGAImage depth_map(width, height, TGAImage::RGB);

    // Get the min and max value of the z-buffer, except for the pixels with value of minus infinity.
    float zbuffer_min = numeric_limits<float>::max();
    float zbuffer_max = -numeric_limits<float>::max();
    for (int i = 0; i < width * height; i++) {
        if (zbuffer[i] > -numeric_limits<float>::max()) {
            zbuffer_min = min(zbuffer_min, zbuffer[i]);
            zbuffer_max = max(zbuffer_max, zbuffer[i]);
        }
    }

    for (int i = 0; i < width * height; i++) {
        // Normalize the z-buffer to [0, 1], except for the pixels with value of minus infinity.
        if (zbuffer[i] > -numeric_limits<float>::max()) {
            zbuffer[i] = (zbuffer[i] - zbuffer_min) / (zbuffer_max - zbuffer_min);
            int x = i % width;
            int y = i / width;
            depth_map.set(x, y, TGAColor(zbuffer[i] * 255, zbuffer[i] * 255, zbuffer[i] * 255, 255));
        }
    }

    // Origin at the left bottom corner of the image.
    depth_map.flip_vertically();
    depth_map.write_tga_file("depth_map.tga");
    
    // Release memory.
    delete model;
    return 0;
}