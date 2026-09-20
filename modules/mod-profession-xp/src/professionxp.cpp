/*
 * Character experience for gathering and crafting.
 *
 * The original CoA granted ordinary character experience for mining, herbing, skinning and
 * crafting, and that behaviour was lost in the move to this core: see issues #368, #1410,
 * #1966 and #4266. The amount is what a mob of the player's own level awards, scaled by the
 * colour of the node or recipe against the player's skill: orange, yellow, green, and grey
 * which always awards nothing.
 *
 * Colour alone is enough to stop a node being farmed, because skill only ever rises: an orange
 * node turns yellow, then green, then grey on its own. Scaling by the node's tier against the
 * player's level was tried and removed, since the two measures disagree - colour is relative to
 * skill and tier to level, so a level 22 who had just learned mining was shown an orange copper
 * vein that the tier rule called trivial and paid nothing for.
 *
 * No canonical formula survived, and the reports disagree on the amount: #1410 remembers an
 * orange recipe paying a full same-level kill, while #1966 ("maybe half a mob kill?") and
 * #368 ("some small 20-50xp gain", against the 60 this grants at that level on the full
 * rate) both describe roughly half of one. The multipliers are therefore presets in
 * modules/professionxp.conf rather than constants here, and ".reload config" applies an edit
 * without a restart.
 *
 * Experience lands on every gather and every craft, not only on the ones that raise the skill:
 * #368 reports it for gathering a herb, which frequently does not raise the skill at all. That
 * also covers what #4266 asks for, since a craft that produces a skill up is paid like any
 * other.
 */

#include "Configuration/Config.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Formulas.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <string>

namespace
{
    constexpr char ENABLE_KEY[] = "Profession.XP.Enable";
    constexpr char PRESET_KEY[] = "Profession.XP.Preset";
    constexpr char CRAFTING_FACTOR_KEY[] = "Profession.XP.CraftingFactor";
    constexpr char FOLLOW_RATE_KEY[] = "Profession.XP.FollowPlayerRate";

    constexpr char CUSTOM_ORANGE_KEY[] = "Profession.XP.Custom.Orange";
    constexpr char CUSTOM_YELLOW_KEY[] = "Profession.XP.Custom.Yellow";
    constexpr char CUSTOM_GREEN_KEY[] = "Profession.XP.Custom.Green";

    constexpr char MINING_KEY[] = "Profession.XP.Skill.Mining";
    constexpr char HERBALISM_KEY[] = "Profession.XP.Skill.Herbalism";
    constexpr char SKINNING_KEY[] = "Profession.XP.Skill.Skinning";
    constexpr char PROSPECTING_KEY[] = "Profession.XP.Skill.Prospecting";
    constexpr char LOCKPICKING_KEY[] = "Profession.XP.Skill.Lockpicking";
    constexpr char CRAFTING_KEY[] = "Profession.XP.Skill.Crafting";

    /// Fractions of what a mob of the player's own level awards. "balanced" is the default
    /// because two of the three reports that name an amount describe about half a kill.
    constexpr float FLAVOUR_ORANGE = 0.15f;
    constexpr float BALANCED_ORANGE = 0.50f;
    constexpr float REALPATH_ORANGE = 1.00f;

    /// Yellow and green as fractions of orange, kept in one place so a preset only has to
    /// name its orange value and a custom curve stays the one way to break the ratio.
    constexpr float YELLOW_OF_ORANGE = 0.60f;
    constexpr float GREEN_OF_ORANGE = 0.30f;

    /// Experience is granted from several map threads at once, so every value the hooks read
    /// is cached in an atomic and refreshed only when the config is loaded or reloaded.
    std::atomic<bool> g_enabled{false};
    std::atomic<float> g_orange{0.0f};
    std::atomic<float> g_yellow{0.0f};
    std::atomic<float> g_green{0.0f};
    std::atomic<float> g_craftingFactor{1.0f};
    std::atomic<bool> g_followPlayerRate{true};

    std::atomic<bool> g_mining{true};
    std::atomic<bool> g_herbalism{true};
    std::atomic<bool> g_skinning{true};
    std::atomic<bool> g_prospecting{true};
    std::atomic<bool> g_lockpicking{false};
    std::atomic<bool> g_crafting{true};

    float ColourMultiplier(uint32 current, uint32 grey, uint32 green, uint32 yellow)
    {
        if (current >= grey)
            return 0.0f;
        if (current >= green)
            return g_green.load(std::memory_order_relaxed);
        if (current >= yellow)
            return g_yellow.load(std::memory_order_relaxed);
        return g_orange.load(std::memory_order_relaxed);
    }

    /// What a mob of the player's own level is worth, scaled by the colour multiplier.
    uint32 ComputeAward(Player const* player, float multiplier)
    {
        if (multiplier <= 0.0f)
            return 0;

        uint8 const level = player->GetLevel();

        uint32 const base = Acore::XP::BaseGain(level, level, CONTENT_1_60);
        if (!base)
            return 0;

        return uint32(std::lround(base * multiplier));
    }

    void Award(Player* player, uint32 amount)
    {
        if (!amount)
            return;

        // Player::GiveXP does not raise this hook itself; each caller in the core raises it
        // first. Leaving it out would hide this experience from the realm's rate modules, so
        // a player on ".xp 5" would earn five times as much from a mob as from a node.
        if (g_followPlayerRate.load(std::memory_order_relaxed))
            sScriptMgr->OnPlayerGiveXP(player, amount, nullptr, PlayerXPSource::XPSOURCE_KILL);

        player->GiveXP(amount, nullptr);
    }

