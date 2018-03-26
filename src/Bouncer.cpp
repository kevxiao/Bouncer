#include "Bouncer.hpp"
#include "scene_lua.hpp"
using namespace std;

#include "glframework/GlErrorCheck.hpp"
#include "glframework/MathUtils.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <imgui/imgui.h>
#include <imgui_impl_glfw_gl3.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

using namespace glm;

const float EPSILON = 0.0000001;
const unsigned int SHADOW_HEIGHT = 2048;
const unsigned int SHADOW_WIDTH = 2048;
const unsigned int MAX_PARTICLES = 2000;

//----------------------------------------------------------------------------------------
// Constructor
Bouncer::Bouncer(const std::string & arenaFile)
    : m_arenaFile(arenaFile),
      m_keyUp(false),
      m_keyDown(false),
      m_keyLeft(false),
      m_keyRight(false),
      m_keyForward(false),
      m_lastMouseX(0.),
      m_lastMouseY(0.),
      m_gamePaused(true),
      m_titleScreen(true),
      m_showFps(false),
      m_particleLife(0)
{

}

//----------------------------------------------------------------------------------------
// Destructor
Bouncer::~Bouncer()
{

}

//----------------------------------------------------------------------------------------
/*
 * Called once, at program start.
 */
void Bouncer::init()
{
    m_soundEngine = irrklang::createIrrKlangDevice();

    // Set the background colour.
    glClearColor(0.35, 0.35, 0.35, 1.0);

    createShaderProgram();

    enableVertexShaderInputSlots();

    processLuaSceneFile(m_arenaFile, m_arenaNode);
    processLuaSceneFile(string("Assets/ball.lua"), m_ballNode);
    processLuaSceneFile(string("Assets/player.lua"), m_playerNode);

    // Load and decode all .obj files at once here.  You may add additional .obj files to
    // this list in order to support rendering additional mesh types.  All vertex
    // positions, and normals will be extracted and stored within the MeshConsolidator
    // class.
    unique_ptr<MeshConsolidator> meshConsolidator (new MeshConsolidator{
            getAssetFilePath("cube.obj"),
            getAssetFilePath("sphere.obj"),
            getAssetFilePath("sphere_flip.obj"),
            getAssetFilePath("icosphere.obj"),
            getAssetFilePath("icosphere_flip.obj"),
    });

    // Acquire the BatchInfoMap from the MeshConsolidator.
    meshConsolidator->getBatchInfoMap(m_batchInfoMap);

    // Take all vertex data within the MeshConsolidator and upload it to VBOs on the GPU.
    uploadVertexDataToVbos(*meshConsolidator);

    mapVboDataToVertexShaderInputLocations();

    mapTextures();

    initShadowMap(SHADOW_WIDTH, SHADOW_HEIGHT);

    initDepthMap();

    initPostProcessFb();

    initLightSources();

    initPerspectiveMatrix();

    initViewMatrix();

    m_prevTime = chrono::high_resolution_clock::now();
    //glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwGetCursorPos(m_window, &m_lastMouseX, &m_lastMouseY);

    initGameParams();

    initPlayer();

    initAnimations();

    random_device rd;
    m_randomGenerator.seed(rd());

    m_soundEngine->play2D(getAssetFilePath("ambient.mp3").c_str(), true);
}

//----------------------------------------------------------------------------------------
void Bouncer::processLuaSceneFile(const std::string & filename, std::shared_ptr<SceneNode> &node) {
    node = std::shared_ptr<SceneNode>(import_lua(filename));
    if (!node) {
        std::cerr << "Could not open " << filename << std::endl;
    }
}

//----------------------------------------------------------------------------------------
void Bouncer::createShaderProgram()
{
    m_shader.generateProgramObject();
    m_shader.attachVertexShader( getShaderFilePath("VertexShader.vs").c_str() );
    m_shader.attachFragmentShader( getShaderFilePath("FragmentShader.fs").c_str() );
    m_shader.link();

    m_texShader.generateProgramObject();
    m_texShader.attachVertexShader( getShaderFilePath("TextureVertexShader.vs").c_str() );
    m_texShader.attachFragmentShader( getShaderFilePath("TextureFragmentShader.fs").c_str() );
    m_texShader.link();

    m_depthShader.generateProgramObject();
    m_depthShader.attachVertexShader( getShaderFilePath("DepthVertexShader.vs").c_str() );
    m_depthShader.attachGeometryShader( getShaderFilePath("DepthGeometryShader.gs").c_str() );
    m_depthShader.attachFragmentShader( getShaderFilePath("DepthFragmentShader.fs").c_str() );
    m_depthShader.link();

    m_depthShader2.generateProgramObject();
    m_depthShader2.attachVertexShader( getShaderFilePath("DepthVertexShader.vs").c_str() );
    m_depthShader2.attachGeometryShader( getShaderFilePath("DepthGeometryShader.gs").c_str() );
    m_depthShader2.attachFragmentShader( getShaderFilePath("DepthFragmentShader.fs").c_str() );
    m_depthShader2.link();

    m_instancedShader.generateProgramObject();
    m_instancedShader.attachVertexShader( getShaderFilePath("InstancedVertexShader.vs").c_str() );
    m_instancedShader.attachFragmentShader( getShaderFilePath("InstancedFragmentShader.fs").c_str() );
    m_instancedShader.link();

    m_motionDepthShader.generateProgramObject();
    m_motionDepthShader.attachVertexShader( getShaderFilePath("MotionDepthVertexShader.vs").c_str() );
    m_motionDepthShader.attachFragmentShader( getShaderFilePath("MotionDepthFragmentShader.fs").c_str() );
    m_motionDepthShader.link();

    m_postProcessShader.generateProgramObject();
    m_postProcessShader.attachVertexShader( getShaderFilePath("PostProcessVertexShader.vs").c_str() );
    m_postProcessShader.attachFragmentShader( getShaderFilePath("PostProcessFragmentShader.fs").c_str() );
    m_postProcessShader.link();
}

