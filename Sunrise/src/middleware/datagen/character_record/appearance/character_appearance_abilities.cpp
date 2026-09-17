#include <array>

#include "../../../../core/logging/log.h"

#include "../../../../state/build_data/runtime.h"
#include "../../../../state/runtime/subclass_ability_override.h"
#include "internal.h"

namespace sunrise::middleware::datagen::character_record::appearance {
namespace {

namespace buckets = state::build_data::abilities;
namespace override_state = state::runtime::subclass_ability_override;

/** The authored equipment slot that holds the subclass. */
constexpr std::size_t kSubclassSlot =
    static_cast<std::size_t>(state::account::inventory::EquipmentSlot::subclass);

/** @param item Authored subclass item. @return Its 5 selected socket entries. */
[[nodiscard]] buckets::Selection
selection_of(const state::account::inventory::Item& item) noexcept {
    return {item.movementAbilityEntry,
            item.grenadeAbilityEntry,
            item.superAbilityEntry,
            item.meleeAbilityEntry,
            item.classAbilityEntry};
}

/** Copies one published bucket into a character-record bucket without carrying stale hashes. */
void copy_bucket(const buckets::Bucket& source, layout::AbilityBucket& target) noexcept {
    target = {};
    target.kind = static_cast<std::int8_t>(source.kind);
    for (std::size_t entry = 0; entry < source.hashCount && entry < target.hashes.size(); ++entry) {
        target.hashes[entry] = source.hashes[entry];
    }
}

/** Adds one individual native tree-node hash to a target bucket without replacing existing hashes. */
void merge_hash(std::uint32_t hash, std::int8_t sourceKind, layout::AbilityBucket& target) noexcept {
    if (hash == 0) return;
    for (const std::uint32_t held : target.hashes) if (held == hash) return;
    if (target.kind == layout::kEmptyKey && sourceKind != layout::kEmptyKey) target.kind = sourceKind;
    for (std::uint32_t& held : target.hashes) {
        if (held == 0 || held == layout::kNoHash) { held = hash; return; }
    }
}

/** Finds the destination bucket with the same native semantic kind, or a free bucket for transplant. */
[[nodiscard]] std::size_t semantic_target_bucket(std::int8_t sourceKind,
                                                 layout::Appearance& appearance) noexcept {
    for (std::size_t index = 0; index < appearance.abilityBuckets.size(); ++index) {
        if (appearance.abilityBuckets[index].kind == sourceKind) return index;
    }
    for (std::size_t index = 0; index < appearance.abilityBuckets.size(); ++index) {
        if (appearance.abilityBuckets[index].kind == layout::kEmptyKey) return index;
    }
    return appearance.abilityBuckets.size();
}

/** Merges the full native bucket payload into its semantic destination without clearing target state. */
void merge_native_bucket(const buckets::Bucket& source,
                         std::uint32_t treeNodeHash,
                         layout::AbilityBucket& target) noexcept {
    const auto sourceKind = static_cast<std::int8_t>(source.kind);
    for (std::size_t index = 0; index < source.hashCount && index < source.hashes.size(); ++index) {
        merge_hash(source.hashes[index], sourceKind, target);
    }
    merge_hash(treeNodeHash, sourceKind, target);
}

/** The native 4-node path group (Way / Code / Attunement) selected by a Super-path override. */
struct PathBundle {
    std::array<std::uint8_t, state::kMaxAttunementBundleSize> members{};
    std::size_t count{};
};

[[nodiscard]] bool path_bundle(const override_state::Choice& choice, PathBundle& bundle) noexcept {
    bundle = {};
    if (!choice.enabled || choice.sourcePath >= 3) return false;
    state::build_data::socket_entry_lists::EntryTable entries{};
    if (!state::build_data::find_socket_entry_table(choice.sourceSocketEntryListIndex, entries)) {
        return false;
    }
    std::array<std::uint8_t, 256> populations{};
    for (const auto& entry : entries.entries) {
        if (entry.group != state::build_data::socket_entry_lists::kNoEntryGroup) {
            ++populations[entry.group];
        }
    }
    std::uint8_t pathGroup = state::build_data::socket_entry_lists::kNoEntryGroup;
    for (std::size_t group = 0; group < populations.size(); ++group) {
        if (populations[group] > state::kMaxAttunementBundleSize) {
            pathGroup = static_cast<std::uint8_t>(group);
            break;
        }
    }
    if (pathGroup == state::build_data::socket_entry_lists::kNoEntryGroup) return false;

    std::array<std::uint8_t, state::build_data::socket_entry_lists::kEntryCapacity> members{};
    std::size_t memberCount = 0;
    for (std::size_t index = 0; index < entries.entries.size(); ++index) {
        if (entries.entries[index].group == pathGroup && memberCount < members.size()) {
            members[memberCount++] = static_cast<std::uint8_t>(index);
        }
    }
    const std::size_t begin = static_cast<std::size_t>(choice.sourcePath)
                              * state::kMaxAttunementBundleSize;
    if (begin >= memberCount) return false;
    const std::size_t end = (std::min)(memberCount, begin + state::kMaxAttunementBundleSize);
    for (std::size_t index = begin; index < end; ++index) {
        bundle.members[bundle.count++] = members[index];
    }
    return bundle.count != 0;
}

} // namespace

/** Fills the 12 ability buckets from the character's subclass and ability picks. */
bool apply_ability_buckets(const state::CharacterState& character,
                           const family4::loadout::ResolvedInstances& instances,
                           layout::Appearance& appearance) noexcept {
    for (std::size_t index = 0; index < instances.itemCount; ++index) {
        if (instances.items[index].equipmentSlot != kSubclassEquipmentSlot) {
            continue;
        }
        const auto& subclassItem = character.equipment.slots[kSubclassSlot];
        if (!subclassItem.has_value()) {
            return false;
        }
        details::Definition detail{};
        buckets::Definition published{};
        if (!state::build_data::find_configured_item_detail(
                instances.items[index].instance.baseDefinitionIndex, detail)) {
            return false;
        }
        if (!state::build_data::find_ability_buckets(
                detail.socketEntryListIndex, selection_of(*subclassItem), published)) {
            // The domain has not caught up with this selection yet. Publish empty buckets for
            // this encode, like a character with no subclass, instead of failing: a hard failure
            // aborts the whole Family-0/3 snapshot even though the selection did commit.
            return true;
        }
        for (std::size_t bucket = 0; bucket < appearance.abilityBuckets.size(); ++bucket) {
            copy_bucket(published.buckets[bucket], appearance.abilityBuckets[bucket]);
        }

        // AIO Subclass Editor overrides are applied at the final character-record publication
        // boundary. The authored subclass item remains intact while the client receives the
        // selected foreign ability bucket in the semantic slot it already expects.
        using override_slot = override_state::Slot;
        struct OverrideRoute {
            override_slot slot;
            std::uint8_t targetEntry;
        };
        const std::array<OverrideRoute, 5> overrideRoutes{{
            {override_slot::movement, subclassItem->movementAbilityEntry},
            {override_slot::grenade, subclassItem->grenadeAbilityEntry},
            {override_slot::super, subclassItem->superAbilityEntry},
            {override_slot::melee, subclassItem->meleeAbilityEntry},
            {override_slot::classAbility, subclassItem->classAbilityEntry},
        }};
        for (const OverrideRoute& route : overrideRoutes) {
            override_state::Choice choice{};
            if (!override_state::get(character.soid, route.slot, choice)) {
                continue;
            }
            std::uint8_t targetBucket =
                state::build_data::socket_entry_buckets::kNoDestinationBucket;
            std::uint8_t sourceBucket =
                state::build_data::socket_entry_buckets::kNoDestinationBucket;
            buckets::Definition sourcePublished{};
            if (!state::build_data::find_socket_entry_bucket(
                    detail.socketEntryListIndex, route.targetEntry, targetBucket)
                || !state::build_data::find_socket_entry_bucket(choice.sourceSocketEntryListIndex,
                                                                 choice.sourceEntry,
                                                                 sourceBucket)
                || targetBucket >= appearance.abilityBuckets.size()
                || sourceBucket >= sourcePublished.buckets.size()
                || !state::build_data::find_ability_buckets(choice.sourceSocketEntryListIndex,
                                                             choice.sourceSelection,
                                                             sourcePublished)) {
                continue;
            }
            copy_bucket(sourcePublished.buckets[sourceBucket], appearance.abilityBuckets[targetBucket]);
        }

        // A Super selector entry represents a complete native Way / Code / Attunement path, not
        // only the rendered Super. Carry that path's bundled melee and passive buckets with it.
        // An explicit Melee override still wins. Individual Secondary Perks are layered below.
        override_state::Choice superChoice{};
        override_state::Choice meleeChoice{};
        const bool hasSuperPath = override_state::get(character.soid, override_slot::super, superChoice)
                                  && superChoice.sourcePath < 3;
        const bool hasMeleeOverride = override_state::get(character.soid, override_slot::melee, meleeChoice);
        if (hasSuperPath) {
            PathBundle bundle{};
            buckets::Definition sourcePublished{};
            std::uint8_t sourceMeleeBucket =
                state::build_data::socket_entry_buckets::kNoDestinationBucket;
            std::uint8_t sourceSuperBucket =
                state::build_data::socket_entry_buckets::kNoDestinationBucket;
            std::uint8_t targetMeleeBucket =
                state::build_data::socket_entry_buckets::kNoDestinationBucket;
            if (path_bundle(superChoice, bundle)
                && state::build_data::find_ability_buckets(superChoice.sourceSocketEntryListIndex,
                                                           superChoice.sourceSelection,
                                                           sourcePublished)
                && state::build_data::find_socket_entry_bucket(superChoice.sourceSocketEntryListIndex,
                                                                state::kDefaultMeleeAbilityEntry,
                                                                sourceMeleeBucket)
                && state::build_data::find_socket_entry_bucket(superChoice.sourceSocketEntryListIndex,
                                                                state::kDefaultSuperAbilityEntry,
                                                                sourceSuperBucket)
                && state::build_data::find_socket_entry_bucket(detail.socketEntryListIndex,
                                                                subclassItem->meleeAbilityEntry,
                                                                targetMeleeBucket)) {
                for (std::size_t member = 0; member < bundle.count; ++member) {
                    std::uint8_t sourceBucket =
                        state::build_data::socket_entry_buckets::kNoDestinationBucket;
                    if (!state::build_data::find_socket_entry_bucket(superChoice.sourceSocketEntryListIndex,
                                                                     bundle.members[member],
                                                                     sourceBucket)
                        || sourceBucket >= sourcePublished.buckets.size()) {
                        continue;
                    }
                    if (sourceBucket == sourceSuperBucket) {
                        // Super itself was already copied through the explicit semantic route above.
                        continue;
                    }
                    if (sourceBucket == sourceMeleeBucket) {
                        if (!hasMeleeOverride && targetMeleeBucket < appearance.abilityBuckets.size()) {
                            copy_bucket(sourcePublished.buckets[sourceBucket],
                                        appearance.abilityBuckets[targetMeleeBucket]);
                        }
                        continue;
                    }
                    // Do not copy any other path member by raw bucket index. Bucket numbering is
                    // local to a subclass definition; carrying a source index into the destination
                    // can overwrite Movement/Grenade/Class Ability with an unrelated passive node.
                    // Secondary path perks are handled explicitly below.
                }
            }
        }

        // Keep native overflow unchanged. Secondary Perks are individual path nodes, not whole
        // Ways/Codes/Attunements: route each selected node back into the exact semantic bucket
        // discovered from its source entry. This is required for active nodes such as Phoenix Dive,
        // Arc Soul and Second Shield; placing their hash in generic overflow is not sufficient.
        std::size_t overflowCount = 0;
        for (std::size_t entry = 0; entry < published.overflowCount
             && overflowCount < appearance.overflowHashes.size(); ++entry) {
            const std::uint32_t hash = published.overflow[entry];
            if (hash != 0) appearance.overflowHashes[overflowCount++] = hash;
        }
        for (const override_slot slot : {override_slot::secondary1, override_slot::secondary2}) {
            override_state::Choice choice{};
            if (!override_state::get(character.soid, slot, choice) || choice.secondaryHash == 0) continue;

            // Passive tree nodes are also valid overflow hashes in the character appearance record.
            // Publish there first so nodes without a normal ability-bucket destination remain usable.
            bool alreadyOverflow = false;
            for (std::size_t held = 0; held < overflowCount; ++held) {
                if (appearance.overflowHashes[held] == choice.secondaryHash) {
                    alreadyOverflow = true;
                    break;
                }
            }
            if (!alreadyOverflow && overflowCount < appearance.overflowHashes.size()) {
                appearance.overflowHashes[overflowCount++] = choice.secondaryHash;
            }

            std::uint8_t sourceBucket = state::build_data::socket_entry_buckets::kNoDestinationBucket;
            buckets::Definition sourcePublished{};
            if (!state::build_data::find_socket_entry_bucket(choice.sourceSocketEntryListIndex,
                                                               choice.sourceEntry, sourceBucket)
                || !state::build_data::find_ability_buckets(choice.sourceSocketEntryListIndex,
                                                             choice.sourceSelection, sourcePublished)
                || sourceBucket >= sourcePublished.buckets.size()) {
                core::log::writef(core::log::Channel::middleware, core::log::Level::warn,
                    "ev=subclass_secondary_publish result=no_source entry=%u hash=0x%08X",
                    static_cast<unsigned>(choice.sourceEntry), choice.secondaryHash);
                continue;
            }

            const auto& nativeBucket = sourcePublished.buckets[sourceBucket];
            const auto sourceKind = static_cast<std::int8_t>(nativeBucket.kind);
            const std::size_t targetBucket = semantic_target_bucket(sourceKind, appearance);
            if (targetBucket >= appearance.abilityBuckets.size()) {
                core::log::writef(core::log::Channel::middleware, core::log::Level::warn,
                    "ev=subclass_secondary_publish result=no_target entry=%u hash=0x%08X source_bucket=%u kind=%d",
                    static_cast<unsigned>(choice.sourceEntry), choice.secondaryHash,
                    static_cast<unsigned>(sourceBucket), static_cast<int>(sourceKind));
                continue;
            }

            if (choice.secondaryNativeTransplant) {
                merge_native_bucket(nativeBucket, choice.secondaryHash, appearance.abilityBuckets[targetBucket]);
            } else {
                merge_hash(choice.secondaryHash, sourceKind, appearance.abilityBuckets[targetBucket]);
            }
            core::log::writef(core::log::Channel::middleware, core::log::Level::info,
                "ev=subclass_secondary_publish result=ok entry=%u hash=0x%08X source_bucket=%u target_bucket=%zu kind=%d native_hashes=%u route=%s",
                static_cast<unsigned>(choice.sourceEntry), choice.secondaryHash,
                static_cast<unsigned>(sourceBucket), targetBucket, static_cast<int>(sourceKind),
                static_cast<unsigned>(nativeBucket.hashCount),
                choice.secondaryNativeTransplant ? "native+overflow" : "semantic+overflow");
        }
        return true;
    }
    // A character with no subclass equipped publishes empty buckets, which is what the client
    // computes for it as well.
    return true;
}

} // namespace sunrise::middleware::datagen::character_record::appearance
