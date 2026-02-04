#pragma once

#include <RE/Skyrim.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <spdlog/spdlog.h>

namespace VRSexMenu
{
    // State of undress for an actor
    enum class UndressState : uint8_t
    {
        Dressed = 0,           // Default - wearing whatever they have
        PartiallyUndressed,    // Outer armor removed, undergarments remain
        FullyUndressed         // Everything removed
    };

    // Helper namespace for armor classification
    namespace UndressHelper
    {
        // Armor slots that count as "outer armor" (removed in partial undress)
        // Body, Hands, Feet, Shield, Head, Hair (helmets)
        inline bool IsOuterArmor(RE::TESObjectARMO* armor)
        {
            if (!armor) return false;

            using Slot = RE::BGSBipedObjectForm::BipedObjectSlot;
            using SlotType = std::underlying_type_t<Slot>;

            auto slots = armor->GetSlotMask();

            // Check for body armor slots (30-39 primarily)
            // Slot 30 = Head, 31 = Hair, 32 = Body, 33 = Hands, 37 = Feet, 39 = Shield
            constexpr SlotType outerSlots =
                static_cast<SlotType>(Slot::kHead) |      // 30 - Helmets
                static_cast<SlotType>(Slot::kHair) |      // 31 - Hair/Wigs that are armor
                static_cast<SlotType>(Slot::kBody) |      // 32 - Cuirass/Robes
                static_cast<SlotType>(Slot::kHands) |     // 33 - Gauntlets
                static_cast<SlotType>(Slot::kForearms) |  // 34 - Bracers
                static_cast<SlotType>(Slot::kCalves) |    // 37 - Greaves/Boots
                static_cast<SlotType>(Slot::kShield) |    // 39 - Shields
                static_cast<SlotType>(Slot::kTail);       // 40 - Tails/Cloaks

            return (static_cast<SlotType>(slots) & outerSlots) != 0;
        }

        // Everything else (circlets, rings, amulets, underwear mods, etc.)
        inline bool IsUndergarment(RE::TESObjectARMO* armor)
        {
            return armor && !IsOuterArmor(armor);
        }
    }

    // Stored outfit data for an actor (in-memory only, no persistence)
    struct StoredOutfit
    {
        std::unordered_set<RE::FormID> equippedArmor;  // FormIDs of equipped armor
    };

    class UndressManager
    {
    public:
        static UndressManager* GetSingleton()
        {
            static UndressManager instance;
            return &instance;
        }

        // === Undress Operations ===

        // Partial undress: remove outer armor only, store original outfit
        void UndressPartial(RE::Actor* actor)
        {
            if (!actor) return;

            auto* equipManager = RE::ActorEquipManager::GetSingleton();
            if (!equipManager) {
                spdlog::error("UndressManager::UndressPartial - No ActorEquipManager");
                return;
            }

            spdlog::info("UndressManager::UndressPartial - Partially undressing '{}'", actor->GetName());

            // Save original outfit first (only if not already saved)
            SaveOriginalOutfit(actor);

            // Get all armor in inventory
            auto armors = GetInventoryArmor(actor);

            // Unequip outer armor only
            for (auto* armor : armors) {
                if (UndressHelper::IsOuterArmor(armor) && IsArmorEquipped(actor, armor)) {
                    equipManager->UnequipObject(actor, armor, nullptr, 1, nullptr, false, true);
                    spdlog::trace("  - Unequipped outer armor: '{}'", armor->GetFullName());
                }
            }

            // Update state
            SetUndressState(actor, UndressState::PartiallyUndressed);

            spdlog::info("UndressManager::UndressPartial - '{}' is now partially undressed", actor->GetName());
        }

        // Full undress: remove ALL armor
        void UndressFull(RE::Actor* actor)
        {
            if (!actor) return;

            auto* equipManager = RE::ActorEquipManager::GetSingleton();
            if (!equipManager) {
                spdlog::error("UndressManager::UndressFull - No ActorEquipManager");
                return;
            }

            spdlog::info("UndressManager::UndressFull - Fully undressing '{}'", actor->GetName());

            // Save original outfit if not already saved (handles direct full undress)
            SaveOriginalOutfit(actor);

            // Get all armor in inventory
            auto armors = GetInventoryArmor(actor);

            // Unequip ALL armor
            for (auto* armor : armors) {
                if (IsArmorEquipped(actor, armor)) {
                    equipManager->UnequipObject(actor, armor, nullptr, 1, nullptr, false, true);
                    spdlog::trace("  - Unequipped: '{}'", armor->GetFullName());
                }
            }

            // Update state
            SetUndressState(actor, UndressState::FullyUndressed);

            spdlog::info("UndressManager::UndressFull - '{}' is now fully undressed", actor->GetName());
        }

        // Redress: restore original outfit
        void Redress(RE::Actor* actor)
        {
            if (!actor) return;

            spdlog::info("UndressManager::Redress - Re-dressing '{}'", actor->GetName());

            auto* equipManager = RE::ActorEquipManager::GetSingleton();
            if (!equipManager) {
                spdlog::error("UndressManager::Redress - No ActorEquipManager");
                return;
            }

            RE::FormID actorId = actor->GetFormID();

            // Get stored outfit
            StoredOutfit outfit;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto it = m_storedOutfits.find(actorId);
                if (it != m_storedOutfits.end()) {
                    outfit = it->second;
                    m_storedOutfits.erase(it);
                } else {
                    spdlog::warn("UndressManager::Redress - No stored outfit for '{}'", actor->GetName());
                }
            }

