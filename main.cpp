#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
#include <map>
#include <set>
#include <limits>
#include <algorithm>
#include <iomanip>
#include <chrono> // Added for timing
#define GL_SILENCE_DEPRECATION  // Shuts up the "deprecated" warnings
#define GLFW_INCLUDE_GLCOREARB  // Tells GLFW to use the modern OpenGL 3 headers on Mac
#include <GLFW/glfw3.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// --- Configuration ---
const int GRID_RESOLUTION = 32;

// --- Global Variables ---
float yaw   = -90.0f;
float pitch =  0.0f;
float radius = 3.0f;
float lastX =  400, lastY = 300;
bool firstMouse = true;
bool isDragging = false;

// Toggles
bool showVoxels = false;
bool useOrtho = false;
bool showWireframe = false; 

// --- Structures ---
struct Vec3 {
    float x, y, z;
    bool operator<(const Vec3& other) const {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;
        return z < other.z;
    }
};

struct Triangle {
    Vec3 v1, v2, v3;
    Vec3 normal;
};

struct MeshInfo {
    unsigned int triangleCount = 0;
    unsigned int uniqueVertexCount = 0;
    unsigned int shellCount = 0;
    float minEdgeLen = std::numeric_limits<float>::max();
    float maxEdgeLen = 0.0f;
    Vec3 minBounds = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
    Vec3 maxBounds = { std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
    Vec3 center = { 0, 0, 0 };
    float scale = 1.0f;
};

// Data Containers
std::vector<float> renderVertices; 
std::vector<float> renderNormals;
std::vector<float> voxelVertices;
std::vector<Triangle> rawTriangles;
MeshInfo globalInfo; // Made global so analysis function can access bounds

// --- Math Helpers ---
float dist(Vec3 a, Vec3 b) {
    return std::sqrt(std::pow(a.x - b.x, 2) + std::pow(a.y - b.y, 2) + std::pow(a.z - b.z, 2));
}

glm::vec3 toGlm(Vec3 v) { return glm::vec3(v.x, v.y, v.z); }

// Helper: Snap a precise vertex to the nearest SDF Voxel Center
Vec3 snapToSDFGrid(Vec3 v, const MeshInfo& info) {
    float dx = info.maxBounds.x - info.minBounds.x;
    float dy = info.maxBounds.y - info.minBounds.y;
    float dz = info.maxBounds.z - info.minBounds.z;
    float maxDim = std::max(dx, std::max(dy, dz));
    float step = maxDim / GRID_RESOLUTION; // The size of one SDF cell

    // Find grid index
    int xInd = std::round((v.x - info.minBounds.x) / step);
    int yInd = std::round((v.y - info.minBounds.y) / step);
    int zInd = std::round((v.z - info.minBounds.z) / step);

    // Convert back to world position (Voxel Center)
    return {
        info.minBounds.x + xInd * step,
        info.minBounds.y + yInd * step,
        info.minBounds.z + zInd * step
    };
}

// --- Analysis & Export ---
void saveAnalysisToFile(const std::string& filename, bool useSDF) {
    std::cout << "Starting " << (useSDF ? "[SDF Approximation]" : "[Exact Analytic]") << " analysis..." << std::endl;
    
    // Start Timer
    auto start = std::chrono::high_resolution_clock::now();

    std::ofstream out(filename);
    if (!out) { std::cerr << "Failed to create output file!" << std::endl; return; }

    out << "Mode: " << (useSDF ? "SDF_Grid_Approx (Step Loss)" : "Exact_Euclidean") << "\n";
    out << "Triangle_ID | Edge_1_Len | Edge_2_Len | Edge_3_Len | Angle_1(deg) | Angle_2(deg) | Angle_3(deg)\n";
    out << "---------------------------------------------------------------------------------------------\n";

    for (size_t i = 0; i < rawTriangles.size(); ++i) {
        Triangle t = rawTriangles[i];

        // IF SDF MODE: Corrupt the vertices by snapping them to the grid first
        if (useSDF) {
            t.v1 = snapToSDFGrid(t.v1, globalInfo);
            t.v2 = snapToSDFGrid(t.v2, globalInfo);
            t.v3 = snapToSDFGrid(t.v3, globalInfo);
        }
        
        // 1. Calculate Edge Lengths
        float d12 = dist(t.v1, t.v2);
        float d23 = dist(t.v2, t.v3);
        float d31 = dist(t.v3, t.v1);

        // 2. Calculate Angles
        glm::vec3 u = toGlm(t.v2) - toGlm(t.v1);
        glm::vec3 v = toGlm(t.v3) - toGlm(t.v1);
        
        // Safety check for zero-length edges (common in SDF if resolution is too low)
        if (glm::length(u) < 0.0001f || glm::length(v) < 0.0001f) {
             out << i << " | DEGENERATE_TRIANGLE (Collapsed by SDF)\n";
             continue;
        }

        // Angle at V1
        float dot1 = glm::dot(glm::normalize(u), glm::normalize(v));
        float ang1 = glm::degrees(std::acos(std::max(-1.0f, std::min(1.0f, dot1))));

        // Angle at V2
        glm::vec3 u2 = toGlm(t.v1) - toGlm(t.v2);
        glm::vec3 v2 = toGlm(t.v3) - toGlm(t.v2);
        float dot2 = glm::dot(glm::normalize(u2), glm::normalize(v2));
        float ang2 = glm::degrees(std::acos(std::max(-1.0f, std::min(1.0f, dot2))));

        // Angle at V3
        float ang3 = 180.0f - ang1 - ang2;

        out << i << " | " 
            << std::fixed << std::setprecision(4) << d12 << " | " << d23 << " | " << d31 << " | "
            << ang1 << " | " << ang2 << " | " << ang3 << "\n";
    }

    out.close();

    // Stop Timer
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    std::cout << "Analysis Complete!" << std::endl;
    std::cout << ">> File: " << filename << std::endl;
    std::cout << ">> Time Taken: " << duration.count() << " ms" << std::endl;
}

// --- Voxelization (Visuals) ---
void generateVoxels(const MeshInfo& info) {
    voxelVertices.clear();
    float dx = info.maxBounds.x - info.minBounds.x;
    float dy = info.maxBounds.y - info.minBounds.y;
    float dz = info.maxBounds.z - info.minBounds.z;
    float maxDim = std::max(dx, std::max(dy, dz));
    float step = maxDim / GRID_RESOLUTION;
    if (step <= 0.0001f) return;

    std::set<Vec3> occupiedVoxels;
    for(const auto& t : rawTriangles) {
        float tMinX = std::min({t.v1.x, t.v2.x, t.v3.x}); float tMaxX = std::max({t.v1.x, t.v2.x, t.v3.x});
        float tMinY = std::min({t.v1.y, t.v2.y, t.v3.y}); float tMaxY = std::max({t.v1.y, t.v2.y, t.v3.y});
        float tMinZ = std::min({t.v1.z, t.v2.z, t.v3.z}); float tMaxZ = std::max({t.v1.z, t.v2.z, t.v3.z});
        
        int startX = (tMinX - info.minBounds.x)/step; int endX = (tMaxX - info.minBounds.x)/step;
        int startY = (tMinY - info.minBounds.y)/step; int endY = (tMaxY - info.minBounds.y)/step;
        int startZ = (tMinZ - info.minBounds.z)/step; int endZ = (tMaxZ - info.minBounds.z)/step;

        for(int x=startX; x<=endX; x++) for(int y=startY; y<=endY; y++) for(int z=startZ; z<=endZ; z++) {
            float cx = info.minBounds.x + (x)*step; // Simplified for visual alignment
            float cy = info.minBounds.y + (y)*step;
            float cz = info.minBounds.z + (z)*step;
            occupiedVoxels.insert({cx, cy, cz});
        }
    }
    for(const auto& v : occupiedVoxels) {
        voxelVertices.push_back(v.x); voxelVertices.push_back(v.y); voxelVertices.push_back(v.z);
    }
}

// --- STL Loader ---
void loadBinarySTL(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) { std::cerr << "Error opening file\n"; exit(1); }