//----------------------------------------------------------------------------------------
void Bouncer::enableVertexShaderInputSlots()
{
    //-- Enable input slots for m_vao_meshData:
    {
        glGenVertexArrays(1, &m_vao_meshData);
        glBindVertexArray(m_vao_meshData);

        // Enable the vertex shader attribute location for "position" when rendering.
        m_positionAttribLocation = m_shader.getAttribLocation("position");
        glEnableVertexAttribArray(m_positionAttribLocation);

        // Enable the vertex shader attribute location for "normal" when rendering.
        m_normalAttribLocation = m_shader.getAttribLocation("normal");
        glEnableVertexAttribArray(m_normalAttribLocation);

        CHECK_GL_ERRORS;
    }

    //-- Enable input slots for m_vao_meshDataTex:
    {
        glGenVertexArrays(1, &m_vao_meshDataTex);
        glBindVertexArray(m_vao_meshDataTex);

        // Enable the vertex shader attribute location for "position" when rendering.
        m_positionAttribLocationTex = m_texShader.getAttribLocation("position");
        glEnableVertexAttribArray(m_positionAttribLocationTex);

        // Enable the vertex shader attribute location for "normal" when rendering.
        m_normalAttribLocationTex = m_texShader.getAttribLocation("normal");
        glEnableVertexAttribArray(m_normalAttribLocationTex);

        // Enable the vertex shader attribute location for "texcoord_in" when rendering.
        m_texCoordsAttribLocation = m_texShader.getAttribLocation("texcoord_in");
        glEnableVertexAttribArray(m_texCoordsAttribLocation);

        CHECK_GL_ERRORS;
    }

    //-- Enable input slots for m_vao_meshDataDepth:
    {
        glGenVertexArrays(1, &m_vao_meshDataDepth);
        glBindVertexArray(m_vao_meshDataDepth);

        // Enable the vertex shader attribute location for "position" when rendering.
        m_positionAttribLocationDepth = m_depthShader.getAttribLocation("position");
        glEnableVertexAttribArray(m_positionAttribLocationDepth);

        CHECK_GL_ERRORS;
    }

    //-- Enable input slots for m_vao_meshDataDepth2:
    {
        glGenVertexArrays(1, &m_vao_meshDataDepth2);
        glBindVertexArray(m_vao_meshDataDepth2);

        // Enable the vertex shader attribute location for "position" when rendering.
        m_positionAttribLocationDepth2 = m_depthShader.getAttribLocation("position");
        glEnableVertexAttribArray(m_positionAttribLocationDepth2);

        CHECK_GL_ERRORS;
    }

    //-- Enable input slots for m_vao_meshDataIns:
    {
        glGenVertexArrays(1, &m_vao_meshDataIns);
        glBindVertexArray(m_vao_meshDataIns);

        // Enable the vertex shader attribute location for "position" when rendering.
        m_positionAttribLocationIns = m_instancedShader.getAttribLocation("position");
        glEnableVertexAttribArray(m_positionAttribLocationIns);

        // Enable the vertex shader attribute location for "model" when rendering.
        m_modelAttribLocationIns = m_instancedShader.getAttribLocation("model");
        glEnableVertexAttribArray(m_modelAttribLocationIns);
        glEnableVertexAttribArray(m_modelAttribLocationIns + 1);
        glEnableVertexAttribArray(m_modelAttribLocationIns + 2);
        glEnableVertexAttribArray(m_modelAttribLocationIns + 3);

        CHECK_GL_ERRORS;
    }

    //-- Enable input slots for m_vao_meshDataMot:
    {
        glGenVertexArrays(1, &m_vao_meshDataMot);
        glBindVertexArray(m_vao_meshDataMot);

        // Enable the vertex shader attribute location for "position" when rendering.
        m_positionAttribLocationMot = m_motionDepthShader.getAttribLocation("position");
        glEnableVertexAttribArray(m_positionAttribLocationMot);

        CHECK_GL_ERRORS;
    }

    //-- Enable input slots for m_vao_screenQuad:
    {
        glGenVertexArrays(1, &m_vao_screenQuad);
        glBindVertexArray(m_vao_screenQuad);

        // Enable the vertex shader attribute location for "position" when rendering.
        m_positionAttribLocationPost = m_postProcessShader.getAttribLocation("position");
        glEnableVertexAttribArray(m_positionAttribLocationPost);

        // Enable the vertex shader attribute location for "texcoord_in" when rendering.
        m_texCoordsAttribLocationPost = m_postProcessShader.getAttribLocation("texcoord_in");
        glEnableVertexAttribArray(m_texCoordsAttribLocationPost);

        CHECK_GL_ERRORS;
    }

    // Restore defaults
    glBindVertexArray(0);
}

//----------------------------------------------------------------------------------------
void Bouncer::uploadVertexDataToVbos (
        const MeshConsolidator & meshConsolidator
) {
    // Generate VBO to store all vertex position data
    {
        glGenBuffers(1, &m_vbo_vertexPositions);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexPositions);

        glBufferData(GL_ARRAY_BUFFER, meshConsolidator.getNumVertexPositionBytes(),
                meshConsolidator.getVertexPositionDataPtr(), GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        CHECK_GL_ERRORS;
    }

    // Generate VBO to store all vertex normal data
    {
        glGenBuffers(1, &m_vbo_vertexNormals);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexNormals);

        glBufferData(GL_ARRAY_BUFFER, meshConsolidator.getNumVertexNormalBytes(),
                meshConsolidator.getVertexNormalDataPtr(), GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        CHECK_GL_ERRORS;
    }

    // Generate VBO to store all texture coord data
    {
        glGenBuffers(1, &m_vbo_vertexTexCoords);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexTexCoords);

        glBufferData(GL_ARRAY_BUFFER, meshConsolidator.getNumVertexTexCoordsBytes(),
                meshConsolidator.getVertexTexCoordsPtr(), GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        CHECK_GL_ERRORS;
    }


    // Generate VBO to store all particle model matrices
    {
        glGenBuffers(1, &m_vbo_modelIns);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo_modelIns);

        glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        CHECK_GL_ERRORS;
    }

    // Generate VBO to store all screen quad data
    {
        float quadVertices[] = {
            // positions   // texCoords
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,

            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };

        glGenBuffers(1, &m_vbo_screenVertex);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo_screenVertex);

        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        CHECK_GL_ERRORS;
    }
}

