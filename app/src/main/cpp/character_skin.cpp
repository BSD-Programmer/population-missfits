#include "character_skin.h"
#include <cmath>

namespace misfits {

namespace {

// Signature default Base Model Blue palette:
// Deep cobalt-cyan faceted low-poly surfaces with subtle hexagonal panel luminance variation.
constexpr Color4 kBaseBluePrimary   = {0.11f, 0.44f, 0.88f, 1.0f};
constexpr Color4 kBaseBlueSecondary = {0.07f, 0.30f, 0.68f, 1.0f};
constexpr Color4 kBaseBlueHighlight = {0.20f, 0.68f, 0.98f, 1.0f};

const std::vector<SkinMetadata> kSkins = {
    {
        SkinLayerId::None_DefaultBlue,
        "Misfit Standard (Blue Hex Base)",
        "Signature unskinned hexagonal low-poly base model in cobalt & cyan-blue faceted panels.",
        kBaseBluePrimary,
        kBaseBlueSecondary,
        kBaseBlueHighlight,
        6.0f
    },
    {
        SkinLayerId::NeonCircuit_Overlay,
        "Cyber Pulse (Layered Circuit)",
        "Translucent neon-magenta & cyan hexagonal circuit trace overlay + holographic visor + shoulder plates.",
        {0.10f, 0.38f, 0.82f, 1.0f},
        {0.16f, 0.12f, 0.34f, 1.0f},
        {0.95f, 0.18f, 0.85f, 1.0f},
        14.0f
    },
    {
        SkinLayerId::TacticalCamo_Jacket,
        "Vanguard Spec-Ops (Hex Camo)",
        "Layered olive/slate hexagonal camo jacket over the blue base body with tactical chest rig & visor.",
        {0.22f, 0.36f, 0.24f, 1.0f},
        {0.16f, 0.22f, 0.26f, 1.0f},
        {0.42f, 0.86f, 0.48f, 1.0f},
        10.0f
    },
    {
        SkinLayerId::CyberGold_Elite,
        "Sovereign Gold (Apex Overlay)",
        "Anodized gold & obsidian hexagonal armor plating layered over cobalt base joints with crown crest.",
        {0.88f, 0.68f, 0.16f, 1.0f},
        {0.14f, 0.15f, 0.20f, 1.0f},
        {1.00f, 0.88f, 0.35f, 1.0f},
        12.0f
    },
    {
        SkinLayerId::CrimsonMisfit_Armor,
        "Crimson Havoc (Heavy Misfit)",
        "High-contrast crimson & carbon street-brawler jacket with angular shoulder pauldrons and ruby visor.",
        {0.82f, 0.14f, 0.20f, 1.0f},
        {0.15f, 0.14f, 0.18f, 1.0f},
        {1.00f, 0.36f, 0.18f, 1.0f},
        11.0f
    },
    {
        SkinLayerId::ZeroG_Valkyrie,
        "Zero-G Aero (Flight Suit)",
        "Iridescent pearl-violet flight shell with extended glide stabilizers over the cyan hex under-suit.",
        {0.76f, 0.78f, 0.92f, 1.0f},
        {0.34f, 0.22f, 0.64f, 1.0f},
        {0.24f, 0.92f, 1.00f, 1.0f},
        16.0f
    }
};

} // namespace

const std::vector<SkinMetadata>& CharacterSystem::getAvailableSkins() {
    return kSkins;
}

const SkinMetadata& CharacterSystem::getSkinMetadata(SkinLayerId id) {
    size_t idx = static_cast<size_t>(id);
    if (idx >= kSkins.size()) return kSkins[0];
    return kSkins[idx];
}

const char* CharacterSystem::getGenderName(BodyGender gender) {
    return (gender == BodyGender::BaseMale) ? "Base Male (Hex Low-Poly)" : "Base Female (Hex Low-Poly)";
}

Color4 CharacterSystem::evaluateLayeredSkinColor(
    const Vec3& localPos,
    const Vec3& normal,
    uint32_t partId,
    uint32_t facetIndex,
    const CharacterAppearance& appearance
) {
    // 1. Always start from the Base Model Cobalt/Cyan Blue hexagonal facet color
    float facetShade = (facetIndex % 2 == 0) ? 1.0f : 0.86f;
    if (facetIndex == 0 || facetIndex == 3) facetShade = 1.08f;

    Color4 baseBlue = {
        std::clamp(kBaseBluePrimary.r * facetShade, 0.0f, 1.0f),
        std::clamp(kBaseBluePrimary.g * facetShade, 0.0f, 1.0f),
        std::clamp(kBaseBluePrimary.b * facetShade, 0.0f, 1.0f),
        1.0f
    };

    // Joint/undersuit regions retain the signature blue base color on all skins
    // so the layered graphical skin visibly sits atop the blue hexagonal base body.
    if (partId == 2 || partId == 5) {
        baseBlue.r *= 0.78f;
        baseBlue.g *= 0.85f;
        baseBlue.b *= 0.95f;
    }

    const SkinMetadata& meta = getSkinMetadata(appearance.skinLayer);

    // Procedural hexagonal cell coordinate for graphical pattern overlays
    float hexU = localPos.x * meta.hexGridDensity + ((static_cast<int>( std::floor(localPos.y * meta.hexGridDensity) ) & 1) ? 0.5f : 0.0f);
    float hexV = localPos.y * meta.hexGridDensity;
    float cellDist = std::fabs(hexU - std::round(hexU)) + std::fabs(hexV - std::round(hexV));
    bool onHexEdge = (cellDist > 0.38f && cellDist < 0.48f);

    Color4 result = baseBlue;

    if (appearance.skinLayer != SkinLayerId::None_DefaultBlue) {
        // Layer 1: Graphical skin pattern over torso, upper limbs, and helmet
        bool coveredByJacketOrSuit = (partId == 1 || partId == 3 || partId == 6 || partId == 7);
        if (coveredByJacketOrSuit) {
            bool altPanel = ((facetIndex + static_cast<uint32_t>(std::floor((localPos.y + 2.0f) * 4.0f))) % 2 == 0);
            Color4 layerCol = altPanel ? meta.primaryTint : meta.secondaryTint;
            result = {
                layerCol.r * facetShade,
                layerCol.g * facetShade,
                layerCol.b * facetShade,
                1.0f
            };
        }

        // Layer 2: Emissive circuit / hex seam trim overlay
        if (onHexEdge || partId == 7) {
            float blend = (partId == 7) ? 0.65f : 0.45f;
            result.r = result.r * (1.0f - blend) + meta.emissiveHexLineColor.r * blend;
            result.g = result.g * (1.0f - blend) + meta.emissiveHexLineColor.g * blend;
            result.b = result.b * (1.0f - blend) + meta.emissiveHexLineColor.b * blend;
        }
    } else {
        // Default unskinned blue model still gets subtle cyan hexagonal panel edge highlights
        if (onHexEdge) {
            result.r = result.r * 0.75f + kBaseBlueHighlight.r * 0.25f;
            result.g = result.g * 0.75f + kBaseBlueHighlight.g * 0.25f;
            result.b = result.b * 0.75f + kBaseBlueHighlight.b * 0.25f;
        }
    }

    // Team rim identification tint on shoulder/visor accents in Squad Battle mode
    if (appearance.teamId == 1 && partId == 7) {
        result = {0.15f, 0.85f, 1.0f, 1.0f};
    } else if (appearance.teamId == 2 && partId == 7) {
        result = {1.0f, 0.32f, 0.14f, 1.0f};
    }

    (void)normal;
    return result;
}

void CharacterSystem::appendHexPrism(
    MeshData& mesh,
    const Vec3& bottomCenter,
    const Vec3& topCenter,
    float bottomRadiusX,
    float bottomRadiusZ,
    float topRadiusX,
    float topRadiusZ,
    const Color4& baseColor,
    uint32_t partId,
    bool capEnds
) {
    constexpr int kSides = 6; // Hexagonal low-poly geometry standard
    Vec3 axis = topCenter - bottomCenter;
    float len = axis.length();
    if (len < 1e-5f) return;
    Vec3 up = axis / len;

    Vec3 ref = (std::fabs(up.y) < 0.92f) ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
    Vec3 right = cross(ref, up).normalized();
    Vec3 forward = cross(up, right).normalized();

    Vec3 bottomRing[kSides];
    Vec3 topRing[kSides];

    for (int i = 0; i < kSides; ++i) {
        float angle = (static_cast<float>(i) + 0.5f) * (2.0f * kPi / static_cast<float>(kSides));
        float c = std::cos(angle);
        float s = std::sin(angle);
        bottomRing[i] = bottomCenter + right * (c * bottomRadiusX) + forward * (s * bottomRadiusZ);
        topRing[i]    = topCenter    + right * (c * topRadiusX)    + forward * (s * topRadiusZ);
    }

    // Emit 6 flat-shaded facets (each with distinct facet normal for crisp hexagonal low-poly look)
    for (int i = 0; i < kSides; ++i) {
        int next = (i + 1) % kSides;
        Vec3 p0 = bottomRing[i];
        Vec3 p1 = bottomRing[next];
        Vec3 p2 = topRing[next];
        Vec3 p3 = topRing[i];

        Vec3 facetNormal = cross(p1 - p0, p3 - p0).normalized();
        float shade = (i % 2 == 0) ? 1.0f : 0.88f;
        Color4 cFacet{
            std::clamp(baseColor.r * shade, 0.0f, 1.0f),
            std::clamp(baseColor.g * shade, 0.0f, 1.0f),
            std::clamp(baseColor.b * shade, 0.0f, 1.0f),
            baseColor.a
        };

        uint32_t baseIdx = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({p0, facetNormal, cFacet, {0.0f, 0.0f}, partId});
        mesh.vertices.push_back({p1, facetNormal, cFacet, {1.0f, 0.0f}, partId});
        mesh.vertices.push_back({p2, facetNormal, cFacet, {1.0f, 1.0f}, partId});
        mesh.vertices.push_back({p3, facetNormal, cFacet, {0.0f, 1.0f}, partId});

        mesh.indices.push_back(baseIdx + 0);
        mesh.indices.push_back(baseIdx + 1);
        mesh.indices.push_back(baseIdx + 2);
        mesh.indices.push_back(baseIdx + 0);
        mesh.indices.push_back(baseIdx + 2);
        mesh.indices.push_back(baseIdx + 3);
    }

    if (capEnds) {
        // Top cap
        uint32_t topCenterIdx = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({topCenter, up, baseColor, {0.5f, 0.5f}, partId});
        for (int i = 0; i < kSides; ++i) {
            mesh.vertices.push_back({topRing[i], up, baseColor, {0.0f, 0.0f}, partId});
        }
        for (int i = 0; i < kSides; ++i) {
            int next = (i + 1) % kSides;
            mesh.indices.push_back(topCenterIdx);
            mesh.indices.push_back(topCenterIdx + 1 + next);
            mesh.indices.push_back(topCenterIdx + 1 + i);
        }

        // Bottom cap
        Vec3 down = up * -1.0f;
        uint32_t botCenterIdx = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({bottomCenter, down, baseColor, {0.5f, 0.5f}, partId});
        for (int i = 0; i < kSides; ++i) {
            mesh.vertices.push_back({bottomRing[i], down, baseColor, {0.0f, 0.0f}, partId});
        }
        for (int i = 0; i < kSides; ++i) {
            int next = (i + 1) % kSides;
            mesh.indices.push_back(botCenterIdx);
            mesh.indices.push_back(botCenterIdx + 1 + i);
            mesh.indices.push_back(botCenterIdx + 1 + next);
        }
    }
}

MeshData CharacterSystem::buildCharacterMesh(
    const CharacterAppearance& appearance,
    const ArticulatedPose& pose,
    bool firstPersonHideHead
) {
    MeshData mesh;
    mesh.vertices.reserve(720);
    mesh.indices.reserve(1080);

    const bool isMale = (appearance.gender == BodyGender::BaseMale);

    // Distinct Base Male vs Base Female proportions (both constructed from hexagonal low-poly segments):
    // Base Male: broader V-taper shoulders (0.24m half-width), thicker chest/jaw, heavier forearms
    // Base Female: athletic contoured silhouette (0.185m shoulder half-width, tapered 0.115m waist, contoured hip flare)
    const float shoulderSpan = isMale ? 0.235f : 0.185f;
    const float chestWidth   = isMale ? 0.195f : 0.155f;
    const float chestDepth   = isMale ? 0.125f : 0.105f;
    const float waistWidth   = isMale ? 0.145f : 0.112f;
    const float waistDepth   = isMale ? 0.100f : 0.085f;
    const float hipWidth     = isMale ? 0.155f : 0.168f;
    const float hipDepth     = isMale ? 0.105f : 0.102f;
    const float headRadiusX  = isMale ? 0.105f : 0.094f;
    const float headRadiusZ  = isMale ? 0.115f : 0.104f;
    const float limbScale    = isMale ? 1.00f  : 0.88f;

    Quat bodyRot = Quat::fromAxisAngle({0.0f, 1.0f, 0.0f}, pose.bodyYawRad);
    auto toWorld = [&](const Vec3& local) -> Vec3 {
        return pose.rootPos + bodyRot.rotate(local);
    };

    auto segmentColor = [&](const Vec3& localMid, uint32_t partId, uint32_t facetSeed) -> Color4 {
        return evaluateLayeredSkinColor(localMid, {0.0f, 1.0f, 0.0f}, partId, facetSeed, appearance);
    };

    // 1. Pelvis & Lower Torso (Hexagonal multi-tier core)
    Vec3 pelvisBot = toWorld({0.0f, 0.84f, 0.0f});
    Vec3 pelvisTop = toWorld({0.0f, 1.02f, 0.0f});
    Vec3 waistTop  = toWorld({0.0f, 1.22f, 0.0f});
    Vec3 chestTop  = toWorld({0.0f, 1.46f, 0.0f});
    Vec3 neckTop   = toWorld({0.0f, 1.53f, 0.0f});

    appendHexPrism(mesh, pelvisBot, pelvisTop, hipWidth * 0.90f, hipDepth * 0.90f, hipWidth, hipDepth,
                   segmentColor({0.0f, 0.92f, 0.0f}, 2, 0), 2);
    appendHexPrism(mesh, pelvisTop, waistTop, hipWidth, hipDepth, waistWidth, waistDepth,
                   segmentColor({0.0f, 1.12f, 0.0f}, 1, 1), 1);
    appendHexPrism(mesh, waistTop, chestTop, waistWidth, waistDepth, chestWidth, chestDepth,
                   segmentColor({0.0f, 1.34f, 0.0f}, 1, 0), 1);
    appendHexPrism(mesh, chestTop, neckTop, chestWidth * 0.45f, chestDepth * 0.45f, 0.055f, 0.055f,
                   segmentColor({0.0f, 1.50f, 0.0f}, 2, 2), 2);

    // Female athletic chest contour / Male pectoral hex plate
    Vec3 chestPlateBot = toWorld({0.0f, 1.25f, -chestDepth * 0.55f});
    Vec3 chestPlateTop = toWorld({0.0f, 1.42f, -chestDepth * 0.65f});
    appendHexPrism(mesh, chestPlateBot, chestPlateTop,
                   chestWidth * (isMale ? 0.78f : 0.72f), 0.055f,
                   chestWidth * (isMale ? 0.85f : 0.66f), 0.048f,
                   segmentColor({0.0f, 1.35f, -0.1f}, 1, 3), 1);

    // 2. Articulated Head & Layered Visor / Crest
    if (!firstPersonHideHead) {
        Vec3 headBase = toWorld({0.0f, 1.53f, 0.0f});
        Vec3 headMid  = toWorld({0.0f, 1.65f, 0.0f});
        Vec3 headCrown = toWorld({0.0f, 1.76f, 0.0f});

        appendHexPrism(mesh, headBase, headMid, headRadiusX * 0.78f, headRadiusZ * 0.82f, headRadiusX, headRadiusZ,
                       segmentColor({0.0f, 1.60f, 0.0f}, 0, 0), 0);
        appendHexPrism(mesh, headMid, headCrown, headRadiusX, headRadiusZ, headRadiusX * 0.72f, headRadiusZ * 0.75f,
                       segmentColor({0.0f, 1.70f, 0.0f}, 0, 1), 0);

        if (appearance.visorOverlayEnabled) {
            Vec3 visorBot = toWorld({0.0f, 1.61f, -headRadiusZ * 0.82f});
            Vec3 visorTop = toWorld({0.0f, 1.68f, -headRadiusZ * 0.85f});
            Color4 visorCol = getSkinMetadata(appearance.skinLayer).emissiveHexLineColor;
            appendHexPrism(mesh, visorBot, visorTop, headRadiusX * 0.86f, 0.035f, headRadiusX * 0.82f, 0.032f,
                           visorCol, 7);
        }

        // Skin-specific crest/collar overlay on top of the base model
        if (appearance.armorOverlayEnabled && appearance.skinLayer != SkinLayerId::None_DefaultBlue) {
            Vec3 crestBot = toWorld({0.0f, 1.74f, 0.02f});
            Vec3 crestTop = toWorld({0.0f, 1.83f, 0.04f});
            appendHexPrism(mesh, crestBot, crestTop, 0.035f, 0.085f, 0.020f, 0.065f,
                           segmentColor({0.0f, 1.78f, 0.0f}, 7, 3), 7);
        }
    }

    // 3. Articulated Arms (2-bone IK from shoulders to VR hand targets)
    for (int side = -1; side <= 1; side += 2) {
        float sSign = static_cast<float>(side);
        Vec3 shoulderWorld = toWorld({sSign * shoulderSpan, 1.41f, 0.0f});
        Vec3 handTarget = (side < 0) ? pose.leftHandWorld : pose.rightHandWorld;

        // If gliding, extend arms outward in signature T/V glide silhouette
        Vec3 toHand = handTarget - shoulderWorld;
        float dist = toHand.length();
        constexpr float kUpperLen = 0.28f;
        constexpr float kLowerLen = 0.27f;
        float maxReach = kUpperLen + kLowerLen - 0.01f;

        Vec3 dir = (dist > 1e-4f) ? (toHand / dist) : Vec3{sSign * 0.5f, -0.7f, -0.5f}.normalized();
        float clampedDist = std::clamp(dist, 0.12f, maxReach);

        float cosAngle = std::clamp(
            (kUpperLen * kUpperLen + clampedDist * clampedDist - kLowerLen * kLowerLen) / (2.0f * kUpperLen * clampedDist),
            -1.0f, 1.0f
        );
        float bendAngle = std::acos(cosAngle);

        Vec3 pole = bodyRot.rotate({sSign * 0.35f, -0.4f, 0.65f}).normalized();
        Vec3 bendAxis = cross(dir, pole).normalized();
        if (bendAxis.lengthSq() < 1e-4f) bendAxis = {1.0f, 0.0f, 0.0f};

        Quat elbowQ = Quat::fromAxisAngle(bendAxis, bendAngle);
        Vec3 elbowWorld = shoulderWorld + elbowQ.rotate(dir) * kUpperLen;
        Vec3 wristWorld = handTarget;

        // Upper Arm Hex Segment
        appendHexPrism(mesh, shoulderWorld, elbowWorld,
                       0.056f * limbScale, 0.056f * limbScale,
                       0.046f * limbScale, 0.046f * limbScale,
                       segmentColor({sSign * 0.25f, 1.30f, 0.0f}, 3, 0), 3);

        // Forearm Hex Segment
        appendHexPrism(mesh, elbowWorld, wristWorld,
                       0.046f * limbScale, 0.046f * limbScale,
                       0.036f * limbScale, 0.036f * limbScale,
                       segmentColor({sSign * 0.25f, 1.10f, 0.0f}, 4, 1), 4);

        // Hexagonal Low-Poly Hand
        Vec3 handTip = wristWorld + (wristWorld - elbowWorld).normalized() * 0.085f;
        appendHexPrism(mesh, wristWorld, handTip,
                       0.040f * limbScale, 0.028f * limbScale,
                       0.032f * limbScale, 0.022f * limbScale,
                       segmentColor({sSign * 0.25f, 0.98f, 0.0f}, 5, 0), 5);

        // Layered Shoulder Pauldron / Armor Overlay
        if (appearance.armorOverlayEnabled && appearance.skinLayer != SkinLayerId::None_DefaultBlue) {
            Vec3 pauldronBot = shoulderWorld + Vec3{0.0f, -0.03f, 0.0f};
            Vec3 pauldronTop = shoulderWorld + Vec3{sSign * 0.03f, 0.055f, 0.0f};
            appendHexPrism(mesh, pauldronBot, pauldronTop,
                           0.078f * limbScale, 0.072f * limbScale,
                           0.062f * limbScale, 0.058f * limbScale,
                           segmentColor({sSign * 0.28f, 1.44f, 0.0f}, 7, 0), 7);
        }
    }

    // 4. Dorsal Jetpack / Glide Stabilizer Wings (expands when player is gliding!)
    if (appearance.dorsalWingPackEnabled) {
        Vec3 packBot = toWorld({0.0f, 1.18f, chestDepth * 0.85f});
        Vec3 packTop = toWorld({0.0f, 1.43f, chestDepth * 0.90f});
        appendHexPrism(mesh, packBot, packTop, 0.11f, 0.055f, 0.13f, 0.065f,
                       segmentColor({0.0f, 1.30f, 0.15f}, 7, 1), 7);

        float wingSpan = pose.isGliding ? 0.58f : 0.18f;
        for (int side = -1; side <= 1; side += 2) {
            float sSign = static_cast<float>(side);
            Vec3 wingRoot = toWorld({sSign * 0.10f, 1.35f, chestDepth * 0.95f});
            Vec3 wingTip  = toWorld({sSign * wingSpan, pose.isGliding ? 1.40f : 1.24f, chestDepth * 1.10f});
            Color4 wingCol = pose.isGliding
                ? getSkinMetadata(appearance.skinLayer).emissiveHexLineColor
                : segmentColor({sSign * 0.2f, 1.32f, 0.16f}, 7, 3);
            appendHexPrism(mesh, wingRoot, wingTip, 0.025f, 0.065f, 0.015f, 0.035f, wingCol, 7);
        }
    }

    // 5. Articulated Hexagonal Legs (with stride animation or airborne tuck)
    if (!firstPersonHideHead) {
        float hipSpan = isMale ? 0.105f : 0.095f;
        for (int side = -1; side <= 1; side += 2) {
            float sSign = static_cast<float>(side);
            float swing = std::sin(pose.walkCyclePhase + (side < 0 ? 0.0f : kPi)) * 0.16f;
            if (pose.isGliding || pose.isClimbing) swing *= 0.25f;

            Vec3 hipPos   = toWorld({sSign * hipSpan, 0.85f, 0.0f});
            Vec3 kneePos  = toWorld({sSign * hipSpan, 0.46f, swing * 0.5f});
            Vec3 anklePos = toWorld({sSign * hipSpan, 0.08f, -swing * 0.4f});
            Vec3 footTip  = toWorld({sSign * hipSpan, 0.02f, -swing * 0.4f - 0.11f});

            // Thigh Hex Segment
            appendHexPrism(mesh, kneePos, hipPos,
                           0.056f * limbScale, 0.058f * limbScale,
                           0.072f * limbScale, 0.074f * limbScale,
                           segmentColor({sSign * hipSpan, 0.65f, 0.0f}, 6, 0), 6);

            // Shin Hex Segment
            appendHexPrism(mesh, anklePos, kneePos,
                           0.044f * limbScale, 0.046f * limbScale,
                           0.056f * limbScale, 0.058f * limbScale,
                           segmentColor({sSign * hipSpan, 0.28f, 0.0f}, 6, 1), 6);

            // Boot Hex Segment
            appendHexPrism(mesh, anklePos, footTip,
                           0.048f * limbScale, 0.048f * limbScale,
                           0.042f * limbScale, 0.038f * limbScale,
                           segmentColor({sSign * hipSpan, 0.04f, 0.0f}, 2, 0), 2);
        }
    }

    return mesh;
}

} // namespace misfits