    char header[80]; file.read(header, 80);
    uint32_t numTriangles; file.read(reinterpret_cast<char*>(&numTriangles), 4);
    globalInfo.triangleCount = numTriangles;

    std::map<Vec3, int> uniqueVertices;

    for (uint32_t i = 0; i < numTriangles; ++i) {
        float n[3], v1[3], v2[3], v3[3];
        uint16_t attr;
        file.read(reinterpret_cast<char*>(n), 12);
        file.read(reinterpret_cast<char*>(v1), 12);
        file.read(reinterpret_cast<char*>(v2), 12);
        file.read(reinterpret_cast<char*>(v3), 12);
        file.read(reinterpret_cast<char*>(&attr), 2);

        Vec3 V1 = {v1[0], v1[1], v1[2]}; Vec3 V2 = {v2[0], v2[1], v2[2]}; Vec3 V3 = {v3[0], v3[1], v3[2]};
        Vec3 Norm = {n[0], n[1], n[2]};
        rawTriangles.push_back({V1, V2, V3, Norm});

        float* verts[3] = {v1, v2, v3};
        for (int k = 0; k < 3; k++) {
            renderVertices.push_back(verts[k][0]); renderVertices.push_back(verts[k][1]); renderVertices.push_back(verts[k][2]);
            renderNormals.push_back(n[0]); renderNormals.push_back(n[1]); renderNormals.push_back(n[2]);
            globalInfo.minBounds.x = std::min(globalInfo.minBounds.x, verts[k][0]); globalInfo.maxBounds.x = std::max(globalInfo.maxBounds.x, verts[k][0]);
            globalInfo.minBounds.y = std::min(globalInfo.minBounds.y, verts[k][1]); globalInfo.maxBounds.y = std::max(globalInfo.maxBounds.y, verts[k][1]);
            globalInfo.minBounds.z = std::min(globalInfo.minBounds.z, verts[k][2]); globalInfo.maxBounds.z = std::max(globalInfo.maxBounds.z, verts[k][2]);
            uniqueVertices[{verts[k][0], verts[k][1], verts[k][2]}]++;
        }
    }
    globalInfo.uniqueVertexCount = uniqueVertices.size();
    globalInfo.center.x = (globalInfo.minBounds.x + globalInfo.maxBounds.x)/2.0f;
    globalInfo.center.y = (globalInfo.minBounds.y + globalInfo.maxBounds.y)/2.0f;
    globalInfo.center.z = (globalInfo.minBounds.z + globalInfo.maxBounds.z)/2.0f;
    float maxDim = std::max({globalInfo.maxBounds.x-globalInfo.minBounds.x, globalInfo.maxBounds.y-globalInfo.minBounds.y, globalInfo.maxBounds.z-globalInfo.minBounds.z});
    globalInfo.scale = (maxDim > 0) ? (2.0f / maxDim) : 1.0f;

