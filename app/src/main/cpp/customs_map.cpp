#include "customs_map.h"
#include <sstream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <cmath>

#if defined(__unix__) || defined(__ANDROID__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace misfits {

AABB MapObject::computeWorldBounds() const {
    Vec3 halfExtents{1.0f, 1.0f, 1.0f};
    switch (type) {
        case MapAssetType::HexPillar:      halfExtents = {1.2f * scale.x, 4.0f * scale.y, 1.2f * scale.z}; break;
        case MapAssetType::ClimbWall:      halfExtents = {3.0f * scale.x, 2.5f * scale.y, 0.5f * scale.z}; break;
        case MapAssetType::RampIncline:    halfExtents = {2.0f * scale.x, 1.5f * scale.y, 3.0f * scale.z}; break;
        case MapAssetType::SniperTower:    halfExtents = {2.2f * scale.x, 7.5f * scale.y, 2.2f * scale.z}; break;
        case MapAssetType::SkyPlatform:    halfExtents = {3.8f * scale.x, 0.4f * scale.y, 3.8f * scale.z}; break;
        case MapAssetType::CoverCrate:     halfExtents = {0.9f * scale.x, 0.9f * scale.y, 0.9f * scale.z}; break;
        case MapAssetType::JumpPad:        halfExtents = {1.1f * scale.x, 0.25f * scale.y, 1.1f * scale.z}; break;
        case MapAssetType::SpawnFFA:
        case MapAssetType::SpawnSquadAlpha:
        case MapAssetType::SpawnSquadOmega:
        case MapAssetType::WeaponSpawner:
        case MapAssetType::ShieldSodaSpawner:
            halfExtents = {0.8f * scale.x, 0.20f * scale.y, 0.8f * scale.z};
            break;
        case MapAssetType::NeonArchway:    halfExtents = {2.8f * scale.x, 3.5f * scale.y, 0.8f * scale.z}; break;
    }

    // Account for 90-degree / arbitrary yaw rotation on horizontal footprint
    float rad = yawDegrees * kDegToRad;
    float c = std::fabs(std::cos(rad));
    float s = std::fabs(std::sin(rad));
    float rx = halfExtents.x * c + halfExtents.z * s;
    float rz = halfExtents.x * s + halfExtents.z * c;

    return {
        {position.x - rx, position.y, position.z - rz},
        {position.x + rx, position.y + halfExtents.y * 2.0f, position.z + rz}
    };
}

Color4 MapObject::getMaterialColor() const {
    switch (material) {
        case MaterialSwatch::CobaltSteel:   return {0.16f, 0.36f, 0.68f, 1.0f};
        case MaterialSwatch::ObsidianHex:   return {0.15f, 0.17f, 0.22f, 1.0f};
        case MaterialSwatch::NeonCyan:      return {0.12f, 0.82f, 0.96f, 1.0f};
        case MaterialSwatch::CrimsonCore:   return {0.84f, 0.18f, 0.24f, 1.0f};
        case MaterialSwatch::SolarGold:     return {0.92f, 0.72f, 0.18f, 1.0f};
        case MaterialSwatch::EmeraldGrid:   return {0.18f, 0.76f, 0.42f, 1.0f};
        case MaterialSwatch::TitaniumWhite: return {0.78f, 0.82f, 0.88f, 1.0f};
        case MaterialSwatch::VioletPulse:   return {0.58f, 0.26f, 0.88f, 1.0f};
    }
    return {0.2f, 0.4f, 0.7f, 1.0f};
}

uint32_t CustomMap::computeComplexityCost() const {
    uint32_t total = 0;
    for (const auto& obj : objects) {
        switch (obj.type) {
            case MapAssetType::SniperTower: total += 45; break;
            case MapAssetType::NeonArchway: total += 35; break;
            case MapAssetType::HexPillar:
            case MapAssetType::ClimbWall:
            case MapAssetType::SkyPlatform: total += 25; break;
            default: total += 15; break;
        }
    }
    return total;
}

