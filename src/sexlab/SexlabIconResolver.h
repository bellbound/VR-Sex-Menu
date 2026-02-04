#pragma once

#include <string>
#include <vector>

namespace Sexlab {

// Forward declaration
struct Animation;

/// Resolves DDS icon paths for animations based on tags.
/// Uses a priority-based rule system to select icons.
class SexlabIconResolver
{
public:
    static SexlabIconResolver* GetSingleton()
    {
        static SexlabIconResolver instance;
        return &instance;
    }

    /// Get the icon path for an animation.
    /// Matches tags against rules in priority order.
    ///
    /// @param anim Animation to get icon for
    /// @return DDS path (e.g., "Interface/OStim/icons/OStim/vaginalsex.dds")
    std::string GetIconPath(const Animation& anim) const;

private:
    SexlabIconResolver();
    ~SexlabIconResolver() = default;
    SexlabIconResolver(const SexlabIconResolver&) = delete;
    SexlabIconResolver& operator=(const SexlabIconResolver&) = delete;

    /// Icon mapping rule.
    struct IconRule {
        std::string tag;      // Tag to match (case-insensitive)
        std::string iconPath; // Icon path if matched
    };

    /// Rules checked in order (first match wins).
    std::vector<IconRule> m_rules;

    /// Default icon when no rules match.
    std::string m_defaultIcon;

    /// Initialize the rule set.
    void InitializeRules();
};

} // namespace Sexlab