    generateVoxels(globalInfo);
}

// --- Callbacks ---
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    radius -= (float)yoffset * 0.2f;
    if (radius < 0.1f) radius = 0.1f;
    if (radius > 20.0f) radius = 20.0f;
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE) {
        isDragging = false; firstMouse = true; return;
    }
    isDragging = true;
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoffset = xpos - lastX; float yoffset = lastY - ypos;
    lastX = xpos; lastY = ypos;
    yaw += xoffset * 0.5f; pitch += yoffset * 0.5f;
    if(pitch > 89.0f) pitch = 89.0f; if(pitch < -89.0f) pitch = -89.0f;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_V) { showVoxels = !showVoxels; std::cout << "Mode: " << (showVoxels ? "Voxels" : "Mesh") << std::endl; }
        if (key == GLFW_KEY_O) { useOrtho = !useOrtho; std::cout << "Projection: " << (useOrtho ? "Orthographic" : "Perspective") << std::endl; }
        if (key == GLFW_KEY_W) { showWireframe = !showWireframe; std::cout << "Wireframe: " << (showWireframe ? "ON" : "OFF") << std::endl; }
        // --- NEW CONTROLS ---
        if (key == GLFW_KEY_E) { saveAnalysisToFile("geometry_exact.txt", false); } // Exact
        if (key == GLFW_KEY_F) { saveAnalysisToFile("geometry_sdf.txt", true); }    // SDF
        
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);
    }
}

// --- Vertex Shader ---
const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aNormal;
    out vec3 Normal; 
    out vec3 FragPos;
    uniform mat4 model; uniform mat4 view; uniform mat4 projection;
    void main() {
        FragPos = vec3(model * vec4(aPos, 1.0));
        // Pass the normal to the fragment shader
        Normal = mat3(transpose(inverse(model))) * aNormal;  
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)";

// --- Fragment Shader ---
const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    in vec3 Normal; 
    in vec3 FragPos;
    uniform vec3 objectColor;
    uniform vec3 viewPos; 
    void main() {
        // 1. Fix potentially broken or inverted normals
        vec3 norm = normalize(Normal);
        vec3 viewDir = normalize(viewPos - FragPos);
        
        // If the normal is facing away from the camera, flip it (Double-Sided Lighting)
        if (dot(norm, viewDir) < 0.0) norm = -norm;

        // 2. High-Intensity Headlamp
        float diff = max(dot(norm, viewDir), 0.0);
        
        // 3. Ambient + Diffuse (Bosted for visibility)
        vec3 ambient = 0.5 * objectColor;  // 50% base brightness
        vec3 diffuse = diff * objectColor * 1.2; // 120% directional brightness
        
        vec3 result = ambient + diffuse;
        
        // 4. Final Clamp to ensure colors don't wash out
        FragColor = vec4(clamp(result, 0.0, 1.0), 1.0);
    }
)";