CustomsSystem::CustomsSystem() {
    hostedRoom_.roomCode = "MSFT-101";
    hostedRoom_.roomName = "Misfit Arena Official #1";
    hostedRoom_.isPublic = true;
}

CustomsSystem::~CustomsSystem() {
    closeLanSocket();
}

void CustomsSystem::initializeBuiltInMaps(const std::string& storageDirPath) {
    storageDir_ = storageDirPath;
    maps_.clear();
    nextObjectId_ = 100;

    createHexCitadelMap();
    createNeonSpireArenaMap();
    createSkybridgeClashMap();

    activeMapIndex_ = 0;
    hostedRoom_.mapPlaylistIds = {
        maps_[0].mapId,
        maps_[1].mapId,
        maps_[2].mapId
    };
    openLanSocket(hostedRoom_.udpPort);
}

void CustomsSystem::createHexCitadelMap() {
    CustomMap m;
    m.mapId = "hex-citadel-v1";
    m.title = "Hex Citadel";
    m.author = "Population Misfits Studio";
    m.description = "Symmetrical vertical battle citadel with central 15m sniper spire, climbable hex bastions, jump pads, and aerial glide lanes.";
    m.upvotes = 1420;

    auto add = [&](MapAssetType t, MaterialSwatch sw, Vec3 pos, float yaw, Vec3 scl = {1, 1, 1}, uint8_t sub = 0) {
        MapObject o;
        o.id = nextObjectId_++;
        o.type = t;
        o.material = sw;
        o.position = pos;
        o.yawDegrees = yaw;
        o.scale = scl;
        o.climbable = (t != MapAssetType::JumpPad && t != MapAssetType::SpawnFFA &&
                       t != MapAssetType::SpawnSquadAlpha && t != MapAssetType::SpawnSquadOmega &&
                       t != MapAssetType::WeaponSpawner && t != MapAssetType::ShieldSodaSpawner);
        o.subType = sub;
        m.objects.push_back(o);
    };

    // Central Sniper Citadel Spire (15m tall climbable core for high-altitude gliding)
    add(MapAssetType::SniperTower, MaterialSwatch::CobaltSteel, {0.0f, 0.0f, 0.0f}, 0.0f, {1.2f, 1.0f, 1.2f});
    add(MapAssetType::SkyPlatform, MaterialSwatch::NeonCyan, {0.0f, 14.5f, 0.0f}, 0.0f, {1.3f, 1.0f, 1.3f});

    // 6 Surrounding Hexagonal Bastion Pillars + Sky Platforms + Jump Pads
    for (int i = 0; i < 6; ++i) {
        float ang = static_cast<float>(i) * (kPi / 3.0f);
        float px = std::cos(ang) * 16.0f;
        float pz = std::sin(ang) * 16.0f;
        MaterialSwatch sw = (i % 2 == 0) ? MaterialSwatch::ObsidianHex : MaterialSwatch::CobaltSteel;
        add(MapAssetType::HexPillar, sw, {px, 0.0f, pz}, i * 60.0f, {1.1f, 1.1f, 1.1f});
        add(MapAssetType::SkyPlatform, MaterialSwatch::TitaniumWhite, {px, 8.5f, pz}, i * 60.0f, {0.9f, 1.0f, 0.9f});

        // Inner Cover Walls & Crates
        float wx = std::cos(ang + 0.5f) * 9.0f;
        float wz = std::sin(ang + 0.5f) * 9.0f;
        add(MapAssetType::ClimbWall, MaterialSwatch::CobaltSteel, {wx, 0.0f, wz}, i * 60.0f + 30.0f);
        add(MapAssetType::CoverCrate, MaterialSwatch::SolarGold, {wx * 0.7f, 0.0f, wz * 0.7f}, i * 30.0f);
    }

    // Jump pads for instant vertical launches
    add(MapAssetType::JumpPad, MaterialSwatch::NeonCyan, {-11.0f, 0.0f, 0.0f}, 0.0f);
    add(MapAssetType::JumpPad, MaterialSwatch::NeonCyan, {11.0f, 0.0f, 0.0f}, 0.0f);
    add(MapAssetType::JumpPad, MaterialSwatch::NeonCyan, {0.0f, 0.0f, -11.0f}, 0.0f);
    add(MapAssetType::JumpPad, MaterialSwatch::NeonCyan, {0.0f, 0.0f, 11.0f}, 0.0f);

    // Spawn Points (FFA + Squad Alpha + Squad Omega)
    add(MapAssetType::SpawnSquadAlpha, MaterialSwatch::NeonCyan, {-20.0f, 0.0f, -6.0f}, 0.0f);
    add(MapAssetType::SpawnSquadAlpha, MaterialSwatch::NeonCyan, {-20.0f, 0.0f, 6.0f}, 0.0f);
    add(MapAssetType::SpawnSquadOmega, MaterialSwatch::CrimsonCore, {20.0f, 0.0f, -6.0f}, 180.0f);
    add(MapAssetType::SpawnSquadOmega, MaterialSwatch::CrimsonCore, {20.0f, 0.0f, 6.0f}, 180.0f);
    add(MapAssetType::SpawnFFA, MaterialSwatch::EmeraldGrid, {-14.0f, 0.0f, -14.0f}, 45.0f);
    add(MapAssetType::SpawnFFA, MaterialSwatch::EmeraldGrid, {14.0f, 0.0f, -14.0f}, 135.0f);
    add(MapAssetType::SpawnFFA, MaterialSwatch::EmeraldGrid, {-14.0f, 0.0f, 14.0f}, -45.0f);
    add(MapAssetType::SpawnFFA, MaterialSwatch::EmeraldGrid, {14.0f, 0.0f, 14.0f}, -135.0f);

    // Weapon & Shield Soda Spawners
    for (uint8_t w = 0; w < 5; ++w) {
        float ang = w * (2.0f * kPi / 5.0f);
        add(MapAssetType::WeaponSpawner, MaterialSwatch::SolarGold,
            {std::cos(ang) * 6.5f, 0.0f, std::sin(ang) * 6.5f}, 0.0f, {1, 1, 1}, w);
    }
    add(MapAssetType::ShieldSodaSpawner, MaterialSwatch::NeonCyan, {-5.0f, 0.0f, 5.0f}, 0.0f, {1, 1, 1}, 0);
    add(MapAssetType::ShieldSodaSpawner, MaterialSwatch::NeonCyan, {5.0f, 0.0f, -5.0f}, 0.0f, {1, 1, 1}, 0);
    add(MapAssetType::ShieldSodaSpawner, MaterialSwatch::EmeraldGrid, {0.0f, 15.3f, 0.0f}, 0.0f, {1, 1, 1}, 1);

    maps_.push_back(m);
}

