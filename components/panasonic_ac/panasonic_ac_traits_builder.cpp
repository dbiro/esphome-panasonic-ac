#include "panasonic_ac_traits_builder.h"

#include "esphome/core/log.h"

namespace esphome {
namespace panasonic_ac {

static const char* TAG = "panasonic_ac";

PanasonicACTraitsBuilder::PanasonicACTraitsBuilder(climate::Climate &climate_entity, Component &component)
    : climate_(climate_entity), component_(component) {

}

void PanasonicACTraitsBuilder::add_horizontal_swing_mode() {
    // The traits are cached after the first build, so a later change would be silently ignored
    if (this->traits_built_) {
        this->component_.mark_failed(LOG_STR("Horizontal swing mode added after the traits were built"));
        return;
    }

    this->traits.add_supported_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);
    if (this->traits.supports_swing_mode(climate::CLIMATE_SWING_VERTICAL)) {
        this->traits.add_supported_swing_mode(climate::CLIMATE_SWING_BOTH);
    }
}

void PanasonicACTraitsBuilder::add_vertical_swing_mode() {
    // The traits are cached after the first build, so a later change would be silently ignored
    if (this->traits_built_) {
        this->component_.mark_failed(LOG_STR("Vertical swing mode added after the traits were built"));
        return;
    }

    this->traits.add_supported_swing_mode(climate::CLIMATE_SWING_VERTICAL);
    if (this->traits.supports_swing_mode(climate::CLIMATE_SWING_HORIZONTAL)) {
        this->traits.add_supported_swing_mode(climate::CLIMATE_SWING_BOTH);
    }
}

const climate::ClimateTraits &PanasonicACTraitsBuilder::build_traits() {
    if (!this->traits_built_) {
        this->populate_traits_();
        this->traits_built_ = true;
    }
    return this->traits;
}

void PanasonicACTraitsBuilder::populate_traits_() {
    // Enable actions and current temperature support; two point target temperature stays disabled
    this->traits.add_feature_flags(
        climate::CLIMATE_SUPPORTS_ACTION |
        climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE
    );

    // Set visual temperature parameters
    this->traits.set_visual_min_temperature(MIN_TEMPERATURE);
    this->traits.set_visual_max_temperature(MAX_TEMPERATURE);
    this->traits.set_visual_temperature_step(TEMPERATURE_STEP);
    
    // Set default supported modes
    this->traits.set_supported_modes({
        climate::CLIMATE_MODE_OFF,
        climate::CLIMATE_MODE_HEAT,
        climate::CLIMATE_MODE_COOL,
        climate::CLIMATE_MODE_HEAT_COOL,
        climate::CLIMATE_MODE_FAN_ONLY
        // climate::CLIMATE_MODE_DRY, TODO: do i need this?
    });
    
    // Set default OFF swing mode
    this->traits.add_supported_swing_mode(climate::CLIMATE_SWING_OFF);
    
    // Register the custom fan modes and presets on the climate entity itself
    // - Set default custom presets
    this->climate_.set_supported_custom_presets({
        PRESET_NONE,
        PRESET_QUIET,
        PRESET_POWERFUL
    });
    // - Set default fan speed levels
    this->climate_.set_supported_custom_fan_modes({
        FAN_SPEED_LEVEL_AUTO,
        FAN_SPEED_LEVEL_1,
        FAN_SPEED_LEVEL_2,
        FAN_SPEED_LEVEL_3,
        FAN_SPEED_LEVEL_4,
        FAN_SPEED_LEVEL_5,
    });

    ESP_LOGI(TAG, "Climate traits are populated");
}

}
}