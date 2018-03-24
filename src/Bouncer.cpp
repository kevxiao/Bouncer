#include "Bouncer.hpp"
#include "scene_lua.hpp"
using namespace std;

#include "cs488-framework/GlErrorCheck.hpp"
#include "cs488-framework/MathUtils.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <imgui/imgui.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

using namespace glm;

const float EPSILON = 0.0000001;
const unsigned int SHADOW_HEIGHT = 2048;
const unsigned int SHADOW_WIDTH = 2048;

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
      m_paused(true)
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
    // Set the background colour.
    glClearColor(0.35, 0.35, 0.35, 1.0);

    createShaderProgram();

    enableVertexShaderInputSlots();

    processLuaSceneFile(m_arenaFile, m_arenaNode);
    processLuaSceneFile(string("Assets/ball.lua"), m_ballNode);

    // Load and decode all .obj files at once here.  You may add additional .obj files to
    // this list in order to support rendering additional mesh types.  All vertex
    // positions, and normals will be extracted and stored within the MeshConsolidator
    // class.
    unique_ptr<MeshConsolidator> meshConsolidator (new MeshConsolidator{
            getAssetFilePath("cube.obj"),
            getAssetFilePath("sphere.obj"),
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

    initLightSources();

    initPerspectiveMatrix();

    initViewMatrix();

    m_prevTime = chrono::high_resolution_clock::now();
    //glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwGetCursorPos(m_window, &m_lastMouseX, &m_lastMouseY);
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

        // Enable the vertex shader attribute location for "normal" when rendering.
        m_texCoordsAttribLocation = m_texShader.getAttribLocation("texcoord_in");
        glEnableVertexAttribArray(m_texCoordsAttribLocation);

        CHECK_GL_ERRORS;
    }

    //-- Enable input slots for m_vao_meshDataTex:
    {
        glGenVertexArrays(1, &m_vao_meshDataDepth);
        glBindVertexArray(m_vao_meshDataDepth);

        // Enable the vertex shader attribute location for "position" when rendering.
        m_positionAttribLocationDepth = m_depthShader.getAttribLocation("position");
        glEnableVertexAttribArray(m_positionAttribLocationTex);

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

    // Generate VBO to store all vertex position data
    {
        glGenBuffers(1, &m_vbo_vertexPositionsTex);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexPositionsTex);

        glBufferData(GL_ARRAY_BUFFER, meshConsolidator.getNumVertexPositionBytes(),
                meshConsolidator.getVertexPositionDataPtr(), GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        CHECK_GL_ERRORS;
    }

    // Generate VBO to store all vertex normal data
    {
        glGenBuffers(1, &m_vbo_vertexNormalsTex);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexNormalsTex);

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

    // Generate VBO to store all vertex position data
    {
        glGenBuffers(1, &m_vbo_vertexPositionsDepth);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexPositionsDepth);

        glBufferData(GL_ARRAY_BUFFER, meshConsolidator.getNumVertexPositionBytes(),
                meshConsolidator.getVertexPositionDataPtr(), GL_STATIC_DRAW);

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

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexPositionsTex);
    glVertexAttribPointer(m_positionAttribLocationTex, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexNormalsTex);
    glVertexAttribPointer(m_normalAttribLocationTex, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexTexCoords);
    glVertexAttribPointer(m_texCoordsAttribLocation, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Bind VAO in order to record the data mapping.
    glBindVertexArray(m_vao_meshDataDepth);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexPositionsDepth);
    glVertexAttribPointer(m_positionAttribLocationDepth, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

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

    //-- character textures
    glGenTextures(1, &m_characterTextureData);
    glBindTexture(GL_TEXTURE_2D, m_characterTextureData);
    // set the texture wrapping/filtering options (on the currently bound texture object)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);   
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // load and generate the texture
    data = stbi_load(getAssetFilePath("metal.jpg").c_str(), &width, &height, &nrChannels, 0);
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
void Bouncer::initLightSources() {
    // World-space position
    m_light.position = vec3(0.0f, 50.0f, 0.0f);
    m_light.rgbIntensity = vec3(0.8f); // White light

    m_light2.position = vec3(0.0f, -50.0f, 0.0f);
    m_light2.rgbIntensity = vec3(0.8f); // White light
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
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, before guiLogic().
 */
void Bouncer::appLogic()
{
    auto now = chrono::high_resolution_clock::now();
    unsigned int duration = chrono::duration_cast<std::chrono::milliseconds>(now - m_prevTime).count();
    m_prevTime = now;
    movePlayer(duration);

    uploadCommonSceneUniforms();
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after appLogic(), but before the draw() method.
 */
void Bouncer::guiLogic()
{
    // static bool firstRun(true);
    // if (firstRun) {
    //     ImGui::SetNextWindowPos(ImVec2(50, 50));
    //     firstRun = false;
    // }

    // static bool showDebugWindow(true);
    // ImGuiWindowFlags windowFlags(ImGuiWindowFlags_AlwaysAutoResize);
    // float opacity(0.5f);

    // ImGui::Begin("Properties", &showDebugWindow, ImVec2(100,100), opacity,
    //         windowFlags);

    //     if( ImGui::RadioButton("Position / Orientation (P)", !m_jointInteract) ) {
    //         m_jointInteract = false;
    //     }
    //     if( ImGui::RadioButton("Joints (J)", m_jointInteract) ) {
    //         m_jointInteract = true;
    //     }

    //     ImGui::Text( "Framerate: %.1f FPS", ImGui::GetIO().Framerate );

    // ImGui::End();
}

//----------------------------------------------------------------------------------------
// Update mesh specific shader uniforms:
void Bouncer::updateShaderUniforms(
        const GeometryNode & node,
        const mat4 & viewMatrix,
        const mat4 & trans
) {
    mat3 normalMatrix = glm::transpose(glm::inverse(mat3(viewMatrix * trans)));
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
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(viewMatrix));
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
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(viewMatrix));
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

}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after guiLogic().
 */
void Bouncer::draw() {
    // render to depth map
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_depthMap);
    glClear(GL_DEPTH_BUFFER_BIT);
    renderSceneDepth(m_depthShader);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // render to depth map
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_depthMap2);
    glClear(GL_DEPTH_BUFFER_BIT);
    renderSceneDepth(m_depthShader2);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // render to display
    glViewport(0, 0, m_windowWidth, m_windowHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_depthMapTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_depthMapTex2);
    renderScene();
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

//----------------------------------------------------------------------------------------
void Bouncer::renderSceneDepth(const ShaderProgram & shader) {

    mat4 basetrans(1.f);

    glEnable( GL_DEPTH_TEST );
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glBindVertexArray(m_vao_meshDataDepth);
    renderSceneGraph(shader, *m_arenaNode, m_view, basetrans);
    renderSceneGraph(shader, *m_ballNode, m_view, basetrans);
    glBindVertexArray(0);

    glDisable(GL_CULL_FACE);
    glDisable( GL_DEPTH_TEST );
}

//----------------------------------------------------------------------------------------
void Bouncer::renderScene() {

    mat4 basetrans(1.f);

    glEnable( GL_DEPTH_TEST );
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glBindVertexArray(m_vao_meshDataTex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_arenaTextureData);
    renderSceneGraph(m_texShader, *m_arenaNode, m_view, basetrans);
    glBindTexture(GL_TEXTURE_2D, 0);
    renderSceneGraph(m_shader, *m_ballNode, m_view, basetrans);
    glBindVertexArray(0);

    glDisable(GL_CULL_FACE);
    glDisable( GL_DEPTH_TEST );
}

//----------------------------------------------------------------------------------------
void Bouncer::renderSceneGraph(const ShaderProgram & shader, const SceneNode & root, const mat4 & view, const mat4 & trans) {

    const mat4 newtrans = trans * root.trans;
    if (root.m_nodeType == NodeType::GeometryNode) {
        const GeometryNode * geometryNode = static_cast<const GeometryNode *>(&root);

        updateShaderUniforms(*geometryNode, view, newtrans);

        // Get the BatchInfo corresponding to the GeometryNode's unique MeshId.
        BatchInfo batchInfo = m_batchInfoMap[geometryNode->meshId];

        //-- Now render the mesh:
        shader.enable();
        glDrawArrays(GL_TRIANGLES, batchInfo.startIndex, batchInfo.numIndices);
        shader.disable();
    }
    
    for (const SceneNode * node : root.children) {
        renderSceneGraph(shader, *node, view, newtrans);
    }

    CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
/*
 * Called once, after program is signaled to terminate.
 */
void Bouncer::cleanup()
{

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

    if(!m_paused) {
        m_view = rotate(mat4(1.), (float)(yPos - m_lastMouseY) / 1000, vec3(1., 0., 0.)) * m_view;
        m_view = rotate(mat4(1.), (float)(xPos - m_lastMouseX) / 1000, vec3(0., 1., 0.)) * m_view;
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
        // close
        if (key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(m_window, GL_TRUE);
            eventHandled = true;
        }
        // gimme mouse back
        if (key == GLFW_KEY_P) {
            m_paused = !m_paused;
            m_keyUp = false;
            m_keyDown = false;
            m_keyLeft = false;
            m_keyRight = false;
            m_keyForward = false;
            glfwSetInputMode(m_window, GLFW_CURSOR, m_paused ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            glfwGetCursorPos(m_window, &m_lastMouseX, &m_lastMouseY);
            eventHandled = true;
        }
    }

    if( action == GLFW_PRESS && !m_paused) {
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
    }

    return eventHandled;
}

void Bouncer::movePlayer(unsigned int time)
{
    const float moveScale = 100.0;
    float moveX = 0., moveY = 0., moveZ = 0.;
    if(m_keyUp) {
        moveY -= (float)time / moveScale;
    }
    if(m_keyDown) {
        moveY += (float)time / moveScale;
    }
    if(m_keyLeft) {
        moveX += (float)time / moveScale;
    }
    if(m_keyRight) {
        moveX -= (float)time / moveScale;
    }
    if(m_keyForward) {
        moveZ += (float)time / moveScale;
    }

    m_view = translate(mat4(1.), vec3(moveX, moveY, moveZ)) * m_view;
}