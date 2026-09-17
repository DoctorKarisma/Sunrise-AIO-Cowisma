#include "store_internal.h"

namespace sunrise::state::investment::store {
namespace {

/** Restores the complete action table, including unbound input halves. */
bool read_bindings(account::settings::AccountSettings& output) noexcept {
    Statement rows(
        "SELECT action,primary_code,secondary_code FROM account_key_bindings ORDER BY action");
    std::size_t count = 0;
    int result = rows.step();
    while (result == SQLITE_ROW) {
        std::size_t index = 0;
        std::int32_t primary = -1;
        std::int32_t secondary = -1;
        if (!rows.columns(index, primary, secondary) || index != count
            || index >= output.keyBindings.values.size() || primary < -1 || primary > 65535
            || secondary < -1 || secondary > 65535) {
            return false;
        }
        auto& binding = output.keyBindings.values[count++];
        if (primary >= 0) {
            binding.primary = static_cast<std::uint16_t>(primary);
        }
        if (secondary >= 0) {
            binding.secondary = static_cast<std::uint16_t>(secondary);
        }
        result = rows.step();
    }
    output.keyBindings.configured =
        result == SQLITE_DONE && count == output.keyBindings.values.size();
    return output.keyBindings.configured;
}

} // namespace

/** Reads every account preference from one database snapshot. */
bool read_settings(account::settings::AccountSettings& output) noexcept {
    Transaction transaction;
    output = {};
    if (!transaction.ready()) {
        return false;
    }
    Statement root("SELECT key_binding_source FROM account_preferences WHERE id=1");
    if (root.step() != SQLITE_ROW || !root.column(0, output.keyBindingSource)) {
        return false;
    }
    Statement controls(
        "SELECT "
        "button_layout,movement_mode,controller_look_sensitivity,controller_invert_vertical,"
        "controller_auto_look_centering,controller_vibration,controller_swap_shoulders,controller_"
        "invert_horizontal,mouse_look_sensitivity,mouse_invert_vertical,mouse_invert_horizontal,"
        "unidentified_toggle,mouse_aim_smoothing,ads_sensitivity_modifier,double_press_delay FROM "
        "account_controls WHERE id=1");
    if (controls.step() != SQLITE_ROW
        || !controls.columns(output.controls.buttonLayout,
                             output.controls.movementMode,
                             output.controls.controllerLookSensitivity,
                             output.controls.controllerInvertVertical,
                             output.controls.controllerAutoLookCentering,
                             output.controls.controllerVibration,
                             output.controls.controllerSwapShoulders,
                             output.controls.controllerInvertHorizontal,
                             output.controls.mouseLookSensitivity,
                             output.controls.mouseInvertVertical,
                             output.controls.mouseInvertHorizontal,
                             output.controls.unidentifiedToggle,
                             output.controls.mouseAimSmoothing,
                             output.controls.adsSensitivityModifier,
                             output.controls.doublePressDelay)) {
        return false;
    }
    Statement audio("SELECT "
                    "voice_output_mode,team_voice_channel,reserved_mode,migration_version,chat_"
                    "volume,mute_when_unfocused,sound_effects_volume,dialogue_volume,music_volume "
                    "FROM account_audio WHERE id=1");
    if (audio.step() != SQLITE_ROW
        || !audio.columns(output.audio.voiceOutputMode,
                          output.audio.teamVoiceChannel,
                          output.audio.reservedMode,
                          output.audio.migrationVersion,
                          output.audio.chatVolume,
                          output.audio.muteWhenUnfocused,
                          output.audio.soundEffectsVolume,
                          output.audio.dialogueVolume,
                          output.audio.musicVolume)) {
        return false;
    }
    Statement display("SELECT "
                      "brightness,show_fps,hdr_mode,vertical_sync_interval,field_of_view,"
                      "calibration_primary,calibration_alpha FROM account_display WHERE id=1");
    if (display.step() != SQLITE_ROW
        || !display.columns(output.display.brightness,
                            output.display.showFps,
                            output.display.hdrMode,
                            output.display.verticalSyncInterval,
                            output.display.fieldOfView,
                            output.display.calibrationPrimary,
                            output.display.calibrationAlpha)) {
        return false;
    }
    Statement interface(
        "SELECT "
        "subtitles_mode,colorblind_mode,helmet_mode,hud_opacity,display_hints,background_opacity,"
        "reticle_location,reticle_color,text_size,text_color,text_background_style,text_background_"
        "opacity,reserved_text_mode,subtitle_options_entry FROM account_interface WHERE id=1");
    if (interface.step() != SQLITE_ROW
        || !interface.columns(output.interface.subtitlesMode,
                              output.interface.colorblindMode,
                              output.interface.helmetMode,
                              output.interface.hudOpacity,
                              output.interface.displayHints,
                              output.interface.backgroundOpacity,
                              output.interface.reticleLocation,
                              output.interface.reticleColor,
                              output.interface.textSize,
                              output.interface.textColor,
                              output.interface.textBackgroundStyle,
                              output.interface.textBackgroundOpacity,
                              output.interface.reservedTextMode,
                              output.interface.subtitleOptionsEntry)) {
        return false;
    }
    Statement social(
        "SELECT "
        "prefer_good_connection,text_chat_mode,show_real_names,clan_invite_notifications,profanity_"
        "filter,voice_chat_enabled,whisper_chat_mode,team_chat_join_mode,local_chat_join_mode,clan_"
        "chat_join_mode,chat_auto_hide_mode FROM account_social WHERE id=1");
    if (social.step() != SQLITE_ROW
        || !social.columns(output.social.preferGoodConnection,
                           output.social.textChatMode,
                           output.social.showRealNames,
                           output.social.clanInviteNotifications,
                           output.social.profanityFilter,
                           output.social.voiceChatEnabled,
                           output.social.whisperChatMode,
                           output.social.teamChatJoinMode,
                           output.social.localChatJoinMode,
                           output.social.clanChatJoinMode,
                           output.social.chatAutoHideMode)) {
        return false;
    }
    output.configured = true;
    return read_bindings(output) && account::settings::valid(output) && transaction.commit();
}

/** Preference and binding changes commit together. */
bool write_settings(const account::settings::AccountSettings& value) noexcept {
    if (!account::settings::valid(value)) {
        return false;
    }
    Transaction transaction;
    if (!transaction.ready()) {
        return false;
    }
    Statement root("INSERT OR REPLACE INTO account_preferences VALUES(1,?)");
    if (!root.write(value.keyBindingSource)) {
        return false;
    }
    Statement controls(
        "INSERT OR REPLACE INTO account_controls VALUES(1,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    if (!controls.write(value.controls.buttonLayout,
                        value.controls.movementMode,
                        value.controls.controllerLookSensitivity,
                        value.controls.controllerInvertVertical,
                        value.controls.controllerAutoLookCentering,
                        value.controls.controllerVibration,
                        value.controls.controllerSwapShoulders,
                        value.controls.controllerInvertHorizontal,
                        value.controls.mouseLookSensitivity,
                        value.controls.mouseInvertVertical,
                        value.controls.mouseInvertHorizontal,
                        value.controls.unidentifiedToggle,
                        value.controls.mouseAimSmoothing,
                        value.controls.adsSensitivityModifier,
                        value.controls.doublePressDelay)) {
        return false;
    }
    Statement audio("INSERT OR REPLACE INTO account_audio VALUES(1,?,?,?,?,?,?,?,?,?)");
    if (!audio.write(value.audio.voiceOutputMode,
                     value.audio.teamVoiceChannel,
                     value.audio.reservedMode,
                     value.audio.migrationVersion,
                     value.audio.chatVolume,
                     value.audio.muteWhenUnfocused,
                     value.audio.soundEffectsVolume,
                     value.audio.dialogueVolume,
                     value.audio.musicVolume)) {
        return false;
    }
    Statement display("INSERT OR REPLACE INTO account_display VALUES(1,?,?,?,?,?,?,?)");
    if (!display.write(value.display.brightness,
                       value.display.showFps,
                       value.display.hdrMode,
                       value.display.verticalSyncInterval,
                       value.display.fieldOfView,
                       value.display.calibrationPrimary,
                       value.display.calibrationAlpha)) {
        return false;
    }
    Statement interface(
        "INSERT OR REPLACE INTO account_interface VALUES(1,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    if (!interface.write(value.interface.subtitlesMode,
                         value.interface.colorblindMode,
                         value.interface.helmetMode,
                         value.interface.hudOpacity,
                         value.interface.displayHints,
                         value.interface.backgroundOpacity,
                         value.interface.reticleLocation,
                         value.interface.reticleColor,
                         value.interface.textSize,
                         value.interface.textColor,
                         value.interface.textBackgroundStyle,
                         value.interface.textBackgroundOpacity,
                         value.interface.reservedTextMode,
                         value.interface.subtitleOptionsEntry)) {
        return false;
    }
    Statement social("INSERT OR REPLACE INTO account_social VALUES(1,?,?,?,?,?,?,?,?,?,?,?)");
    if (!social.write(value.social.preferGoodConnection,
                      value.social.textChatMode,
                      value.social.showRealNames,
                      value.social.clanInviteNotifications,
                      value.social.profanityFilter,
                      value.social.voiceChatEnabled,
                      value.social.whisperChatMode,
                      value.social.teamChatJoinMode,
                      value.social.localChatJoinMode,
                      value.social.clanChatJoinMode,
                      value.social.chatAutoHideMode)) {
        return false;
    }
    if (!execute("DELETE FROM account_key_bindings")) {
        return false;
    }
    Statement binding("INSERT INTO account_key_bindings VALUES(?,?,?)");
    for (std::size_t index = 0; index < value.keyBindings.values.size(); ++index) {
        const auto& row = value.keyBindings.values[index];
        if (!binding.write(index,
                           row.primary ? static_cast<int>(*row.primary) : -1,
                           row.secondary ? static_cast<int>(*row.secondary) : -1)) {
            return false;
        }
    }
    return transaction.commit();
}

} // namespace sunrise::state::investment::store
