# include "our_gl.h"

// model + view, projection, viewport transformation matrix
mat<4, 4> ModelView;
mat<4, 4> Projection;
mat<4, 4> Viewport;

// model + view transformation
void lookat(const vec3 eye, const vec3 center, const vec3 up) {
    // eye is the position of the camera, and center is the center of the scene, the distance is the focal length, the camera looks at -z direction
    vec3 z = (eye - center).normalized();
    // up is not necessarily perpendicular to z, only vertical after projection onto the screen, so it cannot be used directly as y
    vec3 x = cross(up, z).normalized();
    vec3 y = cross(z, x).normalized();
    // world -> camera, the inverse of rotation matrix is its transpose
    mat<4, 4> rotation = {{{x.x, x.y, x.z, 0}, {y.x, y.y, y.z, 0}, {z.x, z.y, z.z, 0}, {0, 0, 0, 1}}};
    mat<4, 4> translation = {{{1, 0, 0, -eye.x}, {0, 1, 0, -eye.y}, {0, 0, 1, -eye.z}, {0, 0, 0, 1}}};
    ModelView = rotation * translation;
}

// perspective projection transformation, coeff = -1 / f, where f is focal length, coeff = 0 means orthographic projection
void projection(const double coeff) {
    Projection = {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, coeff, 1}}};
}

// viewport transformation from x,y both [-1, 1] to [x, x + w], [y, y + h]
void viewport(const int x, const int y, const int w, const int h) {
    // scaling and translation
    Viewport = {{{w / 2., 0, 0, x + w / 2.}, {0, h / 2., 0, y + h / 2.}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
}

// compute barycentric coordinate of P in triangle tri
vec3 barycentric(const vec2 tri[3], const vec2 P) {
    // Mu = p -> u = M.inverse * p, where M is formed by triangle coordinates (columns), u is barycentric coordinates
    mat<3, 3> M = {{embed<3>(tri[0]), embed<3>(tri[1]), embed<3>(tri[2])}};
    // if determinant of M is 0, then the triangle is degenerate (A, B, C are in the same line)
    if (M.det() < 1e-3)
        // return an arbitrary negative value and the shader will ignore it
        return {-1, 1, 1};
    return M.invert_transpose() * embed<3>(P);
}

// draw triangle
void triangle(const vec4 clip_verts[3], IShader &shader, TGAImage &image, std::vector<double> &zbuffer) {
    // canonical frustum -> screen space
    vec4 pts[3] = {Viewport * clip_verts[0], Viewport * clip_verts[1], Viewport * clip_verts[2]};
    // 3d homogeneous -> 2d cartesian
    vec2 pts_xy[3] = {proj<2>(pts[0] / pts[0][3]), proj<2>(pts[1] / pts[1][3]), proj<2>(pts[2] / pts[2][3])};

    // set bounding box of the triangle
    int bboxmin[2] = {image.width() - 1, image.height() - 1};
    int bboxmax[2] = {0, 0};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 2; j++) {
            bboxmin[j] = std::min(bboxmin[j], static_cast<int>(pts_xy[i][j]));
            bboxmax[j] = std::max(bboxmax[j], static_cast<int>(pts_xy[i][j]));
        }
    }

    #pragma omp parallel for
    // iterate over each pixel in the bounding box
    for (int x = std::max(0, bboxmin[0]); x <= std::min(bboxmax[0], image.width() - 1); x++) {
        for (int y = std::max(0, bboxmin[1]); y <= std::min(bboxmax[1], image.height() - 1); y++) {
            // barycentric coordinates of the pixel
            vec3 bc = barycentric(pts_xy, {static_cast<double>(x), static_cast<double>(y)});
            // interpolated depth of the pixel
            double frag_depth = vec3{pts[0][2] / pts[0][3], pts[1][2] / pts[1][3], pts[2][2] / pts[2][3]} * bc;
            // if the pixel is not inside the triangle (either of the barycentric coordinates is negative), or the depth is smaller than the zbuffer, then skip
            if (bc.x < 0 || bc.y < 0 || bc.z < 0 || frag_depth < zbuffer[x + y * image.width()])
                continue;
                
            TGAColor color;
            // fragment shader can discard the pixel
            if (shader.fragment(bc, color))
                continue;
            // update zbuffer with current depth
            zbuffer[x + y * image.width()] = frag_depth;
            image.set(x, y, color);
        }
    }
}