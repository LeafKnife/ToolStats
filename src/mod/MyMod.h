#pragma once

#include "ll/api/mod/NativeMod.h"

namespace lk {

class MyMod {

public:
    static MyMod& getInstance();

    MyMod() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    /// @return True if the mod is loaded successfully.
    bool load();

    /// @return True if the mod is enabled successfully.
    bool enable();

    /// @return True if the mod is disabled successfully.
    bool disable();

    // TODO: Implement this method if you need to unload the mod.
    // /// @return True if the mod is unloaded successfully.
    // bool unload();

private:
    ll::mod::NativeMod& mSelf;
};

} // namespace lk

namespace tool_stats {
void hookPullFishingHook();
void hookItemStackRequestActionHandlerTransfer();
void hookLootTableUtilsFillContainer();
void hookLootTableUtilsGenerateRandomDeathLoot();
} // namespace tool_stats