void CustomsSystem::createNeonSpireArenaMap() {
    CustomMap m;
    m.mapId = "neon-spire-arena-v1";
    m.title = "Neon Spire Arena";
    m.author = "Misfit Customs Creators";
    m.description = "Twin high-rise cyber towers linked by multi-level climb walls and high-velocity jump pads for intense SMG/Shotgun vertical skirmishes.";
    m.upvotes = 985;

    auto add = [&](MapAssetType t, MaterialSwatch sw, Vec3 pos, float yaw, Vec3 scl = {1, 1, 1}, uint8_t sub = 0) {
        MapObject o;
        o.id = nextObjectId_++;
        o.type = t;
        o.material = sw;
        o.position = pos;
        o.yawDegrees = yaw;
        o.scale = scl;
        o.climbable = true;
        o.subType = sub;
        m.objects.push_back(o);
    };

    add(MapAssetType::SniperTower, MaterialSwatch::VioletPulse, {-10.0f, 0.0f, 0.0f}, 0.0f, {1.1f, 1.2f, 1.1f});
    add(MapAssetType::SniperTower, MaterialSwatch::NeonCyan, {10.0f, 0.0f, 0.0f}, 0.0f, {1.1f, 1.2f, 1.1f});
    add(MapAssetType::NeonArchway, MaterialSwatch::CrimsonCore, {0.0f, 0.0f, 0.0f}, 0.0f, {1.4f, 1.4f, 1.2f});
    add(MapAssetType::SkyPlatform, MaterialSwatch::ObsidianHex, {0.0f, 9.5f, 0.0f}, 0.0f, {1.6f, 1.0f, 1.0f});

    for (int z = -12; z <= 12; z += 8) {
        add(MapAssetType::ClimbWall, MaterialSwatch::CobaltSteel, {-5.0f, 0.0f, static_cast<float>(z)}, 0.0f);
        add(MapAssetType::ClimbWall, MaterialSwatch::CobaltSteel, {5.0f, 0.0f, static_cast<float>(z)}, 0.0f);
        add(MapAssetType::JumpPad, MaterialSwatch::NeonCyan, {0.0f, 0.0f, static_cast<float>(z)}, 0.0f);
    }

    add(MapAssetType::SpawnSquadAlpha, MaterialSwatch::NeonCyan, {-18.0f, 0.0f, 0.0f}, 0.0f);
    add(MapAssetType::SpawnSquadOmega, MaterialSwatch::CrimsonCore, {18.0f, 0.0f, 0.0f}, 180.0f);
    add(MapAssetType::SpawnFFA, MaterialSwatch::EmeraldGrid, {0.0f, 0.0f, -18.0f}, 0.0f);
    add(MapAssetType::SpawnFFA, MaterialSwatch::EmeraldGrid, {0.0f, 0.0f, 18.0f}, 180.0f);
    add(MapAssetType::WeaponSpawner, MaterialSwatch::SolarGold, {-4.0f, 0.0f, 0.0f}, 0.0f, {1, 1, 1}, 1);
    add(MapAssetType::WeaponSpawner, MaterialSwatch::SolarGold, {4.0f, 0.0f, 0.0f}, 0.0f, {1, 1, 1}, 2);
    add(MapAssetType::ShieldSodaSpawner, MaterialSwatch::NeonCyan, {0.0f, 10.3f, 0.0f}, 0.0f, {1, 1, 1}, 0);

    maps_.push_back(m);
}

