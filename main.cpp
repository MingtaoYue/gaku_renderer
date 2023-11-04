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

    Matrix M = Viewport * Projection * ModelView;

    delete model;
    delete [] zbuffer;
    delete [] shadowbuffer;
    return 0;
}

