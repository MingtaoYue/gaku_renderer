#include <vector>
#include <limits>
#include <iostream>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "our_gl.h"

Model *model = NULL;

// pixel size of the output image
const int width = 800;
const int height = 800;

// light direction
Vec3f light_dir(1,1,1);

// camera position
Vec3f eye(1,1,3);
// center of the scene, length from the camera to the center is the focal length
Vec3f center(0,0,0);
// camera up vector, may not be perpendicular to the view vector
Vec3f up(0,1,0);

struct Shader: public IShader {
    // varying- variables are set by the vertex shader and interpolated by the fragment shader
    // texture coordinates
    mat<2,3,float> varying_uv;
    // triangle coordinates (clip coordinates)
    mat<4,3,float> varying_tri;
    // normal vector
    mat<3,3,float> varying_nrm;
    // triangle coordinates in normalized device coordinates
    mat<3,3,float> varying_ndc_tri;

    virtual Vec4f vertex(int iface, int nthvert) {
        // transform the vertex coordinates from world space to canonical view volume (viewport is not applied yet)
        Vec4f gl_Vertex = Projection * ModelView * embed<4>(model->vert(iface, nthvert));
        varying_tri.set_col(nthvert, gl_Vertex);

        // note that the transformation matrix of normal vector is the inverse transpose of the transformation matrix of vertex
        varying_nrm.set_col(nthvert, proj<3>((Projection * ModelView).invert_transpose() * embed<4>(model->normal(iface, nthvert), 0.f)));

        // texture coordinates
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        
        // homogeneous to cartesian coordinates
        varying_ndc_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bc, TGAColor &color) {
        // interpolate the normal vector and texture coordinates, normal vector is also the k vector of the Darboux basis
        Vec3f bn = (varying_nrm * bc).normalize();
        Vec2f uv = varying_uv * bc;

        // set matrix A to compute Darboux basis (tangent space)
        mat<3,3,float> A;
        A[0] = varying_ndc_tri.col(1) - varying_ndc_tri.col(0);
        A[1] = varying_ndc_tri.col(2) - varying_ndc_tri.col(0);
        A[2] = bn;
        mat<3,3,float> A_inverse = A.invert();

        // compute Darboux basis i, j which are gradients of u, v
        Vec3f i = A_inverse * Vec3f(varying_uv[0][1] - varying_uv[0][0], varying_uv[0][2] - varying_uv[0][0], 0);
        Vec3f j = A_inverse * Vec3f(varying_uv[1][1] - varying_uv[1][0], varying_uv[1][2] - varying_uv[1][0], 0);

        // set Darboux basis
        mat<3,3,float> darboux_basis;
        darboux_basis.set_col(0, i.normalize());
        darboux_basis.set_col(1, j.normalize());
        darboux_basis.set_col(2, bn);
        
        // transform the normal vector from Darboux basis to world space
        Vec3f n = (darboux_basis * model->normal(uv)).normalize();

        // diffusion
        float diff = std::max(0.f, n * light_dir);
        color = model->diffuse(uv) * diff;

        return false;
    }
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Enter model path such as \"obj/african_head.obj\"" << std::endl;
        return 1;
    }

    // initialize the zbuffer, depth is set to negative infinity
    float *zbuffer = new float[width * height];
    for (int i = 0; i < width * height; i++) {
        zbuffer[i] = -std::numeric_limits<float>::max();
    }

    // output image
    TGAImage image(width, height, TGAImage::RGB);

    // (model,) view, projection, viewport matrices
    lookat(eye, center, up);
    projection(-1.f / (eye - center).norm());
    viewport(0, 0, width, height);
    
    // transform the light direction
    light_dir = proj<3>((Projection * ModelView * embed<4>(light_dir, 0.f))).normalize();

    // draw each triangle of each model
    for (int m = 1; m < argc; m++) {
        model = new Model(argv[m]);
        Shader shader;
        for (int i = 0; i < model->nfaces(); i++) {
            for (int j = 0; j < 3; j++) {
                shader.vertex(i, j);
            }
            triangle(shader.varying_tri, shader, image, zbuffer);
        }
        delete model;
    }
    // place the origin in the bottom left corner of the image
    image.flip_vertically();
    image.write_tga_file("output.tga");

    delete [] zbuffer;
    return 0;
}

