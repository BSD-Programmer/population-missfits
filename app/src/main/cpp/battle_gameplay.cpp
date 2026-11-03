#include "battle_gameplay.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>

namespace misfits {

namespace {

const WeaponSpec kWeapons[5] = {
    {
        WeaponClass::AssaultRifle_HX4,
        "HX-4 Hex Carbine (AR)",
        19.0f, 0.11f, 30, 1.65f, 1, 0.012f, 1.4f,
        {0.20f, 0.85f, 1.00f, 1.0f}
    },
    {
        WeaponClass::SubmachineGun_V9,
        "V9 Vector Pulse (SMG)",
        13.0f, 0.068f, 36, 1.35f, 1, 0.028f, 1.1f,
        {0.98f, 0.25f, 0.88f, 1.0f}
    },
    {
        WeaponClass::Shotgun_Hex8,
        "M10-Hex Scattergun (Shotgun)",
        11.0f, 0.55f, 8, 2.10f, 8, 0.075f, 4.5f,
        {1.00f, 0.68f, 0.18f, 1.0f}
    },
    {
        WeaponClass::Sniper_Apex50,
        "AWP-Hex50 Rail Sniper",
        85.0f, 1.15f, 5, 2.40f, 1, 0.001f, 6.0f,
        {0.35f, 1.00f, 0.45f, 1.0f}
    },
    {
        WeaponClass::Pistol_Sidearm,
        "PX-Sidearm (Pistol)",
        24.0f, 0.18f, 15, 1.05f, 1, 0.010f, 1.8f,
        {0.95f, 0.92f, 0.42f, 1.0f}
    }
};

const char* kBotNames[11] = {
    "Misfit_Valkyrie01",
    "HexRunner_42",
    "BlueCobalt_07",
    "ApexGlider_99",
    "CyberPulse_14",
    "SovereignHex_88",
    "CrimsonFang_21",
    "ZeroGravity_05",
    "TowerClimber_63",
    "ShieldSoda_19",
    "NeonSpectre_77"
};

float pseudoRand01(uint32_t& seed) {
    seed = seed * 1664525u + 1013904223u;
    return static_cast<float>((seed >> 8) & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

} // namespace

BattleEngine::BattleEngine() = default;

const char* BattleEngine::getTacticalBuildingRoadmapBadgeText() {
    return "TACTICAL WALL BUILDING: ROADMAP (v2) - NOT ACTIVE IN v1 ARENA";
}

const WeaponSpec& BattleEngine::getWeaponSpec(WeaponClass wc) {
    size_t idx = static_cast<size_t>(wc);
    if (idx >= 5) return kWeapons[0];
    return kWeapons[idx];
}

void BattleEngine::initialize(const std::string& storagePath) {
    customs_.initializeBuiltInMaps(storagePath);
    startMatch(customs_.getHostedRoom().rules);
}

void BattleEngine::startMatch(const CustomRoomRules& rules) {
    customs_.getHostedRoom().rules = rules;
    matchTimeRemainingSec_ = static_cast<float>(rules.timeLimitSeconds);
    squadAlphaKills_ = 0;
    squadOmegaKills_ = 0;
    matchFinished_ = false;
    tracers_.clear();
    damagePopups_.clear();
    killFeed_.clear();
    combatants_.clear();

    // Index 0: Local VR Player (Defaults to Base Male with signature Blue Hex Base; customizable in Locker)
    CombatantState player;
    player.id = 1;
    player.displayName = "You (Misfit VR)";
    player.isBot = false;
    player.appearance.gender = BodyGender::BaseMale;
    player.appearance.skinLayer = SkinLayerId::None_DefaultBlue;
    player.appearance.teamId = (rules.mode == BattleModeType::SquadBattle_Arena) ? 1 : 0;
    respawnCombatant(player, 0);
    combatants_.push_back(player);

    // Spawn AI Misfit Bots with alternating Base Male & Base Female hexagonal models + layered skins
    uint8_t numBots = std::min<uint8_t>(rules.botCount, 11);
    for (uint8_t i = 0; i < numBots; ++i) {
        CombatantState bot;
        bot.id = 2u + i;
        bot.displayName = kBotNames[i];
        bot.isBot = true;
        bot.appearance.gender = (i % 2 == 0) ? BodyGender::BaseFemale : BodyGender::BaseMale;
        bot.appearance.skinLayer = static_cast<SkinLayerId>((i + 1) % 6);
        if (rules.mode == BattleModeType::SquadBattle_Arena) {
            bot.appearance.teamId = (i % 2 == 0) ? 2 : 1;
        } else {
            bot.appearance.teamId = 0;
        }
        respawnCombatant(bot, i + 1);
        combatants_.push_back(bot);
    }
}

void BattleEngine::respawnCombatant(CombatantState& c, uint32_t index) {
    const CustomRoomRules& rules = customs_.getHostedRoom().rules;
    c.health = rules.maxHealth;
    c.shield = rules.maxShield * 0.5f;
    c.alive = true;
    c.respawnTimerSec = 0.0f;
    c.velocity = {0.0f, 0.0f, 0.0f};
    c.onGround = true;
    c.isGliding = false;
    c.isClimbingLeft = false;
    c.isClimbingRight = false;

    switch (rules.loadout) {
        case LoadoutPreset::AllWeapons_Balanced:
            c.equippedWeapon = static_cast<WeaponClass>(index % 4);
            break;
        case LoadoutPreset::SniperAndShotty:
            c.equippedWeapon = (index % 2 == 0) ? WeaponClass::Sniper_Apex50 : WeaponClass::Shotgun_Hex8;
            break;
        case LoadoutPreset::SMGRush:
            c.equippedWeapon = WeaponClass::SubmachineGun_V9;
            break;
        case LoadoutPreset::PistolsOnly:
            c.equippedWeapon = WeaponClass::Pistol_Sidearm;
            break;
    }
    c.currentAmmo = getWeaponSpec(c.equippedWeapon).magazineSize;
    c.reloadTimerSec = 0.0f;
    c.fireCooldownSec = 0.0f;

    // Locate matching spawn pad in active custom map
    const auto& objs = customs_.getActiveMap().objects;
    std::vector<Vec3> candidateSpawns;
    for (const auto& o : objs) {
        if (rules.mode == BattleModeType::SquadBattle_Arena) {
            if (c.appearance.teamId == 1 && o.type == MapAssetType::SpawnSquadAlpha) {
                candidateSpawns.push_back(o.position);
            } else if (c.appearance.teamId == 2 && o.type == MapAssetType::SpawnSquadOmega) {
                candidateSpawns.push_back(o.position);
            }
        }
        if (o.type == MapAssetType::SpawnFFA) {
            candidateSpawns.push_back(o.position);
        }
    }

    if (!candidateSpawns.empty()) {
        Vec3 sp = candidateSpawns[index % candidateSpawns.size()];
        c.footPosition = {sp.x, sp.y + 0.05f, sp.z};
    } else {
        float ang = static_cast<float>(index) * (2.0f * kPi / 6.0f);
        c.footPosition = {std::cos(ang) * 14.0f, 0.0f, std::sin(ang) * 14.0f};
    }
}

void BattleEngine::cyclePlayerWeapon() {
    auto& p = combatants_[0];
    uint8_t next = (static_cast<uint8_t>(p.equippedWeapon) + 1) % 5;
    p.equippedWeapon = static_cast<WeaponClass>(next);
    p.currentAmmo = getWeaponSpec(p.equippedWeapon).magazineSize;
    p.reloadTimerSec = 0.0f;
}

void BattleEngine::cyclePlayerGender() {
    auto& p = combatants_[0];
    p.appearance.gender = (p.appearance.gender == BodyGender::BaseMale)
        ? BodyGender::BaseFemale
        : BodyGender::BaseMale;
}

void BattleEngine::cyclePlayerSkin() {
    auto& p = combatants_[0];
    uint8_t next = (static_cast<uint8_t>(p.appearance.skinLayer) + 1) % 6;
    p.appearance.skinLayer = static_cast<SkinLayerId>(next);
}

bool BattleEngine::isHandNearClimbableSurface(const Vec3& handWorldPos, float grabRadius) const {
    for (const auto& obj : customs_.getActiveMap().objects) {
        if (!obj.climbable) continue;
        AABB bounds = obj.computeWorldBounds();
        if (bounds.contains(handWorldPos, grabRadius)) {
            return true;
        }
    }
    return false;
}

float BattleEngine::sampleGroundAndPlatformHeight(const Vec3& footPos, float currentY, bool& outHitJumpPad) const {
    outHitJumpPad = false;
    float bestGroundY = 0.0f;

    for (const auto& obj : customs_.getActiveMap().objects) {
        AABB b = obj.computeWorldBounds();
        if (footPos.x >= b.min.x - 0.30f && footPos.x <= b.max.x + 0.30f &&
            footPos.z >= b.min.z - 0.30f && footPos.z <= b.max.z + 0.30f) {
            if (obj.type == MapAssetType::JumpPad && std::fabs(currentY - b.min.y) < 1.2f) {
                outHitJumpPad = true;
            }
            if (b.max.y <= currentY + 0.55f && b.max.y > bestGroundY) {
                bestGroundY = b.max.y;
            }
        }
    }
    return bestGroundY;
}

void BattleEngine::tick(float dt, const VRControllerInput& input) {
    dt = std::clamp(dt, 0.001f, 0.1f);

    // Update transient visual effects
    for (auto& t : tracers_) t.remainingSec -= dt;
    tracers_.erase(std::remove_if(tracers_.begin(), tracers_.end(),
                                  [](const TracerBeam& b) { return b.remainingSec <= 0.0f; }),
                   tracers_.end());

    for (auto& d : damagePopups_) {
        d.remainingSec -= dt;
        d.worldPos.y += dt * 0.85f;
    }
    damagePopups_.erase(std::remove_if(damagePopups_.begin(), damagePopups_.end(),
                                       [](const DamagePopup& p) { return p.remainingSec <= 0.0f; }),
                        damagePopups_.end());

    for (auto& k : killFeed_) k.remainingSec -= dt;
    killFeed_.erase(std::remove_if(killFeed_.begin(), killFeed_.end(),
                                   [](const KillFeedItem& k) { return k.remainingSec <= 0.0f; }),
                    killFeed_.end());

    // Controller edge triggers for mode switching, locker skins, and map editor
    bool risingY = input.buttonYPressed && !prevBtnY_;
    bool risingX = input.buttonXPressed && !prevBtnX_;
    bool risingB = input.buttonBPressed && !prevBtnB_;

    if (risingY) {
        // Cycle through: In-Game Battle -> Custom Map Editor -> Character Locker -> Custom Room Host -> Building v2 Roadmap
        uint8_t nextTab = (static_cast<uint8_t>(activeMenuTab_) + 1) % 6;
        activeMenuTab_ = static_cast<ActiveMenuTab>(nextTab);
        editorState_.editorActive = (activeMenuTab_ == ActiveMenuTab::CustomMapEditor);
    }

    if (activeMenuTab_ == ActiveMenuTab::CharacterLocker) {
        if (risingX) cyclePlayerSkin();
        if (risingB) cyclePlayerGender();
    } else if (activeMenuTab_ == ActiveMenuTab::CustomMapEditor) {
        tickMapEditorInteraction(dt, input);
    } else if (activeMenuTab_ == ActiveMenuTab::CommunityMapBrowser) {
        if (risingB) {
            size_t nextMap = (customs_.getActiveMapIndex() + 1) % customs_.getMaps().size();
            customs_.selectMap(nextMap);
            startMatch(customs_.getHostedRoom().rules);
        }
        if (risingX) {
            customs_.upvoteMap(customs_.getActiveMapIndex());
        }
    } else if (activeMenuTab_ == ActiveMenuTab::HostCustomRoom) {
        if (risingB) {
            auto& rules = customs_.getHostedRoom().rules;
            rules.mode = (rules.mode == BattleModeType::FreeForAll_Arena)
                ? BattleModeType::SquadBattle_Arena
                : BattleModeType::FreeForAll_Arena;
            startMatch(rules);
        }
        if (risingX) {
            auto& rules = customs_.getHostedRoom().rules;
            rules.gravityMultiplier = (rules.gravityMultiplier < 0.9f) ? 1.0f : 0.45f;
        }
    }

    // Match timer & respawn loop
    if (!matchFinished_ && !editorState_.editorActive) {
        matchTimeRemainingSec_ -= dt;
        if (matchTimeRemainingSec_ <= 0.0f) {
            matchTimeRemainingSec_ = 0.0f;
            matchFinished_ = true;
        }
    }

    for (size_t i = 0; i < combatants_.size(); ++i) {
        auto& c = combatants_[i];
        if (!c.alive) {
            c.respawnTimerSec -= dt;
            if (c.respawnTimerSec <= 0.0f) {
                respawnCombatant(c, static_cast<uint32_t>(i));
            }
        }
    }

    tickLocalPlayerVCSAndMovement(dt, input);
    if (!editorState_.editorActive) {
        tickWeaponsAndShooting(dt, input);
        tickBotsAI(dt);
        tickPickupsAndSpawners(dt);
    }

    customs_.tickLanNetwork(dt, combatants_[0].footPosition, combatants_[0].yawRad, combatants_[0].kills);

    prevBtnA_ = input.buttonAPressed;
    prevBtnB_ = input.buttonBPressed;
    prevBtnX_ = input.buttonXPressed;
    prevBtnY_ = input.buttonYPressed;
    prevRightTrig_ = (input.rightTrigger > 0.65f);
}

void BattleEngine::tickLocalPlayerVCSAndMovement(float dt, const VRControllerInput& input) {
    CombatantState& player = combatants_[0];
    if (!player.alive) return;

    const CustomRoomRules& rules = customs_.getHostedRoom().rules;

    // 1. Snap Turn on Right Thumbstick X
    snapTurnCooldown_ = std::max(0.0f, snapTurnCooldown_ - dt);
    if (std::fabs(input.rightThumbstick.x) > 0.65f && snapTurnCooldown_ <= 0.0f) {
        float sign = (input.rightThumbstick.x > 0.0f) ? -1.0f : 1.0f;
        player.yawRad += sign * (30.0f * kDegToRad);
        snapTurnCooldown_ = 0.28f;
    }

    Quat playerYawQ = Quat::fromAxisAngle({0.0f, 1.0f, 0.0f}, player.yawRad);
    Vec3 worldLeftHand  = player.footPosition + playerYawQ.rotate(input.leftHandPos);
    Vec3 worldRightHand = player.footPosition + playerYawQ.rotate(input.rightHandPos);
    Vec3 worldHead      = player.footPosition + playerYawQ.rotate(input.headPos);

    // 2. VCS Climbing Mechanic (Meta Quest Left/Right Grip on any climbable surface)
    bool wantGrabLeft  = (input.leftGrip > 0.60f);
    bool wantGrabRight = (input.rightGrip > 0.60f);

    if (wantGrabLeft) {
        if (!player.isClimbingLeft && isHandNearClimbableSurface(worldLeftHand, 0.50f)) {
            player.isClimbingLeft = true;
            player.leftClimbAnchor = worldLeftHand;
            player.velocity = {0.0f, 0.0f, 0.0f};
        }
    } else {
        if (player.isClimbingLeft) {
            // One-handed fling / pull-up momentum on release
            player.velocity.y = std::max(player.velocity.y, 4.8f);
        }
        player.isClimbingLeft = false;
    }

    if (wantGrabRight) {
        if (!player.isClimbingRight && isHandNearClimbableSurface(worldRightHand, 0.50f)) {
            player.isClimbingRight = true;
            player.rightClimbAnchor = worldRightHand;
            player.velocity = {0.0f, 0.0f, 0.0f};
        }
    } else {
        if (player.isClimbingRight) {
            player.velocity.y = std::max(player.velocity.y, 4.8f);
        }
        player.isClimbingRight = false;
    }

    bool climbingNow = (player.isClimbingLeft || player.isClimbingRight);
    if (climbingNow) {
        player.isGliding = false;
        player.onGround = false;
        if (player.isClimbingLeft) {
            Vec3 pullDelta = player.leftClimbAnchor - worldLeftHand;
            player.footPosition += pullDelta;
        } else if (player.isClimbingRight) {
            Vec3 pullDelta = player.rightClimbAnchor - worldRightHand;
            player.footPosition += pullDelta;
        }
        return;
    }

    // 3. VCS Gliding Mechanic:
    // Triggered when airborne and either arms are extended outward (wingspan > 0.90m / T-pose)
    // OR player holds Left Trigger (glide stabilizer deploy).
    float leftSpread  = (worldLeftHand - Vec3{worldHead.x, worldLeftHand.y, worldHead.z}).length();
    float rightSpread = (worldRightHand - Vec3{worldHead.x, worldRightHand.y, worldHead.z}).length();
    bool armsOutstretched = (leftSpread > 0.45f && rightSpread > 0.45f);
    bool glideTriggerHeld = (input.leftTrigger > 0.55f);

    if (!player.onGround && (armsOutstretched || glideTriggerHeld)) {
        player.isGliding = true;
    } else if (player.onGround) {
        player.isGliding = false;
    }

    // 4. Locomotion & Aerodynamics
    Quat headWorldQ = playerYawQ * input.headRot;
    Vec3 forwardFlat = headWorldQ.forward();
    forwardFlat.y = 0.0f;
    forwardFlat = forwardFlat.normalized();
    Vec3 rightFlat = cross(forwardFlat, {0.0f, 1.0f, 0.0f}).normalized();

    if (player.isGliding) {
        float glideSpeed = 8.4f * rules.glideSpeedMultiplier;
        Vec3 glideDir = forwardFlat;
        if (glideDir.lengthSq() < 1e-4f) glideDir = {0.0f, 0.0f, -1.0f};
        player.velocity.x = glideDir.x * glideSpeed;
        player.velocity.z = glideDir.z * glideSpeed;
        // Controlled low-sink descent rate (-2.1 m/s)
        player.velocity.y = std::max(player.velocity.y - 4.0f * dt, -2.1f * rules.gravityMultiplier);
    } else {
        float moveSpeed = 5.6f * rules.moveSpeedMultiplier;
        Vec3 wishVel = (forwardFlat * input.leftThumbstick.y + rightFlat * input.leftThumbstick.x) * moveSpeed;
        player.velocity.x = wishVel.x;
        player.velocity.z = wishVel.z;

        if (wishVel.lengthSq() > 0.2f && player.onGround) {
            player.walkPhase += dt * 8.0f;
        }

        // Jump on A button press
        if (input.buttonAPressed && !prevBtnA_ && player.onGround) {
            player.velocity.y = 6.8f;
            player.onGround = false;
        }

        // Gravity
        float grav = 14.5f * rules.gravityMultiplier;
        player.velocity.y -= grav * dt;
    }

    player.footPosition += player.velocity * dt;

    // 5. Ground, Platform, & JumpPad Collision
    bool hitJumpPad = false;
    float floorY = sampleGroundAndPlatformHeight(player.footPosition, player.footPosition.y, hitJumpPad);
    if (hitJumpPad) {
        player.velocity.y = 14.8f; // High-impulse vertical launch into glide altitude
        player.onGround = false;
    } else if (player.footPosition.y <= floorY) {
        player.footPosition.y = floorY;
        player.velocity.y = 0.0f;
        player.onGround = true;
        player.isGliding = false;
    } else {
        player.onGround = false;
    }

    // Keep within Arena bounds (-28m to +28m)
    player.footPosition.x = std::clamp(player.footPosition.x, -28.0f, 28.0f);
    player.footPosition.z = std::clamp(player.footPosition.z, -28.0f, 28.0f);
}

void BattleEngine::tickWeaponsAndShooting(float dt, const VRControllerInput& input) {
    CombatantState& player = combatants_[0];
    if (!player.alive) return;

    const WeaponSpec& spec = getWeaponSpec(player.equippedWeapon);
    player.fireCooldownSec = std::max(0.0f, player.fireCooldownSec - dt);
    player.recoilAccumDeg  = std::max(0.0f, player.recoilAccumDeg - dt * 12.0f);

    if (activeMenuTab_ == ActiveMenuTab::None_InGameHUD) {
        if (input.buttonBPressed && !prevBtnB_) {
            cyclePlayerWeapon();
        }
        if (input.buttonXPressed && !prevBtnX_ && player.reloadTimerSec <= 0.0f && player.currentAmmo < spec.magazineSize) {
            player.reloadTimerSec = customs_.getHostedRoom().rules.instantReload ? 0.05f : spec.reloadDurationSec;
        }
    }

    if (player.reloadTimerSec > 0.0f) {
        player.reloadTimerSec -= dt;
        if (player.reloadTimerSec <= 0.0f) {
            player.reloadTimerSec = 0.0f;
            player.currentAmmo = spec.magazineSize;
        }
        return;
    }

    if (input.rightTrigger > 0.60f && player.fireCooldownSec <= 0.0f && activeMenuTab_ == ActiveMenuTab::None_InGameHUD) {
        if (player.currentAmmo == 0) {
            player.reloadTimerSec = customs_.getHostedRoom().rules.instantReload ? 0.05f : spec.reloadDurationSec;
            return;
        }

        Quat playerYawQ = Quat::fromAxisAngle({0.0f, 1.0f, 0.0f}, player.yawRad);
        Vec3 muzzleWorld = player.footPosition + playerYawQ.rotate(input.rightHandPos);
        Quat aimQ = playerYawQ * input.rightHandRot;
        Vec3 aimDir = aimQ.forward();

        fireWeapon(player, muzzleWorld, aimDir);
    }
}

void BattleEngine::fireWeapon(CombatantState& shooter, const Vec3& muzzleOrigin, const Vec3& aimDir) {
    const WeaponSpec& spec = getWeaponSpec(shooter.equippedWeapon);
    shooter.fireCooldownSec = spec.fireIntervalSec;
    if (!customs_.getHostedRoom().rules.infiniteAmmo && shooter.currentAmmo > 0) {
        shooter.currentAmmo--;
    }
    shooter.recoilAccumDeg = std::min(15.0f, shooter.recoilAccumDeg + spec.recoilKickDeg);

    uint32_t seed = shooter.id * 9781u + shooter.currentAmmo * 313u + static_cast<uint32_t>(matchTimeRemainingSec_ * 100.0f);

    for (uint8_t pellet = 0; pellet < spec.pelletsPerShot; ++pellet) {
        float rx = (pseudoRand01(seed) - 0.5f) * 2.0f * spec.spreadRadians;
        float ry = (pseudoRand01(seed) - 0.5f) * 2.0f * spec.spreadRadians;
        Quat spreadQ = Quat::fromEulerYXZ(rx, ry, 0.0f);
        Vec3 shotDir = spreadQ.rotate(aimDir).normalized();

        float bestDist = 65.0f;

        // 1. Check world obstacle occlusion
        for (const auto& obj : customs_.getActiveMap().objects) {
            if (!obj.climbable) continue;
            AABB b = obj.computeWorldBounds();
            float tHit = 0.0f;
            Vec3 nHit;
            if (b.intersectsRay(muzzleOrigin, shotDir, bestDist, tHit, nHit)) {
                bestDist = tHit;
            }
        }

        // 2. Check Standardized Hitbox across all combatants
        // Per Population: One's verified hitbox architecture, Base Male and Base Female models
        // share the exact same standardized 0.72m x 1.78m x 0.72m hitbox collider regardless of skin layer.
        int hitCombatantIdx = -1;
        bool isHeadshot = false;
        for (size_t i = 0; i < combatants_.size(); ++i) {
            CombatantState& target = combatants_[i];
            if (!target.alive || target.id == shooter.id) continue;
            if (customs_.getHostedRoom().rules.mode == BattleModeType::SquadBattle_Arena &&
                shooter.appearance.teamId != 0 &&
                shooter.appearance.teamId == target.appearance.teamId) {
                continue; // Friendly fire disabled in Squad Arena
            }

            AABB standardizedHitbox{
                {target.footPosition.x - 0.36f, target.footPosition.y,        target.footPosition.z - 0.36f},
                {target.footPosition.x + 0.36f, target.footPosition.y + 1.78f, target.footPosition.z + 0.36f}
            };
            float tHit = 0.0f;
            Vec3 nHit;
            if (standardizedHitbox.intersectsRay(muzzleOrigin, shotDir, bestDist, tHit, nHit)) {
                bestDist = tHit;
                hitCombatantIdx = static_cast<int>(i);
                Vec3 hitPoint = muzzleOrigin + shotDir * tHit;
                isHeadshot = (hitPoint.y >= target.footPosition.y + 1.45f);
            }
        }

        Vec3 endPoint = muzzleOrigin + shotDir * bestDist;
        tracers_.push_back({muzzleOrigin, endPoint, spec.tracerColor, 0.11f});

        if (hitCombatantIdx >= 0) {
            CombatantState& victim = combatants_[hitCombatantIdx];
            float dmg = spec.damagePerHit * (isHeadshot ? 1.5f : 1.0f);
            bool hitShield = (victim.shield > 0.0f);

            if (victim.shield >= dmg) {
                victim.shield -= dmg;
            } else {
                float leftover = dmg - victim.shield;
                victim.shield = 0.0f;
                victim.health = std::max(0.0f, victim.health - leftover);
            }

            damagePopups_.push_back({
                endPoint + Vec3{0.0f, 0.25f, 0.0f},
                static_cast<int32_t>(std::round(dmg)),
                hitShield,
                0.85f
            });

            if (victim.health <= 0.0f && victim.alive) {
                victim.alive = false;
                victim.deaths++;
                victim.respawnTimerSec = 3.5f;
                shooter.kills++;

                if (shooter.appearance.teamId == 1) squadAlphaKills_++;
                else if (shooter.appearance.teamId == 2) squadOmegaKills_++;

                killFeed_.push_back({shooter.displayName, victim.displayName, shooter.equippedWeapon, 5.0f});
                if (killFeed_.size() > 4) killFeed_.erase(killFeed_.begin());

                uint16_t limit = customs_.getHostedRoom().rules.scoreLimitKills;
                if (shooter.kills >= limit || squadAlphaKills_ >= limit || squadOmegaKills_ >= limit) {
                    matchFinished_ = true;
                }
            }
        }
    }
}

void BattleEngine::tickBotsAI(float dt) {
    if (matchFinished_) return;
    const auto& objs = customs_.getActiveMap().objects;

    for (size_t i = 1; i < combatants_.size(); ++i) {
        CombatantState& bot = combatants_[i];
        if (!bot.alive) continue;

        bot.fireCooldownSec = std::max(0.0f, bot.fireCooldownSec - dt);
        bot.aiDecisionTimer -= dt;

        // Find nearest enemy combatant
        int nearestEnemy = -1;
        float nearestDistSq = 99999.0f;
        for (size_t j = 0; j < combatants_.size(); ++j) {
            if (i == j || !combatants_[j].alive) continue;
            if (customs_.getHostedRoom().rules.mode == BattleModeType::SquadBattle_Arena &&
                bot.appearance.teamId == combatants_[j].appearance.teamId) {
                continue;
            }
            float dSq = (combatants_[j].footPosition - bot.footPosition).lengthSq();
            if (dSq < nearestDistSq) {
                nearestDistSq = dSq;
                nearestEnemy = static_cast<int>(j);
            }
        }

        if (nearestEnemy >= 0) {
            const CombatantState& enemy = combatants_[nearestEnemy];
            Vec3 toEnemy = enemy.footPosition - bot.footPosition;
            bot.yawRad = std::atan2(-toEnemy.x, -toEnemy.z);

            Vec3 moveDir = {toEnemy.x, 0.0f, toEnemy.z};
            float flatDist = moveDir.length();
            if (flatDist > 7.5f) {
                moveDir = moveDir / flatDist;
                bot.footPosition.x += moveDir.x * 4.2f * dt;
                bot.footPosition.z += moveDir.z * 4.2f * dt;
                bot.walkPhase += dt * 7.0f;
            }

            // Bots periodically climb/jump onto spires and glide down
            if (bot.aiDecisionTimer <= 0.0f) {
                bot.aiDecisionTimer = 2.2f + static_cast<float>(i % 3) * 0.6f;
                if (bot.onGround && (i % 2 == 0)) {
                    bot.velocity.y = 8.5f;
                    bot.onGround = false;
                }
            }

            if (!bot.onGround) {
                bot.isGliding = (bot.velocity.y < 0.0f && bot.footPosition.y > 2.5f);
                float sink = bot.isGliding ? 3.5f : 13.0f;
                bot.velocity.y -= sink * dt;
                bot.footPosition.y += bot.velocity.y * dt;
            }

            bool hitPad = false;
            float groundY = sampleGroundAndPlatformHeight(bot.footPosition, bot.footPosition.y, hitPad);
            if (hitPad) {
                bot.velocity.y = 14.0f;
                bot.onGround = false;
            } else if (bot.footPosition.y <= groundY) {
                bot.footPosition.y = groundY;
                bot.velocity.y = 0.0f;
                bot.onGround = true;
                bot.isGliding = false;
            }

            // Bot shoots when in range
            if (flatDist < 32.0f && bot.fireCooldownSec <= 0.0f) {
                Vec3 botMuzzle = bot.footPosition + Vec3{0.0f, 1.35f, 0.0f};
                Vec3 targetChest = enemy.footPosition + Vec3{0.0f, 1.25f, 0.0f};
                Vec3 aim = (targetChest - botMuzzle).normalized();
                // Add slight bot aim variance so combat feels fair and fun
                uint32_t seed = bot.id * 4099u + static_cast<uint32_t>(matchTimeRemainingSec_ * 50.0f);
                Quat jitter = Quat::fromEulerYXZ((pseudoRand01(seed) - 0.5f) * 0.09f,
                                                 (pseudoRand01(seed) - 0.5f) * 0.09f, 0.0f);
                fireWeapon(bot, botMuzzle, jitter.rotate(aim));
                bot.fireCooldownSec = std::max(bot.fireCooldownSec, 0.45f);
            }
        }
    }
    (void)objs;
}

void BattleEngine::tickPickupsAndSpawners(float dt) {
    (void)dt;
    CombatantState& player = combatants_[0];
    if (!player.alive) return;

    const CustomRoomRules& rules = customs_.getHostedRoom().rules;
    for (const auto& obj : customs_.getActiveMap().objects) {
        float distSq = (obj.position - player.footPosition).lengthSq();
        if (distSq > 2.2f * 2.2f) continue;

        if (obj.type == MapAssetType::ShieldSodaSpawner) {
            if (obj.subType == 0 && player.shield < rules.maxShield) {
                player.shield = std::min(rules.maxShield, player.shield + 25.0f * dt);
            } else if (obj.subType == 1 && player.health < rules.maxHealth) {
                player.health = std::min(rules.maxHealth, player.health + 25.0f * dt);
            }
        } else if (obj.type == MapAssetType::WeaponSpawner) {
            WeaponClass wc = static_cast<WeaponClass>(obj.subType % 5);
            if (player.equippedWeapon != wc) {
                player.equippedWeapon = wc;
                player.currentAmmo = getWeaponSpec(wc).magazineSize;
            }
        }
    }
}

void BattleEngine::tickMapEditorInteraction(float dt, const VRControllerInput& input) {
    CombatantState& player = combatants_[0];
    Quat playerYawQ = Quat::fromAxisAngle({0.0f, 1.0f, 0.0f}, player.yawRad);
    Vec3 rayOrigin = player.footPosition + playerYawQ.rotate(input.rightHandPos);
    Vec3 rayDir = (playerYawQ * input.rightHandRot).forward();

    // Find hovered map object along controller laser
    editorState_.hoveredObjectId = -1;
    float bestT = 30.0f;
    for (const auto& obj : customs_.getActiveMap().objects) {
        AABB b = obj.computeWorldBounds();
        float tHit = 0.0f;
        Vec3 nHit;
        if (b.intersectsRay(rayOrigin, rayDir, bestT, tHit, nHit)) {
            bestT = tHit;
            editorState_.hoveredObjectId = static_cast<int32_t>(obj.id);
        }
    }

    // Cycle asset type on B button
    if (input.buttonBPressed && !prevBtnB_) {
        uint8_t nextAsset = (static_cast<uint8_t>(editorState_.selectedAsset) + 1) % 13;
        editorState_.selectedAsset = static_cast<MapAssetType>(nextAsset);
    }

    // Cycle material swatch on X button (and recolor hovered object if any)
    if (input.buttonXPressed && !prevBtnX_) {
        uint8_t nextSwatch = (static_cast<uint8_t>(editorState_.selectedSwatch) + 1) % 8;
        editorState_.selectedSwatch = static_cast<MaterialSwatch>(nextSwatch);
        if (editorState_.hoveredObjectId >= 0) {
            customs_.recolorObject(static_cast<uint32_t>(editorState_.hoveredObjectId), editorState_.selectedSwatch);
        }
    }

    // Rotate preview / hovered object with right thumbstick Y
    if (std::fabs(input.rightThumbstick.y) > 0.5f) {
        editorState_.previewYawDeg += input.rightThumbstick.y * 90.0f * dt;
    }

    Vec3 rawTargetPos = rayOrigin + rayDir * editorState_.placementDistance;
    Vec3 snappedPos = customs_.snapPosition(rawTargetPos, editorState_.snapGridSize);

    // Place new object on Right Trigger press
    bool risingTrig = (input.rightTrigger > 0.65f) && !prevRightTrig_;
    if (risingTrig) {
        float snappedYaw = std::round(editorState_.previewYawDeg / editorState_.snapYawStep) * editorState_.snapYawStep;
        customs_.placeObject(editorState_.selectedAsset, editorState_.selectedSwatch,
                             snappedPos, snappedYaw, editorState_.previewScale);
    }

    // Delete hovered object on Left Grip + A button
    if (input.buttonAPressed && !prevBtnA_ && editorState_.hoveredObjectId >= 0) {
        if (input.leftGrip > 0.5f) {
            customs_.deleteObject(static_cast<uint32_t>(editorState_.hoveredObjectId));
        } else {
            customs_.duplicateObject(static_cast<uint32_t>(editorState_.hoveredObjectId), {editorState_.snapGridSize * 2.0f, 0.0f, 0.0f});
        }
    }
}

void BattleEngine::buildFrameGeometry(
    const VRControllerInput& input,
    MeshData& outWorldMesh,
    MeshData& outHudMesh
) const {
    outWorldMesh.clear();
    outHudMesh.clear();

    // 1. Hexagonal Arena Floor Grid (-30m to +30m)
    Color4 floorCol{0.09f, 0.13f, 0.20f, 1.0f};
    CharacterSystem::appendHexPrism(outWorldMesh, {0.0f, -0.4f, 0.0f}, {0.0f, 0.0f, 0.0f},
                                    32.0f, 32.0f, 32.0f, 32.0f, floorCol, 2);

    // Subtle hexagonal floor ring markers
    for (int r = 8; r <= 24; r += 8) {
        Color4 ringCol{0.12f, 0.26f, 0.45f, 1.0f};
        CharacterSystem::appendHexPrism(outWorldMesh, {0.0f, -0.02f, 0.0f}, {0.0f, 0.02f, 0.0f},
                                        static_cast<float>(r), static_cast<float>(r),
                                        static_cast<float>(r), static_cast<float>(r), ringCol, 1);
    }

    // 2. Active Custom Map Objects
    for (const auto& obj : customs_.getActiveMap().objects) {
        bool isHovered = editorState_.editorActive && (static_cast<int32_t>(obj.id) == editorState_.hoveredObjectId);
        CustomsSystem::appendMapObjectGeometry(outWorldMesh, obj, isHovered);
    }

    // 3. Map Editor Ghost Placement Preview + Laser Ray
    const CombatantState& localPlayer = combatants_[0];
    Quat playerYawQ = Quat::fromAxisAngle({0.0f, 1.0f, 0.0f}, localPlayer.yawRad);
    Vec3 worldLeftHand  = localPlayer.footPosition + playerYawQ.rotate(input.leftHandPos);
    Vec3 worldRightHand = localPlayer.footPosition + playerYawQ.rotate(input.rightHandPos);
    Quat worldRightRot  = playerYawQ * input.rightHandRot;

    if (editorState_.editorActive) {
        Vec3 previewPos = customs_.snapPosition(
            worldRightHand + worldRightRot.forward() * editorState_.placementDistance,
            editorState_.snapGridSize
        );
        MapObject ghost;
        ghost.type = editorState_.selectedAsset;
        ghost.material = editorState_.selectedSwatch;
        ghost.position = previewPos;
        ghost.yawDegrees = editorState_.previewYawDeg;
        ghost.scale = editorState_.previewScale;
        CustomsSystem::appendMapObjectGeometry(outWorldMesh, ghost, true);

        // Editor laser beam
        CharacterSystem::appendHexPrism(outWorldMesh, worldRightHand, previewPos,
                                        0.012f, 0.012f, 0.012f, 0.012f,
                                        {0.2f, 1.0f, 0.6f, 1.0f}, 7, false);
    }

    // 4. Render Local Player Body (First-person IK arms & torso, head hidden) + All AI Bots / Peers
    for (size_t i = 0; i < combatants_.size(); ++i) {
        const CombatantState& c = combatants_[i];
        if (!c.alive) {
            // Downed / Respawning Ghost state: translucent cyan-blue low-poly hexagonal crystalline figure
            CharacterAppearance ghostApp = c.appearance;
            ghostApp.skinLayer = SkinLayerId::None_DefaultBlue;
            ArticulatedPose ghostPose;
            ghostPose.rootPos = c.footPosition;
            ghostPose.bodyYawRad = c.yawRad;
            ghostPose.leftHandWorld  = c.footPosition + Vec3{-0.3f, 1.1f, -0.2f};
            ghostPose.rightHandWorld = c.footPosition + Vec3{ 0.3f, 1.1f, -0.2f};
            MeshData gm = CharacterSystem::buildCharacterMesh(ghostApp, ghostPose, i == 0);
            for (auto& v : gm.vertices) {
                v.color = {0.25f, 0.88f, 1.0f, 0.55f};
            }
            uint32_t base = static_cast<uint32_t>(outWorldMesh.vertices.size());
            outWorldMesh.vertices.insert(outWorldMesh.vertices.end(), gm.vertices.begin(), gm.vertices.end());
            for (uint32_t idx : gm.indices) outWorldMesh.indices.push_back(base + idx);
            continue;
        }

        ArticulatedPose pose;
        pose.rootPos = c.footPosition;
        pose.bodyYawRad = c.yawRad;
        pose.isGliding = c.isGliding;
        pose.isClimbing = (c.isClimbingLeft || c.isClimbingRight);
        pose.walkCyclePhase = c.walkPhase;

        if (i == 0) {
            pose.leftHandWorld  = worldLeftHand;
            pose.rightHandWorld = worldRightHand;
        } else {
            Quat botYawQ = Quat::fromAxisAngle({0.0f, 1.0f, 0.0f}, c.yawRad);
            Vec3 lLocal = c.isGliding ? Vec3{-0.62f, 1.38f, 0.0f} : Vec3{-0.24f, 1.22f, -0.32f};
            Vec3 rLocal = c.isGliding ? Vec3{ 0.62f, 1.38f, 0.0f} : Vec3{ 0.24f, 1.25f, -0.38f};
            pose.leftHandWorld  = c.footPosition + botYawQ.rotate(lLocal);
            pose.rightHandWorld = c.footPosition + botYawQ.rotate(rLocal);
        }

        MeshData charMesh = CharacterSystem::buildCharacterMesh(c.appearance, pose, i == 0);
        uint32_t base = static_cast<uint32_t>(outWorldMesh.vertices.size());
        outWorldMesh.vertices.insert(outWorldMesh.vertices.end(), charMesh.vertices.begin(), charMesh.vertices.end());
        for (uint32_t idx : charMesh.indices) outWorldMesh.indices.push_back(base + idx);
    }

    // 5. Render Equipped 3D Hexagonal Weapon in Right Hand
    if (localPlayer.alive) {
        const WeaponSpec& wSpec = getWeaponSpec(localPlayer.equippedWeapon);
        Vec3 gunButt = worldRightHand + worldRightRot.forward() * -0.06f;
        Vec3 gunMuzzle = worldRightHand + worldRightRot.forward() * 0.38f;
        CharacterSystem::appendHexPrism(outWorldMesh, gunButt, gunMuzzle,
                                        0.032f, 0.045f, 0.024f, 0.035f,
                                        {0.20f, 0.24f, 0.30f, 1.0f}, 7);
        CharacterSystem::appendHexPrism(outWorldMesh, gunMuzzle, gunMuzzle + worldRightRot.forward() * 0.08f,
                                        0.018f, 0.018f, 0.018f, 0.018f,
                                        wSpec.tracerColor, 7);
    }

    // 6. Active Weapon Tracers
    for (const auto& tr : tracers_) {
        CharacterSystem::appendHexPrism(outWorldMesh, tr.start, tr.end,
                                        0.016f, 0.016f, 0.010f, 0.010f,
                                        tr.color, 7, false);
    }

    // 7. Wrist & Spatial VR HUD Bars (Health, Shield, Ammo, Mode / Roadmap V2 Badge, Locker 3D Mannequin Preview)
    Quat headWorldQ = playerYawQ * input.headRot;
    Vec3 hudCenter = localPlayer.footPosition + playerYawQ.rotate(input.headPos) + headWorldQ.forward() * 1.45f;
    Vec3 hudRight  = headWorldQ.right();
    Vec3 hudUp     = headWorldQ.up();

    // Health Bar (Emerald Green) & Shield Bar (Cyan Blue) at bottom of spatial visor
    float hpFrac = std::clamp(localPlayer.health / std::max(1.0f, customs_.getHostedRoom().rules.maxHealth), 0.05f, 1.0f);
    float shFrac = std::clamp(localPlayer.shield / std::max(1.0f, customs_.getHostedRoom().rules.maxShield), 0.02f, 1.0f);

    Vec3 barLeft = hudCenter - hudRight * 0.28f - hudUp * 0.34f;
    CharacterSystem::appendHexPrism(outHudMesh, barLeft, barLeft + hudRight * (0.56f * hpFrac),
                                    0.014f, 0.014f, 0.014f, 0.014f,
                                    {0.18f, 0.92f, 0.42f, 1.0f}, 7);
    Vec3 shLeft = barLeft + hudUp * 0.038f;
    CharacterSystem::appendHexPrism(outHudMesh, shLeft, shLeft + hudRight * (0.56f * shFrac),
                                    0.014f, 0.014f, 0.014f, 0.014f,
                                    {0.16f, 0.82f, 1.00f, 1.0f}, 7);

    // Roadmap (v2) Tactical Building indicator pill on top-right HUD
    Vec3 roadmapStart = hudCenter + hudRight * 0.18f + hudUp * 0.36f;
    Vec3 roadmapEnd   = roadmapStart + hudRight * 0.22f;
    CharacterSystem::appendHexPrism(outHudMesh, roadmapStart, roadmapEnd,
                                    0.016f, 0.016f, 0.016f, 0.016f,
                                    {0.95f, 0.65f, 0.15f, 1.0f}, 7);

    // When Character Locker is open, render a full-body 3D turntable mannequin in front of the player
    // so they can inspect the Base Male / Base Female hexagonal blue geometry and layered graphical skins!
    if (activeMenuTab_ == ActiveMenuTab::CharacterLocker) {
        ArticulatedPose previewPose;
        previewPose.rootPos = hudCenter - hudUp * 0.75f + headWorldQ.forward() * 0.45f;
        previewPose.bodyYawRad = localPlayer.yawRad + kPi + matchTimeRemainingSec_ * 0.8f;
        Quat pRot = Quat::fromAxisAngle({0.0f, 1.0f, 0.0f}, previewPose.bodyYawRad);
        previewPose.leftHandWorld  = previewPose.rootPos + pRot.rotate({-0.28f, 1.18f, -0.18f});
        previewPose.rightHandWorld = previewPose.rootPos + pRot.rotate({ 0.28f, 1.18f, -0.18f});
        previewPose.isGliding = false;

        MeshData lockerMesh = CharacterSystem::buildCharacterMesh(localPlayer.appearance, previewPose, false);
        uint32_t base = static_cast<uint32_t>(outHudMesh.vertices.size());
        outHudMesh.vertices.insert(outHudMesh.vertices.end(), lockerMesh.vertices.begin(), lockerMesh.vertices.end());
        for (uint32_t idx : lockerMesh.indices) outHudMesh.indices.push_back(base + idx);
    }
}

} // namespace misfits
