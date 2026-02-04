#pragma once

#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>

namespace Sexlab {

/// Actor type in a SexLab animation
enum class ActorType {
    Male,
    Female,
    CreatureMale,
    CreatureFemale
};

/// Single stage within an actor's animation
struct AnimationStage {
    std::string id;           // Animation event ID
    bool openMouth = false;
    bool strapOn = false;
    bool silent = false;
    int sos = 0;
    int addCum = 0;           // Cum effect flags (can be per-stage)
    float forward = 0.0f;     // Position offset
    int rotate = 0;           // Rotation offset
};

/// Actor slot in an animation
struct AnimationActor {
    ActorType type = ActorType::Male;
    std::string race;         // For creatures
    int addCum = 0;           // Cum effect flags
    std::vector<AnimationStage> stages;
};

/// Complete animation definition
struct Animation {
    std::string registryId;   // Our assigned ID: "{packFile}_{index}"
    std::string slalId;       // Original SLAL ID (e.g., "B_B_FMast1")
    std::string name;         // Display name
    std::string packName;     // Pack file name (e.g., "Billyy_Human")
    std::string packDisplayName; // "Billyy Human" - human readable
    std::vector<std::string> tags; // Parsed from comma-separated string
    std::string sound;
    std::string creatureRace; // Required creature race (if any)
    std::vector<AnimationActor> actors;
    int stageCount = 0;       // Max stages across all actors

    // Computed helpers
    int GetActorCount() const { return static_cast<int>(actors.size()); }

    /// Check if animation has a specific tag (case-insensitive)
    bool HasTag(const std::string& tag) const;

    /// Check if this is a creature animation
    bool IsCreatureAnimation() const;

    /// Check if animation requires a specific race
    bool RequiresRace(const std::string& race) const;
};

/// Singleton that loads and indexes SLAL animations from JSON files.
/// Animations are loaded from: Data/SLAnims/json/*.json
///
/// Usage:
///   auto* loader = SexlabSceneLoader::GetSingleton();
///   loader->EnsureLoaded();
///   const auto* anim = loader->GetAnimation("Billyy_Human_0");
///   for (const auto& anim : loader->GetAllAnimations()) { ... }
///
class SexlabSceneLoader
{
public:
    static SexlabSceneLoader* GetSingleton();

    /// Ensure animations are loaded. Safe to call multiple times.
    /// Only loads once - subsequent calls are no-ops.
    void EnsureLoaded();

    /// Force reload all animations.
    void Reload();

    /// Get all loaded animations.
    const std::vector<Animation>& GetAllAnimations() const;

    /// Get animation by registry ID.
    /// @param registryId Our assigned ID (e.g., "Billyy_Human_0")
    /// @return Pointer to animation or nullptr if not found
    const Animation* GetAnimation(const std::string& registryId) const;

    /// Get animation by SLAL ID (original ID from JSON).
    /// @param slalId Original SLAL ID (e.g., "B_B_FMast1")
    /// @return Pointer to animation or nullptr if not found
    const Animation* GetAnimationBySlalId(const std::string& slalId) const;

    /// Get animations matching a predicate.
    /// @param predicate Function that returns true for animations to include
    /// @return Vector of pointers to matching animations
    std::vector<const Animation*> FindAnimations(
        std::function<bool(const Animation&)> predicate) const;

    /// Get all unique pack names.
    /// @return Vector of pack names (e.g., ["Billyy_Human", "Leito"])
    std::vector<std::string> GetPackNames() const;

    /// Get all unique tags across all animations.
    /// @return Set of all tags (lowercase)
    std::set<std::string> GetAllTags() const;

    /// Get all unique creature races.
    /// @return Set of creature race strings
    std::set<std::string> GetAllCreatureRaces() const;

    /// Check if animations have been loaded.
    bool IsLoaded() const { return m_loaded; }

    /// Get count of loaded animations.
    size_t GetAnimationCount() const { return m_animations.size(); }

    /// Get load errors (if any).
    const std::vector<std::string>& GetLoadErrors() const { return m_loadErrors; }

private:
    SexlabSceneLoader() = default;
    ~SexlabSceneLoader() = default;
    SexlabSceneLoader(const SexlabSceneLoader&) = delete;
    SexlabSceneLoader& operator=(const SexlabSceneLoader&) = delete;

    /// Load all animation packs from the SLAnims folder.
    void LoadAllAnimations();

    /// Load a single pack file.
    /// @param filePath Path to the JSON file
    /// @return True if loaded successfully
    bool LoadPackFile(const std::filesystem::path& filePath);

    /// Parse actor type string to enum.
    /// @param typeStr Type string from JSON (e.g., "Male", "Female", "CreatureMale")
    /// @return Parsed ActorType enum value
    ActorType ParseActorType(const std::string& typeStr) const;

    /// Parse comma-separated tags into vector.
    /// @param tagStr Comma-separated tags (e.g., "Vaginal,Missionary,FM")
    /// @return Vector of individual tags (trimmed, lowercase)
    std::vector<std::string> ParseTags(const std::string& tagStr) const;

    /// Build the index maps after loading.
    void BuildIndex();

    /// Convert pack filename to display name.
    /// @param packName Filename without extension (e.g., "Billyy_Human")
    /// @return Human-readable name (e.g., "Billyy Human")
    std::string MakeDisplayName(const std::string& packName) const;

    std::atomic<bool> m_loaded{false};
    std::mutex m_loadMutex;

    std::vector<Animation> m_animations;
    std::unordered_map<std::string, size_t> m_registryIndex;  // registryId -> index
    std::unordered_map<std::string, size_t> m_slalIdIndex;    // slalId -> index
    std::vector<std::string> m_loadErrors;
};

} // namespace Sexlab
