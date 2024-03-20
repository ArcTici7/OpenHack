#include "hitboxes.hpp"
#include "../../menu/menu.hpp"

#include <deque>

namespace openhack::hacks {

    static ToggleComponent *s_hitboxesToggle = nullptr;
    static bool s_isDead = false, s_skipDrawHook = false;
    static std::deque<gd::cocos2d::CCRect> s_playerTrail1, s_playerTrail2;

    inline void togglePatch() {
        if (!s_hitboxesToggle) return;

        bool enabled = config::get<bool>("hack.hitboxes.enabled", false);
        enabled |= config::get<bool>("hack.hitboxes.death", false);

        s_hitboxesToggle->applyPatch(enabled);
    }

    inline bool
    appendPatch(std::vector<gd::sigscan::Opcode> &opcodes, const std::string &pattern, const std::string &mask) {
        auto patch = gd::sigscan::match(pattern, mask);
        if (patch.empty()) return false;

        opcodes.insert(opcodes.end(), patch.begin(), patch.end());
        return true;
    }

    /// @brief Whether any type of hitboxes should be drawn. Used to make sure we don't mess with the game if the feature is disabled.
    inline bool isEnabled() {
        return config::get<bool>("hack.hitboxes.enabled", false) || config::get<bool>("hack.hitboxes.death", false);
    }

    /// @brief Whether hitboxes should be drawn at the current moment.
    inline bool shouldDrawHitboxes() {
        return config::get<bool>("hack.hitboxes.enabled", false) ||
               (s_isDead && config::get<bool>("hack.hitboxes.death", false));
    }

    /// @brief Check whether hitboxes made by RobTop should be drawn. (e.g. in practice mode)
    inline bool robtopHitboxCheck() {
        auto *playLayer = gd::PlayLayer::get();
        if (!playLayer) return false;
        return playLayer->m_isPracticeMode() && gd::GameManager::sharedState()->getGameVariable("0045");
    }

    /// @brief Clears the hitboxes if the feature is disabled.
    inline void toggleOffIfNeeded() {
        if (isEnabled()) return;

        auto *playLayer = gd::PlayLayer::get();
        if (!playLayer) return;
        auto *debugDrawNode = playLayer->m_debugDrawNode();
        if (!debugDrawNode) return;

        // Toggle the visibility of the debug draw node
        // (if we're not in practice mode and "Show Hitboxes" isn't enabled)
        debugDrawNode->setVisible(robtopHitboxCheck());
    }

    void Hitboxes::onInit() {
        // Set the default value
        config::setIfEmpty("hack.hitboxes.enabled", false);
        config::setIfEmpty("hack.hitboxes.death", false);
        config::setIfEmpty("hack.hitboxes.accurate_player", true);
        config::setIfEmpty("hack.hitboxes.trail", false);
        config::setIfEmpty("hack.hitboxes.trail_max_length", 240.0f);
        config::setIfEmpty("hack.hitboxes.scale", 1.0f);

        config::setIfEmpty("hack.hitboxes.fill", false);
        config::setIfEmpty("hack.hitboxes.fill_alpha", 0.5f);

        config::setIfEmpty("hack.hitboxes.solid_color", gui::Color(0, 0.247, 1));
        config::setIfEmpty("hack.hitboxes.danger_color", gui::Color(1, 0, 0));
        config::setIfEmpty("hack.hitboxes.player_color", gui::Color(1, 1, 0));
        config::setIfEmpty("hack.hitboxes.other_color", gui::Color(0, 1, 0));

        // Initialize the toggle component
        std::vector<gd::sigscan::Opcode> opcodes;
        bool success = true;
        success &= appendPatch(opcodes, "000000740D^80BB??000000", "*0790");
        success &= appendPatch(opcodes, "000000^741380BE??000000740A", "9090???????9090");
        if (!success) return;
        s_hitboxesToggle = new ToggleComponent("", "", opcodes);
        togglePatch();

        // Initialize keybind
        menu::keybinds::setKeybindCallback("hitboxes.death", []() {
            bool enabled = !config::get<bool>("hack.hitboxes.death");
            config::set("hack.hitboxes.death", enabled);
            togglePatch();
            toggleOffIfNeeded();
        });
    }

