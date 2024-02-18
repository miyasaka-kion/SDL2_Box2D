#include "Scene.h"

#include <SDL_render.h>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include <SDL2/SDL.h>
#include <SDL_error.h>
#include <SDL_hints.h>
#include <thread>

#include "Box.h"
#include "Constants.h"
#include "Edge.h"
#include "Entity.h"
#include "Log.h"
#include "MetricConverter.h"
#include "Settings.h"
#include "Camera.h"

Camera g_camera;
static Settings s_settings;
static ImguiSettings s_imguiSettings;

Scene::Scene() {
    // prepare game context
    Init_SDL_Window();
    Init_SDL_Renderer();
    Init_imgui();

    // physics info initialize
    auto gravity = b2Vec2(0.0f, -10.0f);
    m_world = std::make_unique< b2World >(gravity);
    LoadEntities();
    closeGame = false;
}

Scene::~Scene() {
    // clean up game context
    SDL_DestroyRenderer(m_SDL_Renderer);
    SDL_DestroyWindow(m_SDL_Window);
    SDL_Quit();
}

void Scene::Init_SDL_Renderer() {
    m_SDL_Renderer = SDL_CreateRenderer(m_SDL_Window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if(m_SDL_Renderer == NULL) {
        CC_CORE_ERROR("SDL renderer initialization failed!");
        throw std::runtime_error("SDL_Renderer initialized a NULL renderer");
    }
    SDL_RendererInfo info;
    SDL_GetRendererInfo(m_SDL_Renderer, &info);
    CC_CORE_INFO("Current SDL_Renderer: {}", info.name);
}

void Scene::UpdateUI() {
    [[maybe_unused]] ImGuiIO& io = ImGui::GetIO();
        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(float(g_camera.m_width), float(g_camera.m_height)));

    {
        if(s_imguiSettings.show_demo_window)
            ImGui::ShowDemoWindow();
    }

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
    {
        static int entity_counter = 0;

        ImGui::Begin("Control bar");  // Create a window called "Hello, world!" and append into it.

        ImGui::Text("Adjust ...here!");                     // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &s_imguiSettings.show_demo_window);  // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &s_imguiSettings.show_another_window);

        auto gravity = m_world->GetGravity();

        ImGui::SliderFloat("gravity.y", &gravity.y, -10.0f, 0.0f);  // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", ( float* )&m_clear_color);   // Edit 3 floats representing a color

        if(ImGui::Button("load box")) {
            LoadBox();
        }
        ImGui::SameLine();
        if(ImGui::Button("load Edge")) {
            LoadEdge();
        }
        ImGui::Text("counter = %d", entity_counter);

        if(ImGui::Button("clear Entities")) {
            m_entityList.clear();
        }

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::End();
    }
}

void Scene::Run() {
    // game main loop

    bool  calculating = true;
    float progress    = 0.0f;

    while(closeGame != true) {
        [[maybe_unused]] auto io = ImGui::GetIO();
        PollEvents();
        auto gravity = m_world->GetGravity();


        

        UpdateUI();

        // update world info
        m_world->SetGravity(gravity);

        // update world info end

        // render all
        ImGui::Render();
        SDL_RenderSetScale(m_SDL_Renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);

        SDL_RenderClear(m_SDL_Renderer);

        // removeInactive(); this is currently unneeded!
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());

        RenderEntities();
        SDL_SetRenderDrawColor(m_SDL_Renderer, ( Uint8 )(m_clear_color.x * 255), ( Uint8 )(m_clear_color.y * 255), ( Uint8 )(m_clear_color.z * 255), ( Uint8 )(m_clear_color.w * 255));

        SDL_RenderPresent(m_SDL_Renderer);

        m_world->Step(1.0f / s_settings.m_hertz, s_settings.m_velocityIterations, s_settings.m_positionIterations);  // update

        m_world->ClearForces();
    }
}

