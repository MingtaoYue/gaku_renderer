#include <limits>
#include "model.h"
#include "our_gl.h"

// image size
constexpr int width = 800;
constexpr int height = 800;

// light direction
constexpr vec3 light_dir = {1, 1, 1};

// camera position
constexpr vec3 eye = {1, 1, 3};
// center of the scene
constexpr vec3 center = {0, 0, 0};
// up direction (may not be perpendicular to the camera lookat direction)
constexpr vec3 up = {0, 1, 0};

// mvp matrices
extern mat<4, 4> ModelView;
extern mat<4, 4> Projection;

struct Shader: IShader {
    const Model &model;
    // light direction in camera space
    vec3 uniform_l;
    // texture coordinates
    mat<2, 3> varying_uv;
    // normal vector
    mat<3, 3> varying_nrm;
    // triangle in camera space
    mat<3, 3> view_tri;

    Shader(const Model &m): model(m) {
        // transform light direction to camera space
        uniform_l = proj<3>(ModelView * embed<4>(light_dir, 0.)).normalized();
    }

    // vertex shader
    virtual void vertex(const int iface, const int nthvert, vec4 &gl_Position) {
        varying_uv.set_col(nthvert, model.uv(iface, nthvert));
        // transform normal vector to camera space, note that the matrix is the inverse transpose of that of the vertex
        varying_nrm.set_col(nthvert,  proj<3>(ModelView.invert_transpose() * embed<4>(model.normal(iface, nthvert), 0.f)));
        gl_Position = ModelView * embed<4>(model.vert(iface, nthvert));
        // transform triangle to camera space (before projection)
        view_tri.set_col(nthvert, proj<3>(gl_Position) / gl_Position[3]);
        gl_Position = Projection * gl_Position;
    }

    // fragment shader
    virtual bool fragment(const vec3 bc, TGAColor &gl_FragColor) {
        // interpolate normal vector and texture coordinates
        vec3 n = (varying_nrm * bc).normalized();
        vec2 uv = varying_uv * bc;

        // diffuse lighting
        double diff = std::max(0., n * uniform_l);
        // color from texture
        TGAColor color = sample2D(model.diffuse(), uv);
        for (int i = 0; i < 3; i++) {
            gl_FragColor[i] = std::min<int>(color[i] * diff, 255);
        }
        return false;
    }
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Please specify a model to render, like \"../obj/diablo3_pose/diablo3_pose.obj\"" << std::endl;
        return 1;
    }

    // output image
    TGAImage framebuffer(width, height, TGAImage::RGB);

    // set mvp and viewport matrices
    lookat(eye, center, up);
    projection(-1.f / (eye - center).norm());
    viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);

    // init zbuffer to minus infinity
    std::vector<double> zbuffer(width * height, -std::numeric_limits<double>::max());

    // load each model
    for (int m = 1; m < argc; m++) {
        Model model(argv[m]);
        Shader shader(model);
        for (int i = 0; i < model.nfaces(); i++) {
            // set by vertex shader, used by fragment shader
            vec4 clip_verts[3];
            for (int j = 0; j < 3; j++) {
                shader.vertex(i, j, clip_verts[j]);
            }
            triangle(clip_verts, shader, framebuffer, zbuffer);
        }
    }

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}