void CustomsSystem::createSkybridgeClashMap() {
    CustomMap m;
    m.mapId = "skybridge-clash-v1";
    m.title = "Skybridge Clash";
    m.author = "ApexGliderVR";
    m.description = "Suspended multi-tier hexagonal sky islands built for constant climbing, wing-gliding flanks, and long-range sniper duels.";
    m.upvotes = 1190;

    auto add = [&](MapAssetType t, MaterialSwatch sw, Vec3 pos, float yaw, Vec3 scl = {1, 1, 1}, uint8_t sub = 0) {
        MapObject o;
        o.id = nextObjectId_++;
        o.type = t;
        o.material = sw;
        o.position = pos;
        o.yawDegrees = yaw;
        o.scale = scl;
        o.climbable = true;
        o.subType = sub;
        m.objects.push_back(o);
    };

    for (int i = 0; i < 4; ++i) {
        float ang = i * (kPi * 0.5f);
        Vec3 p{std::cos(ang) * 14.0f, 0.0f, std::sin(ang) * 14.0f};
        add(MapAssetType::SniperTower, MaterialSwatch::SolarGold, p, i * 90.0f);
        add(MapAssetType::SkyPlatform, MaterialSwatch::CobaltSteel, {p.x * 0.5f, 11.0f, p.z * 0.5f}, i * 90.0f);
        add(MapAssetType::JumpPad, MaterialSwatch::NeonCyan, {p.x * 0.8f, 0.0f, p.z * 0.8f}, 0.0f);
        add(MapAssetType::SpawnFFA, MaterialSwatch::EmeraldGrid, {p.x * 1.2f, 0.0f, p.z * 1.2f}, i * 90.0f);
    }
    add(MapAssetType::SkyPlatform, MaterialSwatch::CrimsonCore, {0.0f, 14.0f, 0.0f}, 0.0f, {1.5f, 1.0f, 1.5f});
    add(MapAssetType::WeaponSpawner, MaterialSwatch::SolarGold, {0.0f, 14.8f, 0.0f}, 0.0f, {1, 1, 1}, 3);
    add(MapAssetType::ShieldSodaSpawner, MaterialSwatch::NeonCyan, {0.0f, 0.0f, 0.0f}, 0.0f, {1, 1, 1}, 0);

    maps_.push_back(m);
}

