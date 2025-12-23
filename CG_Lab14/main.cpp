#include <GL/glew.h>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <array>

#undef far
#undef near

const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec2 aTexCoord;
    layout (location = 2) in vec3 aNormal;

    out vec2 TexCoord;
    out vec3 FragPos;
    out vec3 Normal;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main() {
        FragPos = vec3(model * vec4(aPos, 1.0));
        Normal = mat3(transpose(inverse(model))) * aNormal;
        TexCoord = aTexCoord;
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;

    in vec2 TexCoord;
    in vec3 FragPos;
    in vec3 Normal;

    uniform sampler2D texture1;

    struct PointLight {
        vec3 position;
        vec3 color;
        float intensity;
    };

    struct DirLight {
        vec3 direction;
        vec3 color;
        float intensity;
    };

    struct SpotLight {
        vec3 position;
        vec3 direction;
        float cutOff;
        vec3 color;
        float intensity;
    };

    uniform PointLight pointLight;
    uniform DirLight dirLight;
    uniform SpotLight spotLight;
    uniform vec3 viewPos;

    void main() {
        vec3 norm = normalize(Normal);
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 lightDir = normalize(-dirLight.direction);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * dirLight.color * dirLight.intensity;
        vec3 plDir = normalize(pointLight.position - FragPos);
        float plDiff = max(dot(norm, plDir), 0.0);
        vec3 pointDiffuse = plDiff * pointLight.color * pointLight.intensity;
        vec3 slDir = normalize(spotLight.position - FragPos);
        float theta = dot(normalize(-spotLight.direction), slDir);
        float intensity = clamp((theta - spotLight.cutOff) / (1.0 - spotLight.cutOff), 0.0, 1.0);
        float slDiff = max(dot(norm, slDir), 0.0);
        vec3 spotDiffuse = slDiff * spotLight.color * spotLight.intensity * intensity;
        vec3 texColor = texture(texture1, TexCoord).rgb;
        vec3 result = texColor * (diffuse + pointDiffuse + spotDiffuse);
        FragColor = vec4(result, 1.0);
    }
)";

struct Vector3 { float x, y, z; };
struct Vector2 { float u, v; };

struct Vertex {
    float x, y, z;
    float u, v;
    float nx, ny, nz;
};

struct Mesh {
    GLuint vao;
    GLuint vbo;
    GLuint textureID;
    size_t vertexCount;
};

struct Mat4 {
    float m[16];

    static Mat4 identity() {
        Mat4 res = { 0 };
        res.m[0] = 1; res.m[5] = 1; res.m[10] = 1; res.m[15] = 1;
        return res;
    }

    static Mat4 translate(float x, float y, float z) {
        Mat4 res = identity();
        res.m[12] = x; res.m[13] = y; res.m[14] = z;
        return res;
    }

    static Mat4 scale(float x, float y, float z) {
        Mat4 res = identity();
        res.m[0] = x; res.m[5] = y; res.m[10] = z;
        return res;
    }

    static Mat4 rotateY(float angleDegrees) {
        Mat4 res = identity();
        float rad = angleDegrees * 3.14159f / 180.0f;
        float c = cos(rad);
        float s = sin(rad);
        res.m[0] = c; res.m[2] = s;
        res.m[8] = -s; res.m[10] = c;
        return res;
    }

    static Mat4 perspective(float fov, float aspect, float near, float far) {
        Mat4 res = { 0 };
        float tanHalfFov = tan(fov / 2.0f);
        res.m[0] = 1.0f / (aspect * tanHalfFov);
        res.m[5] = 1.0f / tanHalfFov;
        res.m[10] = -(far + near) / (far - near);
        res.m[11] = -1.0f;
        res.m[14] = -(2.0f * far * near) / (far - near);
        return res;
    }

    Mat4 operator*(const Mat4& other) const {
        Mat4 res = { 0 };
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                for (int i = 0; i < 4; i++) {
                    res.m[c * 4 + r] += m[i * 4 + r] * other.m[c * 4 + i];
                }
            }
        }
        return res;
    }
};

