//-----------------------------------------------------------------------------
//  MeshViewer.cc
//-----------------------------------------------------------------------------
#include "Pre.h"
#include "Core/App.h"
#include "Gfx/Gfx.h"
#include "IO/IO.h"
#include "IMUI/IMUI.h"
#include "Input/Input.h"
#include "HTTP/HTTPFileSystem.h"
#include "Assets/Gfx/MeshLoader.h"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/polar_coordinates.hpp"
#include "shaders.h"

using namespace Oryol;

class MeshViewerApp : public App {
public:
    AppState::Code OnInit();
    AppState::Code OnRunning();
    AppState::Code OnCleanup();

private:
    void handleInput();
    void updateCamera();
    void updateLight();
    void drawUI();
    void createMaterials();
    void loadMesh(const char* path);
    void applyVariables(int materialIndex);

    int32 frameCount = 0;
    ResourceLabel curMeshLabel;
    Id mesh;
    MeshSetup curMeshSetup;
    glm::vec3 eyePos;
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 model;
    glm::mat4 modelViewProj;
    ClearState clearState;

    int32 curMeshIndex = 0;
    static const int32 numMeshes = 3;
    static const char* meshNames[numMeshes];
    static const char* meshPaths[numMeshes];
    static const int32 numShaders = 3;
    static const char* shaderNames[numShaders];
    enum {
        Normals = 0,
        Lambert,
        Phong
    };
    Id shaders[numShaders];
    ResourceLabel curMaterialLabel;
    int numMaterials = 0;
    struct Material {
        int32 shaderIndex = Phong;
        Id drawState;
        glm::vec4 diffuse = glm::vec4(0.0f, 0.24f, 0.64f, 1.0f);
        glm::vec4 specular = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        float specPower = 32.0f;
    } materials[GfxConfig::MaxNumPrimGroups];
    bool gammaCorrect = true;

    struct CameraSetting {
        float dist = 8.0f;
        glm::vec2 orbital = glm::vec2(glm::radians(25.0f), 0.0f);
        float height = 1.0f;
    } camera;
    bool camAutoOrbit = true;
    struct CameraSetting cameraSettings[numMeshes];

    glm::vec2 lightOrbital = glm::vec2(glm::radians(25.0f), 0.0f);
    glm::vec3 lightDir;
    glm::vec4 lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    float lightIntensity = 1.0f;
    bool lightAutoOrbit = false;

    StringBuilder strBuilder;

    const float minCamDist = 0.5f;
    const float maxCamDist = 20.0f;
    const float minLatitude = -85.0f;
    const float maxLatitude = 85.0f;
    const float minCamHeight = -2.0f;
    const float maxCamHeight = 5.0f;
};
OryolMain(MeshViewerApp);

const char* MeshViewerApp::meshNames[numMeshes] = {
    "Tiger",
    "Blitz",
    "Teapot"
};
const char* MeshViewerApp::meshPaths[numMeshes] = {
    "msh:tiger.omsh",
    "msh:opelblitz.omsh",
    "msh:teapot.omsh"
};

const char* MeshViewerApp::shaderNames[numShaders] = {
    "Normals",
    "Lambert",
    "Phong"
};