    void Hitboxes::onDraw() {
        gui::callback([](){
            gui::tooltip("Shows hitboxes for objects in the game");
            menu::keybinds::addMenuKeybind("hitboxes.death", "Show Hitboxes on Death", []() {
                bool enabled = !config::get<bool>("hack.hitboxes.death");
                config::set("hack.hitboxes.death", enabled);
                togglePatch();
                toggleOffIfNeeded();
            });
        });
        if (gui::toggleSetting("Show Hitboxes", "hack.hitboxes.enabled", []() {
            gui::checkbox("Accurate Player Hitbox", "hack.hitboxes.accurate_player");
            gui::tooltip("Makes the player hitbox more accurate.");

            gui::checkbox("Show Trail", "hack.hitboxes.trail");
            gui::tooltip("Makes player hitbox render without clearing.");

            gui::width(70);
            gui::inputFloat("Trail Max Length", "hack.hitboxes.trail_max_length", 0.0f, 10000.0f, "%.0f");
            gui::tooltip("Changes how many frames the trail will last.");

            gui::inputFloat("Border Scale", "hack.hitboxes.scale", 0.1f, 10.0f, "%.1fx");
            gui::tooltip("Changes the scale of the hitbox border.");
            gui::width();

            ImGui::Separator();

            gui::checkbox("Fill Hitboxes", "hack.hitboxes.fill");
            gui::tooltip("Fills the hitboxes with color.");

            gui::width(70);
            gui::inputFloat("Fill Alpha", "hack.hitboxes.fill_alpha", 0.0f, 1.0f, "%.2f");
            gui::tooltip("Changes the opacity of the filled hitboxes.");
            gui::width();

            ImGui::Separator();

            gui::colorEdit("Solid Color", "hack.hitboxes.solid_color");
            gui::tooltip("Color for solid objects (e.g. blocks).");

            gui::colorEdit("Danger Color", "hack.hitboxes.danger_color");
            gui::tooltip("Color for dangerous objects (e.g. spikes).");

            gui::colorEdit("Player Color", "hack.hitboxes.player_color");
            gui::tooltip("Color for the player hitbox.");

            gui::colorEdit("Other Color", "hack.hitboxes.other_color");
            gui::tooltip("Color for other hitboxes (e.g. portals).");
        }, ImVec2(0, 0), 140)) {
            togglePatch();
            toggleOffIfNeeded();
        }

        if (gui::checkbox("Show Hitboxes on Death", "hack.hitboxes.death")) {
            togglePatch();
            toggleOffIfNeeded();
        }
        gui::tooltip("Shows hitboxes when you die.");
        menu::keybinds::addMenuKeybind("hitboxes.death", "Show Hitboxes on Death", []() {
            bool enabled = !config::get<bool>("hack.hitboxes.death", false);
            config::set("hack.hitboxes.death", enabled);
        });
    }
    
    bool Hitboxes::isCheating() {
        return config::get<bool>("hack.hitboxes.enabled", false);
    }

    gd::cocos2d::CCRect getPlayerHitbox(gd::PlayerObject *player) {
        auto *rect = player->getObjectRect();
        return {rect->origin.x, rect->origin.y, rect->size.width, rect->size.height};
    }

    inline void
    drawRect(gd::cocos2d::CCDrawNode *node, const gd::cocos2d::CCRect &rect, const gui::Color &color, float borderWidth,
             const gui::Color &borderColor) {
        std::vector<gd::cocos2d::CCPoint> vertices = {
                gd::cocos2d::CCPoint(rect.getMinX(), rect.getMinY()),
                gd::cocos2d::CCPoint(rect.getMinX(), rect.getMaxY()),
                gd::cocos2d::CCPoint(rect.getMaxX(), rect.getMaxY()),
                gd::cocos2d::CCPoint(rect.getMaxX(), rect.getMinY())
        };
        s_skipDrawHook = true;
        node->drawPolygon(vertices.data(), vertices.size(),
                          (const gd::cocos2d::_ccColor4F &) color, borderWidth,
                          (const gd::cocos2d::_ccColor4F &) borderColor);
    }

    enum HitboxType {
        Solid,
        Danger,
        Player,
        Other
    };

    inline HitboxType getHitboxType(const gui::Color &color) {
        if (color.r == 0.0f)
            return color.b == 1.0f ? HitboxType::Solid : HitboxType::Other;
        else if (color.g == 1.0f)
            return HitboxType::Player;
        return HitboxType::Danger;
    }

