#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#include <stdio.h>
#include <string>

#include <glad/gl.h>
#include <SDL.h>
#include <SDL_opengl.h>

#include "Another.h"


// Shader strings
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 a_Position;
layout (location = 1) in vec2 a_TexCoord;
out vec2 v_TexCoord;

void main() {
    gl_Position = vec4(a_Position, 0.0, 1.0);
    v_TexCoord = a_TexCoord;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core

uniform sampler2D u_MainTex;
uniform float u_Curvature;
uniform float u_VignetteWidth;
uniform vec2 u_Resolution;

uniform float u_ChromaticAberration;
uniform float u_Contrast;
uniform float u_ColorBleed;
uniform float u_Saturation;
uniform float u_ScanlineIntensity;
uniform float u_Warmth;

uniform float u_InterlacingSeverity;
uniform float u_TrackingHeight;
uniform float u_TrackingSeverity;
uniform float u_TrackingSpeed;
uniform float u_ShimmerSpeed;
uniform float u_RGBMaskSize;
uniform bool u_EnableRGBMask;
uniform float u_Time;

in vec2 v_TexCoord;
out vec4 FragColor;

void main() {

    vec2 uv = v_TexCoord;
    vec2 fragCoord = v_TexCoord * u_Resolution;

    // Screen curvature
    vec2 curved_uv = uv * 2.0 - 1.0;
    vec2 offset = curved_uv.yx / (u_Curvature * 12.0);
    curved_uv += curved_uv * offset * offset;
    curved_uv = curved_uv * 0.5 + 0.5;
    
    // Check if we're outside the curved screen
    if(curved_uv.x < 0.0 || curved_uv.x > 1.0 || curved_uv.y < 0.0 || curved_uv.y > 1.0) {
        FragColor = vec4(0.0);
        return;
    }


    // RGB Mask simulation
    float pix = (fragCoord.y + floor(sin(u_Time * u_ShimmerSpeed))) * u_Resolution.x + fragCoord.x;
    pix = floor(pix);
    vec4 colMask = vec4(
        mod(pix, u_RGBMaskSize),
        mod(pix + 1.0, u_RGBMaskSize),
        mod(pix + 2.0, u_RGBMaskSize),
        1.0
    );
    colMask = colMask / (u_RGBMaskSize - 1.0) + 0.5;

    // Add scanlines
    float scan = mod(fragCoord.y, 3.0);
    float scanline = sin(v_TexCoord.y * u_Resolution.y * 2.0) * u_ScanlineIntensity + (1.0 - u_ScanlineIntensity);
    
    // Sample the texture with slight RGB separation for chromatic aberration
    vec2 red_uv = curved_uv + vec2(u_ChromaticAberration, 0.0);
    vec2 blue_uv = curved_uv - vec2(u_ChromaticAberration, 0.0);
    
    vec4 col;
    col.r = texture(u_MainTex, red_uv).r;
    col.g = texture(u_MainTex, uv).g;
    col.b = texture(u_MainTex, blue_uv).b;
    col.a = 1.0;

    // Apply color mask
    // col *= colMask;

    if(u_EnableRGBMask) {
        col *= colMask;
    }
    
    // Enhance color vibrance
    vec3 contrast = (col.rgb - 0.5) * u_Contrast + 0.5;
    col.rgb = mix(col.rgb, contrast, 0.5);
    
    // Apply color bleeding effect
    float bleed = sin(v_TexCoord.y * u_Resolution.y * 2.0) * u_ColorBleed;
    col.r += bleed;
    col.b -= bleed;
    
    // Boost saturation
    vec3 gray = vec3(dot(col.rgb, vec3(0.2126, 0.7152, 0.0722)));
    col.rgb = mix(gray, col.rgb, u_Saturation);
    
    // Apply scanlines and vignette
    vec2 centeredUV = curved_uv * 2.0 - 1.0;
    vec2 vignette = u_VignetteWidth / u_Resolution;
    float vignetteEffect = smoothstep(0.0, vignette.x, 1.0 - abs(centeredUV.x)) *
                          smoothstep(0.0, vignette.y, 1.0 - abs(centeredUV.y));
    
    col.rgb *= scanline * vignetteEffect * scan;
    col.rgb *= vec3(u_Warmth, 1.0, 2.0 - u_Warmth);
    
    FragColor = col;
}
)";

