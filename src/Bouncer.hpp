#pragma once

#include "cs488-framework/CS488Window.hpp"
#include "cs488-framework/OpenGLImport.hpp"
#include "cs488-framework/ShaderProgram.hpp"
#include "cs488-framework/MeshConsolidator.hpp"

#include "SceneNode.hpp"
#include "GeometryNode.hpp"
#include "JointNode.hpp"

#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <chrono>

typedef std::chrono::high_resolution_clock Clock;

struct LightSource {
    glm::vec3 position;
    glm::vec3 rgbIntensity;
};


class Bouncer : public CS488Window {
public:
    Bouncer(const std::string & arenaFile);
    virtual ~Bouncer();

protected:
    virtual void init() override;
    virtual void appLogic() override;
    virtual void guiLogic() override;
    virtual void draw() override;
    virtual void cleanup() override;

    //-- Virtual callback methods
    virtual bool cursorEnterWindowEvent(int entered) override;
    virtual bool mouseMoveEvent(double xPos, double yPos) override;
    virtual bool mouseButtonInputEvent(int button, int actions, int mods) override;
    virtual bool mouseScrollEvent(double xOffSet, double yOffSet) override;
    virtual bool windowResizeEvent(int width, int height) override;
    virtual bool keyInputEvent(int key, int action, int mods) override;

    //-- One time initialization methods:
    void processLuaSceneFile(const std::string & filename);
    void createShaderProgram();
    void enableVertexShaderInputSlots();
    void uploadVertexDataToVbos(const MeshConsolidator & meshConsolidator);
    void mapVboDataToVertexShaderInputLocations();
    void initViewMatrix();
    void initLightSources();
    void initPerspectiveMatrix();
    
    //-- Rendering methods:
    void uploadCommonSceneUniforms();
    void updateShaderUniforms(const GeometryNode & node, const glm::mat4 & viewMatrix, const glm::mat4 & trans);
    void renderSceneGraph(const SceneNode &node, const glm::mat4 & trans);

    //-- Interaction methods:
    void movePlayer(unsigned int time);

    glm::mat4 m_perpsective;
    glm::mat4 m_view;

    LightSource m_light;

    //-- GL resources for mesh geometry data:
    GLuint m_vao_meshData;
    GLuint m_vbo_vertexPositions;
    GLuint m_vbo_vertexNormals;
    GLint m_positionAttribLocation;
    GLint m_normalAttribLocation;
    ShaderProgram m_shader;

    BatchInfoMap m_batchInfoMap;

    std::string m_arenaFile;

    std::shared_ptr<SceneNode> m_rootNode;

    // mouse states
    double m_lastMouseX;
    double m_lastMouseY;

    // key states
    bool m_keyUp;
    bool m_keyDown;
    bool m_keyLeft;
    bool m_keyRight;
    bool m_keyForward;

    // timer states
    std::chrono::time_point<std::chrono::high_resolution_clock> m_prevTime;
};