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
    void processLuaSceneFile(const std::string & filename, std::shared_ptr<SceneNode> &node);
    void createShaderProgram();
    void enableVertexShaderInputSlots();
    void uploadVertexDataToVbos(const MeshConsolidator & meshConsolidator);
    void mapVboDataToVertexShaderInputLocations();
    void mapTextures();
    void initShadowMap(GLuint width, GLuint height);
    void initViewMatrix();
    void initLightSources();
    void initPerspectiveMatrix();
    
    //-- Rendering methods:
    void uploadCommonSceneUniforms();
    void updateShaderUniforms(const GeometryNode & node, const glm::mat4 & viewMatrix, const glm::mat4 & trans);
    void renderSceneDepth(const ShaderProgram & shader);
    void renderScene();
    void renderSceneGraph(const ShaderProgram & shader, const SceneNode &node, const glm::mat4 & view, const glm::mat4 & trans);

    //-- Interaction methods:
    void movePlayer(unsigned int time);

    glm::mat4 m_perpsective;
    glm::mat4 m_view;
    GLfloat m_farPlane;

    LightSource m_light;
    LightSource m_light2;
    glm::mat4 m_lightPerspsective;
    std::vector<glm::mat4> m_lightViews;
    std::vector<glm::mat4> m_lightViews2;

    std::vector<LightSource> m_lights;
    std::vector<std::vector<glm::mat4> > m_lightsViews;

    //-- GL resources for shader:
    GLuint m_vao_meshData;
    GLuint m_vbo_vertexPositions;
    GLuint m_vbo_vertexNormals;
    GLint m_positionAttribLocation;
    GLint m_normalAttribLocation;
    ShaderProgram m_shader;

    //-- GL resources for texture shader:
    GLuint m_vao_meshDataTex;
    GLuint m_vbo_vertexPositionsTex;
    GLuint m_vbo_vertexNormalsTex;
    GLuint m_vbo_vertexTexCoords;
    GLint m_positionAttribLocationTex;
    GLint m_normalAttribLocationTex;
    GLint m_texCoordsAttribLocation;
    GLuint m_arenaTextureData;
    GLuint m_characterTextureData;
    ShaderProgram m_texShader;

    //-- GL resources for depth shader:
    GLuint m_vao_meshDataDepth;
    GLuint m_vbo_vertexPositionsDepth;
    GLint m_positionAttribLocationDepth;
    GLuint m_fbo_depthMap;
    GLuint m_depthMapTex;
    ShaderProgram m_depthShader;

    //-- GL resources for depth shader 2:
    GLuint m_vao_meshDataDepth2;
    GLuint m_vbo_vertexPositionsDepth2;
    GLint m_positionAttribLocationDepth2;
    GLuint m_fbo_depthMap2;
    GLuint m_depthMapTex2;
    ShaderProgram m_depthShader2;

    BatchInfoMap m_batchInfoMap;

    std::string m_arenaFile;

    std::shared_ptr<SceneNode> m_arenaNode;
    std::shared_ptr<SceneNode> m_ballNode;
    std::shared_ptr<SceneNode> m_playerNode;

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

    // game states
    bool m_paused;
};