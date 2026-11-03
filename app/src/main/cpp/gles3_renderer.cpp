#include "gles3_renderer.h"
#include <GLES3/gl3.h>
#include <cstring>

namespace misfits {

namespace {

const char* kVertexShaderSrc = R"(#version 300 es
precision highp float;

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec2 aUv;

uniform mat4 uViewProj;

out vec3 vWorldPos;
out vec3 vNormal;
out vec4 vColor;
out vec2 vUv;

void main() {
    vWorldPos = aPos;
    vNormal = aNormal;
    vColor = aColor;
    vUv = aUv;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)";

const char* kFragmentShaderSrc = R"(#version 300 es
precision mediump float;

in vec3 vWorldPos;
in vec3 vNormal;
in vec4 vColor;
in vec2 vUv;

uniform vec3 uCameraPos;
uniform float uTime;

out vec4 fragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(vec3(0.42, 0.82, 0.38));
    vec3 V = normalize(uCameraPos - vWorldPos);

    // Crisp flat-faceted directional + sky/ground hemisphere lighting for hexagonal low-poly geometry
    float ndl = max(dot(N, L), 0.0);
    float hemi = 0.55 + 0.45 * N.y;
    float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0) * 0.28;

    // Subtle hexagonal world-space grid highlight on arena surfaces
    float hexPulse = 0.04 * sin(vWorldPos.y * 8.0 + uTime * 2.5);
    vec3 lit = vColor.rgb * (0.38 * hemi + 0.68 * ndl) + vec3(0.15, 0.65, 0.98) * rim + hexPulse;

    fragColor = vec4(clamp(lit, 0.0, 1.0), vColor.a);
}
)";

GLuint compileGlShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glDeleteShader(s);
        return 0;
    }
    return s;
}

} // namespace

Gles3Renderer::~Gles3Renderer() {
    shutdown();
}

bool Gles3Renderer::initialize() {
    GLuint vs = compileGlShader(GL_VERTEX_SHADER, kVertexShaderSrc);
    GLuint fs = compileGlShader(GL_FRAGMENT_SHADER, kFragmentShaderSrc);
    if (!vs || !fs) return false;

    shaderProgram_ = glCreateProgram();
    glAttachShader(shaderProgram_, vs);
    glAttachShader(shaderProgram_, fs);
    glLinkProgram(shaderProgram_);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint linked = 0;
    glGetProgramiv(shaderProgram_, GL_LINK_STATUS, &linked);
    if (!linked) return false;

    uViewProjLoc_  = glGetUniformLocation(shaderProgram_, "uViewProj");
    uCameraPosLoc_ = glGetUniformLocation(shaderProgram_, "uCameraPos");
    uTimeLoc_      = glGetUniformLocation(shaderProgram_, "uTime");

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ibo_);
    glGenFramebuffers(1, &fbo_);
    glGenRenderbuffers(1, &depthRbo_);

    return true;
}

void Gles3Renderer::shutdown() {
    if (depthRbo_) { glDeleteRenderbuffers(1, &depthRbo_); depthRbo_ = 0; }
    if (fbo_)      { glDeleteFramebuffers(1, &fbo_); fbo_ = 0; }
    if (ibo_)      { glDeleteBuffers(1, &ibo_); ibo_ = 0; }
    if (vbo_)      { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_)      { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (shaderProgram_) { glDeleteProgram(shaderProgram_); shaderProgram_ = 0; }
}

void Gles3Renderer::beginEyeFrame(
    int width,
    int height,
    unsigned int colorTextureId,
    const Color4& clearColor
) {
    if (colorTextureId != 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTextureId, 0);

        if (width != depthWidth_ || height != depthHeight_) {
            depthWidth_ = width;
            depthHeight_ = height;
            glBindRenderbuffer(GL_RENDERBUFFER, depthRbo_);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo_);
        }
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Gles3Renderer::renderMesh(
    const MeshData& mesh,
    const Mat4& viewProj,
    const Vec3& cameraWorldPos,
    float timeSeconds
) {
    if (mesh.vertices.empty() || mesh.indices.empty()) return;

    glUseProgram(shaderProgram_);
    glUniformMatrix4fv(uViewProjLoc_, 1, GL_FALSE, viewProj.m);
    glUniform3f(uCameraPosLoc_, cameraWorldPos.x, cameraWorldPos.y, cameraWorldPos.z);
    glUniform1f(uTimeLoc_, timeSeconds);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(Vertex)),
                 mesh.vertices.data(),
                 GL_STREAM_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(uint32_t)),
                 mesh.indices.data(),
                 GL_STREAM_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, pos)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, normal)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, color)));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, uv)));

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Gles3Renderer::endEyeFrame() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace misfits