const CustomMap& CustomsSystem::getActiveMap() const {
    return maps_[activeMapIndex_];
}

CustomMap& CustomsSystem::getActiveMapMutable() {
    return maps_[activeMapIndex_];
}

void CustomsSystem::selectMap(size_t index) {
    if (index < maps_.size()) {
        activeMapIndex_ = index;
        hostedRoom_.activeMapIndex = static_cast<uint32_t>(index);
    }
}

Vec3 CustomsSystem::snapPosition(const Vec3& rawPos, float gridStep) const {
    if (gridStep <= 0.01f) return rawPos;
    return {
        std::round(rawPos.x / gridStep) * gridStep,
        std::max(0.0f, std::round(rawPos.y / gridStep) * gridStep),
        std::round(rawPos.z / gridStep) * gridStep
    };
}

uint32_t CustomsSystem::placeObject(MapAssetType type, MaterialSwatch swatch, const Vec3& pos, float yawDeg, const Vec3& scale) {
    CustomMap& map = getActiveMapMutable();
    if (map.computeComplexityCost() >= CustomMap::kMaxComplexityBudget) {
        return 0;
    }
    MapObject obj;
    obj.id = nextObjectId_++;
    obj.type = type;
    obj.material = swatch;
    obj.position = pos;
    obj.yawDegrees = yawDeg;
    obj.scale = scale;
    obj.climbable = (type != MapAssetType::JumpPad && type != MapAssetType::SpawnFFA &&
                     type != MapAssetType::SpawnSquadAlpha && type != MapAssetType::SpawnSquadOmega &&
                     type != MapAssetType::WeaponSpawner && type != MapAssetType::ShieldSodaSpawner);
    map.objects.push_back(obj);
    return obj.id;
}

bool CustomsSystem::deleteObject(uint32_t objectId) {
    auto& objs = getActiveMapMutable().objects;
    for (auto it = objs.begin(); it != objs.end(); ++it) {
        if (it->id == objectId) {
            objs.erase(it);
            return true;
        }
    }
    return false;
}

uint32_t CustomsSystem::duplicateObject(uint32_t objectId, const Vec3& offset) {
    const auto& objs = getActiveMap().objects;
    for (const auto& o : objs) {
        if (o.id == objectId) {
            return placeObject(o.type, o.material, o.position + offset, o.yawDegrees, o.scale);
        }
    }
    return 0;
}

bool CustomsSystem::transformObject(uint32_t objectId, const Vec3& newPos, float newYawDeg, const Vec3& newScale) {
    for (auto& o : getActiveMapMutable().objects) {
        if (o.id == objectId) {
            o.position = newPos;
            o.yawDegrees = newYawDeg;
            o.scale = {
                std::clamp(newScale.x, 0.25f, 4.0f),
                std::clamp(newScale.y, 0.25f, 4.0f),
                std::clamp(newScale.z, 0.25f, 4.0f)
            };
            return true;
        }
    }
    return false;
}