    void Hitboxes::modifyDraw(gui::Color &color, float &borderWidth, gui::Color &borderColor) {
        if (s_skipDrawHook) {
            s_skipDrawHook = false;
            return;
        }
        if (!gd::PlayLayer::get()) return;
        if (!shouldDrawHitboxes()) return;

        bool accuratePlayer = false;

        // Modify the color based on the type of object
        auto type = getHitboxType(borderColor);
        switch (type) {
            case HitboxType::Solid:
                borderColor = config::get<gui::Color>("hack.hitboxes.solid_color");
                break;
            case HitboxType::Danger:
                borderColor = config::get<gui::Color>("hack.hitboxes.danger_color");
                break;
            case HitboxType::Player:
                accuratePlayer = config::get<bool>("hack.hitboxes.accurate_player", true);
                borderColor = accuratePlayer ? gui::Color(0.f, 0.f, 0.f, 0.f) : config::get<gui::Color>(
                        "hack.hitboxes.player_color");
                break;
            case HitboxType::Other:
                borderColor = config::get<gui::Color>("hack.hitboxes.other_color");
                break;
        }

        // Change scale of the border
        borderWidth *= accuratePlayer ? 0.f : config::get<float>("hack.hitboxes.scale", 1.0f);

        // Change fill opacity
        if (config::get<bool>("hack.hitboxes.fill", false)) {
            color.r = borderColor.r;
            color.g = borderColor.g;
            color.b = borderColor.b;
            color.a = accuratePlayer ? 0.f : config::get<float>("hack.hitboxes.fill_alpha", 0.5f);
        }
    }

    void Hitboxes::postUpdate() {
        if (!isEnabled()) return;

        auto *playLayer = gd::PlayLayer::get();
        auto *debugDrawNode = playLayer->m_debugDrawNode();

        bool enabled = shouldDrawHitboxes();
        debugDrawNode->setVisible(enabled || robtopHitboxCheck());

        if (playLayer->m_isFlipped())
            playLayer->updateDebugDraw();

        // Accurate player hitbox
        if (config::get<bool>("hack.hitboxes.accurate_player", true)) {
            auto color = config::get<gui::Color>("hack.hitboxes.player_color");
            auto fill = config::get<bool>("hack.hitboxes.fill", false);
            auto fillAlpha = config::get<float>("hack.hitboxes.fill_alpha", 0.5f);
            auto scale = config::get<float>("hack.hitboxes.scale", 1.0f);
            gui::Color borderColor = color;
            gui::Color fillColor(color.r, color.g, color.b, fill ? fillAlpha : 0.f);
            float borderWidth = 0.25f * scale;

            auto *player1 = playLayer->m_player1();
            if (player1) {
                auto hitbox = getPlayerHitbox(player1);
                drawRect(debugDrawNode, hitbox, fillColor, borderWidth, borderColor);
            }

            auto *player2 = playLayer->m_player2();
            if (player2) {
                auto hitbox = getPlayerHitbox(player2);
                drawRect(debugDrawNode, hitbox, fillColor, borderWidth, borderColor);
            }
        }

        // Draw player trail
        if (config::get<bool>("hack.hitboxes.trail", false)) {
            auto color = config::get<gui::Color>("hack.hitboxes.player_color");
            auto fill = config::get<bool>("hack.hitboxes.fill", false);
            auto fillAlpha = config::get<float>("hack.hitboxes.fill_alpha", 0.5f);
            auto scale = config::get<float>("hack.hitboxes.scale", 1.0f);
            gui::Color borderColor = color;
            gui::Color fillColor(color.r, color.g, color.b, fill ? fillAlpha : 0.f);
            float borderWidth = 0.25f * scale;

            for (auto &hitbox: s_playerTrail1)
                drawRect(debugDrawNode, hitbox, fillColor, borderWidth, borderColor);
            for (auto &hitbox: s_playerTrail2)
                drawRect(debugDrawNode, hitbox, fillColor, borderWidth, borderColor);
        }
    }

    void Hitboxes::resetLevel() {
        s_isDead = false;

        // Clear player trail
        s_playerTrail1.clear();
        s_playerTrail2.clear();
    }

    void Hitboxes::fullReset() {
        s_isDead = false;
    }

    void Hitboxes::destroyPlayer() {
        auto *playLayer = gd::PlayLayer::get();
        auto *player = playLayer->m_player1();
        if (player && !player->m_isDead()) return;
        s_isDead = true;
    }

    void Hitboxes::processCommands() {
        if (!config::get<bool>("hack.hitboxes.trail", false)) return;

        // Add player trail to the queue
        auto *playLayer = gd::PlayLayer::get();
        if (!playLayer) return;

        auto *player1 = playLayer->m_player1();
        auto *player2 = playLayer->m_player2();

        if (player1 && !player1->m_isDead()) {
            auto hitbox = getPlayerHitbox(player1);
            s_playerTrail1.push_back(hitbox);
        }

        if (player2 && !player2->m_isDead()) {
            auto hitbox = getPlayerHitbox(player2);
            s_playerTrail2.push_back(hitbox);
        }

        // Remove old player trail
        auto maxLength = static_cast<size_t>(config::get<float>("hack.hitboxes.trail_max_length", 100.0f));
        while (s_playerTrail1.size() > maxLength)
            s_playerTrail1.pop_front();
        while (s_playerTrail2.size() > maxLength)
            s_playerTrail2.pop_front();
    }

}