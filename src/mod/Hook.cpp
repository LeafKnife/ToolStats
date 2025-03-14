#include "mc/world/actor/ActorType.h"
#include "mod/MyMod.h"

#include <ll/api/memory/Hook.h>
#include <ll/api/memory/Memory.h>
#include <ll/api/utils/SystemUtils.h>
#include <mc/deps/core/math/Vec3.h>
#include <mc/deps/core/utility/AutomaticID.h>
#include <mc/deps/ecs/WeakEntityRef.h>
#include <mc/deps/shared_types/legacy/ContainerType.h>
#include <mc/util/LootTableUtils.h>
#include <mc/world/Container.h>
#include <mc/world/SimpleSparseContainer.h>
#include <mc/world/actor/Actor.h>
#include <mc/world/actor/FishingHook.h>
#include <mc/world/actor/item/ItemActor.h>
#include <mc/world/actor/player/Inventory.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/PlayerInventory.h>
#include <mc/world/containers/ContainerEnumName.h>
#include <mc/world/containers/FullContainerName.h>
#include <mc/world/inventory/network/ContainerScreenContext.h>
#include <mc/world/inventory/network/ItemStackNetManagerServer.h>
#include <mc/world/inventory/network/ItemStackNetResult.h>
#include <mc/world/inventory/network/ItemStackRequestActionHandler.h>
#include <mc/world/inventory/network/ItemStackRequestActionTransferBase.h>
#include <mc/world/inventory/network/ItemStackRequestActionType.h>
#include <mc/world/inventory/network/ItemStackRequestSlotInfo.h>
#include <mc/world/item/Item.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/item/ItemStackBase.h>
#include <mc/world/level/Level.h>
#include <mc/world/level/block/actor/RandomizableBlockActorContainerBase.h>
#include <mc/world/level/storage/loot/LootTable.h>

#include <magic_enum.hpp>
#include <string>