bool CustomsSystem::recolorObject(uint32_t objectId, MaterialSwatch newSwatch) {
    for (auto& o : getActiveMapMutable().objects) {
        if (o.id == objectId) {
            o.material = newSwatch;
            return true;
        }
    }
    return false;
}

std::string CustomsSystem::exportMapCode(const CustomMap& map) {
    // Compact human-shareable Map Code format:
    // PM1|<mapId>|<title>|<author>|t,m,x,y,z,yaw,sx,sy,sz;...
    std::ostringstream ss;
    ss << "PM1|" << map.mapId << "|" << map.title << "|" << map.author << "|";
    ss << std::fixed << std::setprecision(1);
    for (size_t i = 0; i < map.objects.size(); ++i) {
        const auto& o = map.objects[i];
        if (i > 0) ss << ";";
        ss << static_cast<int>(o.type) << ","
           << static_cast<int>(o.material) << ","
           << o.position.x << "," << o.position.y << "," << o.position.z << ","
           << o.yawDegrees << ","
           << o.scale.x << "," << o.scale.y << "," << o.scale.z << ","
           << static_cast<int>(o.subType);
    }
    return ss.str();
}

bool CustomsSystem::importMapCode(const std::string& code, CustomMap& outMap) {
    if (code.rfind("PM1|", 0) != 0) return false;
    std::vector<std::string> sections;
    std::stringstream ss(code);
    std::string item;
    while (std::getline(ss, item, '|')) {
        sections.push_back(item);
    }
    if (sections.size() < 5) return false;

    outMap.mapId = sections[1];
    outMap.title = sections[2];
    outMap.author = sections[3];
    outMap.description = "Imported via Population Misfits Share Code";
    outMap.upvotes = 1;
    outMap.objects.clear();

    std::stringstream objStream(sections[4]);
    std::string objToken;
    uint32_t idCounter = 1000;
    while (std::getline(objStream, objToken, ';')) {
        if (objToken.empty()) continue;
        std::stringstream fieldStream(objToken);
        std::string f;
        std::vector<float> vals;
        while (std::getline(fieldStream, f, ',')) {
            vals.push_back(std::strtof(f.c_str(), nullptr));
        }
        if (vals.size() >= 9) {
            MapObject o;
            o.id = idCounter++;
            o.type = static_cast<MapAssetType>(static_cast<int>(vals[0]) % 13);
            o.material = static_cast<MaterialSwatch>(static_cast<int>(vals[1]) % 8);
            o.position = {vals[2], vals[3], vals[4]};
            o.yawDegrees = vals[5];
            o.scale = {vals[6], vals[7], vals[8]};
            o.subType = (vals.size() >= 10) ? static_cast<uint8_t>(vals[9]) : 0;
            o.climbable = (o.type != MapAssetType::JumpPad && o.type != MapAssetType::SpawnFFA &&
                           o.type != MapAssetType::SpawnSquadAlpha && o.type != MapAssetType::SpawnSquadOmega &&
                           o.type != MapAssetType::WeaponSpawner && o.type != MapAssetType::ShieldSodaSpawner);
            outMap.objects.push_back(o);
        }
    }
    return !outMap.objects.empty();
}

std::string CustomsSystem::serializeMapJson(const CustomMap& map) {
    std::ostringstream ss;
    ss << "{\n"
       << "  \"schema\": \"population-misfits-custom-map-v1\",\n"
       << "  \"mapId\": \"" << map.mapId << "\",\n"
       << "  \"title\": \"" << map.title << "\",\n"
       << "  \"author\": \"" << map.author << "\",\n"
       << "  \"upvotes\": " << map.upvotes << ",\n"
       << "  \"complexity\": " << map.computeComplexityCost() << ",\n"
       << "  \"shareCode\": \"" << exportMapCode(map) << "\",\n"
       << "  \"objectCount\": " << map.objects.size() << "\n"
       << "}\n";
    return ss.str();
}

