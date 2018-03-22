#include "Bouncer.hpp"
#include "scene_lua.hpp"
using namespace std;

#include "cs488-framework/GlErrorCheck.hpp"
#include "cs488-framework/MathUtils.hpp"

#include <imgui/imgui.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

using namespace glm;

const float EPSILON = 0.0000001;

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
      m_lastMouseY(0.)
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
    glClearColor(0., 0., 0., 1.0);

    createShaderProgram();

    glGenVertexArrays(1, &m_vao_meshData);
    enableVertexShaderInputSlots();

    processLuaSceneFile(m_arenaFile);

    // Load and decode all .obj files at once here.  You may add additional .obj files to
    // this list in order to support rendering additional mesh types.  All vertex
    // positions, and normals will be extracted and stored within the MeshConsolidator
    // class.
    unique_ptr<MeshConsolidator> meshConsolidator (new MeshConsolidator{
            getAssetFilePath("cube.obj"),
            getAssetFilePath("sphere.obj"),
    });


    // Acquire the BatchInfoMap from the MeshConsolidator.
    meshConsolidator->getBatchInfoMap(m_batchInfoMap);

    // Take all vertex data within the MeshConsolidator and upload it to VBOs on the GPU.
    uploadVertexDataToVbos(*meshConsolidator);

    mapVboDataToVertexShaderInputLocations();

    initPerspectiveMatrix();

    initViewMatrix();

    initLightSources();

    m_prevTime = chrono::high_resolution_clock::now();
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwGetCursorPos(m_window, &m_lastMouseX, &m_lastMouseY);
}

//----------------------------------------------------------------------------------------
void Bouncer::processLuaSceneFile(const std::string & filename) {
    // This version of the code treats the Lua file as an Asset,
    // so that you'd launch the program with just the filename
    // of a puppet in the Assets/ directory.
    // std::string assetFilePath = getAssetFilePath(filename.c_str());
    // m_rootNode = std::shared_ptr<SceneNode>(import_lua(assetFilePath));

    // This version of the code treats the main program argument
    // as a straightforward pathname.
    m_rootNode = std::shared_ptr<SceneNode>(import_lua(filename));
    if (!m_rootNode) {
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
}

//----------------------------------------------------------------------------------------
void Bouncer::enableVertexShaderInputSlots()
{
    //-- Enable input slots for m_vao_meshData:
    {
        glBindVertexArray(m_vao_meshData);

        // Enable the vertex shader attribute location for "position" when rendering.
        m_positionAttribLocation = m_shader.getAttribLocation("position");
        glEnableVertexAttribArray(m_positionAttribLocation);

        // Enable the vertex shader attribute location for "normal" when rendering.
        m_normalAttribLocation = m_shader.getAttribLocation("normal");
        glEnableVertexAttribArray(m_normalAttribLocation);

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
}

//----------------------------------------------------------------------------------------
void Bouncer::mapVboDataToVertexShaderInputLocations()
{
    // Bind VAO in order to record the data mapping.
    glBindVertexArray(m_vao_meshData);

    // Tell GL how to map data from the vertex buffer "m_vbo_vertexPositions" into the
    // "position" vertex attribute location for any bound vertex shader program.
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexPositions);
    glVertexAttribPointer(m_positionAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Tell GL how to map data from the vertex buffer "m_vbo_vertexNormals" into the
    // "normal" vertex attribute location for any bound vertex shader program.
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexNormals);
    glVertexAttribPointer(m_normalAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    //-- Unbind target, and restore default values:
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
void Bouncer::initPerspectiveMatrix()
{
    float aspect = ((float)m_windowWidth) / m_windowHeight;
    m_perpsective = glm::perspective(degreesToRadians(60.0f), aspect, 0.1f, 100.0f);
}


//----------------------------------------------------------------------------------------
void Bouncer::initViewMatrix() {
    m_view = glm::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, -1.0f),
            vec3(0.0f, 1.0f, 0.0f));
}

//----------------------------------------------------------------------------------------
void Bouncer::initLightSources() {
    // World-space position
    m_light.position = vec3(-2.0f, 5.0f, 0.5f);
    m_light.rgbIntensity = vec3(0.8f); // White light
}

//----------------------------------------------------------------------------------------
void Bouncer::uploadCommonSceneUniforms() {
    m_shader.enable();
    {
        //-- Set Perpsective matrix uniform for the scene:
        GLint location = m_shader.getUniformLocation("Perspective");
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_perpsective));
        CHECK_GL_ERRORS;

        //-- Set LightSource uniform for the scene:
        {
            location = m_shader.getUniformLocation("light.position");
            glUniform3fv(location, 1, value_ptr(m_light.position));
            location = m_shader.getUniformLocation("light.rgbIntensity");
            glUniform3fv(location, 1, value_ptr(m_light.rgbIntensity));
            CHECK_GL_ERRORS;
        }

        //-- Set background light ambient intensity
        {
            location = m_shader.getUniformLocation("ambientIntensity");
            vec3 ambientIntensity(0.08f);
            glUniform3fv(location, 1, value_ptr(ambientIntensity));
            CHECK_GL_ERRORS;
        }
    }
    m_shader.disable();
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

    m_shader.enable();
    {
        //-- Set ModelView matrix:
        GLint location = m_shader.getUniformLocation("ModelView");
        mat4 modelView = viewMatrix * trans;
        glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(modelView));
        CHECK_GL_ERRORS;

        //-- Set NormMatrix:
        location = m_shader.getUniformLocation("NormalMatrix");
        mat3 normalMatrix = glm::transpose(glm::inverse(mat3(modelView)));
        glUniformMatrix3fv(location, 1, GL_FALSE, value_ptr(normalMatrix));
        CHECK_GL_ERRORS;

        //-- Set Material values:
        location = m_shader.getUniformLocation("material.kd");
        vec3 kd = node.material.kd;
        glUniform3fv(location, 1, value_ptr(kd));
        CHECK_GL_ERRORS;
        location = m_shader.getUniformLocation("material.ks");
        vec3 ks = node.material.ks;
        glUniform3fv(location, 1, value_ptr(ks));
        CHECK_GL_ERRORS;
        location = m_shader.getUniformLocation("material.shininess");
        glUniform1f(location, node.material.shininess);
        CHECK_GL_ERRORS;


    }
    m_shader.disable();

}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after guiLogic().
 */
