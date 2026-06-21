#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include <GL3/gl3.h>
#include <GL3/gl3w.h>

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <cmath>
#include <iostream>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/geometric.hpp>
#include <limits>
#include <algorithm>
#include <vector>

#include "camera.h"
#include "model.h"
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define BUFFER_OFFSET(a) ((void*)(a))

// File dialog functions for Windows
#ifdef _WIN32
std::string OpenFileDialog()
{
    OPENFILENAMEA ofn = {};
    CHAR szFile[MAX_PATH] = {};

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Model Files (*.in)\0*.in\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn))
    {
        return std::string(ofn.lpstrFile);
    }
    return "";
}

std::string OpenTextureDialog()
{
    OPENFILENAMEA ofn = {};
    CHAR szFile[MAX_PATH] = {};

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Bitmap Files (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn))
    {
        return std::string(ofn.lpstrFile);
    }
    return "";
}

std::string SaveFileDialog()
{
    OPENFILENAMEA ofn = {};
    CHAR szFile[MAX_PATH] = {};

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Model Files (*.in)\0*.in\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileNameA(&ofn))
    {
        return std::string(ofn.lpstrFile);
    }
    return "";
}
#else
// Placeholder implementations for non-Windows systems
std::string OpenFileDialog()
{
    std::cout << "File dialog not implemented for this platform" << std::endl;
    return "";
}

std::string SaveFileDialog()
{
    std::cout << "File dialog not implemented for this platform" << std::endl;
    return "";
}
#endif


typedef struct {
    GLenum       type;
    const char* filename;
    GLuint       shader;
} ShaderInfo;

enum VAO_IDs { LoadedModel0, NumVAOs };
enum Buffer_IDs { VertexBuffer, NormalBuffer, TexBuffer, NumBuffers };
enum Attrib_IDs { vPosition = 0, vNormal = 1, vTexCoord = 2 };

GLuint  VAOs[NumVAOs];
GLuint  Buffers[NumBuffers];

static const GLchar*
ReadShader(const char* filename)
{
    FILE* infile = fopen(filename, "rb");

    if (!infile) {
        std::cerr << "Unable to open file '" << filename << "'" << std::endl;
        return NULL;
    }

    fseek(infile, 0, SEEK_END);
    int len = ftell(infile);
    fseek(infile, 0, SEEK_SET);

    GLchar* source = new GLchar[len + 1];

    fread(source, 1, len, infile);
    fclose(infile);

    source[len] = 0;

    return const_cast<const GLchar*>(source);
}

GLuint LoadShaders(ShaderInfo* shaders)
{
    if (shaders == NULL) { return 0; }

    GLuint program = glCreateProgram();

    ShaderInfo* entry = shaders;
    while (entry->type != GL_NONE) {
        GLuint shader = glCreateShader(entry->type);

        entry->shader = shader;

        const GLchar* source = ReadShader(entry->filename);
        if (source == NULL) {
            for (entry = shaders; entry->type != GL_NONE; ++entry) {
                glDeleteShader(entry->shader);
                entry->shader = 0;
            }

            return 0;
        }

        glShaderSource(shader, 1, &source, NULL);
        delete[] source;

        glCompileShader(shader);

        GLint compiled;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLsizei len;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

            GLchar* log = new GLchar[len + 1];
            glGetShaderInfoLog(shader, len, &len, log);
            std::cerr << "Shader compilation failed: " << log << std::endl;
            delete[] log;

            return 0;
        }

        glAttachShader(program, shader);

        ++entry;
    }

    glLinkProgram(program);

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLsizei len;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);

        GLchar* log = new GLchar[len + 1];
        glGetProgramInfoLog(program, len, &len, log);
        std::cerr << "Shader linking failed: " << log << std::endl;
        delete[] log;

        for (entry = shaders; entry->type != GL_NONE; ++entry) {
            glDeleteShader(entry->shader);
            entry->shader = 0;
        }

        return 0;
    }

    return program;
}

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Mouse movement
double delta = 0.0f;
double rightDelta = 0.0f;
double lastX;
double lastRightX;
double raw_sens = 0.01f;
bool mousePressed = false;
bool rightMousePressed = false;
bool firstMouse = true;

// Keyboard debounce for file operations
bool lastCtrlO = false;
bool lastCtrlS = false;
bool lastCtrlMouse = false;
bool lastCtrlMouseMiddle = false;
bool lastRightMouse = false;
bool lastEsc = false;

// Polygon selection state
bool showPolygonEditor = false;
int selectedPolygon = -1;


// SOURCE: https://learnopengl.com/Getting-started/Camera
// mostly
// only uses horizontal movement here
void CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        double dummy;
        glfwGetCursorPos(window, &lastX, &dummy);
        lastRightX = lastX;
        firstMouse = false;
    }

    if (mousePressed)
        delta = (xpos - lastX) * raw_sens;

    if (rightMousePressed)
        rightDelta = (xpos - lastRightX) * raw_sens;

    lastX = xpos;
    lastRightX = xpos;
}

// source: old fcg material
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        delta = 0.0f;

        if (action == GLFW_PRESS)
            mousePressed = true;
        else if (action == GLFW_RELEASE)
            mousePressed = false;
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        rightDelta = 0.0f;

        if (action == GLFW_PRESS)
        {
            rightMousePressed = true;
            double dummyY;
            glfwGetCursorPos(window, &lastRightX, &dummyY);
        }
        else if (action == GLFW_RELEASE)
        {
            rightMousePressed = false;
            rightDelta = 0.0f;
        }
    }
}

static void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    if (width <= 0 || height <= 0)
        return;

    GLFWwindow* previous_context = glfwGetCurrentContext();
    glfwMakeContextCurrent(window);
    glViewport(0, 0, width, height);
    glfwMakeContextCurrent(previous_context);
}

void window_focus_callback(GLFWwindow* window, int focused) {
    if (focused) {
        // The window gained input focus
        //std::cout << "Window focused!" << std::endl;
    } else {
        // The window lost input focus
        //std::cout << "Window lost focus!" << std::endl;
        ImGui::SetWindowFocus(NULL);
    }
}

// Helper function to load and setup a model with GPU resources
// Returns true if successful, false otherwise
bool LoadModelFromFile(const char* filepath, Model& model, float*& model_vert, float*& model_norm, float*& model_tex,
                       GLuint& modelTextureID, int& NumVertices, glm::vec3& bbox_center, glm::vec3& bbox_size)
{
    // Clean up existing model data
    if (model_vert) delete[] model_vert;
    if (model_norm) delete[] model_norm;
    if (model_tex) delete[] model_tex;
    if (modelTextureID != 0)
    {
        glDeleteTextures(1, &modelTextureID);
        modelTextureID = 0;
    }

    // Read the model file
    model = Model(); // Reset model to default state
    try
    {
        model.read_from_file(filepath);
    }
    catch (...)
    {
        printf("ERROR: Failed to load model from %s\n", filepath);
        return false;
    }

    // Sanity check prints: texture edition
    if (model.has_texture == false)
    {
        printf("model has no texture\n");
    }
    else
    {
        printf("model has texture\n");
    }

    model_vert = NULL;
    model_norm = NULL;
    model_tex = NULL;

    model.tris_to_arrays(model_vert, model_norm, model_tex);

    // If model has texture, load the texture image
    if (model.has_texture)
    {
        stbi_set_flip_vertically_on_load(true);
        int texWidth = 0, texHeight = 0, texChannels = 0;
        unsigned char* img = stbi_load("../../data/textures/mandrill_256.bmp", &texWidth, &texHeight, &texChannels, 0);
        if (!img)
        {
            printf("WARNING: failed to load texture image\n");
        }
        else
        {
            // Sanity check texture boogaloo
            printf("texture loaded: width=%d, height=%d, channels=%d\n", texWidth, texHeight, texChannels);
            printf("first pixel RGBA: %d, %d, %d, %d\n", img[0], img[1], img[2], (texChannels == 4) ? img[3] : 255);

            glGenTextures(1, &modelTextureID);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, modelTextureID);
            GLenum format = (texChannels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_2D, 0, format, texWidth, texHeight, 0, format, GL_UNSIGNED_BYTE, img);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glGenerateMipmap(GL_TEXTURE_2D);
            stbi_image_free(img);
        }
    }

    NumVertices = model.num_triangles * 3;

    bbox_center = (model.bbox_max + model.bbox_min) / 2.0f;
    bbox_size = model.bbox_max - model.bbox_min;

    // Sanity check prints
    printf("model bbox_min: %f, %f, %f\n", model.bbox_min.x, model.bbox_min.y, model.bbox_min.z);
    printf("model bbox_max: %f, %f, %f\n", model.bbox_max.x, model.bbox_max.y, model.bbox_max.z);
    printf("model centered at %f, %f, %f\n", bbox_center.x, bbox_center.y, bbox_center.z);
    printf("model size is %f, %f, %f\n", bbox_size.x, bbox_size.y, bbox_size.z);

    return true;
}