bool CustomsSystem::saveMapToDisk(const CustomMap& map, const std::string& directoryPath) {
    if (directoryPath.empty()) return false;
    std::string path = directoryPath + "/" + map.mapId + ".json";
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) return false;
    out << serializeMapJson(map);
    out.close();
    return true;
}

void CustomsSystem::upvoteMap(size_t mapIndex) {
    if (mapIndex < maps_.size()) {
        maps_[mapIndex].upvotes++;
    }
}

void CustomsSystem::addMapToRoomPlaylist(const std::string& mapId) {
    hostedRoom_.mapPlaylistIds.push_back(mapId);
}

std::string CustomsSystem::hostNewRoom(const std::string& roomName, bool isPublic, const CustomRoomRules& rules) {
    static uint32_t roomCounter = 204;
    roomCounter += 37;
    hostedRoom_.roomCode = "MSFT-" + std::to_string(roomCounter % 900 + 100);
    hostedRoom_.roomName = roomName;
    hostedRoom_.isPublic = isPublic;
    hostedRoom_.rules = rules;
    hostedRoom_.activeMapIndex = static_cast<uint32_t>(activeMapIndex_);
    openLanSocket(hostedRoom_.udpPort);
    return hostedRoom_.roomCode;
}

bool CustomsSystem::joinLanRoomByCode(const std::string& roomCode) {
    if (roomCode.empty()) return false;
    hostedRoom_.roomCode = roomCode;
    return true;
}

