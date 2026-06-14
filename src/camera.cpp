#include "camera.h"
#include <cmath>//sin,cos
#include <algorithm>

void Camera::reset_uvn()
{
    // default values in assignment
    u = glm::vec3(1.0f,0.0f,0.0f);
    v = glm::vec3(0.0f,1.0f,0.0f);
    n = glm::vec3(0.0f,0.0f,-1.0f);
}

void Camera::translate_along_axis(char axis, float k)
{
    switch (axis)
    {
        case 'X':
            pos += k*u;
            break;
        case 'Y':
            pos += k*v;
            break;
        case 'Z':
            pos += k*(-n);
            break;
    }
}

void Camera::rotate_along_axis(char axis, float k)
{
    float seno = std::sin(k);
    float coss = std::cos(k);

    switch (axis)
    {
        case 'X'://pitch
            v =  v*coss + n*seno;
            n = -v*seno + n*coss;
            break;
        case 'Y'://yaw
            u =  u*coss + n*seno;
            n = -u*seno + n*coss;
            break;
        case 'Z'://roll
            u =  u*coss + v*seno;
            v = -u*seno + v*coss;
            break;
    }

    u = glm::normalize(u);
    v = glm::normalize(v);
    n = glm::normalize(n);
}

// usagem: dado um ponto lookat, recalcula uvn
void Camera::recalculate_uvn(glm::vec3 lookat)
{
    // copiado dos slides de lookat
    n = glm::normalize(pos-lookat);
    u = glm::normalize(glm::cross(v,n));
    v = glm::normalize(glm::cross(n,u));
}

void Camera::set_default_distance(glm::vec3 m_size)
{
    pos = glm::vec3(0.0f,0.0f,0.0f);

    // this works well enough
    // baseado parcialmente em
    // https://stackoverflow.com/questions/11289641/how-to-calculate-the-camera-position-so-that-a-specified-3d-model-fills-the-vie
    // https://stackoverflow.com/questions/30709002/camera-position-based-on-model-size
    float half_fov = glm::radians(60.0f) / 2.0f;
    float bigger_side = std::max(m_size.x, m_size.y) / 2.0f;

    float distance = m_size.z + (bigger_side / std::tan(half_fov));
    pos += n * distance;
}