//----------------------------------------------------------------------------------------
void Bouncer::mapVboDataToVertexShaderInputLocations()
{
    // Bind VAO in order to record the data mapping.
    glBindVertexArray(m_vao_meshData);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexPositions);
    glVertexAttribPointer(m_positionAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexNormals);
    glVertexAttribPointer(m_normalAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Bind VAO in order to record the data mapping.
    glBindVertexArray(m_vao_meshDataTex);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexPositions);
    glVertexAttribPointer(m_positionAttribLocationTex, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexNormals);
    glVertexAttribPointer(m_normalAttribLocationTex, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexTexCoords);
    glVertexAttribPointer(m_texCoordsAttribLocation, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Bind VAO in order to record the data mapping.
    glBindVertexArray(m_vao_meshDataDepth);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexPositions);
    glVertexAttribPointer(m_positionAttribLocationDepth, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Bind VAO in order to record the data mapping.
    glBindVertexArray(m_vao_meshDataDepth2);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexPositions);
    glVertexAttribPointer(m_positionAttribLocationDepth2, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Bind VAO in order to record the data mapping.
    glBindVertexArray(m_vao_meshDataIns);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexPositions);
    glVertexAttribPointer(m_positionAttribLocationIns, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    auto v4size = sizeof(vec4);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_modelIns);
    glVertexAttribPointer(m_modelAttribLocationIns, 4, GL_FLOAT, GL_FALSE, 4 * v4size, nullptr);
    glVertexAttribPointer(m_modelAttribLocationIns + 1, 4, GL_FLOAT, GL_FALSE, 4 * v4size, (void*)v4size);
    glVertexAttribPointer(m_modelAttribLocationIns + 2, 4, GL_FLOAT, GL_FALSE, 4 * v4size, (void*)(2 * v4size));
    glVertexAttribPointer(m_modelAttribLocationIns + 3, 4, GL_FLOAT, GL_FALSE, 4 * v4size, (void*)(3 * v4size));

    glVertexAttribDivisor(m_modelAttribLocationIns, 1);
    glVertexAttribDivisor(m_modelAttribLocationIns + 1, 1);
    glVertexAttribDivisor(m_modelAttribLocationIns + 2, 1);
    glVertexAttribDivisor(m_modelAttribLocationIns + 3, 1);

    // Bind VAO in order to record the data mapping.
    glBindVertexArray(m_vao_meshDataMot);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexPositions);
    glVertexAttribPointer(m_positionAttribLocationMot, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Bind VAO in order to record the data mapping.
    glBindVertexArray(m_vao_screenQuad);

    auto fsize = sizeof(float);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_screenVertex);
    glVertexAttribPointer(m_positionAttribLocationPost, 2, GL_FLOAT, GL_FALSE, 4 * fsize, nullptr);
    glVertexAttribPointer(m_texCoordsAttribLocationPost, 2, GL_FLOAT, GL_FALSE, 4 * fsize, (void*)(2 * fsize));

    //-- Unbind target, and restore default values:
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
void Bouncer::mapTextures()
{
    int width, height, nrChannels;
    unsigned char *data;

    //-- arena textures
    glGenTextures(1, &m_arenaTextureData);
    glBindTexture(GL_TEXTURE_2D, m_arenaTextureData);
    // set the texture wrapping/filtering options (on the currently bound texture object)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);   
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // load and generate the texture
    data = stbi_load(getAssetFilePath("stone.jpg").c_str(), &width, &height, &nrChannels, 0);
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cout << "Failed to load texture" << std::endl;
    }
    stbi_image_free(data);
    data = nullptr;
    glBindTexture(GL_TEXTURE_2D, 0);

    CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
void Bouncer::initShadowMap(GLuint width, GLuint height)
{
    // depth map for light 1
    glGenFramebuffers(1, &m_fbo_depthMap);
    glGenTextures(1, &m_depthMapTex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_depthMapTex);
    for (unsigned int i = 0; i < 6; ++i)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL); 
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE); 
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_depthMap);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_depthMapTex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    CHECK_GL_ERRORS;

    // depth map for light 2
    glGenFramebuffers(1, &m_fbo_depthMap2);
    glGenTextures(1, &m_depthMapTex2);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_depthMapTex2);
    for (unsigned int i = 0; i < 6; ++i)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL); 
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE); 
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_depthMap2);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_depthMapTex2, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
void Bouncer::initDepthMap()
{
    // depth map for motion blur
    glGenFramebuffers(1, &m_fbo_depthMapMot);
    glGenTextures(1, &m_depthMapTexMot);
    glBindTexture(GL_TEXTURE_2D, m_depthMapTexMot);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, m_windowWidth, m_windowHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_depthMapMot);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthMapTexMot, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
void Bouncer::initPostProcessFb()
{
    // depth map for motion blur
    glGenFramebuffers(1, &m_fbo_screen);
    glGenTextures(1, &m_screenTex);
    glBindTexture(GL_TEXTURE_2D, m_screenTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_windowWidth, m_windowHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_screen);

    glGenRenderbuffers(1, &m_rbo_screen);
    glBindRenderbuffer(GL_RENDERBUFFER, m_rbo_screen);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, m_windowWidth, m_windowHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_rbo_screen);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_screenTex, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
void Bouncer::initPerspectiveMatrix()
{
    float aspect = ((float)m_windowWidth) / m_windowHeight;
    m_farPlane = 100.0f;
    m_perpsective = glm::perspective(degreesToRadians(60.0f), aspect, 0.1f, m_farPlane);

    m_lightPerspsective = glm::perspective(degreesToRadians(90.0f), 1.0f, 0.1f, m_farPlane);
}


//----------------------------------------------------------------------------------------
void Bouncer::initViewMatrix() {
    m_view = glm::lookAt(vec3(0.0f, 0.0f, 20.0f), vec3(0.0f, 0.0f, 0.0f),
            vec3(0.0f, 1.0f, 0.0f));
}