void CustomsSystem::openLanSocket(uint16_t port) {
#if defined(__unix__) || defined(__ANDROID__)
    if (udpSocketFd_ >= 0) return;
    udpSocketFd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocketFd_ < 0) return;

    int broadcastEnable = 1;
    ::setsockopt(udpSocketFd_, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
    int reuse = 1;
    ::setsockopt(udpSocketFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    int flags = ::fcntl(udpSocketFd_, F_GETFL, 0);
    if (flags >= 0) {
        ::fcntl(udpSocketFd_, F_SETFL, flags | O_NONBLOCK);
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (::bind(udpSocketFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(udpSocketFd_);
        udpSocketFd_ = -1;
    }
#else
    (void)port;
#endif
}

void CustomsSystem::closeLanSocket() {
#if defined(__unix__) || defined(__ANDROID__)
    if (udpSocketFd_ >= 0) {
        ::close(udpSocketFd_);
        udpSocketFd_ = -1;
    }
#endif
}

void CustomsSystem::tickLanNetwork(float dt, const Vec3& localHeadPos, float localYaw, uint32_t localScore) {
    lanBroadcastTimer_ += dt;
#if defined(__unix__) || defined(__ANDROID__)
    if (udpSocketFd_ < 0) return;

    // Non-blocking poll for any peer packets on the LAN
    char buf[256];
    sockaddr_in sender{};
    socklen_t senderLen = sizeof(sender);
    while (::recvfrom(udpSocketFd_, buf, sizeof(buf) - 1, 0,
                      reinterpret_cast<sockaddr*>(&sender), &senderLen) > 0) {
        hostedRoom_.connectedLanClients = std::max<uint8_t>(1, hostedRoom_.connectedLanClients);
    }

    // Broadcast room heartbeat & player state every 0.25s on localhost/LAN
    if (lanBroadcastTimer_ >= 0.25f) {
        lanBroadcastTimer_ = 0.0f;
        char packet[192];
        int len = std::snprintf(
            packet, sizeof(packet),
            "PM_ROOM|%s|%s|%.2f,%.2f,%.2f|%.2f|%u",
            hostedRoom_.roomCode.c_str(),
            getActiveMap().mapId.c_str(),
            localHeadPos.x, localHeadPos.y, localHeadPos.z,
            localYaw,
            localScore
        );
        if (len > 0) {
            sockaddr_in dest{};
            dest.sin_family = AF_INET;
            dest.sin_port = htons(hostedRoom_.udpPort);
            dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            ::sendto(udpSocketFd_, packet, static_cast<size_t>(len), 0,
                     reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
        }
    }
#else
    (void)localHeadPos;
    (void)localYaw;
    (void)localScore;
#endif
}

const char* CustomsSystem::getAssetTypeName(MapAssetType type) {
    switch (type) {
        case MapAssetType::HexPillar:         return "Hex Pillar (Climbable)";
        case MapAssetType::ClimbWall:         return "Tactical Climb Wall";
        case MapAssetType::RampIncline:       return "Assault Ramp";
        case MapAssetType::SniperTower:       return "15m Sniper Spire";
        case MapAssetType::SkyPlatform:       return "Hex Sky Platform";
        case MapAssetType::CoverCrate:        return "Hex Cover Crate";
        case MapAssetType::JumpPad:           return "Vertical Jump Pad";
        case MapAssetType::SpawnFFA:          return "FFA Spawn Pad";
        case MapAssetType::SpawnSquadAlpha:   return "Squad Alpha Spawn";
        case MapAssetType::SpawnSquadOmega:   return "Squad Omega Spawn";
        case MapAssetType::WeaponSpawner:     return "Weapon Spawner Pad";
        case MapAssetType::ShieldSodaSpawner: return "Shield Soda Spawner";
        case MapAssetType::NeonArchway:       return "Neon Hex Archway";
    }
    return "Hex Asset";
}

const char* CustomsSystem::getSwatchName(MaterialSwatch swatch) {
    switch (swatch) {
        case MaterialSwatch::CobaltSteel:   return "Cobalt Steel";
        case MaterialSwatch::ObsidianHex:   return "Obsidian Hex";
        case MaterialSwatch::NeonCyan:      return "Neon Cyan";
        case MaterialSwatch::CrimsonCore:   return "Crimson Core";
        case MaterialSwatch::SolarGold:     return "Solar Gold";
        case MaterialSwatch::EmeraldGrid:   return "Emerald Grid";
        case MaterialSwatch::TitaniumWhite: return "Titanium White";
        case MaterialSwatch::VioletPulse:   return "Violet Pulse";
    }
    return "Cobalt Steel";
}

void CustomsSystem::appendMapObjectGeometry(MeshData& mesh, const MapObject& obj, bool highlighted) {
    AABB b = obj.computeWorldBounds();
    Color4 col = obj.getMaterialColor();
    if (highlighted) {
        col.r = std::min(1.0f, col.r + 0.30f);
        col.g = std::min(1.0f, col.g + 0.30f);
        col.b = std::min(1.0f, col.b + 0.30f);
    }

    Vec3 bottomCenter{obj.position.x, b.min.y, obj.position.z};
    Vec3 topCenter{obj.position.x, b.max.y, obj.position.z};
    float rx = (b.max.x - b.min.x) * 0.5f;
    float rz = (b.max.z - b.min.z) * 0.5f;

    if (obj.type == MapAssetType::SniperTower) {
        // Multi-tiered hexagonal spire with sniper perch ring
        Vec3 midCenter{obj.position.x, b.min.y + (b.max.y - b.min.y) * 0.78f, obj.position.z};
        CharacterSystem::appendHexPrism(mesh, bottomCenter, midCenter, rx, rz, rx * 0.68f, rz * 0.68f, col, 1);
        Color4 perchCol{0.14f, 0.82f, 0.96f, 1.0f};
        CharacterSystem::appendHexPrism(mesh, midCenter, topCenter, rx * 1.15f, rz * 1.15f, rx * 1.05f, rz * 1.05f, perchCol, 7);
    } else {
        CharacterSystem::appendHexPrism(mesh, bottomCenter, topCenter, rx, rz, rx, rz, col, 1);
    }
}

} // namespace misfits