//-----------------------------------------------------------------------------
AppState::Code
MeshViewerApp::OnInit() {

    // setup IO system
    IOSetup ioSetup;
    ioSetup.FileSystems.Add("http", HTTPFileSystem::Creator());
    ioSetup.Assigns.Add("msh:", ORYOL_SAMPLE_URL);
    IO::Setup(ioSetup);

    // setup rendering and input system
    Gfx::Setup(GfxSetup::WindowMSAA4(800, 512, "Oryol Mesh Viewer"));
    Input::Setup();

    // setup IMUI ui system
    IMUI::Setup();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    ImVec4 defaultBlue(0.0f, 0.5f, 1.0f, 0.7f);
    style.Colors[ImGuiCol_TitleBg] = defaultBlue;
    style.Colors[ImGuiCol_TitleBgCollapsed] = defaultBlue;
    style.Colors[ImGuiCol_SliderGrab] = defaultBlue;
    style.Colors[ImGuiCol_SliderGrabActive] = defaultBlue;
    style.Colors[ImGuiCol_Button] = defaultBlue;
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.5f, 1.0f, 0.3f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.0f, 0.5f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.0f, 0.5f, 1.0f, 0.5f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.0f, 0.5f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.0f, 0.5f, 1.0f, 0.3f);
    style.Colors[ImGuiCol_Header] = defaultBlue;
    style.Colors[ImGuiCol_HeaderHovered] = defaultBlue;
    style.Colors[ImGuiCol_HeaderActive] = defaultBlue;

    this->shaders[Normals] = Gfx::CreateResource(Shaders::Normals::CreateSetup());
    this->shaders[Lambert] = Gfx::CreateResource(Shaders::Lambert::CreateSetup());
    this->shaders[Phong]   = Gfx::CreateResource(Shaders::Phong::CreateSetup());
    this->loadMesh(this->meshPaths[this->curMeshIndex]);

    // setup projection and view matrices
    const float32 fbWidth = (const float32) Gfx::DisplayAttrs().FramebufferWidth;
    const float32 fbHeight = (const float32) Gfx::DisplayAttrs().FramebufferHeight;
    this->proj = glm::perspectiveFov(glm::radians(60.0f), fbWidth, fbHeight, 0.01f, 100.0f);

    // non-standard camera settings when switching objects
    // teapot:
    this->cameraSettings[2].dist = 0.8f;
    this->cameraSettings[2].height = 0.0f;

    this->clearState.Color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
    return App::OnInit();
}

//-----------------------------------------------------------------------------
AppState::Code
MeshViewerApp::OnRunning() {

    this->frameCount++;
    this->handleInput();
    this->updateCamera();
    this->updateLight();

    Gfx::ApplyDefaultRenderTarget(this->clearState);
    this->drawUI();
    for (int i = 0; i < this->numMaterials; i++) {
        Gfx::ApplyDrawState(this->materials[i].drawState);
        this->applyVariables(i);
        Gfx::Draw(i);
    }
    ImGui::Render();
    Gfx::CommitFrame();
    return Gfx::QuitRequested() ? AppState::Cleanup : AppState::Running;
}

//-----------------------------------------------------------------------------
AppState::Code
MeshViewerApp::OnCleanup() {
    IMUI::Discard();
    Input::Discard();
    Gfx::Discard();
    IO::Discard();
    return App::OnCleanup();
}

//-----------------------------------------------------------------------------
void
MeshViewerApp::handleInput() {

    // dragging flag on/off
    static bool dragging = false;
    if (ImGui::IsMouseHoveringAnyWindow() && ImGui::IsMouseDragging()) {
        dragging = true;
    }
    if (!ImGui::IsMouseDragging()) {
        dragging = false;
    }

    // rotate camera with mouse if not UI-dragging
    if (!(dragging || ImGui::IsMouseHoveringAnyWindow())) {
        const Mouse& mouse = Input::Mouse();
        if (mouse.Attached) {
            if (mouse.ButtonPressed(Mouse::LMB)) {
                this->camera.orbital.y -= mouse.Movement.x * 0.01f;
                this->camera.orbital.x = glm::clamp(this->camera.orbital.x + mouse.Movement.y * 0.01f, glm::radians(minLatitude), glm::radians(maxLatitude));
            }
            this->camera.dist = glm::clamp(this->camera.dist + mouse.Scroll.y * 0.1f, minCamDist, maxCamDist);
        }
    }
}

//------------------------------------------------------------------------------
void
MeshViewerApp::updateCamera() {
    if (this->camAutoOrbit) {
        this->camera.orbital.y += 0.01f;
        if (this->camera.orbital.y > glm::radians(360.0f)) {
            this->camera.orbital.y = 0.0f;
        }
    }
    this->eyePos = glm::euclidean(this->camera.orbital) * this->camera.dist;
    glm::vec3 poi  = glm::vec3(0.0f, this->camera.height, 0.0f);
    this->view = glm::lookAt(this->eyePos + poi, poi, glm::vec3(0.0f, 1.0f, 0.0f));
    this->modelViewProj = this->proj * this->view;
}

