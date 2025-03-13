#include <ll/api/memory/Hook.h>
#include <ll/api/memory/Memory.h>
#include <ll/api/utils/SystemUtils.h>
#include <mc/deps/core/math/Vec3.h>
#include <mc/deps/ecs/WeakEntityRef.h>
#include <mc/deps/shared_types/legacy/ContainerType.h>
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

#include <magic_enum.hpp>

namespace tool_stats {
enum class LoreType {
    Created,
    Traded,
    Fished,
    Found,
};
inline std::string getNow() {
    auto now = ll::sys_utils::getLocalTime();
    return fmt::format(
        "{:04d}/{:02d}/{:02d} {:02d}:{:02d}:{:02d}",
        now.tm_year + 1900,
        now.tm_mon + 1,
        now.tm_mday,
        now.tm_hour,
        now.tm_min,
        now.tm_sec
    );
}

inline void setLore(ItemStack& item, std::string playerName, LoreType type) {
    auto lores = item.getCustomLore();
    lores.push_back((std::string)magic_enum::enum_name(type) + "By: " + playerName);
    lores.push_back("Time: " + getNow());
    item.setCustomLore(lores);
};

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
        return origin(requestAction, isSwap, isSrcHintSlot, isDstHintSlot);
    ::std::shared_ptr<::SimpleSparseContainer> srcContainer = _getOrInitSparseContainer(src.mFullContainerName);
    if (!srcContainer) return origin(requestAction, isSwap, isSrcHintSlot, isDstHintSlot);
    auto const& srcItem    = srcContainer->getItem(src.mSlot);
    auto&       screenCtx  = getScreenContext();
    auto        screenType = screenCtx.mUnk2a0ccb.as<SharedTypes::Legacy::ContainerType>();
    auto        itemType   = srcItem.getTypeName();
    if (srcItem.isDamageableItem()) {
        switch (screenType) {
        case SharedTypes::Legacy::ContainerType::Workbench:
        case SharedTypes::Legacy::ContainerType::Inventory:
            setLore(const_cast<ItemStack&>(srcItem), mPlayer.getRealName(), LoreType::Created);
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

void hookItemStackRequestActionHandlerTransfer() { trasferHandlerHook::hook(); };
void hookPullFishingHook() { PullFishingHook::hook(); }
} // namespace tool_stats