//----------------------------------------------------------------------------------------
void Bouncer::initLightSources() {
    // World-space position
    m_light.position = vec3(0.0f, 49.0f, 0.0f);
    m_light.rgbIntensity = vec3(0.8f); // White light

    m_light2.position = vec3(0.0f, -49.0f, 0.0f);
    m_light2.rgbIntensity = vec3(0.8f); // White light

    m_lightViews.push_back(glm::lookAt(m_light.position, m_light.position + glm::vec3( 1.0, 0.0, 0.0), glm::vec3(0.0,-1.0, 0.0)));
    m_lightViews.push_back(glm::lookAt(m_light.position, m_light.position + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0,-1.0, 0.0)));
    m_lightViews.push_back(glm::lookAt(m_light.position, m_light.position + glm::vec3( 0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0)));
    m_lightViews.push_back(glm::lookAt(m_light.position, m_light.position + glm::vec3( 0.0,-1.0, 0.0), glm::vec3(0.0, 0.0,-1.0)));
    m_lightViews.push_back(glm::lookAt(m_light.position, m_light.position + glm::vec3( 0.0, 0.0, 1.0), glm::vec3(0.0,-1.0, 0.0)));
    m_lightViews.push_back(glm::lookAt(m_light.position, m_light.position + glm::vec3( 0.0, 0.0,-1.0), glm::vec3(0.0,-1.0, 0.0)));

    m_lightViews2.push_back(glm::lookAt(m_light2.position, m_light2.position + glm::vec3( 1.0, 0.0, 0.0), glm::vec3(0.0,-1.0, 0.0)));
    m_lightViews2.push_back(glm::lookAt(m_light2.position, m_light2.position + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0,-1.0, 0.0)));
    m_lightViews2.push_back(glm::lookAt(m_light2.position, m_light2.position + glm::vec3( 0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0)));
    m_lightViews2.push_back(glm::lookAt(m_light2.position, m_light2.position + glm::vec3( 0.0,-1.0, 0.0), glm::vec3(0.0, 0.0,-1.0)));
    m_lightViews2.push_back(glm::lookAt(m_light2.position, m_light2.position + glm::vec3( 0.0, 0.0, 1.0), glm::vec3(0.0,-1.0, 0.0)));
    m_lightViews2.push_back(glm::lookAt(m_light2.position, m_light2.position + glm::vec3( 0.0, 0.0,-1.0), glm::vec3(0.0,-1.0, 0.0)));
}

//----------------------------------------------------------------------------------------
void Bouncer::initGameParams() {
    uniform_real_distribution<float> dist(-1.0, 1.0);
    m_ballDir = normalize(vec3(dist(m_randomGenerator), dist(m_randomGenerator), dist(m_randomGenerator)));
    m_ballNode->trans = mat4(1);
    m_boost = 1.0f;
}

//----------------------------------------------------------------------------------------
void Bouncer::initPlayer() {
    m_playerNode->trans = mat4(1);
    m_playerNode->translate(vec3(0.0f, -1.0f, 15.0f));
}

//----------------------------------------------------------------------------------------
void Bouncer::initAnimations() {
    m_animDuration = 2000;
    m_animElapsed = 0;
    m_animPlay = false;
    std::vector<vec3> directions = {
        vec3(1.0f, 0.0f, 0.0f),
        vec3(-1.0f, 0.0f, 0.0f),
        vec3(0.0f, 1.0f, 0.0f),
        vec3(0.0f, -1.0f, 0.0f),
        vec3(0.0f, 0.0f, 1.0f),
        vec3(0.0f, 0.0f, -1.0f)
    };
    uint i = 0;
    for (auto& el : m_playerNode->children.front()->children) {
        vector<Keyframe> frames;
        frames.push_back((Keyframe){0, vec3(0)});
        frames.push_back((Keyframe){500, directions[i] * 5.0f});
        frames.push_back((Keyframe){800, directions[i] * 2.0f});
        frames.push_back((Keyframe){1200, directions[i] * 6.0f});
        frames.push_back((Keyframe){1600, directions[i] * 4.0f});
        frames.push_back((Keyframe){1800, directions[i] * 6.0f});
        frames.push_back((Keyframe){2000, vec3(0)});
        m_keyframes.push_back(frames);
        m_animPosOrig.push_back(el->trans);
        m_animIndex.push_back(0);
        ++i;
    }
}

