#pragma once

#include "math3d.h"
#include "character_skin.h"
#include <vector>
#include <string>
#include <cstdint>

namespace misfits {

enum class MapAssetType : uint8_t {
    HexPillar = 0,       // Climbable tall hexagonal column
    ClimbWall = 1,       // Wide climbable tactical arena wall
    RampIncline = 2,     // Angled ramp for fast vertical transitions
    SniperTower = 3,     // Elevated hexagonal perch for gliding & sniping
    SkyPlatform = 4,     // Floating hexagonal skybridge platform
    CoverCrate = 5,      // Hexagonal low cover crate
    JumpPad = 6,         // High-impulse vertical launch pad
    SpawnFFA = 7,        // Free-For-All player/bot spawn pad
    SpawnSquadAlpha = 8, // Squad Alpha (Blue) spawn pad
    SpawnSquadOmega = 9, // Squad Omega (Orange) spawn pad
    WeaponSpawner = 10,  // Spawns AR/SMG/Shotgun/Sniper/Pistol pickups
    ShieldSodaSpawner = 11, // Spawns Shield Soda (+50 shield) / Medkit (+50 HP)
    NeonArchway = 12     // Decorative illuminated hexagonal archway
};

enum class MaterialSwatch : uint8_t {
    CobaltSteel = 0,
    ObsidianHex = 1,
    NeonCyan = 2,
    CrimsonCore = 3,
    SolarGold = 4,
    EmeraldGrid = 5,
    TitaniumWhite = 6,
    VioletPulse = 7
};

struct MapObject {
    uint32_t id = 0;
    MapAssetType type = MapAssetType::HexPillar;
    MaterialSwatch material = MaterialSwatch::CobaltSteel;
    Vec3 position{0.0f, 0.0f, 0.0f};
    float yawDegrees = 0.0f;
    Vec3 scale{1.0f, 1.0f, 1.0f};
    bool climbable = true;
    uint8_t subType = 0; // For WeaponSpawner: weapon class index; for ShieldSodaSpawner: 0=ShieldSoda, 1=HealthKit

    AABB computeWorldBounds() const;
    Color4 getMaterialColor() const;
};

struct CustomMap {
    std::string mapId;
    std::string title;
    std::string author;
    std::string description;
    uint32_t upvotes = 0;
    uint32_t version = 1;
    std::vector<MapObject> objects;

    uint32_t computeComplexityCost() const;
    static constexpr uint32_t kMaxComplexityBudget = 5000;
};

enum class BattleModeType : uint8_t {
    FreeForAll_Arena = 0,
    SquadBattle_Arena = 1
};

enum class LoadoutPreset : uint8_t {
    AllWeapons_Balanced = 0,
    SniperAndShotty = 1,
    SMGRush = 2,
    PistolsOnly = 3
};

struct CustomRoomRules {
    BattleModeType mode = BattleModeType::FreeForAll_Arena;
    float gravityMultiplier = 1.0f;     // 0.35x (Low-G Moon) to 1.5x
    float moveSpeedMultiplier = 1.0f;   // 0.75x to 2.0x
    float glideSpeedMultiplier = 1.15f; // 0.8x to 2.0x
    float maxHealth = 100.0f;
    float maxShield = 100.0f;
    LoadoutPreset loadout = LoadoutPreset::AllWeapons_Balanced;
    uint8_t botCount = 5;               // 0 to 11 Misfit bots
    uint16_t scoreLimitKills = 20;      // Kills to win Arena match
    uint16_t timeLimitSeconds = 300;    // 5-minute default rapid Arena match
    bool infiniteAmmo = false;
    bool instantReload = false;
};

struct HostedCustomRoom {
    std::string roomCode;               // e.g., "MSFT-402"
    std::string roomName;
    bool isPublic = true;
    uint32_t activeMapIndex = 0;
    std::vector<std::string> mapPlaylistIds;
    CustomRoomRules rules;
    uint16_t udpPort = 39770;
    uint8_t connectedLanClients = 0;
};

struct EditorToolState {
    bool editorActive = false;
    MapAssetType selectedAsset = MapAssetType::HexPillar;
    MaterialSwatch selectedSwatch = MaterialSwatch::CobaltSteel;
    float snapGridSize = 1.0f;          // 0.5m, 1.0m, or 2.0m grid snap
    float snapYawStep = 30.0f;          // 30-degree hexagonal rotation snap
    float placementDistance = 6.0f;
    Vec3 previewScale{1.0f, 1.0f, 1.0f};
    float previewYawDeg = 0.0f;
    int32_t hoveredObjectId = -1;
    int32_t grabbedObjectId = -1;
};

class CustomsSystem {
public:
    CustomsSystem();
    ~CustomsSystem();

    void initializeBuiltInMaps(const std::string& storageDirPath);

    const std::vector<CustomMap>& getMaps() const { return maps_; }
    const CustomMap& getActiveMap() const;
    CustomMap& getActiveMapMutable();
    void selectMap(size_t index);
    size_t getActiveMapIndex() const { return activeMapIndex_; }

    // Map Editor operations
    uint32_t placeObject(MapAssetType type, MaterialSwatch swatch, const Vec3& pos, float yawDeg, const Vec3& scale);
    bool deleteObject(uint32_t objectId);
    uint32_t duplicateObject(uint32_t objectId, const Vec3& offset);
    bool transformObject(uint32_t objectId, const Vec3& newPos, float newYawDeg, const Vec3& newScale);
    bool recolorObject(uint32_t objectId, MaterialSwatch newSwatch);
    Vec3 snapPosition(const Vec3& rawPos, float gridStep) const;

    // Serialization: Shareable Map Code ("PM1:...") and JSON file persistence
    static std::string exportMapCode(const CustomMap& map);
    static bool importMapCode(const std::string& code, CustomMap& outMap);
    static std::string serializeMapJson(const CustomMap& map);
    static bool saveMapToDisk(const CustomMap& map, const std::string& directoryPath);

    // Community Map Browser actions
    void upvoteMap(size_t mapIndex);
    void addMapToRoomPlaylist(const std::string& mapId);

    // Custom Room Hosting & LAN UDP Discovery/Sync
    HostedCustomRoom& getHostedRoom() { return hostedRoom_; }
    const HostedCustomRoom& getHostedRoom() const { return hostedRoom_; }
    std::string hostNewRoom(const std::string& roomName, bool isPublic, const CustomRoomRules& rules);
    bool joinLanRoomByCode(const std::string& roomCode);
    void tickLanNetwork(float dt, const Vec3& localHeadPos, float localYaw, uint32_t localScore);

    static const char* getAssetTypeName(MapAssetType type);
    static const char* getSwatchName(MaterialSwatch swatch);
    static void appendMapObjectGeometry(MeshData& mesh, const MapObject& obj, bool highlighted = false);

private:
    std::vector<CustomMap> maps_;
    size_t activeMapIndex_ = 0;
    uint32_t nextObjectId_ = 100;
    std::string storageDir_;
    HostedCustomRoom hostedRoom_;
    int udpSocketFd_ = -1;
    float lanBroadcastTimer_ = 0.0f;

    void createHexCitadelMap();
    void createNeonSpireArenaMap();
    void createSkybridgeClashMap();
    void openLanSocket(uint16_t port);
    void closeLanSocket();
};

} // namespace misfits