Mat4 createLookAt(Vector3 eye, Vector3 center, Vector3 upVec) {
    Vector3 f = { center.x - eye.x, center.y - eye.y, center.z - eye.z };
    float fLen = sqrt(f.x * f.x + f.y * f.y + f.z * f.z);
    f = { f.x / fLen, f.y / fLen, f.z / fLen };
    float upLen = sqrt(upVec.x * upVec.x + upVec.y * upVec.y + upVec.z * upVec.z);
    Vector3 up = { upVec.x / upLen, upVec.y / upLen, upVec.z / upLen };
    Vector3 s = { f.y * up.z - f.z * up.y, f.z * up.x - f.x * up.z, f.x * up.y - f.y * up.x };
    float sLen = sqrt(s.x * s.x + s.y * s.y + s.z * s.z);
    s = { s.x / sLen, s.y / sLen, s.z / sLen };
    Vector3 u = { s.y * f.z - s.z * f.y, s.z * f.x - s.x * f.z, s.x * f.y - s.y * f.x };
    Mat4 res = Mat4::identity();
    res.m[0] = s.x; res.m[4] = s.y; res.m[8] = s.z;
    res.m[1] = u.x; res.m[5] = u.y; res.m[9] = u.z;
    res.m[2] = -f.x; res.m[6] = -f.y; res.m[10] = -f.z;
    Mat4 trans = Mat4::translate(-eye.x, -eye.y, -eye.z);
    return res * trans;
}

Mesh loadObjToGPU(const std::string& filename) {
    std::vector<Vector3> temp_pos;
    std::vector<Vector2> temp_uv;
    std::vector<Vector3> temp_norm;
    std::vector<Vertex> finalVertices;
    std::ifstream file(filename);
    Mesh mesh = { 0, 0, 0, 0 };
    if (!file.is_open()) return mesh;
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string prefix;
        ss >> prefix;
        if (prefix == "v") { Vector3 v; ss >> v.x >> v.y >> v.z; temp_pos.push_back(v); }
        else if (prefix == "vt") { Vector2 v; ss >> v.u >> v.v; temp_uv.push_back(v); }
        else if (prefix == "vn") { Vector3 v; ss >> v.x >> v.y >> v.z; temp_norm.push_back(v); }
        else if (prefix == "f") {
            std::string vertexStr;
            while (ss >> vertexStr) {
                std::stringstream vs(vertexStr);
                std::string segment;
                std::vector<std::string> indices;
                while (std::getline(vs, segment, '/')) indices.push_back(segment);
                Vertex v = { 0 };
                int posIdx = std::stoi(indices[0]) - 1;
                v.x = temp_pos[posIdx].x;
                v.y = temp_pos[posIdx].y;
                v.z = temp_pos[posIdx].z;
                if (indices.size() > 1 && !indices[1].empty()) {
                    int uvIdx = std::stoi(indices[1]) - 1;
                    v.u = temp_uv[uvIdx].u;
                    v.v = temp_uv[uvIdx].v;
                }
                if (indices.size() > 2) {
                    int normIdx = std::stoi(indices[2]) - 1;
                    v.nx = temp_norm[normIdx].x;
                    v.ny = temp_norm[normIdx].y;
                    v.nz = temp_norm[normIdx].z;
                }
                finalVertices.push_back(v);
            }
        }
    }
    mesh.vertexCount = finalVertices.size();
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, finalVertices.size() * sizeof(Vertex), finalVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    return mesh;
}

GLuint createTexture(int r, int g, int b) {
    sf::Image img;
    img.create(64, 64);
    for (unsigned int x = 0; x < 64; ++x) {
        for (unsigned int y = 0; y < 64; ++y) {
            sf::Color c = ((x / 8 + y / 8) % 2 == 0) ? sf::Color(r, g, b) : sf::Color(r / 2, g / 2, b / 2);
            img.setPixel(x, y, c);
        }
    }
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.getPixelsPtr());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return tex;
}

GLuint compileShaders() {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSource, NULL);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSource, NULL);
    glCompileShader(fs);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

struct PointLight { Vector3 position; Vector3 color; float intensity; };
struct DirLight { Vector3 direction; Vector3 color; float intensity; };
struct SpotLight { Vector3 position; Vector3 direction; float cutOff; Vector3 color; float intensity; };

