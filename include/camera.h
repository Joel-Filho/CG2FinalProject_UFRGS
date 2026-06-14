#ifndef CAMERA_H
#define CAMERA_H

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/geometric.hpp>

struct Camera
{
    glm::vec3 pos;
    glm::vec3 u;
    glm::vec3 v;
    glm::vec3 n;

    void reset_uvn();
    void translate_along_axis(char axis, float k);
    void rotate_along_axis(char axis, float k);
    void recalculate_uvn(glm::vec3 lookat);
    void set_default_distance(glm::vec3 m_size);
};

#endif // CAMERA_H
