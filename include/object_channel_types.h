#ifndef TOCCATA_UI_OBJECT_CHANNEL_TYPES_H
#define TOCCATA_UI_OBJECT_CHANNEL_TYPES_H

#include <piranha.h>

namespace toccata {

    struct ObjectChannel {
        static const piranha::ChannelType SettingChannel;
        static const piranha::ChannelType ProfileChannel;
        static const piranha::ChannelType HeatMapChannel;
    };

    template <typename Type>
    extern inline const piranha::ChannelType *LookupChannelType() {
        static_assert(false, "Invalid type lookup");
        return nullptr;
    }

    // Forward declaration of all types
    class Profile;
    class Setting;
    class HeatMap;

    // Helper macro
#define ASSIGN_CHANNEL_TYPE(type, channel) template <> extern inline const piranha::ChannelType *LookupChannelType<type>() { return &ObjectChannel::channel; }

    // Register all types
    ASSIGN_CHANNEL_TYPE(Profile, ProfileChannel);
    ASSIGN_CHANNEL_TYPE(Setting, SettingChannel);
    ASSIGN_CHANNEL_TYPE(HeatMap, HeatMapChannel);

} /* namespace toccata */

#endif /* TOCCATA_UI_OBJECT_CHANNEL_TYPES_H */