//----------------------------------------------------------------------------------------
void Bouncer::uploadCommonSceneUniforms() {
    vec3 ambientIntensity(0.33f);
    GLint location;
    m_shader.enable();
    {
        //-- Set Perpsective matrix uniform for the scene:
        location = m_shader.getUniformLocation("Perspective");
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_perpsective));

        //-- Set LightSource uniform for the scene:
        {
            location = m_shader.getUniformLocation("light.position");
            glUniform3fv(location, 1, value_ptr(m_light.position));
            location = m_shader.getUniformLocation("light.rgbIntensity");
            glUniform3fv(location, 1, value_ptr(m_light.rgbIntensity));
            CHECK_GL_ERRORS;
        }

        //-- Set LightSource 2 uniform for the scene:
        {
            location = m_shader.getUniformLocation("light2.position");
            glUniform3fv(location, 1, value_ptr(m_light2.position));
            location = m_shader.getUniformLocation("light2.rgbIntensity");
            glUniform3fv(location, 1, value_ptr(m_light2.rgbIntensity));
            CHECK_GL_ERRORS;
        }

        //-- Set background light ambient intensity
        {
            location = m_shader.getUniformLocation("ambientIntensity");
            glUniform3fv(location, 1, value_ptr(ambientIntensity));
            CHECK_GL_ERRORS;
        }

        //-- Set Far Plane:
        location = m_shader.getUniformLocation("farPlane");
        glUniform1f(location, m_farPlane);
        CHECK_GL_ERRORS;

        //-- Set texture uniforms
        location = m_shader.getUniformLocation("shadowMap");
        glUniform1i(location, 0);
        location = m_shader.getUniformLocation("shadowMap2");
        glUniform1i(location, 1);
    }
    m_shader.disable();

    m_texShader.enable();
    {
        //-- Set Perpsective matrix uniform for the scene:
        location = m_texShader.getUniformLocation("Perspective");
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_perpsective));

        //-- Set LightSource uniform for the scene:
        {
            location = m_texShader.getUniformLocation("light.position");
            glUniform3fv(location, 1, value_ptr(m_light.position));
            location = m_texShader.getUniformLocation("light.rgbIntensity");
            glUniform3fv(location, 1, value_ptr(m_light.rgbIntensity));
            CHECK_GL_ERRORS;
        }

        //-- Set LightSource 2 uniform for the scene:
        {
            location = m_texShader.getUniformLocation("light2.position");
            glUniform3fv(location, 1, value_ptr(m_light2.position));
            location = m_texShader.getUniformLocation("light2.rgbIntensity");
            glUniform3fv(location, 1, value_ptr(m_light2.rgbIntensity));
            CHECK_GL_ERRORS;
        }

        //-- Set background light ambient intensity
        {
            location = m_texShader.getUniformLocation("ambientIntensity");
            glUniform3fv(location, 1, value_ptr(ambientIntensity));
            CHECK_GL_ERRORS;
        }

        //-- Set Far Plane matrix:
        location = m_texShader.getUniformLocation("farPlane");
        glUniform1f(location, m_farPlane);
        CHECK_GL_ERRORS;

        //-- Set texture uniforms
        location = m_texShader.getUniformLocation("shadowMap");
        glUniform1i(location, 0);
        location = m_texShader.getUniformLocation("shadowMap2");
        glUniform1i(location, 1);
        location = m_texShader.getUniformLocation("matTexture");
        glUniform1i(location, 2);
    }
    m_texShader.disable();

    m_depthShader.enable();
    {
        //-- Set Perpsective matrix uniform for the scene:
        location = m_depthShader.getUniformLocation("Perspective");
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_lightPerspsective));
        CHECK_GL_ERRORS;

        //-- Set View matrices for cubemap:
        for (unsigned int i = 0; i < m_lightViews.size(); ++i) {
            location = m_depthShader.getUniformLocation(("Views[" + std::to_string(i) + "]").c_str());
            glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_lightViews[i]));
        }
        CHECK_GL_ERRORS;

        //-- Set Light Position matrix:
        vec3 pos = vec3(m_view * vec4(m_light.position, 1.0f));
        location = m_depthShader.getUniformLocation("LightPos");
        glUniform3fv(location, 1, value_ptr(m_light.position));
        CHECK_GL_ERRORS;

        //-- Set Far Plane:
        location = m_depthShader.getUniformLocation("FarPlane");
        glUniform1f(location, m_farPlane);
        CHECK_GL_ERRORS;
    }
    m_depthShader.disable();

    m_depthShader2.enable();
    {
        //-- Set Perpsective matrix uniform for the scene:
        location = m_depthShader2.getUniformLocation("Perspective");
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_lightPerspsective));
        CHECK_GL_ERRORS;

        //-- Set View matrices for cubemap:
        for (unsigned int i = 0; i < m_lightViews2.size(); ++i) {
            location = m_depthShader2.getUniformLocation(("Views[" + std::to_string(i) + "]").c_str());
            glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_lightViews2[i]));
        }
        CHECK_GL_ERRORS;

        //-- Set Light Position matrix:
        vec3 pos = vec3(m_view * vec4(m_light2.position, 1.0f));
        location = m_depthShader2.getUniformLocation("LightPos");
        glUniform3fv(location, 1, value_ptr(m_light2.position));
        CHECK_GL_ERRORS;

        //-- Set Far Plane:
        location = m_depthShader2.getUniformLocation("FarPlane");
        glUniform1f(location, m_farPlane);
        CHECK_GL_ERRORS;
    }
    m_depthShader2.disable();

    m_instancedShader.enable();
    {
        location = m_instancedShader.getUniformLocation("Perspective");
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_perpsective));
    }
    m_instancedShader.disable();

    m_motionDepthShader.enable();
    {
        //-- Set Perpsective matrix uniform for the scene:
        location = m_motionDepthShader.getUniformLocation("Perspective");
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_perpsective));
        CHECK_GL_ERRORS;
    }
    m_motionDepthShader.disable();

    m_postProcessShader.enable();
    {
        //-- Set Perpsective matrix uniform for the scene:
        location = m_postProcessShader.getUniformLocation("Perspective");
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_perpsective));
        location = m_postProcessShader.getUniformLocation("PerspectivePrev");
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_perpsectivePrev));
        location = m_postProcessShader.getUniformLocation("MotionBlur");
        glUniform1i(location, m_boost > 1.0f);
        location = m_postProcessShader.getUniformLocation("GaussianBlur");
        glUniform1i(location, m_gamePaused);
        location = m_postProcessShader.getUniformLocation("Darken");
        glUniform1i(location, m_gamePaused);

        //-- Set texture uniforms
        location = m_postProcessShader.getUniformLocation("screenTexture");
        glUniform1i(location, 0);
        location = m_postProcessShader.getUniformLocation("depthTexture");
        glUniform1i(location, 1);
    }
    m_postProcessShader.disable();
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, before guiLogic().
 */