int main() {
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.majorVersion = 3;
    settings.minorVersion = 3;
    settings.attributeFlags = sf::ContextSettings::Core;
    sf::Window window(sf::VideoMode(800, 600), "Modern OpenGL", sf::Style::Default, settings);
    window.setVerticalSyncEnabled(true);
    window.setActive(true);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return -1;
    glEnable(GL_DEPTH_TEST);
    GLuint shaderProgram = compileShaders();
    glUseProgram(shaderProgram);
    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint projLoc = glGetUniformLocation(shaderProgram, "projection");
    GLint texLoc = glGetUniformLocation(shaderProgram, "texture1");
    Mesh floor = loadObjToGPU("floor.obj");
    floor.textureID = createTexture(100, 100, 100);
    Mesh cube = loadObjToGPU("cube.obj");
    cube.textureID = createTexture(200, 50, 50);
    Mesh pyramid = loadObjToGPU("pyramid.obj");
    pyramid.textureID = createTexture(50, 200, 50);
    Mesh pillar = loadObjToGPU("pillar.obj");
    pillar.textureID = createTexture(50, 50, 200);
    Mesh gem = loadObjToGPU("gem.obj");
    gem.textureID = createTexture(200, 200, 50);
    glUniform1i(texLoc, 0);
    PointLight pointLight = { {5.0f, 5.0f, 5.0f}, {1.0f, 1.0f, 1.0f}, 1.0f };
    DirLight dirLight = { {-1.0f, -1.0f, -1.0f}, {0.8f, 0.8f, 0.8f}, 0.5f };
    SpotLight spotLight = { {0.0f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, cos(15.0f * 3.14159 / 180.0), {1.0f, 0.8f, 0.8f}, 1.0f };
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::Resized) glViewport(0, 0, event.size.width, event.size.height);
        }


        float pointSpeed = 0.2f;
        float spotSpeed = 0.2f;
        float intensityStep = 0.1f;
        float spotCutStep = 0.05f;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::I)) pointLight.position.z -= pointSpeed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::K)) pointLight.position.z += pointSpeed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::J)) pointLight.position.x -= pointSpeed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::L)) pointLight.position.x += pointSpeed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::U)) pointLight.intensity += intensityStep;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::O)) pointLight.intensity -= intensityStep;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) spotLight.position.z -= spotSpeed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) spotLight.position.z += spotSpeed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) spotLight.position.x -= spotSpeed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) spotLight.position.x += spotSpeed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Q)) spotLight.direction.y += 0.1f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::E)) spotLight.direction.y -= 0.1f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Z)) spotLight.cutOff = std::min(1.0f, spotLight.cutOff + spotCutStep);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::X)) spotLight.cutOff = std::max(0.0f, spotLight.cutOff - spotCutStep);

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::T)) dirLight.direction.x += 0.1f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::G)) dirLight.direction.x -= 0.1f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Y)) dirLight.direction.y += 0.1f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::H)) dirLight.direction.y -= 0.1f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::U)) dirLight.intensity += 0.1f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::J)) dirLight.intensity -= 0.1f;

        glUniform3f(glGetUniformLocation(shaderProgram, "pointLight.position"), pointLight.position.x, pointLight.position.y, pointLight.position.z);
        glUniform3f(glGetUniformLocation(shaderProgram, "pointLight.color"), pointLight.color.x, pointLight.color.y, pointLight.color.z);
        glUniform1f(glGetUniformLocation(shaderProgram, "pointLight.intensity"), pointLight.intensity);

        glUniform3f(glGetUniformLocation(shaderProgram, "spotLight.position"), spotLight.position.x, spotLight.position.y, spotLight.position.z);
        glUniform3f(glGetUniformLocation(shaderProgram, "spotLight.direction"), spotLight.direction.x, spotLight.direction.y, spotLight.direction.z);
        glUniform1f(glGetUniformLocation(shaderProgram, "spotLight.cutOff"), spotLight.cutOff);
        glUniform3f(glGetUniformLocation(shaderProgram, "spotLight.color"), spotLight.color.x, spotLight.color.y, spotLight.color.z);
        glUniform1f(glGetUniformLocation(shaderProgram, "spotLight.intensity"), spotLight.intensity);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);
        float aspect = (float)window.getSize().x / window.getSize().y;
        float fovRad = 45.0f * 3.14159f / 180.0f;
        Mat4 projection = Mat4::perspective(fovRad, aspect, 0.1f, 100.0f);
        Vector3 eye = { 10.0f, 8.0f, 10.0f };
        Vector3 center = { 0.0f, 1.0f, 0.0f };
        Vector3 up = { 0.0f, 1.0f, 0.0f };
        Mat4 view = createLookAt(eye, center, up);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, view.m);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection.m);
        GLint viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos");
        glUniform3f(viewPosLoc, eye.x, eye.y, eye.z);
        glUniform3f(glGetUniformLocation(shaderProgram, "pointLight.position"), pointLight.position.x, pointLight.position.y, pointLight.position.z);
        glUniform3f(glGetUniformLocation(shaderProgram, "pointLight.color"), pointLight.color.x, pointLight.color.y, pointLight.color.z);
        glUniform1f(glGetUniformLocation(shaderProgram, "pointLight.intensity"), pointLight.intensity);
        glUniform3f(glGetUniformLocation(shaderProgram, "dirLight.direction"), dirLight.direction.x, dirLight.direction.y, dirLight.direction.z);
        glUniform3f(glGetUniformLocation(shaderProgram, "dirLight.color"), dirLight.color.x, dirLight.color.y, dirLight.color.z);
        glUniform1f(glGetUniformLocation(shaderProgram, "dirLight.intensity"), dirLight.intensity);
        glUniform3f(glGetUniformLocation(shaderProgram, "spotLight.position"), spotLight.position.x, spotLight.position.y, spotLight.position.z);
        glUniform3f(glGetUniformLocation(shaderProgram, "spotLight.direction"), spotLight.direction.x, spotLight.direction.y, spotLight.direction.z);
        glUniform1f(glGetUniformLocation(shaderProgram, "spotLight.cutOff"), spotLight.cutOff);
        glUniform3f(glGetUniformLocation(shaderProgram, "spotLight.color"), spotLight.color.x, spotLight.color.y, spotLight.color.z);
        glUniform1f(glGetUniformLocation(shaderProgram, "spotLight.intensity"), spotLight.intensity);
        {
            Mat4 model = Mat4::identity();
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model.m);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, floor.textureID);
            glBindVertexArray(floor.vao);
            glDrawArrays(GL_TRIANGLES, 0, floor.vertexCount);
        }
        {
            Mat4 model = Mat4::scale(1.5f, 1.5f, 1.5f);
            Mat4 trans = Mat4::translate(0.0f, 2.0f, 0.0f);
            model = trans * model;
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model.m);
            glBindTexture(GL_TEXTURE_2D, gem.textureID);
            glBindVertexArray(gem.vao);
            glDrawArrays(GL_TRIANGLES, 0, gem.vertexCount);
        }
        {
            Mat4 rot = Mat4::rotateY(30.0f);
            Mat4 trans = Mat4::translate(-3.0f, 0.5f, -3.0f);
            Mat4 model = trans * rot;
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model.m);
            glBindTexture(GL_TEXTURE_2D, cube.textureID);
            glBindVertexArray(cube.vao);
            glDrawArrays(GL_TRIANGLES, 0, cube.vertexCount);
        }
        {
            Mat4 model = Mat4::translate(3.0f, 0.0f, 3.0f);
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model.m);
            glBindTexture(GL_TEXTURE_2D, pyramid.textureID);
            glBindVertexArray(pyramid.vao);
            glDrawArrays(GL_TRIANGLES, 0, pyramid.vertexCount);
        }
        {
            Mat4 model = Mat4::translate(-3.0f, 1.5f, 3.0f);
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model.m);
            glBindTexture(GL_TEXTURE_2D, pillar.textureID);
            glBindVertexArray(pillar.vao);
            glDrawArrays(GL_TRIANGLES, 0, pillar.vertexCount);
        }
        {
            Mat4 model = Mat4::translate(3.0f, 0.5f, -3.0f);
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model.m);
            glBindTexture(GL_TEXTURE_2D, cube.textureID);
            glBindVertexArray(cube.vao);
            glDrawArrays(GL_TRIANGLES, 0, cube.vertexCount);
        }
        window.display();
    }
    glDeleteVertexArrays(1, &floor.vao); glDeleteBuffers(1, &floor.vbo);
    glDeleteVertexArrays(1, &cube.vao); glDeleteBuffers(1, &cube.vbo);
    glDeleteVertexArrays(1, &pyramid.vao); glDeleteBuffers(1, &pyramid.vbo);
    glDeleteVertexArrays(1, &pillar.vao); glDeleteBuffers(1, &pillar.vbo);
    glDeleteVertexArrays(1, &gem.vao); glDeleteBuffers(1, &gem.vbo);
    return 0;
}
