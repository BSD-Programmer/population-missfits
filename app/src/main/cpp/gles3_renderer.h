#pragma once

#include "math3d.h"
#include "character_skin.h"

namespace misfits {

class Gles3Renderer {
public:
    Gles3Renderer() = default;
    ~Gles3Renderer();

    bool initialize();
    void shutdown();

    void beginEyeFrame(
        int width,
        int height,
        unsigned int colorTextureId,
        const Color4& clearColor
    );

    void renderMesh(
        const MeshData& mesh,
        const Mat4& viewProj,
        const Vec3& cameraWorldPos,
        float timeSeconds
    );

    void endEyeFrame();

private:
    unsigned int shaderProgram_ = 0;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int ibo_ = 0;
    unsigned int fbo_ = 0;
    unsigned int depthRbo_ = 0;
    int depthWidth_ = 0;
    int depthHeight_ = 0;

    int uViewProjLoc_ = -1;
    int uCameraPosLoc_ = -1;
    int uTimeLoc_ = -1;
};

} // namespace misfits