//-----------------------------------------------------------------------------
void
MeshViewerApp::updateLight() {
    if (this->lightAutoOrbit) {
        this->lightOrbital.y += 0.01f;
        if (this->lightOrbital.y > glm::radians(360.0f)) {
            this->lightOrbital.y = 0.0f;
        }
    }
    this->lightDir = glm::euclidean(this->lightOrbital);
}

//-----------------------------------------------------------------------------
void
MeshViewerApp::drawUI() {
    const char* state;
    switch (Gfx::QueryResourceInfo(this->mesh).State) {
        case ResourceState::Valid: state = "Loaded"; break;
        case ResourceState::Failed: state = "Load Failed"; break;
        case ResourceState::Pending: state = "Loading..."; break;
        default: state = "Invalid"; break;
    }
    IMUI::NewFrame();
    ImGui::Begin("Mesh Viewer", nullptr, ImVec2(240, 300), 0.25f, 0);
    ImGui::PushItemWidth(130.0f);
    if (ImGui::Combo("##mesh", (int*) &this->curMeshIndex, this->meshNames, numMeshes)) {
        this->camera = this->cameraSettings[this->curMeshIndex];
        this->loadMesh(this->meshPaths[this->curMeshIndex]);
    }
    ImGui::Text("state: %s\n", state);
    if (this->curMeshSetup.NumPrimitiveGroups() > 0) {
        ImGui::Text("primitive groups:\n");
        for (int i = 0; i < this->curMeshSetup.NumPrimitiveGroups(); i++) {
            ImGui::Text("%d: %d triangles\n", i, this->curMeshSetup.PrimitiveGroup(i).NumElements / 3);
        }
    }
    if (ImGui::CollapsingHeader("Camera")) {
        ImGui::SliderFloat("Dist##cam", &this->camera.dist, minCamDist, maxCamDist);
        ImGui::SliderFloat("Height##cam", &this->camera.height, minCamHeight, maxCamHeight);
        ImGui::SliderAngle("Long##cam", &this->camera.orbital.y, 0.0f, 360.0f);
        ImGui::SliderAngle("Lat##cam", &this->camera.orbital.x, minLatitude, maxLatitude);
        ImGui::Checkbox("Auto Orbit##cam", &this->camAutoOrbit);
        if (ImGui::Button("Reset##cam")) {
            this->camera.dist = 8.0f;
            this->camera.height = 1.0f;
            this->camera.orbital = glm::vec2(0.0f, 0.0f);
            this->camAutoOrbit = false;
        }
    }
    if (ImGui::CollapsingHeader("Light")) {
        ImGui::SliderAngle("Long##light", &this->lightOrbital.y, 0.0f, 360.0f);
        ImGui::SliderAngle("Lat##light", &this->lightOrbital.x, minLatitude, maxLatitude);
        ImGui::ColorEdit3("Color##light", &this->lightColor.x);
        ImGui::SliderFloat("Intensity##light", &this->lightIntensity, 0.0f, 5.0f);
        ImGui::Checkbox("Auto Orbit##light", &this->lightAutoOrbit);
        ImGui::Checkbox("Gamma Correct##light", &this->gammaCorrect);
        if (ImGui::Button("Reset##light")) {
            this->lightOrbital = glm::vec2(glm::radians(25.0f), 0.0f);
            this->lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            this->lightIntensity = 1.0f;
            this->lightAutoOrbit = false;
        }
    }
    for (int i = 0; i < this->numMaterials; i++) {
        this->strBuilder.Format(32, "Material %d", i);
        if (ImGui::CollapsingHeader(this->strBuilder.AsCStr())) {
            this->strBuilder.Format(32, "shader##mat%d", i);
            if (ImGui::Combo(strBuilder.AsCStr(), &this->materials[i].shaderIndex, this->shaderNames, numShaders)) {
                this->createMaterials();
            }
            if ((Lambert == this->materials[i].shaderIndex) || (Phong == this->materials[i].shaderIndex)) {
                this->strBuilder.Format(32, "diffuse##%d", i);
                ImGui::ColorEdit3(this->strBuilder.AsCStr(), &this->materials[i].diffuse.x);
            }
            if (Phong == this->materials[i].shaderIndex) {
                this->strBuilder.Format(32, "specular##%d", i);
                ImGui::ColorEdit3(this->strBuilder.AsCStr(), &this->materials[i].specular.x);
                this->strBuilder.Format(32, "power##%d", i);
                ImGui::SliderFloat(this->strBuilder.AsCStr(), &this->materials[i].specPower, 1.0f, 512.0f);
            }
        }
    }
    ImGui::PopItemWidth();
    ImGui::End();
}