void Bouncer::appLogic()
{
    auto now = chrono::high_resolution_clock::now();
    unsigned int duration = chrono::duration_cast<std::chrono::milliseconds>(now - m_prevTime).count();
    if(!m_gamePaused) {
        if (m_particleLife > 0) {
            m_particleLife -= duration;
            if (m_particleLife <= 0) {
                m_particles.clear();
                m_particlesMotion.clear();
            } else {
                const float moveScale = 100.0;
                for (unsigned int i = 0; i < m_particles.size(); ++i) {
                    m_particles[i] = translate(m_particles[i], m_particlesMotion[i] * 4.0f * (duration / moveScale));
                    m_particles[i] = m_particles[i] * scale(mat4(1), vec3(0.8, 0.8, 0.8));
                }
            }
        }
        unsigned int i = 0;
        if(m_animPlay) {
            if(m_animElapsed < m_animDuration) {
                for (auto& el : m_playerNode->children.front()->children) {
                    if(m_animElapsed >= m_keyframes[i][m_animIndex[i]].time) {
                        ++m_animIndex[i];
                    }
                    if(m_animIndex[i] < m_keyframes[i].size()) {
                        // calculate position between keyframes
                        float inbetw = (float)(m_animElapsed - m_keyframes[i][m_animIndex[i] - 1].time) / (float)(m_keyframes[i][m_animIndex[i]].time - m_keyframes[i][m_animIndex[i] - 1].time);
                        vec3 pos = m_keyframes[i][m_animIndex[i] - 1].position + (m_keyframes[i][m_animIndex[i]].position - m_keyframes[i][m_animIndex[i] - 1].position) * inbetw;
                        el->trans = translate(m_animPosOrig[i], pos);
                    }
                    ++i;
                }
                m_animElapsed += duration;
            } else {
                for (auto& el : m_playerNode->children.front()->children) {
                    el->trans = m_animPosOrig[i];
                    m_animIndex[i] = 0;
                    ++i;
                }
                m_animElapsed = 0;
                m_animPlay = false;
            }
        }
        if(m_boost > 1.0f + EPSILON && (chrono::duration_cast<std::chrono::milliseconds>(m_prevTime.time_since_epoch()).count() % 500 > chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 500)) {
            m_soundEngine->play2D(getAssetFilePath("swoosh.wav").c_str(), false);
        }
        movePlayer(duration);
        moveBall(duration);
    }
    if(m_titleScreen) {
        m_view = m_view * rotate(mat4(1), 0.005f, vec3(0, 1, 0));
    }

    uploadCommonSceneUniforms();
    m_prevTime = now;
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after appLogic(), but before the draw() method.
 */
void Bouncer::guiLogic()
{
    ImGui_ImplGlfwGL3_NewFrame();

    static bool firstRun(true);
    if (firstRun) {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        firstRun = false;
    }

    static bool showUIWindow(true);
    ImGuiWindowFlags windowFlags(ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse);

    ImGui::SetWindowSize("Window", ImVec2(0, 0));
    ImGui::SetWindowSize("Window", ImVec2(m_windowWidth, m_windowHeight));
    ImGui::Begin("Window", &showUIWindow, ImVec2(m_windowWidth, m_windowHeight), 0.0f, windowFlags);
        ImVec2 pos;

        if(m_showFps) {
            pos = ImGui::GetCursorPos();
            pos.x = m_windowWidth - 60;
            pos.y = m_windowHeight - 30;
            ImGui::SetCursorPos(pos);
            ImGui::Text( "%.0f FPS", ImGui::GetIO().Framerate );
        }

        if(m_titleScreen) {
            pos = ImGui::GetCursorPos();
            pos.x = (m_windowWidth / 2) - 50;
            pos.y = (m_windowHeight / 3) - 10;
            ImGui::SetCursorPos(pos);
            ImGui::Text("B O U N C E R");

            pos = ImGui::GetCursorPos();
            pos.x = (m_windowWidth / 2) - 100;
            pos.y = (2 * m_windowHeight / 3) - 50;
            ImGui::SetCursorPos(pos);
            if( ImGui::Button("Play!", ImVec2(200, 100)) ) {
                m_soundEngine->play2D(getAssetFilePath("click.wav").c_str(), false);
                startGame();
            }

            pos = ImGui::GetCursorPos();
            pos.x = (m_windowWidth / 2) - 50;
            pos.y = (2 * m_windowHeight / 3) + 100;
            ImGui::SetCursorPos(pos);
            if( ImGui::Button("Exit", ImVec2(100, 50)) ) {
                glfwSetWindowShouldClose(m_window, GL_TRUE);
            }
        } else if (m_gamePaused) {
            pos = ImGui::GetCursorPos();
            pos.x = (m_windowWidth / 2) - 70;
            pos.y = (2 * m_windowHeight / 3) - 35;
            ImGui::SetCursorPos(pos);
            if( ImGui::Button("Resume", ImVec2(140, 70)) ) {
                m_soundEngine->play2D(getAssetFilePath("click.wav").c_str(), false);
                m_gamePaused = false;
                glfwSetInputMode(m_window, GLFW_CURSOR, m_gamePaused ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            }

            pos = ImGui::GetCursorPos();
            pos.x = (m_windowWidth / 2) - 50;
            pos.y = (2 * m_windowHeight / 3) + 100;
            ImGui::SetCursorPos(pos);
            if( ImGui::Button("Main Menu", ImVec2(100, 50)) ) {
                m_soundEngine->play2D(getAssetFilePath("click.wav").c_str(), false);
                startGame();
                m_titleScreen = true;
                m_gamePaused = true;
                glfwSetInputMode(m_window, GLFW_CURSOR, m_gamePaused ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            }
        }

    ImGui::End();
}

//----------------------------------------------------------------------------------------
// Update mesh specific shader uniforms:
void Bouncer::updateShaderUniforms(
        const GeometryNode & node,
        const mat4 & trans
) {
    mat3 normalMatrix = glm::transpose(glm::inverse(mat3(m_view * trans)));
    vec3 kd = node.material.kd;
    vec3 ks = node.material.ks;

    GLint location;
    m_shader.enable();
    {
        //-- Set Model matrix:
        location = m_shader.getUniformLocation("Model");
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(trans));
        CHECK_GL_ERRORS;

        //-- Set View matrix:
        location = m_shader.getUniformLocation("View");
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_view));
        CHECK_GL_ERRORS;

        //-- Set NormMatrix:
        location = m_shader.getUniformLocation("NormalMatrix");
        glUniformMatrix3fv(location, 1, GL_FALSE, value_ptr(normalMatrix));
        CHECK_GL_ERRORS;

        //-- Set Material values:
        location = m_shader.getUniformLocation("material.kd");
        glUniform3fv(location, 1, value_ptr(kd));
        CHECK_GL_ERRORS;
        location = m_shader.getUniformLocation("material.ks");
        glUniform3fv(location, 1, value_ptr(ks));
        CHECK_GL_ERRORS;
        location = m_shader.getUniformLocation("material.shininess");
        glUniform1f(location, node.material.shininess);
        CHECK_GL_ERRORS;
    }
    m_shader.disable();

    m_texShader.enable();
    {
        //-- Set Model matrix:
        location = m_texShader.getUniformLocation("Model");
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(trans));
        CHECK_GL_ERRORS;

        //-- Set View matrix:
        location = m_texShader.getUniformLocation("View");
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_view));
        CHECK_GL_ERRORS;

        //-- Set NormMatrix:
        location = m_texShader.getUniformLocation("NormalMatrix");
        glUniformMatrix3fv(location, 1, GL_FALSE, value_ptr(normalMatrix));
        CHECK_GL_ERRORS;

        //-- Set Material values:
        location = m_texShader.getUniformLocation("material.kd");
        glUniform3fv(location, 1, value_ptr(kd));
        CHECK_GL_ERRORS;
        location = m_texShader.getUniformLocation("material.ks");
        glUniform3fv(location, 1, value_ptr(ks));
        CHECK_GL_ERRORS;
        location = m_texShader.getUniformLocation("material.shininess");
        glUniform1f(location, node.material.shininess);
        CHECK_GL_ERRORS;
    }
    m_texShader.disable();

    m_depthShader.enable();
    {
        //-- Set Model matrix:
        location = m_depthShader.getUniformLocation("Model");
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(trans));
        CHECK_GL_ERRORS;
    }
    m_depthShader.disable();

    m_depthShader2.enable();
    {
        //-- Set Model matrix:
        location = m_depthShader2.getUniformLocation("Model");
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(trans));
        CHECK_GL_ERRORS;
    }
    m_depthShader2.disable();

    m_motionDepthShader.enable();
    {
        //-- Set Model matrix:
        location = m_motionDepthShader.getUniformLocation("Model");
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(trans));
        CHECK_GL_ERRORS;

        //-- Set View matrix:
        location = m_motionDepthShader.getUniformLocation("View");
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_view));
        CHECK_GL_ERRORS;
    }
    m_motionDepthShader.disable();

    m_postProcessShader.enable();
    {
        //-- Set View matrix:
        location = m_postProcessShader.getUniformLocation("View");
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_view));
        CHECK_GL_ERRORS;
        location = m_postProcessShader.getUniformLocation("ViewPrev");
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_viewPrev));
        CHECK_GL_ERRORS;
    }
    m_postProcessShader.disable();
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after guiLogic().
 */