// Globals for the shader system
GLuint framebuffer = 0;
GLuint textureColorbuffer = 0;
GLuint shaderProgram = 0;
GLuint quadVAO = 0;
GLuint quadVBO = 0;

GLfloat curvatureValue = 4.0f;
GLfloat vignetteWidthValue = 2.0f;

GLfloat chromaticAberrationValue = 0.0f;
GLfloat contrastValue = 1.0f;
GLfloat colorBleedValue = 0.0f;
GLfloat saturationValue = 1.0f;
GLfloat scanlineIntensityValue = 0.0f;
GLfloat warmthValue = 1.0f;

GLfloat shimmerSpeed = 30.0f;
GLfloat rgbMaskSize = 2.9f;
float currentTime = 0.0f;  // We'll need to track time
bool enableRGBMask = false;



void UpdateShaderUniforms(SDL_Window* window) {
    glUseProgram(shaderProgram);

    currentTime += 1.0f / 144.0f;
    
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    
    glUniform1f(glGetUniformLocation(shaderProgram, "u_Curvature"), curvatureValue);
    glUniform1f(glGetUniformLocation(shaderProgram, "u_VignetteWidth"), vignetteWidthValue);
    glUniform2f(glGetUniformLocation(shaderProgram, "u_Resolution"), (float)width, (float)height);
    glUniform1f(glGetUniformLocation(shaderProgram, "u_ChromaticAberration"), chromaticAberrationValue);
    glUniform1f(glGetUniformLocation(shaderProgram, "u_Contrast"), contrastValue);
    glUniform1f(glGetUniformLocation(shaderProgram, "u_ColorBleed"), colorBleedValue);
    glUniform1f(glGetUniformLocation(shaderProgram, "u_Saturation"), saturationValue);
    glUniform1f(glGetUniformLocation(shaderProgram, "u_ScanlineIntensity"), scanlineIntensityValue);
    glUniform1f(glGetUniformLocation(shaderProgram, "u_Warmth"), warmthValue);

    glUniform1f(glGetUniformLocation(shaderProgram, "u_Time"), currentTime);
    glUniform1f(glGetUniformLocation(shaderProgram, "u_ShimmerSpeed"), shimmerSpeed);
    glUniform1f(glGetUniformLocation(shaderProgram, "u_EnableRGBMask"), enableRGBMask);
    glUniform1f(glGetUniformLocation(shaderProgram, "u_RGBMaskSize"), rgbMaskSize);

}

void RenderControls() {
    ImGui::Begin("CRT Effects Controls");
    
    if (ImGui::CollapsingHeader("Basic Effects")) {
        ImGui::SliderFloat("Curvature", &curvatureValue, 0.0f, 10.0f);
        ImGui::SliderFloat("Vignette Width", &vignetteWidthValue, 0.0f, 2.0f);
        ImGui::SliderFloat("Contrast", &contrastValue, 0.0f, 3.0f);
        ImGui::SliderFloat("Saturation", &saturationValue, 0.0f, 3.0f);
        ImGui::SliderFloat("Warmth", &warmthValue, 0.8f, 1.2f);
    }
    
    if (ImGui::CollapsingHeader("Scanline Effects")) {
        ImGui::SliderFloat("Scanline Intensity", &scanlineIntensityValue, 0.0f, 0.5f);
        ImGui::SliderFloat("RGB Mask Size", &rgbMaskSize, 1.0f, 4.0f);
        ImGui::SliderFloat("Shimmer Speed", &shimmerSpeed, 0.0f, 60.0f);
    }

    if(ImGui::Checkbox("Enable RGB Mask", &enableRGBMask)) {
        glUseProgram(shaderProgram);
        glUniform1i(glGetUniformLocation(shaderProgram, "u_EnableRGBMask"), enableRGBMask);
    }
    
    if (ImGui::CollapsingHeader("Color Effects")) {
        ImGui::SliderFloat("Chromatic Aberration", &chromaticAberrationValue, 0.0f, 0.01f, "%.4f");
        ImGui::SliderFloat("Color Bleeding", &colorBleedValue, 0.0f, 0.2f, "%.3f");
    }

    if (ImGui::Button("Reset to Defaults")) {
        curvatureValue = 4.0f;
        vignetteWidthValue = 2.0f;
        chromaticAberrationValue = 0.0f;
        contrastValue = 1.0f;
        colorBleedValue = 0.0f;
        saturationValue = 1.0f;
        scanlineIntensityValue = 0.0f;
        warmthValue = 1.0f;

        shimmerSpeed = 30.0f;
        rgbMaskSize = 2.9f;
    }
    
    ImGui::End();
}

