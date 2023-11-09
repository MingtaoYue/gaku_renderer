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