void Bouncer::draw() {
    // render to shadow map
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_depthMap);
    glClear(GL_DEPTH_BUFFER_BIT);
    renderSceneDepth(m_depthShader, m_vao_meshDataDepth);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // render to shadow map 2
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_depthMap2);
    glClear(GL_DEPTH_BUFFER_BIT);
    renderSceneDepth(m_depthShader2, m_vao_meshDataDepth2);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // render to motion blur depth map
    glViewport(0, 0, m_windowWidth, m_windowHeight);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_depthMapMot);
    glClear(GL_DEPTH_BUFFER_BIT);
    renderSceneDepth(m_motionDepthShader, m_vao_meshDataMot);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // render scene to screen framebuffer
    glViewport(0, 0, m_windowWidth, m_windowHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_screen);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_depthMapTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_depthMapTex2);
    renderScene();
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    renderParticles();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // render screen framebuffer to display
    glViewport(0, 0, m_windowWidth, m_windowHeight);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindVertexArray(m_vao_screenQuad);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_screenTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_depthMapTexMot);
    m_postProcessShader.enable();
    glDrawArrays(GL_TRIANGLES, 0, 6);
    m_postProcessShader.disable();
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);

    // store previous view perspective matrices for motion blur
    m_viewPrev = m_view;
    m_perpsectivePrev = m_perpsective;
}

