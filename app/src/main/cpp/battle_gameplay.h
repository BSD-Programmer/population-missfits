#pragma once

#include "math3d.h"
#include "character_skin.h"
#include "customs_map.h"
#include <vector>
#include <string>
#include <cstdint>

namespace misfits {

enum class WeaponClass : uint8_t {
    AssaultRifle_HX4 = 0, // Balanced automatic carbine
    SubmachineGun_V9 = 1, // High-fire-rate close quarters shredder
    Shotgun_Hex8     = 2, // 8-pellet hexagonal spread scattergun
    Sniper_Apex50    = 3, // High-velocity long-range precision rifle
    Pistol_Sidearm   = 4  // Fast-reload backup sidearm
};

struct WeaponSpec {
    WeaponClass weaponClass;
    const char* name;
    float damagePerHit;
    float fireIntervalSec;
    uint16_t magazineSize;
    float reloadDurationSec;
    uint8_t pelletsPerShot;
    float spreadRadians;
    float recoilKickDeg;
    Color4 tracerColor;
};

struct VRControllerInput {
    Vec3 headPos{0.0f, 1.70f, 0.0f};
    Quat headRot = Quat::identity();

    Vec3 leftHandPos{-0.25f, 1.35f, -0.35f};
    Quat leftHandRot = Quat::identity();
    Vec3 rightHandPos{0.25f, 1.35f, -0.35f};
    Quat rightHandRot = Quat::identity();

    Vec2 leftThumbstick{0.0f, 0.0f};
    Vec2 rightThumbstick{0.0f, 0.0f};

    float leftTrigger = 0.0f;
    float rightTrigger = 0.0f;
    float leftGrip = 0.0f;
    float rightGrip = 0.0f;

    bool buttonAPressed = false; // Jump / Confirm UI
    bool buttonBPressed = false; // Cycle Weapon / Editor Tool
    bool buttonXPressed = false; // Reload / Toggle Locker Skin
    bool buttonYPressed = false; // Toggle Map Editor / Custom Room Menu
};

struct TracerBeam {
    Vec3 start;
    Vec3 end;
    Color4 color;
    float remainingSec = 0.12f;
};

struct DamagePopup {
    Vec3 worldPos;
    int32_t damageAmount = 0;
    bool shieldHit = false;
    float remainingSec = 0.85f;
};

struct KillFeedItem {
    std::string killerName;
    std::string victimName;
    WeaponClass weaponUsed;
    float remainingSec = 5.0f;
};

struct CombatantState {
    uint32_t id = 0;
    std::string displayName;
    bool isBot = false;
    CharacterAppearance appearance;

    Vec3 footPosition{0.0f, 0.0f, 0.0f};
    Vec3 velocity{0.0f, 0.0f, 0.0f};
    float yawRad = 0.0f;
    float pitchRad = 0.0f;

    float health = 100.0f;
    float shield = 50.0f;
    bool alive = true;
    float respawnTimerSec = 0.0f;

    WeaponClass equippedWeapon = WeaponClass::AssaultRifle_HX4;
    uint16_t currentAmmo = 30;
    float fireCooldownSec = 0.0f;
    float reloadTimerSec = 0.0f;
    float recoilAccumDeg = 0.0f;

    // Vertical Combat System (VCS) states
    bool onGround = true;
    bool isClimbingLeft = false;
    bool isClimbingRight = false;
    Vec3 leftClimbAnchor{0.0f, 0.0f, 0.0f};
    Vec3 rightClimbAnchor{0.0f, 0.0f, 0.0f};
    bool isGliding = false;
    float walkPhase = 0.0f;

    // Scoreboard stats
    uint32_t kills = 0;
    uint32_t deaths = 0;
    uint32_t assists = 0;