void Bouncer::draw() {

    mat4 basetrans(1.f);

    glEnable( GL_DEPTH_TEST );
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glBindVertexArray(m_vao_meshData);
    renderSceneGraph(*m_rootNode, basetrans);
    glBindVertexArray(0);

    glDisable(GL_CULL_FACE);
    glDisable( GL_DEPTH_TEST );
}

//----------------------------------------------------------------------------------------
void Bouncer::renderSceneGraph(const SceneNode & root, const mat4 & trans) {

    mat4 jointtrans(1.f);
    mat4 newtrans = trans * root.trans;
    if (root.m_nodeType == NodeType::JointNode) {
        const JointNode * jointNode = static_cast<const JointNode *>(&root);
        jointtrans = rotate(jointtrans, radians((float)(jointNode->m_current_x)), vec3(1.f, 0.f, 0.f));
        jointtrans = rotate(jointtrans, radians((float)(jointNode->m_current_y)), vec3(0.f, 1.f, 0.f));
        newtrans = newtrans * jointtrans;
    } else if (root.m_nodeType == NodeType::GeometryNode) {
        const GeometryNode * geometryNode = static_cast<const GeometryNode *>(&root);

        updateShaderUniforms(*geometryNode, m_view, newtrans);

        // Get the BatchInfo corresponding to the GeometryNode's unique MeshId.
        BatchInfo batchInfo = m_batchInfoMap[geometryNode->meshId];

        //-- Now render the mesh:
        m_shader.enable();
        glDrawArrays(GL_TRIANGLES, batchInfo.startIndex, batchInfo.numIndices);
        m_shader.disable();
    }
    
    for (const SceneNode * node : root.children) {
        renderSceneGraph(*node, newtrans);
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

    // Fill in with event handling code...

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

    m_view = rotate(mat4(1.), (float)(yPos - m_lastMouseY) / 1000, vec3(1., 0., 0.)) * m_view;
    m_view = rotate(mat4(1.), (float)(xPos - m_lastMouseX) / 1000, vec3(0., 1., 0.)) * m_view;
    
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
    }

    if( action == GLFW_PRESS ) {
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
    float moveX = 0., moveY = 0., moveZ = 0.;
    if(m_keyUp) {
        moveY -= (float)time / 500;
    }
    if(m_keyDown) {
        moveY += (float)time / 500;
    }
    if(m_keyLeft) {
        moveX += (float)time / 500;
    }
    if(m_keyRight) {
        moveX -= (float)time / 500;
    }
    if(m_keyForward) {
        moveZ += (float)time / 500;
    }

    m_view = translate(mat4(1.), vec3(moveX, moveY, moveZ)) * m_view;
}