            // Unequip everything currently worn first
            auto currentArmor = GetInventoryArmor(actor);
            for (auto* armor : currentArmor) {
                if (IsArmorEquipped(actor, armor)) {
                    equipManager->UnequipObject(actor, armor, nullptr, 1, nullptr, false, true);
                }
            }

            // Re-equip stored outfit
            for (RE::FormID armorId : outfit.equippedArmor) {
                auto* form = RE::TESForm::LookupByID(armorId);
                if (auto* armor = form ? form->As<RE::TESObjectARMO>() : nullptr) {
                    // Only equip if they have it in inventory
                    if (HasItemInInventory(actor, armor)) {
                        equipManager->EquipObject(actor, armor, nullptr, 1, nullptr, true, false, false);
                        spdlog::trace("  - Re-equipped: '{}'", armor->GetFullName());
                    }
                }
            }

            // Clear undress state
            ClearUndressState(actor);

            spdlog::info("UndressManager::Redress - '{}' is now dressed", actor->GetName());
        }

        // Clear state (called on gear change via menu or redress)
        void ClearUndressState(RE::Actor* actor)
        {
            if (!actor) return;

            std::lock_guard<std::mutex> lock(m_mutex);

            RE::FormID actorId = actor->GetFormID();
            m_undressStates.erase(actorId);
            m_storedOutfits.erase(actorId);

            spdlog::trace("UndressManager::ClearUndressState - Cleared state for '{}'", actor->GetName());
        }

        // Query state
        UndressState GetUndressState(RE::Actor* actor) const
        {
            if (!actor) return UndressState::Dressed;

            std::lock_guard<std::mutex> lock(m_mutex);

            auto it = m_undressStates.find(actor->GetFormID());
            return (it != m_undressStates.end()) ? it->second : UndressState::Dressed;
        }

        // Check if actor has any undress state
        bool HasUndressState(RE::Actor* actor) const
        {
            return GetUndressState(actor) != UndressState::Dressed;
        }

        // Clear all state (e.g., on game load)
        void Reset()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_undressStates.clear();
            m_storedOutfits.clear();
            spdlog::info("UndressManager::Reset - Cleared all undress states");
        }

    private:
        UndressManager() = default;
        ~UndressManager() = default;
        UndressManager(const UndressManager&) = delete;
        UndressManager& operator=(const UndressManager&) = delete;

        // Save original outfit (only if not already saved)
        void SaveOriginalOutfit(RE::Actor* actor)
        {
            if (!actor) return;

            RE::FormID actorId = actor->GetFormID();

            std::lock_guard<std::mutex> lock(m_mutex);

            // Only save if we don't already have a stored outfit
            if (m_storedOutfits.find(actorId) != m_storedOutfits.end()) {
                return;
            }

            StoredOutfit outfit;

            // Get all equipped armor
            auto armors = GetInventoryArmor(actor);
            for (auto* armor : armors) {
                if (IsArmorEquipped(actor, armor)) {
                    outfit.equippedArmor.insert(armor->GetFormID());
                }
            }

            m_storedOutfits[actorId] = outfit;

            spdlog::info("UndressManager::SaveOriginalOutfit - Saved {} items for '{}'",
                outfit.equippedArmor.size(), actor->GetName());
        }

        // Set undress state for an actor
        void SetUndressState(RE::Actor* actor, UndressState state)
        {
            if (!actor) return;

            std::lock_guard<std::mutex> lock(m_mutex);
            m_undressStates[actor->GetFormID()] = state;
        }

        // Helper: Get all armor in actor's inventory
        std::vector<RE::TESObjectARMO*> GetInventoryArmor(RE::Actor* actor)
        {
            std::vector<RE::TESObjectARMO*> result;
            if (!actor) return result;

            auto inventory = actor->GetInventory([](RE::TESBoundObject& obj) {
                return obj.Is(RE::FormType::Armor);
            });

            for (const auto& [item, data] : inventory) {
                if (data.first <= 0) continue;
                if (auto* armor = item->As<RE::TESObjectARMO>()) {
                    result.push_back(armor);
                }
            }

            return result;
        }

        // Helper: Check if armor is equipped
        bool IsArmorEquipped(RE::Actor* actor, RE::TESObjectARMO* armor)
        {
            if (!actor || !armor) return false;
            auto slotMask = armor->GetSlotMask();
            auto* wornArmor = actor->GetWornArmor(slotMask);
            return wornArmor && wornArmor->GetFormID() == armor->GetFormID();
        }

        // Helper: Check if actor has item in inventory
        bool HasItemInInventory(RE::Actor* actor, RE::TESBoundObject* item)
        {
            if (!actor || !item) return false;

            auto inventory = actor->GetInventory([item](RE::TESBoundObject& obj) {
                return obj.GetFormID() == item->GetFormID();
            });

            return !inventory.empty();
        }

        // Per-actor undress state
        std::unordered_map<RE::FormID, UndressState> m_undressStates;

        // Per-actor stored outfit (for redress)
        std::unordered_map<RE::FormID, StoredOutfit> m_storedOutfits;

        // Mutex for thread safety
        mutable std::mutex m_mutex;
    };

}  // namespace VRSexMenu