    // Bot AI state
    uint32_t aiTargetWaypoint = 0;
    float aiDecisionTimer = 0.0f;
};

enum class ActiveMenuTab : uint8_t {
    None_InGameHUD = 0,
    CharacterLocker = 1,
    CustomMapEditor = 2,
    CommunityMapBrowser = 3,
    HostCustomRoom = 4,
    RoadmapPreview_BuildingV2 = 5
};

class BattleEngine {
public:
    BattleEngine();

    void initialize(const std::string& storagePath);
    void startMatch(const CustomRoomRules& rules);
    void tick(float dt, const VRControllerInput& input);

    // Accessors
    const CombatantState& getLocalPlayer() const { return combatants_[0]; }
    CombatantState& getLocalPlayerMutable() { return combatants_[0]; }
    const std::vector<CombatantState>& getCombatants() const { return combatants_; }
    const std::vector<TracerBeam>& getTracers() const { return tracers_; }
    const std::vector<DamagePopup>& getDamagePopups() const { return damagePopups_; }
    const std::vector<KillFeedItem>& getKillFeed() const { return killFeed_; }

    CustomsSystem& getCustoms() { return customs_; }
    const CustomsSystem& getCustoms() const { return customs_; }
    EditorToolState& getEditorState() { return editorState_; }
    const EditorToolState& getEditorState() const { return editorState_; }

    ActiveMenuTab getActiveMenuTab() const { return activeMenuTab_; }
    void setActiveMenuTab(ActiveMenuTab tab) { activeMenuTab_ = tab; }

    float getMatchTimeRemaining() const { return matchTimeRemainingSec_; }
    uint32_t getSquadAlphaScore() const { return squadAlphaKills_; }
    uint32_t getSquadOmegaScore() const { return squadOmegaKills_; }
    bool isMatchFinished() const { return matchFinished_; }

    // Explicit statement that tactical wall-building in active combat is NOT in v1 (planned for v2 Roadmap)
    static constexpr bool kTacticalCombatBuildingEnabledInV1 = false;
    static const char* getTacticalBuildingRoadmapBadgeText();

    static const WeaponSpec& getWeaponSpec(WeaponClass wc);
    void cyclePlayerWeapon();
    void cyclePlayerGender();
    void cyclePlayerSkin();

    // Builds the combined 3D scene geometry for the current frame (Arena + Custom Map + Bots + Player + HUD/Menus + Weapons)
    void buildFrameGeometry(
        const VRControllerInput& input,
        MeshData& outWorldMesh,
        MeshData& outHudMesh
    ) const;

private:
    CustomsSystem customs_;
    EditorToolState editorState_;
    ActiveMenuTab activeMenuTab_ = ActiveMenuTab::None_InGameHUD;

    std::vector<CombatantState> combatants_;
    std::vector<TracerBeam> tracers_;
    std::vector<DamagePopup> damagePopups_;
    std::vector<KillFeedItem> killFeed_;

    float matchTimeRemainingSec_ = 300.0f;
    uint32_t squadAlphaKills_ = 0;
    uint32_t squadOmegaKills_ = 0;
    bool matchFinished_ = false;

    bool prevBtnA_ = false;
    bool prevBtnB_ = false;
    bool prevBtnX_ = false;
    bool prevBtnY_ = false;
    bool prevRightTrig_ = false;
    float snapTurnCooldown_ = 0.0f;

    void tickLocalPlayerVCSAndMovement(float dt, const VRControllerInput& input);
    void tickWeaponsAndShooting(float dt, const VRControllerInput& input);
    void tickBotsAI(float dt);
    void tickPickupsAndSpawners(float dt);
    void tickMapEditorInteraction(float dt, const VRControllerInput& input);

    void fireWeapon(CombatantState& shooter, const Vec3& muzzleOrigin, const Vec3& aimDir);
    void respawnCombatant(CombatantState& c, uint32_t index);
    bool isHandNearClimbableSurface(const Vec3& handWorldPos, float grabRadius) const;
    float sampleGroundAndPlatformHeight(const Vec3& footPos, float currentY, bool& outHitJumpPad) const;
};

} // namespace misfits
