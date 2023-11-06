#include <vector>
#include <limits>
#include <iostream>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "our_gl.h"

Model *model = NULL;
float *shadowbuffer = NULL;

// pixel size of the output image
const int width = 800;
const int height = 800;

// light direction (parallel light)
Vec3f light_dir(1, 1, 0);

// camera position
Vec3f eye(1,1,4);
// center of the scene, length from the camera to the center is the focal length
Vec3f center(0,0,0);
// camera up vector, may not be perpendicular to the view vector
Vec3f up(0,1,0);

struct DepthShader: public IShader {
    mat<3, 3, float> varying_tri;

    DepthShader() {
        varying_tri = mat<3, 3, float>();
    }

    virtual Vec4f vertex(int iface, int nthvert) {
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        gl_Vertex = Viewport * Projection * ModelView * gl_Vertex;
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor &color) {
        Vec3f p = varying_tri * bar;
        color = TGAColor(255, 255, 255) * (p.z / depth);
        return false;
    }
};

struct Shader: public IShader {
    // varying- variables are set by the vertex shader and interpolated by the fragment shader
    // texture coordinates
    mat<2,3,float> varying_uv;
    // triangle coordinates (clip coordinates)
    mat<3,3,float> varying_tri;

    // uniform- variables are passed from the application to the shader
    // transform from the output image screen space to the shadow buffer screen space
    mat<4, 4, float> uniform_MShadow;


    virtual Vec4f vertex(int iface, int nthvert) {
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        Vec4f gl_Vertex = Viewport * Projection * ModelView * embed<4>(model->vert(iface, nthvert));
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bc, TGAColor &color) {
        Vec4f p = embed<4>(varying_tri * bc);
        // corresponding coordinates in the shadow buffer
        Vec4f shadowbuffer_p = uniform_MShadow * p;
        Vec3f shadowbuffer_p_cartesian = proj<3>(shadowbuffer_p / shadowbuffer_p[3]);
        // index in the shadow buffer array
        int idx = int(shadowbuffer_p_cartesian.x) + int(shadowbuffer_p_cartesian.y) * width;
        // if the (reprojected) depth of the fragment is smaller than that of the shadow buffer, then the fragment is in shadow
        float shadow = shadowbuffer[idx] < shadowbuffer_p_cartesian.z ? 1 : 0;
        color = TGAColor(255, 255, 255) * shadow;
        return false;
    }
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Enter model path such as \"obj/african_head.obj\"" << std::endl;
        return 1;
    }

    // initialize the zbuffer and shadow buffer, depth is set to negative infinity
    float *zbuffer = new float[width * height];
    shadowbuffer = new float[width * height];
    for (int i = 0; i < width * height; i++) {
        zbuffer[i] = -std::numeric_limits<float>::max();
        shadowbuffer[i] = -std::numeric_limits<float>::max();
    }

    model = new Model(argv[1]);
    light_dir.normalize();
    
    // render the shadow buffer
    {
        TGAImage depthimage(width, height, TGAImage::RGB);
        // place the camera at the light source position, here the position is equal to the direction
        lookat(light_dir, center, up);
        // focal length is set to infinity to achieve parallel light
        projection(0);
        viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);
        
        DepthShader depthshader;
        Vec4f screen_coords[3];
        for (int i = 0; i < model->nfaces(); i++) {
            for (int j = 0; j < 3; j++) {
                screen_coords[j] = depthshader.vertex(i, j);
            }
            // shadow buffer is the zbuffer when the camera is at the light source position
            triangle(screen_coords, depthshader, depthimage, shadowbuffer);
        }
        depthimage.flip_vertically();
        depthimage.write_tga_file("depthimage.tga");
    }

    // matrix to transform from the world space to the shadow buffer screen space
    Matrix M = Viewport * Projection * ModelView;

    // render the image
    {
        TGAImage frame(width, height, TGAImage::RGB);
        lookat(eye, center, up);
        projection(-1.f / (eye - center).norm());
        viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);

        Shader shader;
        // first transform from the image screen space back to the world space, then to the shadow buffer screen space
        shader.uniform_MShadow = M * ((Viewport * Projection * ModelView).invert());
        Vec4f screen_coords[3];
        for (int i = 0; i < model->nfaces(); i++) {
            for (int j = 0; j < 3; j++) {
                screen_coords[j] = shader.vertex(i, j);
            }
            triangle(screen_coords, shader, frame, zbuffer);
        }
        frame.flip_vertically();
        frame.write_tga_file("output.tga");
    }

    delete model;
    delete [] zbuffer;
    delete [] shadowbuffer;
    return 0;
}