void Scene::Init_SDL_Window() {
    // SDL_Init begin
    if(SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        CC_ERROR("SDL_Init error: {}", SDL_GetError());
    }

    SDL_DisplayMode DM;
    SDL_GetCurrentDisplayMode(0, &DM);
    auto Width  = DM.w;
    auto Height = DM.h;

    CC_CORE_INFO("Width of the Screen: {}", Width);
    CC_CORE_INFO("Height of the Screen: {}", Height);

    CC_CORE_INFO("The rendering scale is {} pixels per meter. (px/1.0f)", c_pixelPerMeter);
    
    g_camera.m_width = s_settings.m_windowWidth;
	g_camera.m_height = s_settings.m_windowHeight;

    m_SDL_Window = SDL_CreateWindow("SDL with box2d Game Test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g_camera.m_width, g_camera.m_height, SDL_WINDOW_SHOWN);

    if(m_SDL_Window == NULL) {
        CC_CORE_ERROR("SDL window failed to initialize! ");
        throw std::runtime_error("SDL_CreateWindow generate a NULL window");
    }
    CC_CORE_INFO("SDL window successfully initialized.");
}

void Scene::Init_imgui() {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ( void )io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable docking functionalities
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(m_SDL_Window, m_SDL_Renderer);
    ImGui_ImplSDLRenderer2_Init(m_SDL_Renderer);
}

void Scene::PollEvents() {
    while(SDL_PollEvent(&m_SDL_Event)) {
        ImGui_ImplSDL2_ProcessEvent(&m_SDL_Event);
        switch(m_SDL_Event.type) {
        case SDL_QUIT:
            closeGame = true;
            CC_CORE_INFO("SDL_QUIT Triggered.");
            break;
        case SDL_KEYDOWN:
            switch(m_SDL_Event.key.keysym.sym) {
            case SDLK_ESCAPE:
                closeGame = true;
                CC_CORE_INFO("ESC pressed!");
                CC_CORE_INFO("SDL_QUIT Triggered.");
                break;
            case SDLK_r:
                LoadBox();
                CC_CORE_INFO("r key pressed");
                break;

            case SDLK_a:
                g_camera.m_center.x -= 0.5f;
                CC_CORE_INFO("a key pressed");
                break;

            case SDLK_d:
                g_camera.m_center.x += 0.5f;
                CC_CORE_INFO("d key pressed");
                break;

            case SDLK_w:
                g_camera.m_center.y += 0.5f;
                CC_CORE_INFO("w key pressed");
                break;

            case SDLK_s:
                g_camera.m_center.y -= 0.5f;
                CC_CORE_INFO("s key pressed");
                break;
            }
            break;
        }
    }
}

void Scene::RenderEntities() {
    for(const auto& entity : m_entityList) {
        entity->Render();
    }
}

void Scene::RemoveInactive() {
    m_entityList.erase(std::remove_if(std::begin(m_entityList), std::end(m_entityList), [](const std::unique_ptr< Entity >& entity) { return !entity->isActive; }), m_entityList.end());
}

// test part
void Scene::LoadEntities() {
    LoadBox();
    LoadEdge();
}

void Scene::LoadBox() {
    auto box = std::make_unique< Box >(m_world.get(), m_SDL_Renderer);

    box->Init(c_OriginPos, b2Vec2(c_OriginalBoxWidth, c_OriginalBoxHeight), c_OriginalVelocity, c_originalAngle);
    
    m_entityList.push_back(std::move(box));
}

void Scene::LoadEdge() {
    // some constants
    // start ground point
    b2Vec2 startpoint;
    startpoint.x = -3.0f;
    startpoint.y = -2.0;

    // end ground point
    b2Vec2 endpoint;
    endpoint.x = 3.0;
    endpoint.y = -2.0;
    // constants end

    auto edge = std::make_unique< Edge >(m_world.get(), m_SDL_Renderer);
    edge->Init(startpoint, endpoint);

    m_entityList.push_back(std::move(edge));
}