int main() {
    // Change the name of the file to whatever you want to , and then run the command "g++ -std=c++17 main.cpp -o StlViewer -I/opt/homebrew/include -L/opt/homebrew/lib -lglfw -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo"
    // Post which you can run the viewer with "./StlViewer" in the terminal. Make sure to have the STL file in the same directory as the executable, or provide a relative/absolute path to it.
    // The above command assumes you have GLFW installed via Homebrew on a Mac. If you're on Windows or Linux, the compilation command and library linking will differ.
    std::string filename = "Mar_Polish.stl";
    std::cout << "Loading " << filename << "..." << std::endl;
    loadBinarySTL(filename);

    std::cout << "========================================" << std::endl;
    std::cout << "       CONTROLS & SHORTCUTS             " << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << " [V] Toggle Voxel/Mesh View" << std::endl;
    std::cout << " [W] Toggle Wireframe" << std::endl;
    std::cout << " [O] Toggle Orthographic/Perspective" << std::endl;
    std::cout << " [E] Export EXACT Analysis (Best)" << std::endl;
    std::cout << " [F] Export SDF/Grid Analysis (Experiment)" << std::endl;
    std::cout << "========================================" << std::endl;

    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

    GLFWwindow* window = glfwCreateWindow(800, 600, "STL Analysis Tool", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

    // Build Shaders
    unsigned int vS = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vS, 1, &vertexShaderSource, NULL); glCompileShader(vS);
    unsigned int fS = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fS, 1, &fragmentShaderSource, NULL); glCompileShader(fS);
    unsigned int prog = glCreateProgram(); glAttachShader(prog, vS); glAttachShader(prog, fS); glLinkProgram(prog);

    // Buffers
    unsigned int VAO, VBO, NVBO, vVAO, vVBO;
    glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO); glGenBuffers(1, &NVBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO); glBufferData(GL_ARRAY_BUFFER, renderVertices.size()*4, renderVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, (void*)0); glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, NVBO); glBufferData(GL_ARRAY_BUFFER, renderNormals.size()*4, renderNormals.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 12, (void*)0); glEnableVertexAttribArray(1);

    glGenVertexArrays(1, &vVAO); glGenBuffers(1, &vVBO);
    glBindVertexArray(vVAO);
    glBindBuffer(GL_ARRAY_BUFFER, vVBO); glBufferData(GL_ARRAY_BUFFER, voxelVertices.size()*4, voxelVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 12, (void*)0); glEnableVertexAttribArray(1);

    glUseProgram(prog);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);

    while (!glfwWindowShouldClose(window)) {
        // glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float camX = sin(glm::radians(yaw)) * cos(glm::radians(pitch)) * radius;
        float camY = sin(glm::radians(pitch)) * radius;
        float camZ = cos(glm::radians(yaw)) * cos(glm::radians(pitch)) * radius;
        glm::mat4 view = glm::lookAt(glm::vec3(camX, camY, camZ), glm::vec3(0,0,0), glm::vec3(0,1,0));
        glUniform3f(glGetUniformLocation(prog, "viewPos"), camX, camY, camZ);
        glm::mat4 projection;
        if (useOrtho) {
            float aspect = 800.0f / 600.0f;
            float orthoScale = radius * 0.5f; 
            projection = glm::ortho(-aspect * orthoScale, aspect * orthoScale, -orthoScale, orthoScale, 0.1f, 100.0f);
        } else {
            projection = glm::perspective(glm::radians(45.0f), 800.0f/600.0f, 0.1f, 100.0f);
        }

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(globalInfo.scale));
        model = glm::translate(model, glm::vec3(-globalInfo.center.x, -globalInfo.center.y, -globalInfo.center.z));

        glUniformMatrix4fv(glGetUniformLocation(prog, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(prog, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(prog, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        // Look for colors
        if(!showVoxels) {
            glUniform3f(glGetUniformLocation(prog, "objectColor"), 0.5f, 0.7f, 0.9f);
            
            // --- NEW WIREFRAME LOGIC ---
            if (showWireframe) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // Draw lines only
            } else {
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // Draw solid faces
            }
            // ---------------------------

            glBindVertexArray(VAO); 
            glDrawArrays(GL_TRIANGLES, 0, renderVertices.size() / 3);
            
            // Reset to FILL mode so it doesn't accidentally affect other things later
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); 

        } else {
            glPointSize(10.0f);
            glUniform3f(glGetUniformLocation(prog, "objectColor"), 0.0f, 1.0f, 1.0f);
            glBindVertexArray(vVAO); glDrawArrays(GL_POINTS, 0, voxelVertices.size() / 3);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate(); 
    return 0;
}