    bool GatheringEnabled(uint32 skillId)
    {
        switch (skillId)
        {
        case SKILL_MINING:
            return g_mining.load(std::memory_order_relaxed);
        case SKILL_HERBALISM:
            return g_herbalism.load(std::memory_order_relaxed);
        case SKILL_SKINNING:
            return g_skinning.load(std::memory_order_relaxed);
        case SKILL_JEWELCRAFTING:
        case SKILL_INSCRIPTION:
            // Prospecting and milling reach the gathering path, not the crafting one.
            return g_prospecting.load(std::memory_order_relaxed);
        case SKILL_LOCKPICKING:
            return g_lockpicking.load(std::memory_order_relaxed);
        default:
            return false;
        }
    }

    std::string LoweredOption(char const* key, char const* fallback)
    {
        std::string value = sConfigMgr->GetOption<std::string>(key, fallback);
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return char(std::tolower(c)); });
        return value;
    }

    void RefreshSettings()
    {
        g_enabled.store(sConfigMgr->GetOption<bool>(ENABLE_KEY, true), std::memory_order_relaxed);

        std::string const preset = LoweredOption(PRESET_KEY, "balanced");

        float orange = BALANCED_ORANGE;
        float yellow = BALANCED_ORANGE * YELLOW_OF_ORANGE;
        float green = BALANCED_ORANGE * GREEN_OF_ORANGE;

        if (preset == "custom")
        {
            orange = sConfigMgr->GetOption<float>(CUSTOM_ORANGE_KEY, BALANCED_ORANGE);
            yellow = sConfigMgr->GetOption<float>(CUSTOM_YELLOW_KEY, BALANCED_ORANGE * YELLOW_OF_ORANGE);
            green = sConfigMgr->GetOption<float>(CUSTOM_GREEN_KEY, BALANCED_ORANGE * GREEN_OF_ORANGE);
        }
        else
        {
            if (preset == "flavour" || preset == "flavor")
                orange = FLAVOUR_ORANGE;
            else if (preset == "realpath")
                orange = REALPATH_ORANGE;
            else if (preset != "balanced")
                LOG_ERROR("module", "mod-profession-xp: unknown {} '{}', falling back to 'balanced'.",
                    PRESET_KEY, preset);

            yellow = orange * YELLOW_OF_ORANGE;
            green = orange * GREEN_OF_ORANGE;
        }

        g_orange.store(std::max(0.0f, orange), std::memory_order_relaxed);
        g_yellow.store(std::max(0.0f, yellow), std::memory_order_relaxed);
        g_green.store(std::max(0.0f, green), std::memory_order_relaxed);

        g_craftingFactor.store(std::max(0.0f, sConfigMgr->GetOption<float>(CRAFTING_FACTOR_KEY, 1.0f)),
            std::memory_order_relaxed);
        g_followPlayerRate.store(sConfigMgr->GetOption<bool>(FOLLOW_RATE_KEY, true), std::memory_order_relaxed);

        g_mining.store(sConfigMgr->GetOption<bool>(MINING_KEY, true), std::memory_order_relaxed);
        g_herbalism.store(sConfigMgr->GetOption<bool>(HERBALISM_KEY, true), std::memory_order_relaxed);
        g_skinning.store(sConfigMgr->GetOption<bool>(SKINNING_KEY, true), std::memory_order_relaxed);
        g_prospecting.store(sConfigMgr->GetOption<bool>(PROSPECTING_KEY, true), std::memory_order_relaxed);
        g_lockpicking.store(sConfigMgr->GetOption<bool>(LOCKPICKING_KEY, false), std::memory_order_relaxed);
        g_crafting.store(sConfigMgr->GetOption<bool>(CRAFTING_KEY, true), std::memory_order_relaxed);
    }
}

class profession_xp : public PlayerScript
{
public:
    profession_xp() : PlayerScript("profession_xp", {
        PLAYERHOOK_ON_UPDATE_GATHERING_SKILL,
        PLAYERHOOK_ON_UPDATE_CRAFTING_SKILL
    }) { }

    /// The core hands over the three colour thresholds it has just worked out.
    void OnPlayerUpdateGatheringSkill(Player* player, uint32 skillId, uint32 current, uint32 grey, uint32 green,
        uint32 yellow, uint32& /*gain*/) override
    {
        if (!g_enabled.load(std::memory_order_relaxed) || !GatheringEnabled(skillId))
            return;

        Award(player, ComputeAward(player, ColourMultiplier(current, grey, green, yellow)));
    }

    /// A recipe turns grey at TrivialSkillLineRankHigh and yellow at TrivialSkillLineRankLow,
    /// with green halfway between, which is the ordering the gathering path uses as well.
    void OnPlayerUpdateCraftingSkill(Player* player, SkillLineAbilityEntry const* skill, uint32 current,
        uint32& /*gain*/) override
    {
        if (!g_enabled.load(std::memory_order_relaxed) || !g_crafting.load(std::memory_order_relaxed) || !skill)
            return;

        uint32 const grey = skill->TrivialSkillLineRankHigh;
        uint32 const yellow = skill->TrivialSkillLineRankLow;
        uint32 const green = (grey + yellow) / 2;

        float const multiplier = ColourMultiplier(current, grey, green, yellow)
            * g_craftingFactor.load(std::memory_order_relaxed);

        Award(player, ComputeAward(player, multiplier));
    }
};

/// The config has already been reloaded by the time this runs, at startup and on
/// ".reload config", so editing modules/professionxp.conf by hand needs no restart.
class profession_xp_config : public WorldScript
{
public:
    profession_xp_config() : WorldScript("profession_xp_config") { }

    void OnBeforeConfigLoad(bool /*reload*/) override
    {
        RefreshSettings();
    }
};

void AddSC_profession_xp()
{
    new profession_xp();
    new profession_xp_config();
}