void InitializeShaderSystem(int width, int height) {
    // Create framebuffer
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    // Create texture to render to
    glGenTextures(1, &textureColorbuffer);
    glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);

    // Create and compile shaders
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if(!success) {
        GLchar infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        printf("Vertex shader compilation failed: %s\n", infoLog);
}

    // Setup quad vertices
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
}

void RenderFrame(SDL_Window *window) {
    // First pass: Render ImGui to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Your normal ImGui rendering goes here
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Second pass: Render framebuffer to screen with shader
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shaderProgram);
    
    // Update uniforms
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    glUniform1f(glGetUniformLocation(shaderProgram, "u_Curvature"), curvatureValue);
    glUniform1f(glGetUniformLocation(shaderProgram, "u_VignetteWidth"), vignetteWidthValue);
    glUniform2f(glGetUniformLocation(shaderProgram, "u_Resolution"), (float)width, (float)height);


    glBindVertexArray(quadVAO);
    glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void CleanupShaderSystem() {
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &textureColorbuffer);
    glDeleteProgram(shaderProgram);
}

// Main


int main(int argc, char* argv[]){
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return -1;
    }

    // OpenGL setup
    const char* glsl_version = "#version 330 core";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // Setup SDL 
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);

    if (window == nullptr) {

        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return -1;
    }

    // Setup OpenGL
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    if (gl_context == nullptr) {

        printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
        return -1;
    }


    SDL_GL_MakeCurrent(window, gl_context);

    int version = gladLoadGL((GLADloadfunc) SDL_GL_GetProcAddress);

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        printf("GL %d.%d\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
        return -1;
    }


    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    io.Fonts->AddFontFromFileTTF("../fonts/pokemon-dp-pro.otf", 23.0f);

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {

        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;        
    }

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    InitializeShaderSystem(width, height);

    bool show_demo_window = true;
    bool done = false;
    
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)){

            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                done = true;
            }
            if (event.type == SDL_WINDOWEVENT && 
                event.window.event == SDL_WINDOWEVENT_RESIZED) {
                // Handle resize
                width = event.window.data1;
                height = event.window.data2;
                glViewport(0, 0, width, height);
                
                // Recreate framebuffer texture with new size
                glDeleteTextures(1, &textureColorbuffer);
                glGenTextures(1, &textureColorbuffer);
                glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, 
                            GL_UNSIGNED_BYTE, NULL);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                                     GL_TEXTURE_2D, textureColorbuffer, 0);

                if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                    printf("Framebuffer is not complete after resize!\n");
                }

                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }
        }

        // Start the ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        MyApp::RenderUI();

        RenderControls();
        

        if (show_demo_window) 
            ImGui::ShowDemoWindow(&show_demo_window);

        UpdateShaderUniforms(window);
            
        ImGui::Render();
        RenderFrame(window);
        //ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Shutdown

    CleanupShaderSystem();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}