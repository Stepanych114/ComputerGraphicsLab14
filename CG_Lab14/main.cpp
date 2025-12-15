#include <GL/glew.h>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>

struct Vector3 {
    float x, y, z;
};

struct Vector2 {
    float u, v;
};

struct Vertex {
    Vector3 position;
    Vector2 texCoord;
    Vector3 normal;
};

struct Mesh {
    std::vector<Vertex> vertices;
    GLuint textureID;
};

Mesh loadObj(const std::string& filename) {
    std::vector<Vector3> temp_pos;
    std::vector<Vector2> temp_uv;
    std::vector<Vector3> temp_norm;
    Mesh mesh;

    std::ifstream file(filename);
    if (!file.is_open()) return mesh;

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string prefix;
        ss >> prefix;

        if (prefix == "v") {
            Vector3 v;
            ss >> v.x >> v.y >> v.z;
            temp_pos.push_back(v);
        }
        else if (prefix == "vt") {
            Vector2 v;
            ss >> v.u >> v.v;
            temp_uv.push_back(v);
        }
        else if (prefix == "vn") {
            Vector3 v;
            ss >> v.x >> v.y >> v.z;
            temp_norm.push_back(v);
        }
        else if (prefix == "f") {
            std::string vertexStr;
            while (ss >> vertexStr) {
                std::stringstream vs(vertexStr);
                std::string segment;
                std::vector<std::string> indices;
                while (std::getline(vs, segment, '/')) {
                    indices.push_back(segment);
                }

                Vertex v;
                int posIdx = std::stoi(indices[0]) - 1;
                v.position = temp_pos[posIdx];

                if (indices.size() > 1 && !indices[1].empty()) {
                    int uvIdx = std::stoi(indices[1]) - 1;
                    v.texCoord = temp_uv[uvIdx];
                }

                if (indices.size() > 2) {
                    int normIdx = std::stoi(indices[2]) - 1;
                    v.normal = temp_norm[normIdx];
                }
                mesh.vertices.push_back(v);
            }
        }
    }
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return tex;
}

void drawMesh(const Mesh& mesh) {
    glBindTexture(GL_TEXTURE_2D, mesh.textureID);
    glBegin(GL_TRIANGLES);
    for (const auto& v : mesh.vertices) {
        glNormal3f(v.normal.x, v.normal.y, v.normal.z);
        glTexCoord2f(v.texCoord.u, v.texCoord.v);
        glVertex3f(v.position.x, v.position.y, v.position.z);
    }
    glEnd();
}

void setLookAt(float eyeX, float eyeY, float eyeZ, float centerX, float centerY, float centerZ, float upX, float upY, float upZ) {
    Vector3 f = { centerX - eyeX, centerY - eyeY, centerZ - eyeZ };
    float flen = sqrt(f.x * f.x + f.y * f.y + f.z * f.z);
    f = { f.x / flen, f.y / flen, f.z / flen };

    Vector3 up = { upX, upY, upZ };
    Vector3 s = { f.y * up.z - f.z * up.y, f.z * up.x - f.x * up.z, f.x * up.y - f.y * up.x };
    float slen = sqrt(s.x * s.x + s.y * s.y + s.z * s.z);
    s = { s.x / slen, s.y / slen, s.z / slen };

    Vector3 u = { s.y * f.z - s.z * f.y, s.z * f.x - s.x * f.z, s.x * f.y - s.y * f.x };

    float viewMat[16] = {
        s.x, u.x, -f.x, 0,
        s.y, u.y, -f.y, 0,
        s.z, u.z, -f.z, 0,
        0, 0, 0, 1
    };
    glMultMatrixf(viewMat);
    glTranslatef(-eyeX, -eyeY, -eyeZ);
}

int main() {
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.stencilBits = 8;
    settings.antialiasingLevel = 4;
    settings.majorVersion = 3;
    settings.minorVersion = 0;

    sf::Window window(sf::VideoMode(800, 600), "Static Scene No Lighting", sf::Style::Default, settings);
    window.setVerticalSyncEnabled(true);
    window.setActive(true);

    glewInit();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    Mesh floor = loadObj("floor.obj");
    floor.textureID = createTexture(100, 100, 100);

    Mesh cube = loadObj("cube.obj");
    cube.textureID = createTexture(200, 50, 50);

    Mesh pyramid = loadObj("pyramid.obj");
    pyramid.textureID = createTexture(50, 200, 50);

    Mesh pillar = loadObj("pillar.obj");
    pillar.textureID = createTexture(50, 50, 200);

    Mesh gem = loadObj("gem.obj");
    gem.textureID = createTexture(200, 200, 50);

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::Resized) glViewport(0, 0, event.size.width, event.size.height);
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float aspect = (float)window.getSize().x / window.getSize().y;
        float fov = 45.0f * 3.14159f / 180.0f;
        float fH = tan(fov / 2) * 0.1f;
        float fW = fH * aspect;
        glFrustum(-fW, fW, -fH, fH, 0.1f, 100.0f);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        setLookAt(10.0f, 8.0f, 10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

        drawMesh(floor);

        glPushMatrix();
        glTranslatef(0.0f, 2.0f, 0.0f);
        glScalef(1.5f, 1.5f, 1.5f);
        drawMesh(gem);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(-3.0f, 0.5f, -3.0f);
        glRotatef(30.0f, 0.0f, 1.0f, 0.0f);
        drawMesh(cube);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(3.0f, 0.0f, 3.0f);
        drawMesh(pyramid);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(-3.0f, 1.5f, 3.0f);
        drawMesh(pillar);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(3.0f, 0.5f, -3.0f);
        drawMesh(cube);
        glPopMatrix();

        window.display();
    }

    return 0;
}