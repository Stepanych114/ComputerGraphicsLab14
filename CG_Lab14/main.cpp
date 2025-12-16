#include <GL/glew.h>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <iomanip>

bool g_pointLightOn = true;
bool g_directionalLightOn = true;
bool g_spotLightOn = true;

GLfloat g_pointLightPos[] = { 0.0f, 5.0f, 0.0f, 1.0f };
float g_pointLightIntensity = 1.0f;

GLfloat g_directionalLightDir[] = { -1.0f, -1.0f, -1.0f, 0.0f };

GLfloat g_spotLightPos[] = { -3.0f, 4.0f, -3.0f, 1.0f };
float g_spotLightAngle = 25.0f;


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

void setPhongMaterial(float r, float g, float b, float shininess) {
    GLfloat ambient[] = { 0.1f * r, 0.1f * g, 0.1f * b, 1.0f };
    GLfloat diffuse[] = { r, g, b, 1.0f };
    GLfloat specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, specular);
    glMaterialf(GL_FRONT, GL_SHININESS, shininess);
}

void setToonMaterial(float r, float g, float b) {
    GLfloat ambient[] = { 0.2f * r, 0.2f * g, 0.2f * b, 1.0f };
    GLfloat diffuse[] = { r, g, b, 1.0f };
    GLfloat specular[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, specular);
    glMaterialf(GL_FRONT, GL_SHININESS, 0.0f);
}


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
    settings.majorVersion = 2;
    settings.minorVersion = 1;

    sf::Window window(sf::VideoMode(800, 600), "Interactive Lighting Scene", sf::Style::Default, settings);
    window.setVerticalSyncEnabled(true);
    window.setActive(true);

    glewInit();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);

    GLfloat global_ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global_ambient);

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

            if (event.type == sf::Event::KeyPressed) {
                float move_speed = 0.5f;
                float angle_speed = 2.0f;
                float intensity_speed = 0.1f;
                float dir_speed = 0.1f;

                switch (event.key.code) {
                case sf::Keyboard::Num1: g_pointLightOn = !g_pointLightOn; std::cout << "Point light: " << (g_pointLightOn ? "ON" : "OFF") << std::endl; break;
                case sf::Keyboard::Num2: g_directionalLightOn = !g_directionalLightOn; std::cout << "Directional light: " << (g_directionalLightOn ? "ON" : "OFF") << std::endl; break;
                case sf::Keyboard::Num3: g_spotLightOn = !g_spotLightOn; std::cout << "Spotlight: " << (g_spotLightOn ? "ON" : "OFF") << std::endl; break;

                case sf::Keyboard::W: g_pointLightPos[2] -= move_speed; break;
                case sf::Keyboard::S: g_pointLightPos[2] += move_speed; break;
                case sf::Keyboard::A: g_pointLightPos[0] -= move_speed; break;
                case sf::Keyboard::D: g_pointLightPos[0] += move_speed; break;
                case sf::Keyboard::Q: g_pointLightPos[1] += move_speed; break;
                case sf::Keyboard::E: g_pointLightPos[1] -= move_speed; break;
                case sf::Keyboard::Up: g_pointLightIntensity += intensity_speed; if (g_pointLightIntensity > 5.0f) g_pointLightIntensity = 5.0f; break;
                case sf::Keyboard::Down: g_pointLightIntensity -= intensity_speed; if (g_pointLightIntensity < 0.0f) g_pointLightIntensity = 0.0f; break;

                case sf::Keyboard::T: g_directionalLightDir[1] += dir_speed; break;
                case sf::Keyboard::G: g_directionalLightDir[1] -= dir_speed; break; 
                case sf::Keyboard::F: g_directionalLightDir[0] -= dir_speed; break; 
                case sf::Keyboard::H: g_directionalLightDir[0] += dir_speed; break; 


                case sf::Keyboard::I: g_spotLightPos[2] -= move_speed; break;
                case sf::Keyboard::K: g_spotLightPos[2] += move_speed; break;
                case sf::Keyboard::J: g_spotLightPos[0] -= move_speed; break;
                case sf::Keyboard::L: g_spotLightPos[0] += move_speed; break;
                case sf::Keyboard::U: g_spotLightPos[1] += move_speed; break;
                case sf::Keyboard::O: g_spotLightPos[1] -= move_speed; break;
                case sf::Keyboard::Left: g_spotLightAngle -= angle_speed; if (g_spotLightAngle < 0.0f) g_spotLightAngle = 0.0f; break;
                case sf::Keyboard::Right: g_spotLightAngle += angle_speed; if (g_spotLightAngle > 90.0f) g_spotLightAngle = 90.0f; break;

                default: break;
                }
            }
        }

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
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

        if (g_pointLightOn) {
            glEnable(GL_LIGHT0);
            GLfloat intensity[] = { 1.0f * g_pointLightIntensity, 0.8f * g_pointLightIntensity, 0.8f * g_pointLightIntensity, 1.0f };
            glLightfv(GL_LIGHT0, GL_POSITION, g_pointLightPos);
            glLightfv(GL_LIGHT0, GL_DIFFUSE, intensity);
            glLightfv(GL_LIGHT0, GL_SPECULAR, intensity);
        }
        else {
            glDisable(GL_LIGHT0);
        }

        if (g_directionalLightOn) {
            glEnable(GL_LIGHT1);
            GLfloat light_intensity[] = { 0.8f, 0.8f, 0.8f, 1.0f };
            glLightfv(GL_LIGHT1, GL_POSITION, g_directionalLightDir);
            glLightfv(GL_LIGHT1, GL_DIFFUSE, light_intensity);
            glLightfv(GL_LIGHT1, GL_SPECULAR, light_intensity);
        }
        else {
            glDisable(GL_LIGHT1);
        }

        if (g_spotLightOn) {
            glEnable(GL_LIGHT2);
            GLfloat light_intensity[] = { 1.0f, 1.0f, 0.5f, 1.0f };
            GLfloat spot_direction[] = {
                0.0f - g_spotLightPos[0],
                2.0f - g_spotLightPos[1],
                0.0f - g_spotLightPos[2]
            };
            glLightfv(GL_LIGHT2, GL_POSITION, g_spotLightPos);
            glLightfv(GL_LIGHT2, GL_SPOT_DIRECTION, spot_direction);
            glLightf(GL_LIGHT2, GL_SPOT_CUTOFF, g_spotLightAngle);
            glLightf(GL_LIGHT2, GL_SPOT_EXPONENT, 15.0f);
            glLightfv(GL_LIGHT2, GL_DIFFUSE, light_intensity);
            glLightfv(GL_LIGHT2, GL_SPECULAR, light_intensity);
        }
        else {
            glDisable(GL_LIGHT2);
        }

        setPhongMaterial(0.7f, 0.7f, 0.7f, 32.0f);
        drawMesh(floor);

        glPushMatrix();
        glTranslatef(0.0f, 2.0f, 0.0f);
        glScalef(1.5f, 1.5f, 1.5f);
        setPhongMaterial(0.9f, 0.9f, 0.2f, 128.0f);
        drawMesh(gem);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(-3.0f, 0.5f, -3.0f);
        glRotatef(30.0f, 0.0f, 1.0f, 0.0f);
        setToonMaterial(0.8f, 0.2f, 0.2f);
        drawMesh(cube);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(3.0f, 0.0f, 3.0f);
        setPhongMaterial(0.2f, 0.8f, 0.2f, 16.0f);
        drawMesh(pyramid);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(-3.0f, 1.5f, 3.0f);
        setToonMaterial(0.2f, 0.2f, 0.8f);
        drawMesh(pillar);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(3.0f, 0.5f, -3.0f);
        setPhongMaterial(0.8f, 0.2f, 0.2f, 64.0f);
        drawMesh(cube);
        glPopMatrix();

        window.display();
    }

    return 0;
}