// Helper function to update GPU buffers with model data
void UpdateModelBuffers(const Model& model, float* model_vert, float* model_norm, float* model_tex, int NumVertices)
{
    // Delete existing buffers
    glDeleteBuffers(NumBuffers, Buffers);

    // Create new buffers
    glCreateBuffers(NumBuffers, Buffers);
    glBindVertexArray(VAOs[LoadedModel0]);

    // Update vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, Buffers[VertexBuffer]);
    glBufferStorage(GL_ARRAY_BUFFER, NumVertices * 3 * sizeof(float), model_vert, GL_DYNAMIC_STORAGE_BIT);
    glVertexAttribPointer(vPosition, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
    glEnableVertexAttribArray(vPosition);

    // Update normal buffer
    glBindBuffer(GL_ARRAY_BUFFER, Buffers[NormalBuffer]);
    glBufferStorage(GL_ARRAY_BUFFER, NumVertices * 3 * sizeof(float), model_norm, GL_DYNAMIC_STORAGE_BIT);
    glVertexAttribPointer(vNormal, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
    glEnableVertexAttribArray(vNormal);

    // Update texture buffer if present
    if (model.has_texture && model_tex != NULL)
    {
        glBindBuffer(GL_ARRAY_BUFFER, Buffers[TexBuffer]);
        glBufferStorage(GL_ARRAY_BUFFER, NumVertices * 2 * sizeof(float), model_tex, GL_DYNAMIC_STORAGE_BIT);
        glVertexAttribPointer(vTexCoord, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
        glEnableVertexAttribArray(vTexCoord);
    }
    else
        glDisableVertexAttribArray(vTexCoord);
}

void RegenArraysUpdateCPU(Model& model, float*& model_vert, float*& model_norm, float*& model_tex, int& NumVertices, glm::vec3& bbox_center, glm::vec3& bbox_size, GLFWwindow* window)
{
    // regenerate arrays and update GPU
    float* new_vert = NULL;
    float* new_norm = NULL;
    float* new_tex = NULL;
    model.tris_to_arrays(new_vert, new_norm, new_tex);

    if (model_vert) delete[] model_vert;
    if (model_norm) delete[] model_norm;
    if (model_tex) delete[] model_tex;

    model_vert = new_vert;
    model_norm = new_norm;
    model_tex = new_tex;

    NumVertices = model.num_triangles * 3;

    // update bbox center/size used by model matrix
    bbox_center = (model.bbox_max + model.bbox_min) / 2.0f;
    bbox_size = model.bbox_max - model.bbox_min;

    GLFWwindow* prev = glfwGetCurrentContext();
    if (prev != window)
        glfwMakeContextCurrent(window);

    UpdateModelBuffers(model, model_vert, model_norm, model_tex, NumVertices);

    if (prev != window)
        glfwMakeContextCurrent(prev);
}

// Append a triangle to the model (resizes triangle array and updates bbox)
bool AppendTriangle(Model &model, const Triangle &tri)
{
    int oldN = model.num_triangles;
    Triangle* newArr = new Triangle[oldN + 1];
    if (oldN > 0 && model.triangles)
    {
        std::memcpy(newArr, model.triangles, oldN * sizeof(Triangle));
    }
    newArr[oldN] = tri;
    if (model.triangles)
        delete[] model.triangles;
    model.triangles = newArr;
    model.num_triangles = oldN + 1;

    // update bbox
    for (int i = 0; i < 3; ++i)
    {
        glm::vec3 v = glm::vec3(tri.vertex[i]);
        if (oldN == 0 && i == 0)
        {
            model.bbox_min = v;
            model.bbox_max = v;
        }
        model.bbox_min = glm::min(model.bbox_min, v);
        model.bbox_max = glm::max(model.bbox_max, v);
    }

    return true;
}

// Deletes a triangle from the model (resizes triangle array and updates bbox)
bool DeleteTriangle(Model &model, int selectedPolygon)
{
    int N = model.num_triangles;
    if (selectedPolygon >= 0 && selectedPolygon < N)
    {
        Triangle* newArr = nullptr;
        if (N-1 > 0)
        {
            newArr = new Triangle[N-1];
            int idx = 0;
            for (int t = 0; t < N; ++t)
            {
                if (t == selectedPolygon) continue;
                newArr[idx++] = model.triangles[t];
            }
        }
        if (model.triangles) delete[] model.triangles;
        model.triangles = newArr;
        model.num_triangles = N-1;

        // recompute bbox
        if (model.num_triangles > 0)
        {
            model.bbox_min = glm::vec3(FLT_MAX);
            model.bbox_max = glm::vec3(-FLT_MAX);
            for (int t = 0; t < model.num_triangles; ++t)
            {
                for (int v = 0; v < 3; ++v)
                {
                    glm::vec3 vv = glm::vec3(model.triangles[t].vertex[v]);
                    model.bbox_min = glm::min(model.bbox_min, vv);
                    model.bbox_max = glm::max(model.bbox_max, vv);
                }
            }
        }
        return true;
    }

    return false;
}

void MoveCoincidentVertices(Model &model, glm::vec3 oldPos, glm::vec3 newPos)
{
    glm::vec3 delta = newPos - oldPos;
    const float vertex_eps = 1e-5f;
    const float delta_eps = 1e-9f;

    // If delta is non-zero, apply to all vertices that match oldPos within eps
    if (glm::length(delta) > delta_eps)
    {
        for (int ti = 0; ti < model.num_triangles; ++ti)
        {
            for (int vi = 0; vi < 3; ++vi)
            {
                glm::vec3 v = glm::vec3(model.triangles[ti].vertex[vi]);
                if (glm::length(v - oldPos) <= vertex_eps)
                {
                    glm::vec4 updated = model.triangles[ti].vertex[vi];
                    updated.x += delta.x;
                    updated.y += delta.y;
                    updated.z += delta.z;
                    model.triangles[ti].vertex[vi] = updated;
                }
            }
        }
    }
}

void RecalculateNormals(Model &model, int tri_index)
{
    const glm::vec3 a = glm::vec3(model.triangles[tri_index].vertex[0]);
    const glm::vec3 b = glm::vec3(model.triangles[tri_index].vertex[1]);
    const glm::vec3 c = glm::vec3(model.triangles[tri_index].vertex[2]);

    const glm::vec3 n = glm::cross(b-a,c-a);

    glm::vec4 normal = glm::vec4(glm::normalize(n),0.0f);

    for (int i = 0; i < 3; ++i)
    {
        model.triangles[tri_index].normal[i] = normal;
    }
}

// Picking result
struct PickResult { int tri; int vert; bool found; };

// Find the closest vertex to mouse position (window coordinates, top-left origin)
PickResult PickClosestVertex(const Model& model, const glm::mat4& model_matrix, const glm::mat4& view, const glm::mat4& proj, int width, int height, double mouseX, double mouseY)
{
    PickResult res; res.found = false; res.tri = -1; res.vert = -1;
    if (model.num_triangles <= 0) return res;

    float bestDist2 = std::numeric_limits<float>::max();

    for (int i = 0; i < model.num_triangles; ++i)
    {
        for (int v = 0; v < 3; ++v)
        {
            glm::vec4 pos = model.triangles[i].vertex[v];
            glm::vec4 world = model_matrix * glm::vec4(pos.x, pos.y, pos.z, 1.0f);
            glm::vec4 clip = proj * view * world;
            if (std::fabs(clip.w) < 1e-6f) continue;
            glm::vec3 ndc = glm::vec3(clip) / clip.w; // -1..1

            float winX = (ndc.x * 0.5f + 0.5f) * (float)width;
            float winY = (1.0f - (ndc.y * 0.5f + 0.5f)) * (float)height; // convert to top-left origin

            float dx = winX - (float)mouseX;
            float dy = winY - (float)mouseY;
            float d2 = dx*dx + dy*dy;

            if (d2 < bestDist2)
            {
                bestDist2 = d2;
                res.found = true;
                res.tri = i;
                res.vert = v;
            }
        }
    }

    return res;
}

// Find the closest triangle (by projected centroid) to mouse position
int PickClosestTriangle(const Model& model, const glm::mat4& model_matrix, const glm::mat4& view, const glm::mat4& proj, int width, int height, double mouseX, double mouseY)
{
    if (model.num_triangles <= 0) return -1;

    float bestDist2 = std::numeric_limits<float>::max();
    int bestTri = -1;

    for (int i = 0; i < model.num_triangles; ++i)
    {
        glm::vec3 centroid(0.0f);
        for (int v = 0; v < 3; ++v)
            centroid += glm::vec3(model.triangles[i].vertex[v]);
        centroid /= 3.0f;

        glm::vec4 world = model_matrix * glm::vec4(centroid, 1.0f);
        glm::vec4 clip = proj * view * world;
        if (std::fabs(clip.w) < 1e-6f) continue;
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        float winX = (ndc.x * 0.5f + 0.5f) * (float)width;
        float winY = (1.0f - (ndc.y * 0.5f + 0.5f)) * (float)height;

        float dx = winX - (float)mouseX;
        float dy = winY - (float)mouseY;
        float d2 = dx*dx + dy*dy;

        if (d2 < bestDist2)
        {
            bestDist2 = d2;
            bestTri = i;
        }
    }

    return bestTri;
}

int main( int argc, char** argv )
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100 (WebGL 1.0)
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
    const char* glsl_version = "#version 300 es";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    // INIT GUI WINDOW
    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only
    GLFWwindow* window_gui = glfwCreateWindow((int)(400 * main_scale), (int)(700 * main_scale), "GUI", NULL, NULL);
    if (window_gui == NULL)
        return 1;
    glfwMakeContextCurrent(window_gui);
    //glfwSwapInterval(1); // Enable vsync this

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)

    ImGui_ImplGlfw_InitForOpenGL(window_gui, true);
    ImGui_ImplOpenGL3_Init(glsl_version);


    glfwSetWindowFocusCallback(window_gui, window_focus_callback);

    // INIT RENDER WINDOW
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(600, 600, "Mixer", NULL, NULL);
    if (window == NULL)
        return 1;

    glfwMakeContextCurrent(window);
    gl3wInit();
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    glViewport(0, 0, 600, 600);

    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);

    // READ MODEL
    Model model;
    float* model_vert = NULL;
    float* model_norm = NULL;
    float* model_tex = NULL;
    GLuint modelTextureID = 0;
    int NumVertices = 0;
    glm::vec3 bbox_center(0.0f);
    glm::vec3 bbox_size(0.0f);

    if (!LoadModelFromFile("../../data/cube_text.in", model, model_vert, model_norm, model_tex, modelTextureID, NumVertices, bbox_center, bbox_size))
    {
        printf("ERROR: Failed to load default model\n");
        return 1;
    }


    // VAOS AND VBOS
    glGenVertexArrays(NumVAOs, VAOs);
    //opengl
    glBindVertexArray(VAOs[LoadedModel0]);
    glCreateBuffers(NumBuffers, Buffers);
    // openGL shaders
    ShaderInfo  shaders[] =
    {
        { GL_VERTEX_SHADER, "../../src/triangles.vert" },
        { GL_FRAGMENT_SHADER, "../../src/triangles.frag" },
        { GL_NONE, NULL }
    };
    GLuint program = LoadShaders(shaders);
    glUseProgram(program);
    //subrotinas de shading (para openGL)
    GLuint vert_noLighting    = glGetSubroutineIndex(program, GL_VERTEX_SHADER, "noLighting");
    GLuint vert_gDiffuse      = glGetSubroutineIndex(program, GL_VERTEX_SHADER, "gDiffuse");
    GLuint vert_gSpecular     = glGetSubroutineIndex(program, GL_VERTEX_SHADER, "gSpecular");
    GLuint vert_diff_uniform  = glGetSubroutineUniformLocation(program, GL_VERTEX_SHADER, "vertexDiff");
    GLuint vert_spec_uniform  = glGetSubroutineUniformLocation(program, GL_VERTEX_SHADER, "vertexSpec");

    GLuint frag_noShading           = glGetSubroutineIndex(program, GL_FRAGMENT_SHADER, "noShading");
    GLuint frag_gouraud             = glGetSubroutineIndex(program, GL_FRAGMENT_SHADER, "gouraud");
    GLuint frag_phong               = glGetSubroutineIndex(program, GL_FRAGMENT_SHADER, "phong");
    GLuint frag_shading_uniform     = glGetSubroutineUniformLocation(program, GL_FRAGMENT_SHADER, "shading");


    glUseProgram(program);
    glBindVertexArray(VAOs[LoadedModel0]);

    // manda posições dos vértices pra gpu (openGL exclusive)
    glBindBuffer(GL_ARRAY_BUFFER, Buffers[VertexBuffer]);
    glBufferStorage(GL_ARRAY_BUFFER, NumVertices * 3 * sizeof(float), model_vert, GL_DYNAMIC_STORAGE_BIT);

    glVertexAttribPointer(vPosition, 3, GL_FLOAT,
        GL_FALSE, 0, BUFFER_OFFSET(0));
    glEnableVertexAttribArray(vPosition);

    // manda normais dos vértices pra gpu
    glBindBuffer(GL_ARRAY_BUFFER, Buffers[NormalBuffer]);
    glBufferStorage(GL_ARRAY_BUFFER, NumVertices * 3 * sizeof(float), model_norm, GL_DYNAMIC_STORAGE_BIT);

    glVertexAttribPointer(vNormal, 3, GL_FLOAT,
        GL_FALSE, 0, BUFFER_OFFSET(0));
    glEnableVertexAttribArray(vNormal);

    // texture coordinates buffer (if present)
    if (model.has_texture && model_tex != NULL)
    {
        glBindBuffer(GL_ARRAY_BUFFER, Buffers[TexBuffer]);
        glBufferStorage(GL_ARRAY_BUFFER, NumVertices * 2 * sizeof(float), model_tex, GL_DYNAMIC_STORAGE_BIT);
        glVertexAttribPointer(vTexCoord, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
        glEnableVertexAttribArray(vTexCoord);
    }

    GLuint model_matrix_uniform         = glGetUniformLocation(program, "model");
    GLuint view_matrix_uniform          = glGetUniformLocation(program, "view");
    GLuint projection_matrix_uniform    = glGetUniformLocation(program, "projection");
    GLuint color_uniform                = glGetUniformLocation(program, "color");
    GLuint amb_intensity_uniform        = glGetUniformLocation(program, "ambientStrength");
    GLuint spec_intensity_uniform       = glGetUniformLocation(program, "specularStrength");
    GLuint shininess_uniform            = glGetUniformLocation(program, "shininess");
    GLuint modelTextureUniform          = glGetUniformLocation(program, "modelTexture");
    GLuint useTextureUniform            = glGetUniformLocation(program, "useTexture");

    // model matrix é constante para o mesmo modelo:
    // translação até a posição (0,0,0), para facilitar a nossa vida (lookat constante)
    glm::mat4 model_matrix = glm::translate(glm::mat4(1.0f),-bbox_center);
    glUniformMatrix4fv(model_matrix_uniform, 1 , GL_FALSE , glm::value_ptr(model_matrix));
    // TURN ON DEPTH BUFFERING
    glEnable(GL_DEPTH_TEST);

    //Visualization aids
    bool draw_normals = 0;
    //Modeling aids
    int vertexmouseaxis = 0; //0 - X, 1 - Y, 2 - Z
    int polygonmouseaxis = 0; //0 - X, 1 - Y, 2 - Z
    float morphsens = 1.0f;

    //Texture
    bool texture_used = true;
    int resampling_used = 0; //0 - Nearest Neighbor, 1 - Bilinear, 2 - Trilinear(Mip mapping)
    //Shaders
    int shader_used = 3; //0 - Gourand AD, 1 - Gourand ADS, 2 - Phong, 3 - none

    // RENDER STATE
    ImVec4 draw_color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    int primitive_type = GL_FILL;
    int front_face_orientation = GL_CCW;
    bool backface_cull = true;
    float ambientStrength = 0.2f;
    float specularStrength = 0.9f;
    int shininess = 0;

    // CONTROLS STATE
    int axis = 'X';
    int do_rotate = true;//true=rotate, false=translate
    bool lookat = false;

    // Vertex editor state
    bool showVertexEditor = false;
    int selectedTri = -1;
    int selectedVert = -1;
    float selPos[3] = {0.0f,0.0f,0.0f};
    float selNormal[3] = {0.0f,0.0f,0.0f};
    float selTex[2] = {0.0f,0.0f};
    float selColor[3] = {0.0f,0.0f,0.0f};

    bool move_coincident = true;

    // Add Triangle modal state
    bool showAddTriangleModal = false;
    float addPos[9] = {0.0f, 0.0f, 0.0f,
                       1.0f, 0.0f, 0.0f,
                       0.0f, 1.0f, 0.0f};
    float addNormal[3] = {0.0f, 0.0f, 1.0f};
    float addTex[6] = {0.0f, 0.0f,
                      1.0f, 0.0f,
                      0.0f, 1.0f};
    float addColor[3] = {1.0f, 1.0f, 1.0f};

    float sens = 1.0f;

    // CAMERA STATE
    Camera camera;
    camera.reset_uvn();
    camera.set_default_distance(bbox_size);

    // PROJECTION STATE
    float horfov = 60.0f;
    float vertfov = 60.0f; //esse novo menciona mudar fov vertical e horizontal, separadamente
    float near_plane = 1.0f;
    float far_plane = 3000.0f;

    // Show file dialog on startup for model selection
    glfwMakeContextCurrent(window);
    {
        std::string startup_filepath = OpenFileDialog();
        if (!startup_filepath.empty())
        {
            if (LoadModelFromFile(startup_filepath.c_str(), model, model_vert, model_norm, model_tex, modelTextureID, NumVertices, bbox_center, bbox_size))
            {
                UpdateModelBuffers(model, model_vert, model_norm, model_tex, NumVertices);
                printf("Startup model loaded: %s\n", startup_filepath.c_str());
            }
        }
    }

    // RENDER LOOP
    while (!glfwWindowShouldClose(window) && !glfwWindowShouldClose(window_gui))
    {
        glfwPollEvents();

        //READ INFO FROM GUI
        glfwMakeContextCurrent(window_gui);
        if (glfwGetWindowAttrib(window_gui, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiIO& io = ImGui::GetIO();
        bool wantTextInput = io.WantTextInput;

        int ctrlState = glfwGetKey(window_gui, GLFW_KEY_LEFT_CONTROL);
        if (ctrlState == GLFW_RELEASE)
            ctrlState = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL);
        bool ctrlPressed = (ctrlState == GLFW_PRESS);

        int oState = glfwGetKey(window_gui, GLFW_KEY_O);
        if (oState == GLFW_RELEASE)
            oState = glfwGetKey(window, GLFW_KEY_O);
        int sState = glfwGetKey(window_gui, GLFW_KEY_S);
        if (sState == GLFW_RELEASE)
            sState = glfwGetKey(window, GLFW_KEY_S);
        int tState = glfwGetKey(window_gui, GLFW_KEY_T);
        if (tState == GLFW_RELEASE)
            tState = glfwGetKey(window, GLFW_KEY_T);
        int escState = glfwGetKey(window_gui, GLFW_KEY_ESCAPE);
        if (escState == GLFW_RELEASE)
            escState = glfwGetKey(window, GLFW_KEY_ESCAPE);
        bool escPressed = (escState == GLFW_PRESS);

        if (escPressed)
        {
            if (!lastEsc)
            {
                showVertexEditor = false;
                showPolygonEditor = false;
                selectedTri = -1;
                selectedVert = -1;
                selectedPolygon = -1;
                lastEsc = true;
            }
        }
        else
        {
            lastEsc = false;
        }

        if (!wantTextInput)
        {
            // Ctrl+O to open file
            if (ctrlPressed && oState == GLFW_PRESS)
            {
                if (!lastCtrlO)  // Trigger only on transition to pressed
                {
                    lastCtrlO = true;
                    std::string filepath = OpenFileDialog();
                    if (!filepath.empty())
                    {
                        glfwMakeContextCurrent(window);
                        if (LoadModelFromFile(filepath.c_str(), model, model_vert, model_norm, model_tex, modelTextureID, NumVertices, bbox_center, bbox_size))
                        {
                            UpdateModelBuffers(model, model_vert, model_norm, model_tex, NumVertices);
                            printf("Model loaded successfully: %s\n", filepath.c_str());
                        }
                        else
                        {
                            printf("Failed to load model: %s\n", filepath.c_str());
                        }
                        glfwMakeContextCurrent(window_gui);  // Restore GUI context
                    }
                }
            }
            else
            {
                lastCtrlO = false;
            }

            // Ctrl+S to save file
            if (ctrlPressed && sState == GLFW_PRESS)
            {
                if (!lastCtrlS)  // Trigger only on transition to pressed
                {
                    lastCtrlS = true;
                    std::string filepath = SaveFileDialog();
                    if (!filepath.empty())
                    {
                        model.write_to_file(filepath.c_str());
                    }
                }
            }
            else
            {
                lastCtrlS = false;
            }

            // Ctrl+T to change texture
            if (ctrlPressed && tState == GLFW_PRESS)
            {
                static bool lastCtrlT = false;
                if (!lastCtrlT)
                {
                    lastCtrlT = true;
                    std::string texpath = OpenTextureDialog();
                    if (!texpath.empty())
                    {
                        // load texture image and upload in render context
                        GLFWwindow* prev = glfwGetCurrentContext();
                        if (prev != window)
                            glfwMakeContextCurrent(window);

                        if (modelTextureID != 0)
                        {
                            glDeleteTextures(1, &modelTextureID);
                            modelTextureID = 0;
                        }

                        stbi_set_flip_vertically_on_load(true);
                        int texW = 0, texH = 0, texC = 0;
                        unsigned char* img = stbi_load(texpath.c_str(), &texW, &texH, &texC, 0);
                        if (!img)
                        {
                            printf("WARNING: failed to load texture image %s\n", texpath.c_str());
                        }
                        else
                        {
                            GLenum format = (texC == 4) ? GL_RGBA : GL_RGB;
                            glGenTextures(1, &modelTextureID);
                            glActiveTexture(GL_TEXTURE0);
                            glBindTexture(GL_TEXTURE_2D, modelTextureID);
                            glTexImage2D(GL_TEXTURE_2D, 0, format, texW, texH, 0, format, GL_UNSIGNED_BYTE, img);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                            glGenerateMipmap(GL_TEXTURE_2D);
                            stbi_image_free(img);
                            model.has_texture = true;
                            printf("Texture loaded: %s (w=%d h=%d c=%d)\n", texpath.c_str(), texW, texH, texC);
                        }

                        if (prev != window)
                            glfwMakeContextCurrent(prev);
                    }
                }
                else
                {
                    lastCtrlT = false;
                }
            }
        }
        else
        {
            lastCtrlO = false;
            lastCtrlS = false;
        }

        ImGui::Begin("Mixer v0.1");
        //Comentei o vsync. Agora o counter de fps dá valores além de 75. go figure.
        ImGui::SeparatorText("Help:");
        ImGui::Text("Ctrl+S: Save");
        ImGui::Text("Ctrl+O: Open");
        ImGui::Text("Ctrl+T: Texture swap");
        ImGui::Text("Ctrl+left click: Select vertex");
        ImGui::Text("Ctrl+middle click: Select polygon");
        ImGui::Text("ESC: Deselect polygon/vertex");
        ImGui::SeparatorText("Framerate");
        ImGui::Text("%.1f FPS", io.Framerate);
        ImGui::SeparatorText("Visualization Options");
        ImGui::Checkbox("View normal",&draw_normals);
        ImGui::SeparatorText("Modeling Features");
        if (ImGui::Button("Add triangle")) {
            // This code runs ONLY on the frame the user clicks the button
            showAddTriangleModal = true;
        }

        // Add Triangle modal window
        if (showAddTriangleModal)
        {
            ImGui::Begin("Add Triangle", &showAddTriangleModal, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Vertex Positions");
            ImGui::InputFloat3("V0", &addPos[0]);
            ImGui::InputFloat3("V1", &addPos[3]);
            ImGui::InputFloat3("V2", &addPos[6]);
            ImGui::Separator();
            ImGui::Text("Normal (applied to all vertices)");
            ImGui::InputFloat3("Normal", addNormal);
            ImGui::Separator();
            ImGui::Text("Texcoords (per-vertex)");
            ImGui::InputFloat2("T0", &addTex[0]);
            ImGui::InputFloat2("T1", &addTex[2]);
            ImGui::InputFloat2("T2", &addTex[4]);
            ImGui::Separator();
            ImGui::Text("Color (RGB)");
            ImGui::InputFloat3("Color", addColor);

            if (ImGui::Button("OK"))
            {
                // build triangle
                Triangle tri;
                for (int i = 0; i < 3; ++i)
                {
                    tri.vertex[i] = glm::vec4(addPos[i*3+0], addPos[i*3+1], addPos[i*3+2], 1.0f);
                    tri.normal[i] = glm::vec4(addNormal[0], addNormal[1], addNormal[2], 0.0f);
                    tri.color[i] = glm::vec4(addColor[0], addColor[1], addColor[2], 1.0f);
                    tri.texcoord[i] = glm::vec2(addTex[i*2+0], addTex[i*2+1]);
                    tri.material_index[i] = 0;
                }

                // append triangle to model
                AppendTriangle(model, tri);

                RegenArraysUpdateCPU(model, model_vert, model_norm, model_tex, NumVertices, bbox_center, bbox_size, window);

                showAddTriangleModal = false;
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                showAddTriangleModal = false;
            }

            ImGui::End();
        }

        /* ignore this lowkey
        if (ImGui::Button("Recalculate Model Normals"))
        {
            for (int t = 0; t < model.num_triangles; ++t)
                RecalculateNormals(model, t);

            RegenArraysUpdateCPU(model, model_vert, model_norm, model_tex, NumVertices, bbox_center, bbox_size, window);
        }
        */

        if (model.has_texture)
        {
            ImGui::SeparatorText("Texture Options");
            ImGui::Checkbox("Texture",&texture_used);
            ImGui::SameLine();
            if (!texture_used) ImGui::BeginDisabled();
            ImGui::RadioButton("NN",&resampling_used,0);
            ImGui::SameLine();
            ImGui::RadioButton("Bilinear",&resampling_used,1);
            ImGui::SameLine();
            ImGui::RadioButton("Trilinear",&resampling_used,2);
            if (!texture_used) ImGui::EndDisabled();
        }

        ImGui::SeparatorText("Shading Options");
        //if (renderingAPI == 1) //shaders só pra OpenGL; Not anymore they aren't.
            //ImGui::BeginDisabled();
        ImGui::RadioButton("GourAD",&shader_used,0);
        ImGui::SameLine();
        ImGui::RadioButton("GourADS",&shader_used,1);
        ImGui::SameLine();
        ImGui::RadioButton("Phong",&shader_used,2);
        ImGui::SameLine();
        ImGui::RadioButton("none",&shader_used,3);
        //if (renderingAPI == 1)
            //ImGui::EndDisabled();

        if (shader_used < 3) //se houver algum shader (e estiver usando OpenGL) not anymore lul
        {
            ImGui::SliderFloat("Ambient", &ambientStrength, 0.0f, 1.0f, "%.2f"); //ativa slider de luz ambiente
            if (shader_used > 0) //se usar iluminação especular (gourADS, Phong)
                {
                    ImGui::SliderFloat("Specular", &specularStrength, 0.0f, 1.0f, "%.2f"); //ativa sliders de iluminação especular e shininess
                    ImGui::SliderInt("Shininess", &shininess, 0, 8, "%d");
                }
        }

        ImGui::ColorEdit3("Color", (float*)&draw_color);

        ImGui::RadioButton("Points",&primitive_type,GL_POINT);
        ImGui::SameLine();
        ImGui::RadioButton("Lines",&primitive_type,GL_LINE);
        ImGui::SameLine();
        ImGui::RadioButton("Tris",&primitive_type,GL_FILL);

        ImGui::Checkbox("Backface Culling", &backface_cull);

        if (!backface_cull)
            ImGui::BeginDisabled();

        ImGui::Text("Front Face:");
        ImGui::SameLine();
        ImGui::RadioButton("CCW",&front_face_orientation,GL_CCW);
        ImGui::SameLine();
        ImGui::RadioButton("CW",&front_face_orientation,GL_CW);

        if (!backface_cull)
            ImGui::EndDisabled();

        ImGui::SeparatorText("Camera Values");

        ImGui::Text("Pos: %.3f %.3f %.3f", camera.pos.x, camera.pos.y, camera.pos.z);
        ImGui::Text("u: %.3f %.3f %.3f", camera.u.x, camera.u.y, camera.u.z);
        ImGui::Text("v: %.3f %.3f %.3f", camera.v.x, camera.v.y, camera.v.z);
        ImGui::Text("n: %.3f %.3f %.3f", camera.n.x, camera.n.y, camera.n.z);

        ImGui::SeparatorText("Camera Control Options");

        const char* x_label = do_rotate ? "Pitch" : "X";
        const char* y_label = do_rotate ? "Yaw" : "Y";
        const char* z_label = do_rotate ? "Roll" : "Z";

        ImGui::Text("Axis:");
        ImGui::SameLine();
        ImGui::RadioButton(x_label,&axis,'X');
        ImGui::SameLine();
        ImGui::RadioButton(y_label,&axis,'Y');
        ImGui::SameLine();
        ImGui::RadioButton(z_label,&axis,'Z');

        //só translação quando lookat = true
        if (lookat)
            ImGui::BeginDisabled();

        ImGui::RadioButton("Translation",&do_rotate,false);
        ImGui::SameLine();
        ImGui::RadioButton("Rotation",&do_rotate,true);

        if (lookat)
            ImGui::EndDisabled();

        if (ImGui::Checkbox("Look at Object", &lookat))
        {
            if (lookat)
                do_rotate = false;
        }

        if (ImGui::Button("Reset Camera"))
        {
            camera.reset_uvn();
            camera.set_default_distance(bbox_size);
        }

        ImGui::SliderFloat("Mouse Sens.", &sens, 0.0f, 10.0f, "%.2f");
        if (ImGui::Button("Reset Sens."))
            sens = 1.0f;

        ImGui::SeparatorText("Projection Options");

        ImGui::SliderFloat("HFOV", &horfov, 1.0f, 180.0f, "%.1f°");
        ImGui::SliderFloat("VFOV", &vertfov, 1.0f, 180.0f, "%.1f°");

        ImGui::InputFloat("Near plane", &near_plane, 0.01f, 1.0f, "%.3f");
        ImGui::InputFloat("Far plane", &far_plane, 0.01f, 1.0f, "%.3f");

        if (ImGui::Button("Reset Projection"))
        {
            horfov = 60.0f;
            vertfov = 60.0f;
            near_plane = 1.0f;
            far_plane = 3000.0f;
        }


        ImGui::Separator();

        ImGui::End();

        // Vertex Editor window
        if (showVertexEditor && selectedTri >= 0 && selectedVert >= 0)
        {

            ImGui::Begin("Vertex Editor", &showVertexEditor);
            ImGui::Text("Triangle %d - Vertex %d", selectedTri, selectedVert);

            ImGui::Checkbox("Move Coincident Vertices", &move_coincident);

            bool changed = false;
            if (ImGui::InputFloat3("Position", selPos))
            {
                // Move the selected vertex and any coincident vertices (within epsilon)
                glm::vec3 newPos(selPos[0], selPos[1], selPos[2]);
                glm::vec3 oldPos = glm::vec3(model.triangles[selectedTri].vertex[selectedVert]);

                if (move_coincident)
                {
                    MoveCoincidentVertices(model,oldPos,newPos);
                }
                else
                {
                    // tiny/no movement — still set the selected vertex precisely
                    model.triangles[selectedTri].vertex[selectedVert] = glm::vec4(newPos, 1.0f);
                }

                changed = true;
            }

            if (ImGui::InputFloat3("Normal", selNormal))
            {
                model.triangles[selectedTri].normal[selectedVert] = glm::vec4(selNormal[0], selNormal[1], selNormal[2], 0.0f);
                changed = true;
            }

            if (model.has_texture)
            {
                if (ImGui::InputFloat2("Texcoord", selTex))
                {
                    model.triangles[selectedTri].texcoord[selectedVert] = glm::vec2(selTex[0], selTex[1]);
                    changed = true;
                }
            }

            if (ImGui::InputFloat3("Color (RGB)", selColor))
            {
                model.triangles[selectedTri].color[selectedVert].x = selColor[0];
                model.triangles[selectedTri].color[selectedVert].y = selColor[1];
                model.triangles[selectedTri].color[selectedVert].z = selColor[2];
                changed = true;
            }

            ImGui::Text("Right click + drag: move vertex");
            ImGui::Text("Mouse morphing axis");
            ImGui::SameLine();
            ImGui::RadioButton("X",&vertexmouseaxis,0);
            ImGui::SameLine();
            ImGui::RadioButton("Y",&vertexmouseaxis,1);
            ImGui::SameLine();
            ImGui::RadioButton("Z",&vertexmouseaxis,2);
            ImGui::SliderFloat("Morphing Sens.", &morphsens, 0.0f, 10.0f, "%.2f");

            if (changed)
            {
                RegenArraysUpdateCPU(model, model_vert, model_norm, model_tex, NumVertices, bbox_center, bbox_size, window);
            }

            ImGui::End();

            if (!showVertexEditor)
            {
                selectedTri = -1;
                selectedVert = -1;
            }
        }

        // Polygon Editor window
        if (showPolygonEditor && selectedPolygon >= 0)
        {
            ImGui::Begin("Polygon Editor", &showPolygonEditor);
            ImGui::Text("Triangle %d", selectedPolygon);

            ImGui::Checkbox("Move Coincident Vertices", &move_coincident);

            ImGui::Text("Ctrl + Right click + drag: move polygon");
            ImGui::Text("Mouse morphing axis:");
            ImGui::SameLine();
            ImGui::RadioButton("X",&polygonmouseaxis,0);
            ImGui::SameLine();
            ImGui::RadioButton("Y",&polygonmouseaxis,1);
            ImGui::SameLine();
            ImGui::RadioButton("Z",&polygonmouseaxis,2);
            ImGui::SliderFloat("Morphing Sens.", &morphsens, 0.0f, 10.0f, "%.2f");

            bool polyChanged = false;
            for (int i = 0; i < 3; ++i)
            {
                float vbuf[3];
                vbuf[0] = model.triangles[selectedPolygon].vertex[i].x;
                vbuf[1] = model.triangles[selectedPolygon].vertex[i].y;
                vbuf[2] = model.triangles[selectedPolygon].vertex[i].z;
                if (ImGui::InputFloat3((std::string("V") + std::to_string(i)).c_str(), vbuf))
                {
                    // Move the selected vertex and any coincident vertices (within epsilon)
                    glm::vec3 newPos(vbuf[0], vbuf[1], vbuf[2]);
                    glm::vec3 oldPos = glm::vec3(model.triangles[selectedPolygon].vertex[i]);
                    

                    if (move_coincident)
                    {
                        MoveCoincidentVertices(model,oldPos,newPos);
                    }
                    else
                    {
                        // tiny/no movement — still set the selected vertex precisely
                        model.triangles[selectedPolygon].vertex[i] = glm::vec4(newPos, 1.0f);
                    }
                    polyChanged = true;
                }
            }

            float nbuf[3];
            nbuf[0] = model.triangles[selectedPolygon].normal[0].x;
            nbuf[1] = model.triangles[selectedPolygon].normal[0].y;
            nbuf[2] = model.triangles[selectedPolygon].normal[0].z;
            if (ImGui::InputFloat3("Normal (per-vertex)", nbuf))
            {
                for (int i = 0; i < 3; ++i)
                    model.triangles[selectedPolygon].normal[i] = glm::vec4(nbuf[0], nbuf[1], nbuf[2], 0.0f);
                polyChanged = true;
            }

            float tbuf[2];
            tbuf[0] = model.triangles[selectedPolygon].texcoord[0].x;
            tbuf[1] = model.triangles[selectedPolygon].texcoord[0].y;
            if (ImGui::InputFloat2("Tex0", tbuf)) { model.triangles[selectedPolygon].texcoord[0] = glm::vec2(tbuf[0], tbuf[1]); polyChanged = true; }
            tbuf[0] = model.triangles[selectedPolygon].texcoord[1].x; tbuf[1] = model.triangles[selectedPolygon].texcoord[1].y;
            if (ImGui::InputFloat2("Tex1", tbuf)) { model.triangles[selectedPolygon].texcoord[1] = glm::vec2(tbuf[0], tbuf[1]); polyChanged = true; }
            tbuf[0] = model.triangles[selectedPolygon].texcoord[2].x; tbuf[1] = model.triangles[selectedPolygon].texcoord[2].y;
            if (ImGui::InputFloat2("Tex2", tbuf)) { model.triangles[selectedPolygon].texcoord[2] = glm::vec2(tbuf[0], tbuf[1]); polyChanged = true; }

            if (polyChanged)
            {
                RegenArraysUpdateCPU(model, model_vert, model_norm, model_tex, NumVertices, bbox_center, bbox_size, window);
            }

            ImGui::Separator();
            if (ImGui::Button("Recalculate Normals"))
            {
                RecalculateNormals(model, selectedPolygon);
                RegenArraysUpdateCPU(model, model_vert, model_norm, model_tex, NumVertices, bbox_center, bbox_size, window);
            }

            if (ImGui::Button("Delete Triangle"))
            {
                if (DeleteTriangle(model, selectedPolygon))
                {
                    RegenArraysUpdateCPU(model, model_vert, model_norm, model_tex, NumVertices, bbox_center, bbox_size, window);

                    showPolygonEditor = false;
                    selectedPolygon = -1;
                }
            }

            if (ImGui::Button("Subdivide Triangle (Center)"))
            {
                Triangle oldtri = model.triangles[selectedPolygon];

                //creates new "center" vertex, average of each vertex's attributes

                glm::vec4 newvert(0.0f, 0.0f, 0.0f, 1.0f);
                glm::vec4 newnorm(0.0f, 0.0f, 0.0f, 0.0f);
                glm::vec4 newcolor(0.0f, 0.0f, 0.0f, 1.0f);
                glm::vec2 newtexcoord(0.0f, 0.0f);
                for (int v = 0; v < 3; ++v)
                {
                    newvert += oldtri.vertex[v];
                    newnorm += oldtri.normal[v];
                    newcolor += oldtri.color[v];
                    newtexcoord += oldtri.texcoord[v];
                }
                newvert /= 3.0f;
                newnorm /= 3.0f;
                newcolor /= 3.0f;
                newtexcoord /= 3.0f;

                // build 3 triangles
                // winding order should match...?
                Triangle newtri[3];
                for (int i = 0; i < 3; ++i)
                {
                    int idx0 = i;//0,1,2
                    int idx1 = (i+1)%3;//1,2,0

                    newtri[i].vertex[0] = oldtri.vertex[idx0];
                    newtri[i].vertex[1] = oldtri.vertex[idx1];
                    newtri[i].vertex[2] = newvert;

                    newtri[i].normal[0] = oldtri.normal[idx0];
                    newtri[i].normal[1] = oldtri.normal[idx1];
                    newtri[i].normal[2] = newnorm;

                    newtri[i].color[0] = oldtri.color[idx0];
                    newtri[i].color[1] = oldtri.color[idx1];
                    newtri[i].color[2] = newcolor;

                    newtri[i].texcoord[0] = oldtri.texcoord[idx0];
                    newtri[i].texcoord[1] = oldtri.texcoord[idx1];
                    newtri[i].texcoord[2] = newtexcoord;
                }

                // append triangles to model
                for (int i = 0; i < 3; ++i)
                    AppendTriangle(model, newtri[i]);

                // delete origin triangle
                DeleteTriangle(model, selectedPolygon);

                // regenerate arrays and update GPU
                RegenArraysUpdateCPU(model, model_vert, model_norm, model_tex, NumVertices, bbox_center, bbox_size, window);

                showPolygonEditor = false;
                selectedPolygon = -1;
            }

            ImGui::End();

            if (!showPolygonEditor)
            {
                selectedPolygon = -1;
            }
        }

        //RENDER GUI
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window_gui, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //reset background AND depth buffer
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_gui);

        //DO CAMERA OPERATIONS FROM INFO + MOUSE CONTROLS
        if (!ImGui::GetIO().WantCaptureMouse)
        {
            if (do_rotate)
            {
                camera.rotate_along_axis(axis, delta*sens);
            }
            else
            {
                if (lookat)
                {
                    glm::vec3 lookat_target(0.0f, 0.0f, 0.0f);
                    glm::vec3 offset = camera.pos - lookat_target;
                    float radius = glm::length(offset);
                    if (radius > 1e-6f)
                    {
                        float angle = delta * sens / radius;
                        if (axis == 'X')
                        {
                            // Orbit horizontally around the target using the camera's up vector.
                            glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), angle, camera.v);
                            camera.pos = lookat_target + glm::vec3(rotation * glm::vec4(offset, 1.0f));
                        }
                        else if (axis == 'Y')
                        {
                            // Orbit vertically around the target using the camera's right vector.
                            glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), angle, camera.u);
                            camera.pos = lookat_target + glm::vec3(rotation * glm::vec4(offset, 1.0f));
                        }
                        else
                        {
                            camera.translate_along_axis(axis, delta*sens);
                        }
                    }
                }
                else
                {
                    camera.translate_along_axis(axis, delta*sens);
                }
            }
            // Vertex dragging with right mouse button (when a vertex is selected in the vertex editor)
            if (rightMousePressed && selectedTri >= 0 && selectedVert >= 0)
            {
                float dragAmount = (float)(rightDelta * sens * morphsens);
                if (std::fabs(dragAmount) > 0.0f)
                {
                    glm::vec3 oldPos = glm::vec3(model.triangles[selectedTri].vertex[selectedVert]);
                    glm::vec3 newPos = oldPos;
                    if (vertexmouseaxis == 0)
                        newPos.x += dragAmount;
                    else if (vertexmouseaxis == 1)
                        newPos.y += dragAmount;
                    else if (vertexmouseaxis == 2)
                        newPos.z += dragAmount;

                    if (move_coincident)
                    {
                        MoveCoincidentVertices(model, oldPos, newPos);
                    }
                    else
                    {
                        model.triangles[selectedTri].vertex[selectedVert] = glm::vec4(newPos, 1.0f);
                    }

                    selPos[0] = newPos.x;
                    selPos[1] = newPos.y;
                    selPos[2] = newPos.z;

                    RegenArraysUpdateCPU(model, model_vert, model_norm, model_tex, NumVertices, bbox_center, bbox_size, window);
                }
            }

            // Polygon dragging with Ctrl + Right Mouse (when a polygon is selected in the polygon editor)
            // Dragging moves each distinct vertex of the polygon along the selected polygon axis.
            if (rightMousePressed && showPolygonEditor && selectedPolygon >= 0)
            {
                bool ctrlPressedLocal = (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
                if (ctrlPressedLocal)
                {
                    float dragAmount = (float)(rightDelta * sens * morphsens);
                    if (std::fabs(dragAmount) > 0.0f)
                    {
                        const float vertex_eps = 1e-5f;
                        std::vector<glm::vec3> processed;

                        for (int vi = 0; vi < 3; ++vi)
                        {
                            glm::vec3 oldPos = glm::vec3(model.triangles[selectedPolygon].vertex[vi]);

                            // skip if we already moved vertices at approximately the same position
                            bool already = false;
                            for (const auto &p : processed)
                            {
                                if (glm::length(p - oldPos) <= vertex_eps) { already = true; break; }
                            }
                            if (already) continue;

                            glm::vec3 newPos = oldPos;
                            if (polygonmouseaxis == 0)
                                newPos.x += dragAmount;
                            else if (polygonmouseaxis == 1)
                                newPos.y += dragAmount;
                            else
                                newPos.z += dragAmount;

                            if (move_coincident)
                            {
                                MoveCoincidentVertices(model, oldPos, newPos);
                            }
                            else
                            {
                                // update only this polygon's vertex
                                for (int k = 0; k < 3; ++k)
                                {
                                    glm::vec3 vv = glm::vec3(model.triangles[selectedPolygon].vertex[k]);
                                    if (glm::length(vv - oldPos) <= vertex_eps)
                                    {
                                        model.triangles[selectedPolygon].vertex[k] = glm::vec4(newPos, 1.0f);
                                    }
                                }
                            }

                            processed.push_back(oldPos);
                        }

                        RegenArraysUpdateCPU(model, model_vert, model_norm, model_tex, NumVertices, bbox_center, bbox_size, window);
                    }
                }
            }
        }

        glm::vec3 lookat_pos;

        if (lookat)
        {
            lookat_pos = glm::vec3(0.0f,0.0f,0.0f); // center of model
            camera.recalculate_uvn(lookat_pos); // for some reason this inverts the U vector when used after reset (default uvn). ok.
        }
        else
            lookat_pos = camera.pos - camera.n; // n points backwards

        //SET RENDER MATRICES
        // Texture uniform setup will happen after context switch in OpenGL path

    //IF USED TO BE HERE
        glfwMakeContextCurrent(window);

            // NOW bind texture and set uniforms in the correct context
        glUseProgram(program);
        // Recompute model matrix each frame (centering at bbox_center)
        model_matrix = glm::translate(glm::mat4(1.0f), -bbox_center);
        glUniformMatrix4fv(model_matrix_uniform, 1 , GL_FALSE , glm::value_ptr(model_matrix));
        if (modelTextureID != 0) // if we have a texture loaded, bind it and set the uniform. If not, just set the uniform to 0 and the shader will ignore it.
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, modelTextureID);
            // set filtering based on resampling choice
            if (resampling_used == 0) // nearest neighbor
            {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            }
            else if (resampling_used == 1) // bilinear
            {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            }
            else // trilinear
            {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glGenerateMipmap(GL_TEXTURE_2D);
            }
            glUniform1i(modelTextureUniform, 0);
        }
        else // if we don't have a texture, unbind any previously bound texture and set the uniform to 0 so the shader doesn't try to sample it.
        {
            glUniform1i(modelTextureUniform, 0);
        }
        // set uniform to tell shader whether to use texture or not. We only use the texture if the model has a texture, the user has it enabled, and we successfully loaded a texture.
        glUniform1i(useTextureUniform, (model.has_texture && texture_used && modelTextureID != 0) ? 1 : 0);

        //SET OPENGL PARAMETERS FROM INFO
        glfwMakeContextCurrent(window);

        float bg_color[] = {0.0f, 0.0f, 0.0f, 1.0f};//black
        glClearBufferfv(GL_COLOR, 0, bg_color);
        glClear(GL_DEPTH_BUFFER_BIT); //reset depth buffer

        glPolygonMode(GL_FRONT_AND_BACK, primitive_type);

        if (backface_cull)
        {
            glEnable(GL_CULL_FACE);
        }
        else
        {
            glDisable(GL_CULL_FACE);
        }
        glCullFace(GL_BACK);

        glFrontFace(front_face_orientation);

        glUniform4f(color_uniform, draw_color.x, draw_color.y, draw_color.z, draw_color.w);
        glUniform1f(amb_intensity_uniform, ambientStrength);
        glUniform1f(spec_intensity_uniform, specularStrength);
        glUniform1f(shininess_uniform, std::pow(2,shininess)); //https://learnopengl.com/Lighting/Basic-Lighting


        glm::mat4 view_matrix = glm::lookAt(camera.pos,lookat_pos,camera.v);
        glUniformMatrix4fv(view_matrix_uniform, 1 , GL_FALSE , glm::value_ptr(view_matrix));

        int width, height;
        glfwGetWindowSize(window, &width, &height);

        float t = std::fabs(near_plane) * std::tan(glm::radians(vertfov) / 2.0f);
        float b = -t;
        float r = std::fabs(near_plane) * std::tan(glm::radians(horfov) / 2.0f);
        float l = -r;

        glm::mat4 proj_matrix = glm::frustum(l,r,b,t,near_plane,far_plane);
        glUniformMatrix4fv(projection_matrix_uniform, 1 , GL_FALSE , glm::value_ptr(proj_matrix));

        // --- Vertex picking: Ctrl + Left Mouse ---
        int width_win, height_win;
        glfwGetWindowSize(window, &width_win, &height_win);
        bool ctrlPressedLocal = (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
        int leftState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
        int middleState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE);
        if (!io.WantCaptureMouse && ctrlPressedLocal && leftState == GLFW_PRESS && !lastCtrlMouse)
        {
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            PickResult pr = PickClosestVertex(model, model_matrix, view_matrix, proj_matrix, width_win, height_win, mx, my);
            if (pr.found)
            {
                selectedTri = pr.tri;
                selectedVert = pr.vert;
                // populate editor fields
                glm::vec4 p = model.triangles[selectedTri].vertex[selectedVert];
                selPos[0] = p.x; selPos[1] = p.y; selPos[2] = p.z;
                glm::vec4 n = model.triangles[selectedTri].normal[selectedVert];
                selNormal[0] = n.x; selNormal[1] = n.y; selNormal[2] = n.z;
                selTex[0] = model.triangles[selectedTri].texcoord[selectedVert].x;
                selTex[1] = model.triangles[selectedTri].texcoord[selectedVert].y;
                selColor[0] = model.triangles[selectedTri].color[selectedVert].x;
                selColor[1] = model.triangles[selectedTri].color[selectedVert].y;
                selColor[2] = model.triangles[selectedTri].color[selectedVert].z;

                showVertexEditor = true;
            }
        }
        lastCtrlMouse = (ctrlPressedLocal && leftState == GLFW_PRESS);
        // Polygon pick: Ctrl + Middle Mouse
        if (!io.WantCaptureMouse && ctrlPressedLocal && middleState == GLFW_PRESS && !lastCtrlMouseMiddle)
        {
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            int tri = PickClosestTriangle(model, model_matrix, view_matrix, proj_matrix, width_win, height_win, mx, my);
            if (tri >= 0)
            {
                selectedPolygon = tri;
                showPolygonEditor = true;
            }
        }
        lastCtrlMouseMiddle = (ctrlPressedLocal && middleState == GLFW_PRESS);

            //sanity check prints 2 electric boogaloo
            /*
            if (printcontrol0 == 1)
            {
            printf("proj:");
            std::cout << glm::to_string(proj_matrix) << std::endl;
            printf("view:");
            std::cout << glm::to_string(view_matrix) << std::endl;
            printcontrol0 = 0;
            }
            */


        GLuint vs_indices[2];
        GLuint fs_indices[1];
        switch (shader_used)
        {
            case 0:
                vs_indices[vert_diff_uniform]   = vert_gDiffuse;
                vs_indices[vert_spec_uniform]   = vert_noLighting;
                fs_indices[frag_shading_uniform]= frag_gouraud;
                break;
            case 1:
                vs_indices[vert_diff_uniform]   = vert_gDiffuse;
                vs_indices[vert_spec_uniform]   = vert_gSpecular;
                fs_indices[frag_shading_uniform]= frag_gouraud;
                break;
            case 2:
                vs_indices[vert_diff_uniform]   = vert_noLighting;
                vs_indices[vert_spec_uniform]   = vert_noLighting;
                fs_indices[frag_shading_uniform]= frag_phong;
                break;
            case 3:
                vs_indices[vert_diff_uniform]   = vert_noLighting;
                vs_indices[vert_spec_uniform]   = vert_noLighting;
                fs_indices[frag_shading_uniform]= frag_noShading;
                break;
        }
        glUniformSubroutinesuiv(GL_VERTEX_SHADER,   2, vs_indices);
        glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, fs_indices);

        glBindVertexArray(VAOs[LoadedModel0]);
        glDrawArrays(GL_TRIANGLES, 0, NumVertices);

        // Highlight selected polygon if any (draw on top, textureless bright yellow)
        if (selectedPolygon >= 0)
        {
            glUniform1i(useTextureUniform, 0);
            glUniform4f(color_uniform, 1.0f, 1.0f, 0.0f, 1.0f);
            GLuint tri_vs[2] = {0,0}; GLuint tri_fs[1] = {0};
            tri_vs[vert_diff_uniform] = vert_noLighting;
            tri_vs[vert_spec_uniform] = vert_noLighting;
            tri_fs[frag_shading_uniform] = frag_noShading;
            glUniformSubroutinesuiv(GL_VERTEX_SHADER, 2, tri_vs);
            glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, tri_fs);

            // Save/modify GL state so highlight draws on top
            GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
            GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);

            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            glDrawArrays(GL_TRIANGLES, selectedPolygon * 3, 3);

            // Restore GL state
            if (depthWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
            if (cullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
            glPolygonMode(GL_FRONT_AND_BACK, primitive_type);
        }

        if (selectedTri >= 0 && selectedVert >= 0)
        {
            glEnable(GL_PROGRAM_POINT_SIZE);
            glPointSize(10.0f);
            glUniform1i(useTextureUniform, 0);
            glUniform4f(color_uniform, 1.0f, 1.0f, 0.0f, 1.0f);

            GLuint highlight_vs[2];
            GLuint highlight_fs[1];
            highlight_vs[vert_diff_uniform] = vert_noLighting;
            highlight_vs[vert_spec_uniform] = vert_noLighting;
            highlight_fs[frag_shading_uniform] = frag_noShading;
            glUniformSubroutinesuiv(GL_VERTEX_SHADER, 2, highlight_vs);
            glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, highlight_fs);

            glDrawArrays(GL_POINTS, selectedTri * 3 + selectedVert, 1);

            glPointSize(1.0f);

            // Draw selected vertex normal as a line if requested
            if (draw_normals)
            {
                // compute start and end positions in model space so the shader's model matrix is applied once
                glm::vec4 v = model.triangles[selectedTri].vertex[selectedVert];
                glm::vec4 n = model.triangles[selectedTri].normal[selectedVert];
                glm::vec3 start = glm::vec3(v);
                glm::vec3 n3 = glm::normalize(glm::vec3(n));
                float modelSize = glm::length(bbox_size);
                float scale = (modelSize > 1e-6f) ? (modelSize * 0.1f) : 1.0f;
                glm::vec3 end = start + n3 * scale;

                GLfloat lineVerts[6];
                lineVerts[0] = start.x; lineVerts[1] = start.y; lineVerts[2] = start.z;
                lineVerts[3] = end.x;   lineVerts[4] = end.y;   lineVerts[5] = end.z;

                // use a simple no-shading pass for the line
                glUniform1i(useTextureUniform, 0);
                glUniform4f(color_uniform, 0.0f, 1.0f, 1.0f, 1.0f);
                GLuint line_vs[2];
                GLuint line_fs[1];
                line_vs[vert_diff_uniform] = vert_noLighting;
                line_vs[vert_spec_uniform] = vert_noLighting;
                line_fs[frag_shading_uniform] = frag_noShading;
                glUniformSubroutinesuiv(GL_VERTEX_SHADER, 2, line_vs);
                glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, line_fs);

                GLuint tmpVAO = 0, tmpVBO = 0;
                glGenVertexArrays(1, &tmpVAO);
                glGenBuffers(1, &tmpVBO);
                glBindVertexArray(tmpVAO);
                glBindBuffer(GL_ARRAY_BUFFER, tmpVBO);
                glBufferData(GL_ARRAY_BUFFER, sizeof(lineVerts), lineVerts, GL_STATIC_DRAW);
                glVertexAttribPointer(vPosition, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
                glEnableVertexAttribArray(vPosition);
                glLineWidth(2.0f);
                glDrawArrays(GL_LINES, 0, 2);
                glLineWidth(1.0f);
                glDisableVertexAttribArray(vPosition);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glBindVertexArray(0);
                glDeleteBuffers(1, &tmpVBO);
                glDeleteVertexArrays(1, &tmpVAO);
            }
        }

        glfwSwapBuffers(window);

    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

