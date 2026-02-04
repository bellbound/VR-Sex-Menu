#include "OstimTranslationLoader.h"
#include "../log.h"
#include <fstream>
#include <filesystem>
#include <codecvt>
#include <locale>

namespace fs = std::filesystem;

namespace Ostim {

OstimTranslationLoader* OstimTranslationLoader::GetSingleton()
{
    static OstimTranslationLoader instance;
    return &instance;
}

void OstimTranslationLoader::EnsureLoaded()
{
    if (m_loaded) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Double-check after acquiring lock
    if (m_loaded) return;

    LoadAllTranslations();
    m_loaded = true;
}

void OstimTranslationLoader::Reload()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_translations.clear();
    m_loaded = false;

    LoadAllTranslations();
    m_loaded = true;
}

void OstimTranslationLoader::LoadAllTranslations()
{
    // Build path to translation files: Data/Interface/translations/
    char pathBuffer[MAX_PATH];
    GetModuleFileNameA(nullptr, pathBuffer, MAX_PATH);
    std::string exePath(pathBuffer);
    fs::path translationPath = fs::path(exePath).parent_path() / "Data" / "Interface" / "translations";

    spdlog::info("OstimTranslationLoader: Loading translations from '{}'", translationPath.string());

    if (!fs::exists(translationPath)) {
        spdlog::warn("OstimTranslationLoader: Translation directory does not exist: {}", translationPath.string());
        return;
    }

    // Load all OStim translation files
    const char* files[] = { "ONav_ENGLISH.txt", "OScenes_ENGLISH.txt", "OStim_ENGLISH.txt" };
    for (const char* filename : files) {
        fs::path filePath = translationPath / filename;
        if (fs::exists(filePath)) {
            LoadTranslationFile(filePath);
        }
    }

    spdlog::info("OstimTranslationLoader: Loaded {} translations", m_translations.size());
}

void OstimTranslationLoader::LoadTranslationFile(const fs::path& filePath)
{
    // Skyrim translation files are UTF-16 LE with BOM
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("OstimTranslationLoader: Failed to open file: {}", filePath.string());
        return;
    }

    // Read entire file as bytes
    std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    if (buffer.size() < 2) {
        spdlog::error("OstimTranslationLoader: File too small: {}", filePath.string());
        return;
    }

    // Check for UTF-16 LE BOM (0xFF 0xFE)
    size_t startOffset = 0;
    if (static_cast<unsigned char>(buffer[0]) == 0xFF && static_cast<unsigned char>(buffer[1]) == 0xFE) {
        startOffset = 2;  // Skip BOM
    }

    // Convert UTF-16 LE to UTF-8
    std::wstring wideContent;
    for (size_t i = startOffset; i + 1 < buffer.size(); i += 2) {
        wchar_t ch = static_cast<unsigned char>(buffer[i]) | (static_cast<unsigned char>(buffer[i + 1]) << 8);
        wideContent += ch;
    }

    // Convert wstring to UTF-8 string
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::string content = converter.to_bytes(wideContent);

    // Parse line by line
    std::istringstream stream(content);
    std::string line;
    int lineCount = 0;

    while (std::getline(stream, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == ';') continue;

        // Remove carriage return if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Find tab separator between key and value
        size_t tabPos = line.find('\t');
        if (tabPos == std::string::npos) continue;

        std::string key = line.substr(0, tabPos);
        std::string value = line.substr(tabPos + 1);

        // Trim whitespace from key
        while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) {
            key.erase(0, 1);
        }
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) {
            key.pop_back();
        }

        // Skip if not a translation key (must start with $)
        if (key.empty() || key[0] != '$') continue;

        // Store without the $ prefix for easier lookup
        std::string keyWithoutPrefix = key.substr(1);
        m_translations[keyWithoutPrefix] = value;
        lineCount++;
    }

    spdlog::debug("OstimTranslationLoader: Loaded {} translations from {}", lineCount, filePath.filename().string());
}

std::string OstimTranslationLoader::Translate(const std::string& key) const
{
    // Non-const access for lazy loading
    const_cast<OstimTranslationLoader*>(this)->EnsureLoaded();

    // Not a translation key
    if (key.empty() || key[0] != '$') {
        return key;
    }

    // Extract key without $ prefix, but keep any suffix like {1}
    // Example: "$ostim_pat_head{1}" -> lookup "ostim_pat_head{1}" or "ostim_pat_head{}"
    std::string lookupKey = key.substr(1);

    // First try exact match
    auto it = m_translations.find(lookupKey);
    if (it != m_translations.end()) {
        return ProcessTranslation(it->second);
    }

    // Try with {} placeholder - OStim uses {X} where X is actor index
    // Translation files use {} as placeholder, scene files use {0}, {1}, etc.
    // Example: key = "ostim_pat_head{1}" -> try "ostim_pat_head{}"
    size_t braceStart = lookupKey.find('{');
    if (braceStart != std::string::npos) {
        size_t braceEnd = lookupKey.find('}', braceStart);
        if (braceEnd != std::string::npos) {
            // Extract the number inside braces for later substitution
            std::string actorIndex = lookupKey.substr(braceStart + 1, braceEnd - braceStart - 1);

            // Create generic key with empty braces
            std::string genericKey = lookupKey.substr(0, braceStart + 1) + "}" + lookupKey.substr(braceEnd + 1);

            it = m_translations.find(genericKey);
            if (it != m_translations.end()) {
                std::string result = ProcessTranslation(it->second);
                // Replace {X} in result with actual actor index
                // The translation has {{}} which becomes {} after ProcessTranslation
                size_t placeholderPos = result.find("{}");
                if (placeholderPos != std::string::npos) {
                    result.replace(placeholderPos, 2, "{" + actorIndex + "}");
                }
                return result;
            }
        }
    }

    // Not found - return original key
    spdlog::debug("OstimTranslationLoader: Translation not found for '{}'", key);
    return key;
}

std::string OstimTranslationLoader::Translate(const std::string& key, const std::vector<RE::Actor*>& actors) const
{
    std::string result = Translate(key);

    // Replace {0}, {1}, etc. with actor names
    for (size_t i = 0; i < actors.size(); ++i) {
        std::string placeholder = "{" + std::to_string(i) + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            std::string actorName;
            if (actors[i]) {
                actorName = actors[i]->GetDisplayFullName();
            }
            result.replace(pos, placeholder.length(), actorName);
            pos += actorName.length();
        }
    }

    return result;
}

std::string OstimTranslationLoader::ProcessTranslation(const std::string& value) const
{
    // Replace {{}} with {} (translation files use double braces to escape)
    std::string result = value;
    size_t pos = 0;
    while ((pos = result.find("{{}}", pos)) != std::string::npos) {
        result.replace(pos, 4, "{}");
        pos += 2;
    }

    // Also handle {{X}} -> {X} pattern
    pos = 0;
    while ((pos = result.find("{{", pos)) != std::string::npos) {
        size_t endPos = result.find("}}", pos);
        if (endPos != std::string::npos) {
            // Extract content between {{ and }}
            std::string inner = result.substr(pos + 2, endPos - pos - 2);
            result.replace(pos, endPos - pos + 2, "{" + inner + "}");
            pos += inner.length() + 2;
        } else {
            break;
        }
    }

    return result;
}

} // namespace Ostim