//----------------------------------------------------------------------------------------
void Bouncer::renderSceneDepth(const ShaderProgram & shader, GLuint vao) {

    mat4 basetrans(1.f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glBindVertexArray(vao);
    renderSceneGraph(shader, *m_arenaNode, basetrans);
    renderSceneGraph(shader, *m_ballNode, basetrans);
    renderSceneGraph(shader, *m_playerNode, basetrans);
    glBindVertexArray(0);

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
}

//----------------------------------------------------------------------------------------
void Bouncer::renderScene() {

    mat4 basetrans(1.f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glBindVertexArray(m_vao_meshDataTex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_arenaTextureData);
    renderSceneGraph(m_texShader, *m_arenaNode, basetrans);
    glBindTexture(GL_TEXTURE_2D, 0);
    renderSceneGraph(m_shader, *m_ballNode, basetrans);
    renderSceneGraph(m_shader, *m_playerNode, basetrans);
    glBindVertexArray(0);

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
}

//----------------------------------------------------------------------------------------
void Bouncer::renderSceneGraph(const ShaderProgram & shader, const SceneNode & root, const mat4 & trans) {

    const mat4 newtrans = trans * root.trans;
    if (root.m_nodeType == NodeType::GeometryNode) {
        const GeometryNode * geometryNode = static_cast<const GeometryNode *>(&root);

        updateShaderUniforms(*geometryNode, newtrans);

        // Get the BatchInfo corresponding to the GeometryNode's unique MeshId.
        BatchInfo batchInfo = m_batchInfoMap[geometryNode->meshId];

        //-- Now render the mesh:
        shader.enable();
        glDrawArrays(GL_TRIANGLES, batchInfo.startIndex, batchInfo.numIndices);
        shader.disable();
    }
    
    for (const SceneNode * node : root.children) {
        renderSceneGraph(shader, *node, newtrans);
    }

    CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
void Bouncer::renderParticles() {
    if(m_particles.size() == 0) {
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glBindVertexArray(m_vao_meshDataIns);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_modelIns);
    glBufferData(GL_ARRAY_BUFFER, m_particles.size() * sizeof(glm::mat4), m_particles.data(), GL_STATIC_DRAW);

    // set uniforms for instanced shader
    m_instancedShader.enable();
    {
        BatchInfo batchInfo = m_batchInfoMap["cube"];

        GLuint location;
        //-- Set View matrix:
        location = m_instancedShader.getUniformLocation("View");
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_view));
        CHECK_GL_ERRORS;

        //-- Set colour:
        vec3 ptcolour(1,1,0);
        location = m_instancedShader.getUniformLocation("colour");
        glUniform3fv(location, 1, value_ptr(ptcolour));
        CHECK_GL_ERRORS;

        glDrawArraysInstanced(GL_TRIANGLES, batchInfo.startIndex, batchInfo.numIndices, m_particles.size());
    }
    m_instancedShader.disable();

    glBindVertexArray(0);

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
}

//----------------------------------------------------------------------------------------
/*
 * Called once, after program is signaled to terminate.
 */
void Bouncer::cleanup()
{
    m_soundEngine->drop();
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles cursor entering the window area events.
 */
bool Bouncer::cursorEnterWindowEvent (
        int entered
) {
    bool eventHandled(false);

    return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse cursor movement events.
 */
bool Bouncer::mouseMoveEvent (
        double xPos,
        double yPos
) {
    bool eventHandled(false);

    if(!m_gamePaused) {
        float angley = (float)(yPos - m_lastMouseY) / 1000;
        float anglex = (float)(xPos - m_lastMouseX) / 1000;
        m_playerNode->trans = m_playerNode->trans * rotate(mat4(1), -angley, vec3(1, 0, 0));
        m_view = m_view * m_playerNode->trans * rotate(mat4(1), angley, vec3(1, 0, 0)) * inverse(m_playerNode->trans);
        m_playerNode->trans = m_playerNode->trans * rotate(mat4(1), -anglex, vec3(0, 1, 0));
        m_view = m_view * m_playerNode->trans * rotate(mat4(1), anglex, vec3(0, 1, 0)) * inverse(m_playerNode->trans);
    }
    
    // Track previous mouse position for calculating delta
    m_lastMouseX = xPos;
    m_lastMouseY = yPos;

    return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse button events.
 */
bool Bouncer::mouseButtonInputEvent (
        int button,
        int actions,
        int mods
) {
    bool eventHandled(false);

    return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse scroll wheel events.
 */
bool Bouncer::mouseScrollEvent (
        double xOffSet,
        double yOffSet
) {
    bool eventHandled(false);

    // Fill in with event handling code...

    return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles window resize events.
 */
bool Bouncer::windowResizeEvent (
        int width,
        int height
) {
    bool eventHandled(false);
    initPerspectiveMatrix();
    return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles key input events.
 */
bool Bouncer::keyInputEvent (
        int key,
        int action,
        int mods
) {
    bool eventHandled(false);

    if( action == GLFW_PRESS ) {
        // gimme mouse back
        if ((key == GLFW_KEY_ESCAPE && !m_gamePaused) || key == GLFW_KEY_P) {
            m_soundEngine->play2D(getAssetFilePath("click.wav").c_str(), false);
            m_gamePaused = !m_gamePaused;
            m_keyUp = false;
            m_keyDown = false;
            m_keyLeft = false;
            m_keyRight = false;
            m_keyForward = false;
            glfwSetInputMode(m_window, GLFW_CURSOR, m_gamePaused ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            glfwGetCursorPos(m_window, &m_lastMouseX, &m_lastMouseY);
            if(m_titleScreen) {
                startGame();
            }
            eventHandled = true;
        }

        if (key == GLFW_KEY_R) {
            m_soundEngine->play2D(getAssetFilePath("click.wav").c_str(), false);
            startGame();
            m_titleScreen = true;
            m_gamePaused = true;
            glfwSetInputMode(m_window, GLFW_CURSOR, m_gamePaused ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
        }

        if (key == GLFW_KEY_M) {
            m_showFps = !m_showFps;
        }
    }

    if( action == GLFW_PRESS && !m_gamePaused) {
        // movement keys
        if (key == GLFW_KEY_W) {
            m_keyUp = true;
        }
        if (key == GLFW_KEY_S) {
            m_keyDown = true;
        }
        if (key == GLFW_KEY_A) {
            m_keyLeft = true;
        }
        if (key == GLFW_KEY_D) {
            m_keyRight = true;
        }
        if (key == GLFW_KEY_SPACE) {
            m_keyForward = true;
        }
        if (key == GLFW_KEY_LEFT_SHIFT) {
            m_boost = 2.0f;
            m_soundEngine->play2D(getAssetFilePath("swoosh.wav").c_str(), false);
        }
        if (key == GLFW_KEY_Z) {
            m_animPlay = true;
        }
    }

    if( action == GLFW_RELEASE ) {
        // movement keys
        if (key == GLFW_KEY_W) {
            m_keyUp = false;
        }
        if (key == GLFW_KEY_S) {
            m_keyDown = false;
        }
        if (key == GLFW_KEY_A) {
            m_keyLeft = false;
        }
        if (key == GLFW_KEY_D) {
            m_keyRight = false;
        }
        if (key == GLFW_KEY_SPACE) {
            m_keyForward = false;
        }
        if (key == GLFW_KEY_LEFT_SHIFT) {
            m_boost = 1.0f;
        }
    }

    return eventHandled;
}

void Bouncer::movePlayer(unsigned int time)
{
    const float moveScale = 100.0;
    float moveX = 0., moveY = 0., moveZ = 0.;
    if(m_keyUp) {
        moveY -= 1;
    }
    if(m_keyDown) {
        moveY += 1;
    }
    if(m_keyLeft) {
        moveX += 1;
    }
    if(m_keyRight) {
        moveX -= 1;
    }
    if(m_keyForward) {
        moveZ += 1;
    }

    vec3 move(moveX, moveY, moveZ);
    if(length(move) > EPSILON) {
        move = normalize(move) * m_boost; 
    }
    move *= (float)time / moveScale;

    auto oldView = m_view;
    auto oldPlayer = m_playerNode->trans;
    m_view = translate(mat4(1.), move) * m_view;
    m_playerNode->trans = m_playerNode->trans * translate(mat4(1.), -move);
    if (playerCollision()) {
        m_view = oldView;
        m_playerNode->trans = oldPlayer;
    }
}

void Bouncer::moveBall(unsigned int time)
{
    const float moveScale = 100.0;
    if(ballCollision()){
        // particle response
        uniform_real_distribution<float> dist(-1.0, 1.0);
        m_particles.resize(MAX_PARTICLES);
        m_particlesMotion.resize(MAX_PARTICLES);
        for (unsigned int i = 0; i < m_particles.size(); ++i) {
            m_particles[i] = m_ballNode->trans;
            m_particles[i] = scale(m_particles[i], vec3(0.1f, 0.1f, 0.1f));
            m_particlesMotion[i] = vec3(dist(m_randomGenerator), dist(m_randomGenerator), dist(m_randomGenerator));
            while(length(m_particlesMotion[i]) < EPSILON) {
                m_particlesMotion[i] = vec3(dist(m_randomGenerator), dist(m_randomGenerator), dist(m_randomGenerator));
            }
            m_particlesMotion[i] = normalize(m_particlesMotion[i]);
            m_particles[i] = translate(m_particles[i], m_particlesMotion[i] * 0.5f);
        }
        m_particleLife = 700;

        // sound response
        m_soundEngine->play2D(getAssetFilePath("bounce.wav").c_str(), false);
    }

    m_ballNode->translate(m_ballDir * (2 * time / moveScale));
}

bool Bouncer::ballCollision()
{
    bool collision = false;
    vec4 playerPos = m_playerNode->trans * vec4(0,0,0,1);
    vec4 ballPos = m_ballNode->trans * vec4(0,0,0,1);
    if(length(vec3(ballPos - playerPos)) <= 1.5f || length(vec3(ballPos)) >= 49.5f) {
        if(!m_inCollision) {
            if(length(vec3(ballPos - playerPos)) <= 1.5f) {
                collision = true;
                m_inCollision = true;
                vec3 n = normalize(vec3(ballPos - playerPos));
                m_ballDir = normalize(m_ballDir - (2 * dot(m_ballDir, n) * n));
            }

            if(length(vec3(ballPos)) >= 49.5f) {
                collision = true;
                m_inCollision = true;
                vec3 n = normalize(vec3(-ballPos));
                m_ballDir = normalize(m_ballDir - (2 * dot(m_ballDir, n) * n));
            }
        }
    } else if (m_inCollision) {
        m_inCollision = false;
    }
    return collision;
}

bool Bouncer::playerCollision()
{
    bool collision = false;
    vec4 playerPos = m_playerNode->trans * vec4(0,0,0,1);
    if(length(vec3(playerPos)) >= 49.0f) {
        collision = true;
    }
    return collision;
}

void Bouncer::startGame() {
    m_gamePaused = false;
    m_titleScreen = false;
    m_keyUp = false;
    m_keyDown = false;
    m_keyLeft = false;
    m_keyRight = false;
    m_keyForward = false;
    glfwSetInputMode(m_window, GLFW_CURSOR, m_gamePaused ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    glfwGetCursorPos(m_window, &m_lastMouseX, &m_lastMouseY);
    initGameParams();
    initPlayer();
    initViewMatrix();
}