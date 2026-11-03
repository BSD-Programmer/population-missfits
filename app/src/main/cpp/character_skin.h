#pragma once

#include "math3d.h"
#include <vector>
#include <string>
#include <cstdint>

namespace misfits {

enum class BodyGender : uint8_t {
    BaseMale = 0,
    BaseFemale = 1
};

enum class SkinLayerId : uint8_t {
    None_DefaultBlue = 0,       // Signature faceted cobalt/cyan-blue hexagonal base
    NeonCircuit_Overlay = 1,    // Procedural cyan/magenta circuit grid + visor + shoulder pads
    TacticalCamo_Jacket = 2,    // Olive/slate hex camo + tactical harness + helmet visor
    CyberGold_Elite = 3,        // Gold/obsidian metallic hex plates + halo crown + energy core
    CrimsonMisfit_Armor = 4,    // Crimson/carbon battle jacket + spiked pauldrons + red visor
    ZeroG_Valkyrie = 5          // Pearl/violet aero suit + jetpack dorsal fins + cyan visor
};

struct Vertex {
    Vec3 pos;
    Vec3 normal;
    Color4 color;
    Vec2 uv;
    uint32_t partId = 0; // 0=Head, 1=Torso, 2=Pelvis, 3=UpperArm, 4=Forearm, 5=Hand, 6=Leg, 7=ArmorLayer
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    void clear() {
        vertices.clear();
        indices.clear();
    }
    size_t triangleCount() const { return indices.size() / 3; }
};

struct CharacterAppearance {
    BodyGender gender = BodyGender::BaseMale;
    SkinLayerId skinLayer = SkinLayerId::None_DefaultBlue;
    bool visorOverlayEnabled = true;
    bool armorOverlayEnabled = true;
    bool dorsalWingPackEnabled = true;
    uint8_t teamId = 0; // 0 = FFA/Self, 1 = Squad Alpha (Cyan/Blue), 2 = Squad Omega (Orange/Red)
};

struct ArticulatedPose {
    Vec3 rootPos{0.0f, 0.0f, 0.0f};
    float bodyYawRad = 0.0f;
    Vec3 headLocalOffset{0.0f, 1.65f, 0.0f};
    Quat headRot = Quat::identity();
    Vec3 leftHandWorld{0.0f, 1.2f, -0.3f};
    Vec3 rightHandWorld{0.0f, 1.2f, -0.3f};
    bool isGliding = false;
    bool isClimbing = false;
    float walkCyclePhase = 0.0f;
};

struct SkinMetadata {
    SkinLayerId id;
    const char* name;
    const char* description;
    Color4 primaryTint;
    Color4 secondaryTint;
    Color4 emissiveHexLineColor;
    float hexGridDensity;
};

class CharacterSystem {
public:
    static const std::vector<SkinMetadata>& getAvailableSkins();
    static const SkinMetadata& getSkinMetadata(SkinLayerId id);
    static const char* getGenderName(BodyGender gender);

    // Appends a faceted hexagonal prism (6-sided low-poly cylinder with flat facet normals)
    static void appendHexPrism(
        MeshData& mesh,
        const Vec3& bottomCenter,
        const Vec3& topCenter,
        float bottomRadiusX,
        float bottomRadiusZ,
        float topRadiusX,
        float topRadiusZ,
        const Color4& baseColor,
        uint32_t partId,
        bool capEnds = true
    );

    // Builds the complete articulated Base Male or Base Female hexagonal low-poly character mesh
    // with the signature cobalt/cyan-blue base color and optional layered graphical skin & armor overlays.
    static MeshData buildCharacterMesh(
        const CharacterAppearance& appearance,
        const ArticulatedPose& pose,
        bool firstPersonHideHead = false
    );

    // Evaluates the procedural layered graphical skin shader/material color for a vertex on the hex mesh
    static Color4 evaluateLayeredSkinColor(
        const Vec3& localPos,
        const Vec3& normal,
        uint32_t partId,
        uint32_t facetIndex,
        const CharacterAppearance& appearance
    );
};

} // namespace misfits
