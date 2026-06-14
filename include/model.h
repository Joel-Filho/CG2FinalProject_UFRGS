#ifndef MODEL_H
#define MODEL_H

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Triangle
{
    Triangle();

    glm::vec4 vertex[3];
    glm::vec4 normal[3];
    glm::vec4 color[3];
    glm::vec2 texcoord[3];
    int material_index[3];
    glm::vec4 face_normal;
};

struct Material
{
    Material();

    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float shine;
};

struct Model
{
    Model();
    virtual ~Model();

    int num_triangles;
    int num_materials;

    bool has_texture;
    Triangle* triangles;
    Material* materials;

    glm::vec3 bbox_min;
    glm::vec3 bbox_max;

    void read_from_file(const char *FileName);
    void write_to_file(const char *FileName);
    void tris_to_arrays(float* &vert, float* &norm, float* &tex);
};

#endif // MODEL_H
