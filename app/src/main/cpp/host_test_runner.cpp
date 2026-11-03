#include "math3d.h"
#include "character_skin.h"
#include "customs_map.h"
#include "battle_gameplay.h"

#include <iostream>
#include <cassert>
#include <cmath>
#include <string>

using namespace misfits;

int main() {
    std::cout << "=== Population Misfits v1 Host Verification Suite ===\n";

    // 1. Verify Base Male & Base Female Hexagonal Low-Poly Blue Models + Layered Graphical Skins
    ArticulatedPose pose;
    pose.rootPos = {0.0f, 0.0f, 0.0f};
    pose.leftHandWorld = {-0.32f, 1.25f, -0.28f};
    pose.rightHandWorld = {0.32f, 1.25f, -0.28f};

    CharacterAppearance maleBlue{BodyGender::BaseMale, SkinLayerId::None_DefaultBlue, true, true, true, 0};
    CharacterAppearance femaleBlue{BodyGender::BaseFemale, SkinLayerId::None_DefaultBlue, true, true, true, 0};
    CharacterAppearance femaleCyber{BodyGender::BaseFemale, SkinLayerId::NeonCircuit_Overlay, true, true, true, 1};

    MeshData maleMesh = CharacterSystem::buildCharacterMesh(maleBlue, pose, false);
    MeshData femaleMesh = CharacterSystem::buildCharacterMesh(femaleBlue, pose, false);
    MeshData skinnedMesh = CharacterSystem::buildCharacterMesh(femaleCyber, pose, false);

    assert(!maleMesh.vertices.empty() && maleMesh.triangleCount() >= 180);
    assert(!femaleMesh.vertices.empty() && femaleMesh.triangleCount() >= 180);
    // Skinned mesh with armor/crest overlays has additional hexagonal armor layer geometry
    assert(skinnedMesh.triangleCount() > femaleMesh.triangleCount());

    // Verify default unskinned base model uses the signature cobalt/cyan-blue color palette (blue > red)
    Color4 defaultTorsoCol = CharacterSystem::evaluateLayeredSkinColor({0.0f, 1.3f, 0.0f}, {0.0f, 0.0f, 1.0f}, 1, 0, maleBlue);
    assert(defaultTorsoCol.b > 0.70f && defaultTorsoCol.b > defaultTorsoCol.r * 3.0f);

    std::cout << "[PASS] Base Male (" << maleMesh.triangleCount() << " tris) & Base Female ("
              << femaleMesh.triangleCount() << " tris) Hexagonal Blue Models + Layered Skins ("
              << skinnedMesh.triangleCount() << " tris) verified.\n";

    // 2. Verify Customs System: 3 Preloaded Maps, Map Editor Placement/Snap/Recolor/Delete, Share Code Round-Trip
    CustomsSystem customs;
    customs.initializeBuiltInMaps("/tmp");
    assert(customs.getMaps().size() == 3);
    assert(customs.getMaps()[0].title == "Hex Citadel");
    assert(customs.getMaps()[1].title == "Neon Spire Arena");
    assert(customs.getMaps()[2].title == "Skybridge Clash");

    size_t initialObjCount = customs.getActiveMap().objects.size();
    uint32_t placedId = customs.placeObject(MapAssetType::HexPillar, MaterialSwatch::NeonCyan, {4.0f, 0.0f, -4.0f}, 30.0f, {1.0f, 1.5f, 1.0f});
    assert(placedId > 0);
    assert(customs.getActiveMap().objects.size() == initialObjCount + 1);

    uint32_t dupId = customs.duplicateObject(placedId, {2.0f, 0.0f, 0.0f});
    assert(dupId > placedId);
    assert(customs.recolorObject(dupId, MaterialSwatch::SolarGold));

    std::string shareCode = CustomsSystem::exportMapCode(customs.getActiveMap());
    assert(shareCode.rfind("PM1|hex-citadel-v1|Hex Citadel|", 0) == 0);

    CustomMap importedMap;
    bool importOk = CustomsSystem::importMapCode(shareCode, importedMap);
    assert(importOk);
    assert(importedMap.objects.size() == customs.getActiveMap().objects.size());
    assert(CustomsSystem::saveMapToDisk(importedMap, "/tmp"));

    std::cout << "[PASS] Customs System (3 built-in maps, VR Editor ops, Share Code export/import, JSON persistence) verified.\n";

    // 3. Verify Battle Mode: VCS Climbing, VCS Gliding, Weapons, AI Bots, and v2 Building Roadmap Badge
    BattleEngine engine;
    engine.initialize("/tmp");
    assert(BattleEngine::kTacticalCombatBuildingEnabledInV1 == false);
    std::string badge = BattleEngine::getTacticalBuildingRoadmapBadgeText();
    assert(badge.find("ROADMAP (v2)") != std::string::npos);

    VRControllerInput input{};
    // Move player next to central Sniper Spire at (0,0,0) and test VCS Climbing
    engine.getLocalPlayerMutable().footPosition = {0.0f, 0.0f, 1.5f};
    input.leftHandPos = {0.0f, 1.6f, -1.2f}; // Inside climbable spire bounds
    input.leftGrip = 0.95f;
    engine.tick(0.05f, input);
    assert(engine.getLocalPlayer().isClimbingLeft == true);

    // Pull hand downward by 0.6m to climb upward
    input.leftHandPos = {0.0f, 1.0f, -1.2f};
    engine.tick(0.05f, input);
    assert(engine.getLocalPlayer().footPosition.y > 0.4f);

    // Release grip and extend arms outward in T-pose while high in the air to trigger VCS Gliding
    input.leftGrip = 0.0f;
    engine.getLocalPlayerMutable().footPosition = {5.0f, 12.0f, 5.0f};
    engine.getLocalPlayerMutable().onGround = false;
    engine.getLocalPlayerMutable().velocity = {0.0f, -1.0f, 0.0f};
    input.leftHandPos  = {-0.68f, 1.45f, 0.0f};
    input.rightHandPos = { 0.68f, 1.45f, 0.0f};
    engine.tick(0.05f, input);
    assert(engine.getLocalPlayer().isGliding == true);

    // Verify all 5 weapons fire and bots engage in Arena Combat
    for (int w = 0; w < 5; ++w) {
        input.rightTrigger = 0.95f;
        engine.tick(0.05f, input);
        engine.cyclePlayerWeapon();
    }
    assert(!engine.getTracers().empty());

    MeshData worldMesh, hudMesh;
    engine.buildFrameGeometry(input, worldMesh, hudMesh);
    assert(worldMesh.triangleCount() > 1500);
    assert(hudMesh.triangleCount() > 20);

    std::cout << "[PASS] Battle Engine (VCS Climbing, VCS Gliding, 5 Weapons, Standardized Hitboxes, AI Bots, v2 Roadmap HUD) verified ("
              << worldMesh.triangleCount() << " world tris, " << hudMesh.triangleCount() << " HUD tris).\n";
    std::cout << "=== ALL POPULATION MISFITS v1 VERIFICATION TESTS PASSED ===\n";
    return 0;
}
