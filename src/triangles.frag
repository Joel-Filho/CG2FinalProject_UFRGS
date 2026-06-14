#version 450 core


//gouraud
in vec3 gouraudAmbient;
in vec3 gouraudDiffuse;
in vec3 gouraudSpecular;
//phong
in vec4 position_world;
in vec4 normal;
in vec2 TexCoord;
uniform mat4 view;

//GUI
uniform vec4 color;
uniform sampler2D modelTexture;
uniform bool useTexture;
//shading variables
uniform float ambientStrength;
uniform float specularStrength;
uniform float shininess;

//yeah eu entendi mas não esperava a curveball de aprender isso do nada, demorou bem mais do que o ideal
subroutine vec3 shadingMode(vec3 light_color, vec3 obj_color);
subroutine uniform shadingMode shading;
out vec4 fColor;

void main()
{
    // luz branca
    fColor = vec4(shading(vec3(1.0,1.0,1.0), color.rgb), 1.0);
}
// (assumindo que luz vem da câmera)
subroutine (shadingMode) vec3 noShading(vec3 light_color, vec3 obj_color) {
    vec3 baseColor = obj_color;
    if (useTexture)
        baseColor = texture(modelTexture, TexCoord).rgb;
    return baseColor;
}

subroutine (shadingMode) vec3 gouraud(vec3 light_color, vec3 obj_color) {
    vec3 baseColor = obj_color;
    if (useTexture)
        baseColor = texture(modelTexture, TexCoord).rgb;
    return (gouraudAmbient * light_color * baseColor) + (gouraudDiffuse * light_color * baseColor) + (gouraudSpecular * light_color);
}
subroutine (shadingMode) vec3 phong(vec3 light_color, vec3 obj_color) {

	vec3 lambert_diffuse_term;
	vec3 ambient_term;
	vec3 phong_specular_term;

    vec4 light_camera = vec4(2.0, 2.0, 2.0, 1.0);
    vec4 light_world  = inverse(view) * light_camera;
    vec4 eye_world    = inverse(view) * vec4(0.0, 0.0, 0.0, 1.0);
    vec4 p = position_world;

    vec4 n = normalize(normal);
    vec4 l = normalize(light_world - p);
    vec4 v = normalize(eye_world - p);
    vec4 r = -l + 2*n*dot(n,l);
    float q = shininess; //phong shiny factor
    vec3 baseColor = obj_color;
    if (useTexture)
        baseColor = texture(modelTexture, TexCoord).rgb;
    lambert_diffuse_term = light_color * max(0,dot(n,l)) * baseColor;
    ambient_term = light_color * ambientStrength * baseColor;
    phong_specular_term = light_color * pow(max(0,dot(v,r)),q) * specularStrength;

    return (lambert_diffuse_term + ambient_term + phong_specular_term);
}