namespace tool_stats {
auto& logger = lk::MyMod::getInstance().getSelf().getLogger();
enum class LoreType { Crafted, Traded, Fished, Found, Looted, Dropped, Killed };
inline std::string getNow() {
    auto now = ll::sys_utils::getLocalTime();
    return fmt::format("{:04d}/{:02d}/{:02d}", now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);
}

inline void setLore(ItemStack& item, std::string playerName, LoreType type) {
    auto lores = item.getCustomLore();
    if (!playerName.empty())
        lores.push_back("§r§3" + (std::string)magic_enum::enum_name(type) + "By: §r" + "§g" + playerName + "§r");
    lores.push_back("§r§3" + (std::string)magic_enum::enum_name(type) + "At: §r" + "§g" + getNow() + "§r");
    item.setCustomLore(lores);
};

inline void setDeathLore(ItemStack& item, std::string playerName, std::string mobType) {
    auto type  = LoreType::Dropped;
    auto lores = item.getCustomLore();
    lores.push_back("§r§3" + (std::string)magic_enum::enum_name(type) + "By: §r" + "§g" + mobType + "§r");
    if (!playerName.empty()) {
        lores.push_back(
            "§r§3" + (std::string)magic_enum::enum_name(LoreType::Killed) + "By: §r" + "§g" + playerName + "§r"
        );
    }
    lores.push_back("§r§3" + (std::string)magic_enum::enum_name(type) + "At: §r" + "§g" + getNow() + "§r");
    item.setCustomLore(lores);
}

LL_TYPE_INSTANCE_HOOK(
    trasferHandlerHook,
    HookPriority::Normal,
    ItemStackRequestActionHandler,
    &ItemStackRequestActionHandler::_handleTransfer,
    ::ItemStackNetResult,
    ::ItemStackRequestActionTransferBase const& requestAction,
    bool                                        isSrcHintSlot,
    bool                                        isDstHintSlot,
    bool                                        isSwap
) {
    auto& src = ll::memory::dAccess<ItemStackRequestSlotInfo>(&requestAction.mSrc, 4);
    // auto& dst = ll::memory::dAccess<ItemStackRequestSlotInfo>(&requestAction.mDst, 4);

    if (src.mFullContainerName.mName != ContainerEnumName::CreatedOutputContainer)
        return origin(requestAction, isSrcHintSlot, isDstHintSlot, isSwap);
    ::std::shared_ptr<::SimpleSparseContainer> srcContainer = _getOrInitSparseContainer(src.mFullContainerName);
    if (!srcContainer) return origin(requestAction, isSrcHintSlot, isDstHintSlot, isSwap);
    auto const& srcItem    = srcContainer->getItem(src.mSlot);
    auto&       screenCtx  = getScreenContext();
    auto        screenType = screenCtx.mUnk2a0ccb.as<SharedTypes::Legacy::ContainerType>();
    auto        itemType   = srcItem.getTypeName();
    if (srcItem.isDamageableItem()) {
        switch (screenType) {
        case SharedTypes::Legacy::ContainerType::Workbench:
        case SharedTypes::Legacy::ContainerType::Inventory:
            setLore(const_cast<ItemStack&>(srcItem), mPlayer.getRealName(), LoreType::Crafted);
            break;
        case SharedTypes::Legacy::ContainerType::Trade:
            setLore(const_cast<ItemStack&>(srcItem), mPlayer.getRealName(), LoreType::Traded);
            break;
        default:
            break;
        }
    }
    return origin(requestAction, isSrcHintSlot, isDstHintSlot, isSwap);
}

LL_TYPE_INSTANCE_HOOK(
    PullFishingHook,
    HookPriority::Normal,
    FishingHook,
    &FishingHook::_pullCloser,
    void,
    Actor& inEntity,
    float  inSpeed
) {
    if (inEntity.hasType(::ActorType::ItemEntity)) {
        auto& itemActor = static_cast<ItemActor&>(inEntity);
        auto& item      = itemActor.item();
        if (!item.isNull() && item.isDamageableItem()) {
            setLore(item, getPlayerOwner()->getRealName(), LoreType::Fished);
        }
    }
    origin(inEntity, inSpeed);
}

LL_TYPE_STATIC_HOOK(
    LootTableUtilsFillContainerHook,
    HookPriority::Normal,
    Util::LootTableUtils,
    &Util::LootTableUtils::fillContainer,
    void,
    ::Level&             level,
    ::Container&         container,
    ::Random&            random,
    ::std::string const& tableName,
    ::DimensionType      dimensionId,
    ::Actor*             entity
) {
    // logger.info("tableName {} dim {} ctType {}", tableName, dimensionId.id, container.getTypeName());
    origin(level, container, random, tableName, dimensionId, entity);
    std::string name;
    if (entity && entity->hasType(ActorType::Player)) {
        auto player = entity->getWeakEntity().tryUnwrap<Player>();
        name        = player->getRealName();
    }
    // 破坏箱子不记录，仅记录打开箱子
    auto slots = container.getSlots();
    for (int index = 0; (size_t)index < slots.size(); index++) {
        auto& item = container.getItem(index);
        if (item.isNull()) continue;
        if (item.isDamageableItem()) {
            setLore(const_cast<ItemStack&>(item), name, LoreType::Looted);
            container.setContainerChanged(index);
        };
    }
}

LL_TYPE_STATIC_HOOK(
    LootTableUtilsGenerateRandomDeathLoot,
    HookPriority::Normal,
    Util::LootTableUtils,
    &Util::LootTableUtils::generateRandomDeathLoot,
    ::std::vector<::ItemStack>,
    ::LootTable const&         table,
    ::Actor&                   entity,
    ::ActorDamageSource const* damageSource,
    ::ItemStack const*         tool,
    ::Player*                  killer,
    float                      luck
) {

    std::string name;
    if (killer) {
        name = killer->getRealName();
    }
    // logger.info("entity {} tool {} player {}", entity.getTypeName(), tool ? tool->getTypeName() : "", name);
    auto r = origin(table, entity, damageSource, tool, killer, luck);
    for (auto& item : r) {
        if (item.isDamageableItem()) setDeathLore(item, name, entity.getTypeName());
    }
    return r;
}

void hookItemStackRequestActionHandlerTransfer() { trasferHandlerHook::hook(); }
void hookPullFishingHook() { PullFishingHook::hook(); }
void hookLootTableUtilsFillContainer() { LootTableUtilsFillContainerHook::hook(); }
void hookLootTableUtilsGenerateRandomDeathLoot() { LootTableUtilsGenerateRandomDeathLoot::hook(); }
} // namespace tool_stats