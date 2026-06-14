#version 400 core

layout( location = 0 ) in vec4 vPosition;
layout( location = 1 ) in vec4 vNormal;
layout( location = 2 ) in vec2 vTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform float ambientStrength;
uniform float specularStrength;
uniform float shininess;

subroutine vec3 gouraudD(vec4 pos, vec4 normal);
subroutine vec3 gouraudS(vec4 pos, vec4 normal);

subroutine uniform gouraudD vertexDiff;
subroutine uniform gouraudS vertexSpec;

out vec4 position_world;
out vec4 normal;
out vec2 TexCoord;

out vec3 gouraudAmbient;
out vec3 gouraudDiffuse;
out vec3 gouraudSpecular;

void main()
{
    position_world = model * vPosition;
    normal = inverse(transpose(model)) * vNormal; //normal em world
    normal.w = 0.0;

    gl_Position = projection * view * position_world;
    TexCoord = vTexCoord;

    gouraudAmbient  = vec3(1.0, 1.0, 1.0) * ambientStrength;
    gouraudDiffuse  = vertexDiff(position_world, normal);
    gouraudSpecular = vertexSpec(position_world, normal);
}

// (assumindo que luz vem da câmera)
subroutine (gouraudD, gouraudS) vec3 noLighting(vec4 pos, vec4 normal) {
	return vec3(0.0, 0.0, 0.0);
}

subroutine (gouraudD) vec3 gDiffuse(vec4 pos, vec4 normal) {

	vec3 result;

    // Light position is specified in camera coordinates.
    vec4 light_camera = vec4(2.0, 2.0, 2.0, 1.0);
    vec4 light_world  = inverse(view) * light_camera;

    vec4 p = pos;
    vec4 n = normalize(normal);

    // Light vector from surface point to light in world space.
    vec4 l = normalize(light_world - p);

    vec3 I = vec3(1.0,1.0,1.0); // light spectrum

    // Lambert diffuse term
    result = I * max(0, dot(n, l));

    return result;
}

subroutine (gouraudS) vec3 gSpecular(vec4 pos, vec4 normal) {

	vec3 result;

    vec4 light_camera = vec4(2.0, 2.0, 2.0, 1.0);
    vec4 light_world  = inverse(view) * light_camera;
    vec4 eye_world    = inverse(view) * vec4(0.0, 0.0, 0.0, 1.0);

    vec4 p = pos;
    vec4 n = normalize(normal);

    // Light vector from surface point to light in world space.
    vec4 l = normalize(light_world - p);

    // View vector from surface point to eye in world space.
    vec4 v = normalize(eye_world - p);

    // Vetor que define o sentido da reflex�o especular ideal.
    vec4 r = -l + 2*n*dot(n,l);

    // Expoente especular para o modelo de ilumina��o de Phong
    float q = shininess;

    vec3 I = vec3(1.0,1.0,1.0); // espectro da fonte de luz

    // Termo especular utilizando o modelo de ilumina��o de Phong
    result = I*pow(max(0,dot(v,r)),q)*specularStrength; // s� o coeficiente

    return result;
}