//-----------------------------------------------------------------------------
void
MeshViewerApp::createMaterials() {
    o_assert_dbg(this->mesh.IsValid());
    if (this->curMaterialLabel.IsValid()) {
        Gfx::DestroyResources(this->curMaterialLabel);
    }

    this->curMaterialLabel = Gfx::PushResourceLabel();
    for (int i = 0; i < this->numMaterials; i++) {
        auto dss = DrawStateSetup::FromMeshAndProg(this->mesh, this->shaders[this->materials[i].shaderIndex]);
        dss.DepthStencilState.DepthWriteEnabled = true;
        dss.DepthStencilState.DepthCmpFunc = CompareFunc::LessEqual;
        dss.RasterizerState.CullFaceEnabled = true;
        dss.RasterizerState.SampleCount = 4;
        this->materials[i].drawState = Gfx::CreateResource(dss);
    }
    Gfx::PopResourceLabel();
}

//-----------------------------------------------------------------------------
void
MeshViewerApp::loadMesh(const char* path) {

    // unload current mesh
    if (this->curMeshLabel.IsValid()) {
        Gfx::DestroyResources(this->curMeshLabel);
        this->curMeshSetup = MeshSetup();
    }

    // load new mesh, use 'onloaded' callback to capture the mesh setup
    // object of the loaded mesh
    this->numMaterials = 0;
    this->curMeshLabel = Gfx::PushResourceLabel();
    this->mesh = Gfx::LoadResource(MeshLoader::Create(MeshSetup::FromFile(path), 0, [this](MeshSetup& setup) {
        this->curMeshSetup = setup;
        this->numMaterials = setup.NumPrimitiveGroups();
        this->createMaterials();
    }));
    Gfx::PopResourceLabel();
}

//-----------------------------------------------------------------------------
void
MeshViewerApp::applyVariables(int matIndex) {
    switch (this->materials[matIndex].shaderIndex) {
        case Normals:
            // Normals shader
            {
                Shaders::Normals::VSParams vsParams;
                vsParams.ModelViewProjection = this->modelViewProj;
                Gfx::ApplyUniformBlock(vsParams);
            }
            break;
        case Lambert:
            // Lambert shader
            {
                Shaders::Lambert::VSParams vsParams;
                vsParams.ModelViewProjection = this->modelViewProj;
                vsParams.Model = this->model;
                Gfx::ApplyUniformBlock(vsParams);

                Shaders::Lambert::FSParams fsParams;
                fsParams.LightColor = this->lightColor * this->lightIntensity;
                fsParams.LightDir = this->lightDir;
                fsParams.MatDiffuse = this->materials[matIndex].diffuse;
                fsParams.GammaCorrect = this->gammaCorrect;
                Gfx::ApplyUniformBlock(fsParams);
            }
            break;
        case Phong:
            // Phong shader
            {
                Shaders::Phong::VSParams vsParams;
                vsParams.ModelViewProjection = this->modelViewProj;
                vsParams.Model = this->model;
                Gfx::ApplyUniformBlock(vsParams);

                Shaders::Phong::FSParams fsParams;
                fsParams.EyePos = this->eyePos;
                fsParams.LightColor = this->lightColor * this->lightIntensity;
                fsParams.LightDir = this->lightDir;
                fsParams.MatDiffuse = this->materials[matIndex].diffuse;
                fsParams.MatSpecular = this->materials[matIndex].specular;
                fsParams.MatSpecularPower = this->materials[matIndex].specPower;
                fsParams.GammaCorrect = this->gammaCorrect;
                Gfx::ApplyUniformBlock(fsParams);
            }
            break;
            
        default:
            o_error("Unknown shader index, FIXME!");
            break;
    }
}
