#pragma once

#include <RE/Skyrim.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>

namespace Ostim {

/// Singleton that loads and provides OStim translation strings.
/// Loads from: Data/Interface/translations/ONav_ENGLISH.txt, OScenes_ENGLISH.txt, OStim_ENGLISH.txt
///
/// Usage:
///   auto* translator = OstimTranslationLoader::GetSingleton();
///   std::string text = translator->Translate("$ostim_nav_return");  // Returns "return"
///   std::string text = translator->Translate("not a key");          // Returns "not a key" unchanged
///
class OstimTranslationLoader
{
public:
    static OstimTranslationLoader* GetSingleton();

    /// Ensures translations are loaded. Called automatically by Translate().
    void EnsureLoaded();

    /// Force a reload of all translation files.
    void Reload();

    /// Translate a string. If it starts with '$', looks up the translation.
    /// If not found or doesn't start with '$', returns the input unchanged.
    /// Also handles {{X}} -> {X} replacement in translated strings.
    std::string Translate(const std::string& key) const;

    /// Translate a string and substitute actor names for {0}, {1} placeholders.
    /// @param key Translation key (with $ prefix) or plain text
    /// @param actors Array of actors - {0} replaced with actors[0] name, etc.
    std::string Translate(const std::string& key, const std::vector<RE::Actor*>& actors) const;

    /// Check if translations have been loaded.
    bool IsLoaded() const { return m_loaded; }

    /// Get count of loaded translations.
    size_t GetTranslationCount() const { return m_translations.size(); }

private:
    OstimTranslationLoader() = default;
    ~OstimTranslationLoader() = default;
    OstimTranslationLoader(const OstimTranslationLoader&) = delete;
    OstimTranslationLoader& operator=(const OstimTranslationLoader&) = delete;

    void LoadAllTranslations();
    void LoadTranslationFile(const std::filesystem::path& filePath);
    std::string ProcessTranslation(const std::string& value) const;

    std::atomic<bool> m_loaded{false};
    mutable std::mutex m_mutex;

    // Key (without $) -> translated value
    std::unordered_map<std::string, std::string> m_translations;
};

} // namespace Ostim
