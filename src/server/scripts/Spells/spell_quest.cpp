/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Scripts for spells with SPELLFAMILY_GENERIC spells used for quests.
 * Ordered alphabetically using questId and scriptname.
 * Scriptnames of files in this file should be prefixed with "spell_q#questID_".
 */

#include "ScriptMgr.h"
#include "CellImpl.h"
#include "CreatureAIImpl.h"
#include "CreatureTextMgr.h"
#include "GridNotifiersImpl.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "Vehicle.h"

class spell_generic_quest_update_entry_SpellScript : public SpellScript
{
    PrepareSpellScript(spell_generic_quest_update_entry_SpellScript);
    private:
        uint16 _spellEffect;
        uint8 _effIndex;
        uint32 _originalEntry;
        uint32 _newEntry;
        bool _shouldAttack;
        uint32 _despawnTime;

    public:
        spell_generic_quest_update_entry_SpellScript(uint16 spellEffect, uint8 effIndex, uint32 originalEntry, uint32 newEntry, bool shouldAttack, uint32 despawnTime = 0) :
            SpellScript(), _spellEffect(spellEffect), _effIndex(effIndex), _originalEntry(originalEntry),
            _newEntry(newEntry), _shouldAttack(shouldAttack), _despawnTime(despawnTime) { }

        void HandleDummy(SpellEffIndex /*effIndex*/)
        {
            if (Creature* creatureTarget = GetHitCreature())
                if (!creatureTarget->IsPet() && creatureTarget->GetEntry() == _originalEntry)
                {
                    creatureTarget->UpdateEntry(_newEntry);
                    if (_shouldAttack)
                        creatureTarget->EngageWithTarget(GetCaster());

                    if (_despawnTime)
                        creatureTarget->DespawnOrUnsummon(_despawnTime);
                }
        }

        void Register() override
        {
            OnEffectHitTarget += SpellEffectFn(spell_generic_quest_update_entry_SpellScript::HandleDummy, _effIndex, _spellEffect);
        }
};

// http://www.wowhead.com/quest=55 Morbent Fel
// 8913 Sacred Cleansing
enum Quest55Data
{
    NPC_MORBENT             = 1200,
    NPC_WEAKENED_MORBENT    = 24782,
};

class spell_q55_sacred_cleansing : public SpellScriptLoader
{
    public:
        spell_q55_sacred_cleansing() : SpellScriptLoader("spell_q55_sacred_cleansing") { }

        SpellScript* GetSpellScript() const override
        {
            return new spell_generic_quest_update_entry_SpellScript(SPELL_EFFECT_DUMMY, EFFECT_1, NPC_MORBENT, NPC_WEAKENED_MORBENT, true);
        }
};

// 9712 - Thaumaturgy Channel
enum ThaumaturgyChannel
{
    SPELL_THAUMATURGY_CHANNEL = 21029
};

class spell_q2203_thaumaturgy_channel : public SpellScriptLoader
{
    public:
        spell_q2203_thaumaturgy_channel() : SpellScriptLoader("spell_q2203_thaumaturgy_channel") { }

        class spell_q2203_thaumaturgy_channel_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_q2203_thaumaturgy_channel_AuraScript);

            bool Validate(SpellInfo const* /*spellInfo*/) override
            {
                return ValidateSpellInfo({ SPELL_THAUMATURGY_CHANNEL });
            }

            void HandleEffectPeriodic(AuraEffect const* /*aurEff*/)
            {
                PreventDefaultAction();
                if (Unit* caster = GetCaster())
                    caster->CastSpell(caster, SPELL_THAUMATURGY_CHANNEL, false);
            }

            void Register() override
            {
                OnEffectPeriodic += AuraEffectPeriodicFn(spell_q2203_thaumaturgy_channel_AuraScript::HandleEffectPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_q2203_thaumaturgy_channel_AuraScript();
        }
};

// http://www.wowhead.com/quest=5206 Marauders of Darrowshire
// 17271 Test Fetid Skull
enum Quest5206Data
{
    SPELL_CREATE_RESONATING_SKULL = 17269,
    SPELL_CREATE_BONE_DUST = 17270
};

class spell_q5206_test_fetid_skull : public SpellScriptLoader
{
    public:
        spell_q5206_test_fetid_skull() : SpellScriptLoader("spell_q5206_test_fetid_skull") { }

        class spell_q5206_test_fetid_skull_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q5206_test_fetid_skull_SpellScript);

            bool Load() override
            {
                return GetCaster()->GetTypeId() == TYPEID_PLAYER;
            }

            bool Validate(SpellInfo const* /*spellEntry*/) override
            {
                return ValidateSpellInfo({ SPELL_CREATE_RESONATING_SKULL, SPELL_CREATE_BONE_DUST });
            }

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                Unit* caster = GetCaster();
                uint32 spellId = roll_chance_i(50) ? SPELL_CREATE_RESONATING_SKULL : SPELL_CREATE_BONE_DUST;
                caster->CastSpell(caster, spellId, true);
            }

            void Register() override
            {
                OnEffectHit += SpellEffectFn(spell_q5206_test_fetid_skull_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q5206_test_fetid_skull_SpellScript();
        }
};

// http://www.wowhead.com/quest=6124 Curing the Sick (A)
// http://www.wowhead.com/quest=6129 Curing the Sick (H)
// 19512 Apply Salve
enum Quests6124_6129Data
{
    NPC_SICKLY_GAZELLE  = 12296,
    NPC_CURED_GAZELLE   = 12297,
    NPC_SICKLY_DEER     = 12298,
    NPC_CURED_DEER      = 12299,
    DESPAWN_TIME        = 30000
};

class spell_q6124_6129_apply_salve : public SpellScriptLoader
{
    public:
        spell_q6124_6129_apply_salve() : SpellScriptLoader("spell_q6124_6129_apply_salve") { }

        class spell_q6124_6129_apply_salve_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q6124_6129_apply_salve_SpellScript);

            bool Load() override
            {
                return GetCaster()->GetTypeId() == TYPEID_PLAYER;
            }

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                Player* caster = GetCaster()->ToPlayer();
                if (GetCastItem())
                    if (Creature* creatureTarget = GetHitCreature())
                    {
                        uint32 newEntry = 0;
                        switch (caster->GetTeam())
                        {
                            case HORDE:
                                if (creatureTarget->GetEntry() == NPC_SICKLY_GAZELLE)
                                    newEntry = NPC_CURED_GAZELLE;
                                break;
                            case ALLIANCE:
                                if (creatureTarget->GetEntry() == NPC_SICKLY_DEER)
                                    newEntry = NPC_CURED_DEER;
                                break;
                        }
                        if (newEntry)
                        {
                            creatureTarget->UpdateEntry(newEntry);
                            creatureTarget->DespawnOrUnsummon(DESPAWN_TIME);
                            caster->KilledMonsterCredit(newEntry);
                        }
                    }
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q6124_6129_apply_salve_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q6124_6129_apply_salve_SpellScript();
        }
};

// http://www.wowhead.com/quest=10255 Testing the Antidote
// 34665 Administer Antidote
enum Quest10255Data
{
    NPC_HELBOAR     = 16880,
    NPC_DREADTUSK   = 16992,
};

class spell_q10255_administer_antidote : public SpellScriptLoader
{
    public:
        spell_q10255_administer_antidote() : SpellScriptLoader("spell_q10255_administer_antidote") { }

        SpellScript* GetSpellScript() const override
        {
            return new spell_generic_quest_update_entry_SpellScript(SPELL_EFFECT_DUMMY, EFFECT_0, NPC_HELBOAR, NPC_DREADTUSK, true);
        }
};

// http://www.wowhead.com/quest=11396 Bring Down Those Shields (A)
// http://www.wowhead.com/quest=11399 Bring Down Those Shields (H)
enum Quest11396_11399Data
{
    SPELL_FORCE_SHIELD_ARCANE_PURPLE_X3 = 43874,
    SPELL_SCOURGING_CRYSTAL_CONTROLLER = 43878
};

// 43874 Scourge Mur'gul Camp: Force Shield Arcane Purple x3
class spell_q11396_11399_force_shield_arcane_purple_x3 : public SpellScriptLoader
{
    public:
        spell_q11396_11399_force_shield_arcane_purple_x3() : SpellScriptLoader("spell_q11396_11399_force_shield_arcane_purple_x3") { }

        class spell_q11396_11399_force_shield_arcane_purple_x3_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_q11396_11399_force_shield_arcane_purple_x3_AuraScript);

            void HandleEffectApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
            {
                Unit* target = GetTarget();
                target->SetImmuneToPC(true);
                target->AddUnitState(UNIT_STATE_ROOT);
            }

            void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
            {
                GetTarget()->SetImmuneToPC(false);
            }

            void Register() override
            {
                OnEffectApply += AuraEffectApplyFn(spell_q11396_11399_force_shield_arcane_purple_x3_AuraScript::HandleEffectApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
                OnEffectRemove += AuraEffectRemoveFn(spell_q11396_11399_force_shield_arcane_purple_x3_AuraScript::HandleEffectRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
            }

        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_q11396_11399_force_shield_arcane_purple_x3_AuraScript();
        }
};

// 50133 Scourging Crystal Controller
class spell_q11396_11399_scourging_crystal_controller : public SpellScriptLoader
{
    public:
        spell_q11396_11399_scourging_crystal_controller() : SpellScriptLoader("spell_q11396_11399_scourging_crystal_controller") { }

        class spell_q11396_11399_scourging_crystal_controller_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q11396_11399_scourging_crystal_controller_SpellScript);

            bool Validate(SpellInfo const* /*spellEntry*/) override
            {
                return ValidateSpellInfo({ SPELL_FORCE_SHIELD_ARCANE_PURPLE_X3, SPELL_SCOURGING_CRYSTAL_CONTROLLER });
            }

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                if (Unit* target = GetHitUnit())
                    if (target->GetTypeId() == TYPEID_UNIT && target->HasAura(SPELL_FORCE_SHIELD_ARCANE_PURPLE_X3))
                        // Make sure nobody else is channeling the same target
                        if (!target->HasAura(SPELL_SCOURGING_CRYSTAL_CONTROLLER))
                            GetCaster()->CastSpell(target, SPELL_SCOURGING_CRYSTAL_CONTROLLER, GetCastItem());
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q11396_11399_scourging_crystal_controller_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q11396_11399_scourging_crystal_controller_SpellScript();
        }
};

// 43882 Scourging Crystal Controller Dummy
class spell_q11396_11399_scourging_crystal_controller_dummy : public SpellScriptLoader
{
    public:
        spell_q11396_11399_scourging_crystal_controller_dummy() : SpellScriptLoader("spell_q11396_11399_scourging_crystal_controller_dummy") { }

        class spell_q11396_11399_scourging_crystal_controller_dummy_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q11396_11399_scourging_crystal_controller_dummy_SpellScript);

            bool Validate(SpellInfo const* /*spellEntry*/) override
            {
                return ValidateSpellInfo({ SPELL_FORCE_SHIELD_ARCANE_PURPLE_X3 });
            }

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                if (Unit* target = GetHitUnit())
                    if (target->GetTypeId() == TYPEID_UNIT)
                        target->RemoveAurasDueToSpell(SPELL_FORCE_SHIELD_ARCANE_PURPLE_X3);
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q11396_11399_scourging_crystal_controller_dummy_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q11396_11399_scourging_crystal_controller_dummy_SpellScript();
        }
};

// http://www.wowhead.com/quest=11515 Blood for Blood
// 44936 Quest - Fel Siphon Dummy
enum Quest11515Data
{
    NPC_FELBLOOD_INITIATE   = 24918,
    NPC_EMACIATED_FELBLOOD  = 24955
};

class spell_q11515_fel_siphon_dummy : public SpellScriptLoader
{
    public:
        spell_q11515_fel_siphon_dummy() : SpellScriptLoader("spell_q11515_fel_siphon_dummy") { }

        SpellScript* GetSpellScript() const override
        {
            return new spell_generic_quest_update_entry_SpellScript(SPELL_EFFECT_DUMMY, EFFECT_0, NPC_FELBLOOD_INITIATE, NPC_EMACIATED_FELBLOOD, true);
        }
};

// http://www.wowhead.com/quest=11587 Prison Break
// 45449 Arcane Prisoner Rescue
enum Quest11587Data
{
    SPELL_SUMMON_ARCANE_PRISONER_MALE    = 45446,    // Summon Arcane Prisoner - Male
    SPELL_SUMMON_ARCANE_PRISONER_FEMALE  = 45448,    // Summon Arcane Prisoner - Female
    SPELL_ARCANE_PRISONER_KILL_CREDIT    = 45456     // Arcane Prisoner Kill Credit
};

class spell_q11587_arcane_prisoner_rescue : public SpellScriptLoader
{
    public:
        spell_q11587_arcane_prisoner_rescue() : SpellScriptLoader("spell_q11587_arcane_prisoner_rescue") { }

        class spell_q11587_arcane_prisoner_rescue_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q11587_arcane_prisoner_rescue_SpellScript);

            bool Validate(SpellInfo const* /*spellEntry*/) override
            {
                return ValidateSpellInfo({ SPELL_SUMMON_ARCANE_PRISONER_MALE, SPELL_SUMMON_ARCANE_PRISONER_FEMALE, SPELL_ARCANE_PRISONER_KILL_CREDIT });
            }

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                Unit* caster = GetCaster();
                if (Unit* unitTarget = GetHitUnit())
                {
                    uint32 spellId = SPELL_SUMMON_ARCANE_PRISONER_MALE;
                    if (rand32() % 2)
                        spellId = SPELL_SUMMON_ARCANE_PRISONER_FEMALE;
                    caster->CastSpell(caster, spellId, true);
                    unitTarget->CastSpell(caster, SPELL_ARCANE_PRISONER_KILL_CREDIT, true);
                }
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q11587_arcane_prisoner_rescue_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q11587_arcane_prisoner_rescue_SpellScript();
        }
};

// http://www.wowhead.com/quest=11730 Master and Servant
// 46023 The Ultrasonic Screwdriver
enum Quest11730Data
{
    SPELL_SUMMON_SCAVENGEBOT_004A8  = 46063,
    SPELL_SUMMON_SENTRYBOT_57K      = 46068,
    SPELL_SUMMON_DEFENDOTANK_66D    = 46058,
    SPELL_SUMMON_SCAVENGEBOT_005B6  = 46066,
    SPELL_SUMMON_55D_COLLECTATRON   = 46034,
    SPELL_ROBOT_KILL_CREDIT         = 46027,
    NPC_SCAVENGEBOT_004A8           = 25752,
    NPC_SENTRYBOT_57K               = 25753,
    NPC_DEFENDOTANK_66D             = 25758,
    NPC_SCAVENGEBOT_005B6           = 25792,
    NPC_55D_COLLECTATRON            = 25793
};

class spell_q11730_ultrasonic_screwdriver : public SpellScriptLoader
{
    public:
        spell_q11730_ultrasonic_screwdriver() : SpellScriptLoader("spell_q11730_ultrasonic_screwdriver") { }

        class spell_q11730_ultrasonic_screwdriver_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q11730_ultrasonic_screwdriver_SpellScript);

            bool Load() override
            {
                return GetCaster()->GetTypeId() == TYPEID_PLAYER && GetCastItem();
            }

            bool Validate(SpellInfo const* /*spellEntry*/) override
            {
                return ValidateSpellInfo(
                {
                    SPELL_SUMMON_SCAVENGEBOT_004A8,
                    SPELL_SUMMON_SENTRYBOT_57K,
                    SPELL_SUMMON_DEFENDOTANK_66D,
                    SPELL_SUMMON_SCAVENGEBOT_005B6,
                    SPELL_SUMMON_55D_COLLECTATRON,
                    SPELL_ROBOT_KILL_CREDIT
                });
            }

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                Item* castItem = GetCastItem();
                Unit* caster = GetCaster();
                if (Creature* target = GetHitCreature())
                {
                    uint32 spellId = 0;
                    switch (target->GetEntry())
                    {
                        case NPC_SCAVENGEBOT_004A8: spellId = SPELL_SUMMON_SCAVENGEBOT_004A8;    break;
                        case NPC_SENTRYBOT_57K:     spellId = SPELL_SUMMON_SENTRYBOT_57K;        break;
                        case NPC_DEFENDOTANK_66D:   spellId = SPELL_SUMMON_DEFENDOTANK_66D;      break;
                        case NPC_SCAVENGEBOT_005B6: spellId = SPELL_SUMMON_SCAVENGEBOT_005B6;    break;
                        case NPC_55D_COLLECTATRON:  spellId = SPELL_SUMMON_55D_COLLECTATRON;     break;
                        default:
                            return;
                    }
                    caster->CastSpell(caster, spellId, castItem);
                    caster->CastSpell(caster, SPELL_ROBOT_KILL_CREDIT, true);
                    target->DespawnOrUnsummon();
                }
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q11730_ultrasonic_screwdriver_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q11730_ultrasonic_screwdriver_SpellScript();
        }
};

// http://www.wowhead.com/quest=12459 That Which Creates Can Also Destroy
// 49587 Seeds of Nature's Wrath
enum Quest12459Data
{
    NPC_REANIMATED_FROSTWYRM        = 26841,
    NPC_WEAK_REANIMATED_FROSTWYRM   = 27821,

    NPC_TURGID                      = 27808,
    NPC_WEAK_TURGID                 = 27809,

    NPC_DEATHGAZE                   = 27122,
    NPC_WEAK_DEATHGAZE              = 27807,
};

class spell_q12459_seeds_of_natures_wrath : public SpellScriptLoader
{
    public:
        spell_q12459_seeds_of_natures_wrath() : SpellScriptLoader("spell_q12459_seeds_of_natures_wrath") { }

        class spell_q12459_seeds_of_natures_wrath_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12459_seeds_of_natures_wrath_SpellScript);

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                if (Creature* creatureTarget = GetHitCreature())
                {
                    uint32 uiNewEntry = 0;
                    switch (creatureTarget->GetEntry())
                    {
                        case NPC_REANIMATED_FROSTWYRM:  uiNewEntry = NPC_WEAK_REANIMATED_FROSTWYRM; break;
                        case NPC_TURGID:                uiNewEntry = NPC_WEAK_TURGID;               break;
                        case NPC_DEATHGAZE:             uiNewEntry = NPC_WEAK_DEATHGAZE;            break;
                    }
                    if (uiNewEntry)
                        creatureTarget->UpdateEntry(uiNewEntry);
                }
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q12459_seeds_of_natures_wrath_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12459_seeds_of_natures_wrath_SpellScript();
        }
};

// http://www.wowhead.com/quest=12634 Some Make Lemonade, Some Make Liquor
// 51840 Despawn Fruit Tosser
enum Quest12634Data
{
    SPELL_BANANAS_FALL_TO_GROUND    = 51836,
    SPELL_ORANGE_FALLS_TO_GROUND    = 51837,
    SPELL_PAPAYA_FALLS_TO_GROUND    = 51839,
    SPELL_SUMMON_ADVENTUROUS_DWARF  = 52070
};

class spell_q12634_despawn_fruit_tosser : public SpellScriptLoader
{
    public:
        spell_q12634_despawn_fruit_tosser() : SpellScriptLoader("spell_q12634_despawn_fruit_tosser") { }

        class spell_q12634_despawn_fruit_tosser_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12634_despawn_fruit_tosser_SpellScript);

            bool Validate(SpellInfo const* /*spellEntry*/) override
            {
                return ValidateSpellInfo(
                {
                    SPELL_BANANAS_FALL_TO_GROUND,
                    SPELL_ORANGE_FALLS_TO_GROUND,
                    SPELL_PAPAYA_FALLS_TO_GROUND,
                    SPELL_SUMMON_ADVENTUROUS_DWARF
                });
            }

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                uint32 spellId = SPELL_BANANAS_FALL_TO_GROUND;
                switch (urand(0, 3))
                {
                    case 1: spellId = SPELL_ORANGE_FALLS_TO_GROUND; break;
                    case 2: spellId = SPELL_PAPAYA_FALLS_TO_GROUND; break;
                }
                // sometimes, if you're lucky, you get a dwarf
                if (roll_chance_i(5))
                    spellId = SPELL_SUMMON_ADVENTUROUS_DWARF;
                GetCaster()->CastSpell(GetCaster(), spellId, true);
            }

            void Register() override
            {
                OnEffectHit += SpellEffectFn(spell_q12634_despawn_fruit_tosser_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12634_despawn_fruit_tosser_SpellScript();
        }
};

// http://www.wowhead.com/quest=12683 Burning to Help
// 52308 Take Sputum Sample
class spell_q12683_take_sputum_sample : public SpellScriptLoader
{
    public:
        spell_q12683_take_sputum_sample() : SpellScriptLoader("spell_q12683_take_sputum_sample") { }

        class spell_q12683_take_sputum_sample_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12683_take_sputum_sample_SpellScript);

            bool Validate(SpellInfo const* spellInfo) override
            {
                return spellInfo->GetEffects().size() > EFFECT_1;
            }

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                uint32 reqAuraId = GetEffectInfo(EFFECT_1).CalcValue();

                Unit* caster = GetCaster();
                if (caster->HasAuraEffect(reqAuraId, 0))
                {
                    uint32 spellId = GetEffectInfo().CalcValue();
                    caster->CastSpell(caster, spellId, true);
                }
            }

            void Register() override
            {
                OnEffectHit += SpellEffectFn(spell_q12683_take_sputum_sample_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12683_take_sputum_sample_SpellScript();
        }
};

// http://www.wowhead.com/quest=12851 Going Bearback
// 54798 FLAMING Arrow Triggered Effect
enum Quest12851Data
{
    NPC_FROSTGIANT = 29351,
    NPC_FROSTWORG  = 29358,
    SPELL_FROSTGIANT_CREDIT = 58184,
    SPELL_FROSTWORG_CREDIT  = 58183,
    SPELL_IMMOLATION        = 54690,
    SPELL_ABLAZE            = 54683,
};

class spell_q12851_going_bearback : public SpellScriptLoader
{
    public:
        spell_q12851_going_bearback() : SpellScriptLoader("spell_q12851_going_bearback") { }

        class spell_q12851_going_bearback_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_q12851_going_bearback_AuraScript);

            void HandleEffectApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
            {
                if (Unit* caster = GetCaster())
                {
                    Unit* target = GetTarget();
                    // Already in fire
                    if (target->HasAura(SPELL_ABLAZE))
                        return;

                    if (Player* player = caster->GetCharmerOrOwnerPlayerOrPlayerItself())
                    {
                        switch (target->GetEntry())
                        {
                            case NPC_FROSTWORG:
                                target->CastSpell(player, SPELL_FROSTWORG_CREDIT, true);
                                target->CastSpell(target, SPELL_IMMOLATION, true);
                                target->CastSpell(target, SPELL_ABLAZE, true);
                                break;
                            case NPC_FROSTGIANT:
                                target->CastSpell(player, SPELL_FROSTGIANT_CREDIT, true);
                                target->CastSpell(target, SPELL_IMMOLATION, true);
                                target->CastSpell(target, SPELL_ABLAZE, true);
                                break;
                        }
                    }
                }
            }

            void Register() override
            {
                AfterEffectApply += AuraEffectApplyFn(spell_q12851_going_bearback_AuraScript::HandleEffectApply, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
            }

        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_q12851_going_bearback_AuraScript();
        }
};

// http://www.wowhead.com/quest=12937 Relief for the Fallen
// 55804 Healing Finished
enum Quest12937Data
{
    SPELL_TRIGGER_AID_OF_THE_EARTHEN    = 55809,
    NPC_FALLEN_EARTHEN_DEFENDER         = 30035,
};

class spell_q12937_relief_for_the_fallen : public SpellScriptLoader
{
    public:
        spell_q12937_relief_for_the_fallen() : SpellScriptLoader("spell_q12937_relief_for_the_fallen") { }

        class spell_q12937_relief_for_the_fallen_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12937_relief_for_the_fallen_SpellScript);

            bool Load() override
            {
                return GetCaster()->GetTypeId() == TYPEID_PLAYER;
            }

            bool Validate(SpellInfo const* /*spellEntry*/) override
            {
                return ValidateSpellInfo({ SPELL_TRIGGER_AID_OF_THE_EARTHEN });
            }

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                Player* caster = GetCaster()->ToPlayer();
                if (Creature* target = GetHitCreature())
                {
                    caster->CastSpell(caster, SPELL_TRIGGER_AID_OF_THE_EARTHEN, true);
                    caster->KilledMonsterCredit(NPC_FALLEN_EARTHEN_DEFENDER);
                    target->DespawnOrUnsummon();
                }
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q12937_relief_for_the_fallen_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12937_relief_for_the_fallen_SpellScript();
        }
};

enum Whoarethey
{
    SPELL_MALE_DISGUISE = 38080,
    SPELL_FEMALE_DISGUISE = 38081,
    SPELL_GENERIC_DISGUISE = 32756
};

class spell_q10041_q10040_who_are_they : public SpellScriptLoader
{
    public:
        spell_q10041_q10040_who_are_they() : SpellScriptLoader("spell_q10041_q10040_who_are_they") { }

        class spell_q10041_q10040_who_are_they_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q10041_q10040_who_are_they_SpellScript);

            bool Validate(SpellInfo const* /*spellEntry*/) override
            {
                return ValidateSpellInfo({ SPELL_MALE_DISGUISE, SPELL_FEMALE_DISGUISE, SPELL_GENERIC_DISGUISE });
            }

            void HandleScript(SpellEffIndex effIndex)
            {
                PreventHitDefaultEffect(effIndex);
                if (Player* target = GetHitPlayer())
                {
                    target->CastSpell(target, target->getGender() == GENDER_MALE ? SPELL_MALE_DISGUISE : SPELL_FEMALE_DISGUISE, true);
                    target->CastSpell(target, SPELL_GENERIC_DISGUISE, true);
                }
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q10041_q10040_who_are_they_SpellScript::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q10041_q10040_who_are_they_SpellScript();
        }
};

enum symboloflife
{
    SPELL_PERMANENT_FEIGN_DEATH = 29266,
};

// 8593 Symbol of life dummy
class spell_symbol_of_life_dummy : public SpellScriptLoader
{
    public:
        spell_symbol_of_life_dummy() : SpellScriptLoader("spell_symbol_of_life_dummy") { }

        class spell_symbol_of_life_dummy_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_symbol_of_life_dummy_SpellScript);

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                if (Creature* target = GetHitCreature())
                {
                    if (target->HasAura(SPELL_PERMANENT_FEIGN_DEATH))
                    {
                        target->RemoveAurasDueToSpell(SPELL_PERMANENT_FEIGN_DEATH);
                        target->SetDynamicFlags(0);
                        target->SetUnitFlags2(UnitFlags2(0));
                        target->SetHealth(target->GetMaxHealth() / 2);
                        target->SetPower(POWER_MANA, target->GetMaxPower(POWER_MANA) * 0.75f);
                    }
                }
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_symbol_of_life_dummy_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_symbol_of_life_dummy_SpellScript();
        }
};

// http://www.wowhead.com/quest=12659 Scalps!
// 52090 Ahunae's Knife
enum Quest12659Data
{
    NPC_SCALPS_KC_BUNNY = 28622,
};

class spell_q12659_ahunaes_knife : public SpellScriptLoader
{
    public:
        spell_q12659_ahunaes_knife() : SpellScriptLoader("spell_q12659_ahunaes_knife") { }

        class spell_q12659_ahunaes_knife_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12659_ahunaes_knife_SpellScript);

            bool Load() override
            {
                return GetCaster()->GetTypeId() == TYPEID_PLAYER;
            }

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                Player* caster = GetCaster()->ToPlayer();
                if (Creature* target = GetHitCreature())
                {
                    target->DespawnOrUnsummon();
                    caster->KilledMonsterCredit(NPC_SCALPS_KC_BUNNY);
                }
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q12659_ahunaes_knife_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12659_ahunaes_knife_SpellScript();
        }
};

enum StoppingTheSpread
{
    NPC_VILLAGER_KILL_CREDIT                     = 18240,
    SPELL_FLAMES                                 = 39199
};

class spell_q9874_liquid_fire : public SpellScriptLoader
{
    public:
        spell_q9874_liquid_fire() : SpellScriptLoader("spell_q9874_liquid_fire")
        {
        }

        class spell_q9874_liquid_fire_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q9874_liquid_fire_SpellScript);

            bool Load() override
            {
                return GetCaster()->GetTypeId() == TYPEID_PLAYER;
            }

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                Player* caster = GetCaster()->ToPlayer();
                if (Creature* target = GetHitCreature())
                    if (!target->HasAura(SPELL_FLAMES))
                    {
                        caster->KilledMonsterCredit(NPC_VILLAGER_KILL_CREDIT);
                        target->CastSpell(target, SPELL_FLAMES, true);
                        target->DespawnOrUnsummon(60000);
                    }
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q9874_liquid_fire_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q9874_liquid_fire_SpellScript();
        }
};

enum SalvagingLifesStength
{
    NPC_SHARD_KILL_CREDIT                        = 29303,
};

class spell_q12805_lifeblood_dummy : public SpellScriptLoader
{
    public:
        spell_q12805_lifeblood_dummy() : SpellScriptLoader("spell_q12805_lifeblood_dummy")
        {
        }

        class spell_q12805_lifeblood_dummy_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12805_lifeblood_dummy_SpellScript);

            bool Load() override
            {
                return GetCaster()->GetTypeId() == TYPEID_PLAYER;
            }

            void HandleScript(SpellEffIndex /*effIndex*/)
            {
                Player* caster = GetCaster()->ToPlayer();
                if (Creature* target = GetHitCreature())
                {
                    caster->KilledMonsterCredit(NPC_SHARD_KILL_CREDIT);
                    target->CastSpell(target, uint32(GetEffectValue()), true);
                    target->DespawnOrUnsummon(2000);
                }
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q12805_lifeblood_dummy_SpellScript::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12805_lifeblood_dummy_SpellScript();
        }
};

/*
 http://www.wowhead.com/quest=13283 King of the Mountain
 http://www.wowhead.com/quest=13280 King of the Mountain
 59643 Plant Horde Battle Standard
 4338 Plant Alliance Battle Standard
 */
enum BattleStandard
{
    NPC_KING_OF_THE_MOUNTAINT_KC         = 31766,
    SPELL_PLANT_HORDE_BATTLE_STANDARD    = 59643,
    SPELL_HORDE_BATTLE_STANDARD_STATE    = 59642,
    SPELL_ALLIANCE_BATTLE_STANDARD_STATE = 4339,
    SPELL_JUMP_ROCKET_BLAST              = 4340
};

class spell_q13280_13283_plant_battle_standard : public SpellScript
{
    PrepareSpellScript(spell_q13280_13283_plant_battle_standard);

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        uint32 triggeredSpellID = SPELL_ALLIANCE_BATTLE_STANDARD_STATE;

        caster->HandleEmoteCommand(EMOTE_ONESHOT_ROAR);
        if (caster->IsVehicle())
            if (Unit* player = caster->GetVehicleKit()->GetPassenger(0))
                player->ToPlayer()->KilledMonsterCredit(NPC_KING_OF_THE_MOUNTAINT_KC);

        if (GetSpellInfo()->Id == SPELL_PLANT_HORDE_BATTLE_STANDARD)
            triggeredSpellID = SPELL_HORDE_BATTLE_STANDARD_STATE;

        target->RemoveAllAuras();
        target->CastSpell(target, triggeredSpellID, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_q13280_13283_plant_battle_standard::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

class spell_q13280_13283_jump_jets : public SpellScript
{
    PrepareSpellScript(spell_q13280_13283_jump_jets);

    void HandleCast()
    {
        Unit* caster = GetCaster();
        if (caster->IsVehicle())
            if (Unit* rocketBunny = caster->GetVehicleKit()->GetPassenger(1))
                rocketBunny->CastSpell(rocketBunny, SPELL_JUMP_ROCKET_BLAST, true);
    }

    void Register() override
    {
        OnCast += SpellCastFn(spell_q13280_13283_jump_jets::HandleCast);
    }
};

enum ChumTheWaterSummons
{
    SUMMON_ANGRY_KVALDIR = 66737,
    SUMMON_NORTH_SEA_MAKO = 66738,
    SUMMON_NORTH_SEA_THRESHER = 66739,
    SUMMON_NORTH_SEA_BLUE_SHARK = 66740
};

class spell_q14112_14145_chum_the_water: public SpellScriptLoader
{
    public:
        spell_q14112_14145_chum_the_water() : SpellScriptLoader("spell_q14112_14145_chum_the_water") { }

        class spell_q14112_14145_chum_the_water_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q14112_14145_chum_the_water_SpellScript);

            bool Validate(SpellInfo const* /*spellEntry*/) override
            {
                return ValidateSpellInfo({ SUMMON_ANGRY_KVALDIR, SUMMON_NORTH_SEA_MAKO, SUMMON_NORTH_SEA_THRESHER, SUMMON_NORTH_SEA_BLUE_SHARK });
            }

            void HandleScriptEffect(SpellEffIndex /*effIndex*/)
            {
                Unit* caster = GetCaster();
                caster->CastSpell(caster, RAND(SUMMON_ANGRY_KVALDIR, SUMMON_NORTH_SEA_MAKO, SUMMON_NORTH_SEA_THRESHER, SUMMON_NORTH_SEA_BLUE_SHARK));
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q14112_14145_chum_the_water_SpellScript::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q14112_14145_chum_the_water_SpellScript();
        }
};

// http://old01.wowhead.com/quest=9452 - Red Snapper - Very Tasty!
enum RedSnapperVeryTasty
{
    ITEM_RED_SNAPPER             = 23614,
    SPELL_CAST_NET               = 29866,
    SPELL_FISHED_UP_MURLOC       = 29869
};

class spell_q9452_cast_net: public SpellScriptLoader
{
    public:
        spell_q9452_cast_net() : SpellScriptLoader("spell_q9452_cast_net") { }

        class spell_q9452_cast_net_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q9452_cast_net_SpellScript);

            bool Load() override
            {
                return GetCaster()->GetTypeId() == TYPEID_PLAYER;
            }

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                Player* caster = GetCaster()->ToPlayer();
                if (roll_chance_i(66))
                    caster->AddItem(ITEM_RED_SNAPPER, 1);
                else
                    caster->CastSpell(caster, SPELL_FISHED_UP_MURLOC, true);
            }

            void HandleActiveObject(SpellEffIndex effIndex)
            {
                PreventHitDefaultEffect(effIndex);
                GetHitGObj()->SetRespawnTime(roll_chance_i(50) ? 2 * MINUTE : 3 * MINUTE);
                GetHitGObj()->Use(GetCaster());
                GetHitGObj()->SetLootState(GO_JUST_DEACTIVATED);
            }

            void Register() override
            {
                OnEffectHit += SpellEffectFn(spell_q9452_cast_net_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
                OnEffectHitTarget += SpellEffectFn(spell_q9452_cast_net_SpellScript::HandleActiveObject, EFFECT_1, SPELL_EFFECT_ACTIVATE_OBJECT);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q9452_cast_net_SpellScript();
        }
};

enum PoundDrumSpells
{
    SPELL_SUMMON_DEEP_JORMUNGAR     = 66510,
    SPELL_STORMFORGED_MOLE_MACHINE  = 66492
};

class spell_q14076_14092_pound_drum : public SpellScriptLoader
{
    public:
        spell_q14076_14092_pound_drum() : SpellScriptLoader("spell_q14076_14092_pound_drum") { }

        class spell_q14076_14092_pound_drum_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q14076_14092_pound_drum_SpellScript);

            void HandleSummon()
            {
                Unit* caster = GetCaster();

                if (roll_chance_i(80))
                    caster->CastSpell(caster, SPELL_SUMMON_DEEP_JORMUNGAR, true);
                else
                    caster->CastSpell(caster, SPELL_STORMFORGED_MOLE_MACHINE, true);
            }

            void HandleActiveObject(SpellEffIndex /*effIndex*/)
            {
                GetHitGObj()->SetLootState(GO_JUST_DEACTIVATED);
            }

            void Register() override
            {
                OnCast += SpellCastFn(spell_q14076_14092_pound_drum_SpellScript::HandleSummon);
                OnEffectHitTarget += SpellEffectFn(spell_q14076_14092_pound_drum_SpellScript::HandleActiveObject, EFFECT_0, SPELL_EFFECT_ACTIVATE_OBJECT);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q14076_14092_pound_drum_SpellScript();
        }
};

class spell_q12279_cast_net : public SpellScriptLoader
{
    public:
        spell_q12279_cast_net() : SpellScriptLoader("spell_q12279_cast_net") { }

        class spell_q12279_cast_net_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12279_cast_net_SpellScript);

            void HandleActiveObject(SpellEffIndex /*effIndex*/)
            {
                GetHitGObj()->SetLootState(GO_JUST_DEACTIVATED);
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q12279_cast_net_SpellScript::HandleActiveObject, EFFECT_1, SPELL_EFFECT_ACTIVATE_OBJECT);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12279_cast_net_SpellScript();
        }
};

enum HodirsHelm
{
    SAY_1               = 1,
    SAY_2               = 2,
    NPC_KILLCREDIT      = 30210, // Hodir's Helm KC Bunny
    NPC_ICE_SPIKE_BUNNY = 30215
};

class spell_q12987_read_pronouncement : public SpellScriptLoader
{
public:
    spell_q12987_read_pronouncement() : SpellScriptLoader("spell_q12987_read_pronouncement") { }

    class spell_q12987_read_pronouncement_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_q12987_read_pronouncement_AuraScript);

        void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
        {
            // player must cast kill credit and do emote text, according to sniff
            if (Player* target = GetTarget()->ToPlayer())
            {
                if (Creature* trigger = target->FindNearestCreature(NPC_ICE_SPIKE_BUNNY, 25.0f))
                {
                    sCreatureTextMgr->SendChat(trigger, SAY_1, target, CHAT_MSG_ADDON, LANG_ADDON, TEXT_RANGE_NORMAL, 0, SoundKitPlayType::Normal, TEAM_OTHER, false, target);
                    target->KilledMonsterCredit(NPC_KILLCREDIT);
                    sCreatureTextMgr->SendChat(trigger, SAY_2, target, CHAT_MSG_ADDON, LANG_ADDON, TEXT_RANGE_NORMAL, 0, SoundKitPlayType::Normal, TEAM_OTHER, false, target);
                }
            }
        }

        void Register() override
        {
            AfterEffectApply += AuraEffectApplyFn(spell_q12987_read_pronouncement_AuraScript::OnApply, EFFECT_0, SPELL_AURA_NONE, AURA_EFFECT_HANDLE_REAL);
        }
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_q12987_read_pronouncement_AuraScript();
    }
};

enum LeaveNothingToChance
{
    NPC_UPPER_MINE_SHAFT            = 27436,
    NPC_LOWER_MINE_SHAFT            = 27437,

    SPELL_UPPER_MINE_SHAFT_CREDIT   = 48744,
    SPELL_LOWER_MINE_SHAFT_CREDIT   = 48745,
};

class spell_q12277_wintergarde_mine_explosion : public SpellScriptLoader
{
    public:
        spell_q12277_wintergarde_mine_explosion() : SpellScriptLoader("spell_q12277_wintergarde_mine_explosion") { }

        class spell_q12277_wintergarde_mine_explosion_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12277_wintergarde_mine_explosion_SpellScript);

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                if (Creature* unitTarget = GetHitCreature())
                {
                    if (Unit* caster = GetCaster())
                    {
                        if (caster->GetTypeId() == TYPEID_UNIT)
                        {
                            if (Unit* owner = caster->GetOwner())
                            {
                                switch (unitTarget->GetEntry())
                                {
                                    case NPC_UPPER_MINE_SHAFT:
                                        caster->CastSpell(owner, SPELL_UPPER_MINE_SHAFT_CREDIT, true);
                                        break;
                                    case NPC_LOWER_MINE_SHAFT:
                                        caster->CastSpell(owner, SPELL_LOWER_MINE_SHAFT_CREDIT, true);
                                        break;
                                    default:
                                        break;
                                }
                            }
                        }
                    }
                }
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q12277_wintergarde_mine_explosion_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12277_wintergarde_mine_explosion_SpellScript();
        }
};

enum FocusOnTheBeach
{
    SPELL_BUNNY_CREDIT_BEAM = 47390,
};

class spell_q12066_bunny_kill_credit : public SpellScriptLoader
{
public:
    spell_q12066_bunny_kill_credit() : SpellScriptLoader("spell_q12066_bunny_kill_credit") { }

    class spell_q12066_bunny_kill_credit_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_q12066_bunny_kill_credit_SpellScript);

        void HandleDummy(SpellEffIndex /*effIndex*/)
        {
            if (Creature* target = GetHitCreature())
                target->CastSpell(GetCaster(), SPELL_BUNNY_CREDIT_BEAM, false);
        }

        void Register() override
        {
            OnEffectHitTarget += SpellEffectFn(spell_q12066_bunny_kill_credit_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_q12066_bunny_kill_credit_SpellScript();
    }
};

enum ACleansingSong
{
    SPELL_SUMMON_SPIRIT_ATAH        = 52954,
    SPELL_SUMMON_SPIRIT_HAKHALAN    = 52958,
    SPELL_SUMMON_SPIRIT_KOOSU       = 52959,

    AREA_BITTERTIDELAKE             = 4385,
    AREA_RIVERSHEART                = 4290,
    AREA_WINTERGRASPRIVER           = 4388,
};

class spell_q12735_song_of_cleansing : public SpellScriptLoader
{
    public:
        spell_q12735_song_of_cleansing() : SpellScriptLoader("spell_q12735_song_of_cleansing") { }

        class spell_q12735_song_of_cleansing_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12735_song_of_cleansing_SpellScript);

            void HandleScript(SpellEffIndex /*effIndex*/)
            {
                Unit* caster = GetCaster();
                switch (caster->GetAreaId())
                {
                    case AREA_BITTERTIDELAKE:
                        caster->CastSpell(caster, SPELL_SUMMON_SPIRIT_ATAH);
                        break;
                    case AREA_RIVERSHEART:
                        caster->CastSpell(caster, SPELL_SUMMON_SPIRIT_HAKHALAN);
                        break;
                    case AREA_WINTERGRASPRIVER:
                        caster->CastSpell(caster, SPELL_SUMMON_SPIRIT_KOOSU);
                        break;
                    default:
                        break;
                }
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q12735_song_of_cleansing_SpellScript::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12735_song_of_cleansing_SpellScript();
        }
};

enum DefendingWyrmrestTemple
{
    SPELL_SUMMON_WYRMREST_DEFENDER       = 49207
};

class spell_q12372_cast_from_gossip_trigger : public SpellScriptLoader
{
    public:
        spell_q12372_cast_from_gossip_trigger() : SpellScriptLoader("spell_q12372_cast_from_gossip_trigger") { }

        class spell_q12372_cast_from_gossip_trigger_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12372_cast_from_gossip_trigger_SpellScript);

            void HandleScript(SpellEffIndex /*effIndex*/)
            {
                GetCaster()->CastSpell(GetCaster(), SPELL_SUMMON_WYRMREST_DEFENDER, true);
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q12372_cast_from_gossip_trigger_SpellScript::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12372_cast_from_gossip_trigger_SpellScript();
        }
};

// http://www.wowhead.com/quest=12372 Defending Wyrmrest Temple
// 49370 - Wyrmrest Defender: Destabilize Azure Dragonshrine Effect
enum Quest12372Data
{
    // NPCs
    NPC_WYRMREST_TEMPLE_CREDIT       = 27698,
    // Spells
    WHISPER_ON_HIT_BY_FORCE_WHISPER       = 1
};

class spell_q12372_destabilize_azure_dragonshrine_dummy : public SpellScriptLoader
{
    public:
        spell_q12372_destabilize_azure_dragonshrine_dummy() : SpellScriptLoader("spell_q12372_destabilize_azure_dragonshrine_dummy") { }

        class spell_q12372_destabilize_azure_dragonshrine_dummy_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12372_destabilize_azure_dragonshrine_dummy_SpellScript);

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                if (GetHitCreature())
                    if (Unit* caster = GetOriginalCaster())
                        if (Vehicle* vehicle = caster->GetVehicleKit())
                            if (Unit* passenger = vehicle->GetPassenger(0))
                                if (Player* player = passenger->ToPlayer())
                                    player->KilledMonsterCredit(NPC_WYRMREST_TEMPLE_CREDIT);
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q12372_destabilize_azure_dragonshrine_dummy_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12372_destabilize_azure_dragonshrine_dummy_SpellScript();
        }
};

// ID - 50287 Azure Dragon: On Death Force Cast Wyrmrest Defender to Whisper to Controller - Random (cast from Azure Dragons and Azure Drakes on death)
class spell_q12372_azure_on_death_force_whisper : public SpellScriptLoader
{
    public:
        spell_q12372_azure_on_death_force_whisper() : SpellScriptLoader("spell_q12372_azure_on_death_force_whisper") { }

        class spell_q12372_azure_on_death_force_whisper_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12372_azure_on_death_force_whisper_SpellScript);

            void HandleScript(SpellEffIndex /*effIndex*/)
            {
                if (Creature* defender = GetHitCreature())
                    defender->AI()->Talk(WHISPER_ON_HIT_BY_FORCE_WHISPER, defender->GetCharmerOrOwner());
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q12372_azure_on_death_force_whisper_SpellScript::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12372_azure_on_death_force_whisper_SpellScript();
        }
};

// "Bombing Run" and "Bomb Them Again!"
enum Quest11010_11102_11023Data
{
    // Spell
    SPELL_FLAK_CANNON_TRIGGER = 40110,
    SPELL_CHOOSE_LOC          = 40056,
    SPELL_AGGRO_CHECK         = 40112,
    // NPCs
    NPC_FEL_CANNON2           = 23082
};

// 40113 Knockdown Fel Cannon: The Aggro Check Aura
class spell_q11010_q11102_q11023_aggro_check_aura : public SpellScriptLoader
{
    public:
        spell_q11010_q11102_q11023_aggro_check_aura() : SpellScriptLoader("spell_q11010_q11102_q11023_aggro_check_aura") { }

        class spell_q11010_q11102_q11023_aggro_check_aura_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_q11010_q11102_q11023_aggro_check_aura_AuraScript);

            void HandleTriggerSpell(AuraEffect const* /*aurEff*/)
            {
                if (Unit* target = GetTarget())
                    // On trigger proccing
                    target->CastSpell(target, SPELL_AGGRO_CHECK);
            }

            void Register() override
            {
               OnEffectPeriodic += AuraEffectPeriodicFn(spell_q11010_q11102_q11023_aggro_check_aura_AuraScript::HandleTriggerSpell, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_q11010_q11102_q11023_aggro_check_aura_AuraScript();
        }
};

// 40112 Knockdown Fel Cannon: The Aggro Check
class spell_q11010_q11102_q11023_aggro_check : public SpellScriptLoader
{
    public:
        spell_q11010_q11102_q11023_aggro_check() : SpellScriptLoader("spell_q11010_q11102_q11023_aggro_check") { }

        class spell_q11010_q11102_q11023_aggro_check_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q11010_q11102_q11023_aggro_check_SpellScript);

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                if (Player* playerTarget = GetHitPlayer())
                    // Check if found player target is on fly mount or using flying form
                    if (playerTarget->HasAuraType(SPELL_AURA_FLY) || playerTarget->HasAuraType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED))
                        playerTarget->CastSpell(playerTarget, SPELL_FLAK_CANNON_TRIGGER, TRIGGERED_IGNORE_CASTER_MOUNTED_OR_ON_VEHICLE);
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q11010_q11102_q11023_aggro_check_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q11010_q11102_q11023_aggro_check_SpellScript();
        }
};

// 40119 Knockdown Fel Cannon: The Aggro Burst
class spell_q11010_q11102_q11023_aggro_burst : public SpellScriptLoader
{
    public:
        spell_q11010_q11102_q11023_aggro_burst() : SpellScriptLoader("spell_q11010_q11102_q11023_aggro_burst") { }

        class spell_q11010_q11102_q11023_aggro_burst_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_q11010_q11102_q11023_aggro_burst_AuraScript);

            void HandleEffectPeriodic(AuraEffect const* /*aurEff*/)
            {
                if (Unit* target = GetTarget())
                    // On each tick cast Choose Loc to trigger summon
                    target->CastSpell(target, SPELL_CHOOSE_LOC);
            }

            void Register() override
            {
                OnEffectPeriodic += AuraEffectPeriodicFn(spell_q11010_q11102_q11023_aggro_burst_AuraScript::HandleEffectPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_q11010_q11102_q11023_aggro_burst_AuraScript();
        }
};

// 40056 Knockdown Fel Cannon: Choose Loc
class spell_q11010_q11102_q11023_choose_loc : public SpellScriptLoader
{
    public:
        spell_q11010_q11102_q11023_choose_loc() : SpellScriptLoader("spell_q11010_q11102_q11023_choose_loc") { }

        class spell_q11010_q11102_q11023_choose_loc_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q11010_q11102_q11023_choose_loc_SpellScript);

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                Unit* caster = GetCaster();
                // Check for player that is in 65 y range
                std::list<Player*> playerList;
                Trinity::AnyPlayerInObjectRangeCheck checker(caster, 65.0f);
                Trinity::PlayerListSearcher<Trinity::AnyPlayerInObjectRangeCheck> searcher(caster, playerList, checker);
                Cell::VisitWorldObjects(caster, searcher, 65.0f);
                for (std::list<Player*>::const_iterator itr = playerList.begin(); itr != playerList.end(); ++itr)
                    // Check if found player target is on fly mount or using flying form
                    if ((*itr)->HasAuraType(SPELL_AURA_FLY) || (*itr)->HasAuraType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED))
                        // Summom Fel Cannon (bunny version) at found player
                        caster->SummonCreature(NPC_FEL_CANNON2, (*itr)->GetPositionX(), (*itr)->GetPositionY(), (*itr)->GetPositionZ());
            }

            void Register() override
            {
                OnEffectHit += SpellEffectFn(spell_q11010_q11102_q11023_choose_loc_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q11010_q11102_q11023_choose_loc_SpellScript();
        }
};

// 39844 - Skyguard Blasting Charge
// 40160 - Throw Bomb
class spell_q11010_q11102_q11023_q11008_check_fly_mount : public SpellScriptLoader
{
    public:
        spell_q11010_q11102_q11023_q11008_check_fly_mount() : SpellScriptLoader("spell_q11010_q11102_q11023_q11008_check_fly_mount") { }

        class spell_q11010_q11102_q11023_q11008_check_fly_mount_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q11010_q11102_q11023_q11008_check_fly_mount_SpellScript);

            SpellCastResult CheckRequirement()
            {
                Unit* caster = GetCaster();
                // This spell will be cast only if caster has one of these auras
                if (!(caster->HasAuraType(SPELL_AURA_FLY) || caster->HasAuraType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED)))
                    return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;
                return SPELL_CAST_OK;
            }

            void Register() override
            {
                OnCheckCast += SpellCheckCastFn(spell_q11010_q11102_q11023_q11008_check_fly_mount_SpellScript::CheckRequirement);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q11010_q11102_q11023_q11008_check_fly_mount_SpellScript();
        }
};

enum SpellZuldrakRat
{
    SPELL_SUMMON_GORGED_LURKING_BASILISK    = 50928
};

class spell_q12527_zuldrak_rat : public SpellScriptLoader
{
    public:
        spell_q12527_zuldrak_rat() : SpellScriptLoader("spell_q12527_zuldrak_rat") { }

        class spell_q12527_zuldrak_rat_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12527_zuldrak_rat_SpellScript);

            bool Validate(SpellInfo const* /*spell*/) override
            {
                return ValidateSpellInfo({ SPELL_SUMMON_GORGED_LURKING_BASILISK });
            }

            void HandleScriptEffect(SpellEffIndex /* effIndex */)
            {
                if (GetHitAura() && GetHitAura()->GetStackAmount() >= GetSpellInfo()->StackAmount)
                {
                    GetHitUnit()->CastSpell(nullptr, SPELL_SUMMON_GORGED_LURKING_BASILISK, true);
                    if (Creature* basilisk = GetHitUnit()->ToCreature())
                        basilisk->DespawnOrUnsummon();
                }
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q12527_zuldrak_rat_SpellScript::HandleScriptEffect, EFFECT_1, SPELL_EFFECT_SCRIPT_EFFECT);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12527_zuldrak_rat_SpellScript();
        }
};

// 55368 - Summon Stefan
class spell_q12661_q12669_q12676_q12677_q12713_summon_stefan : public SpellScriptLoader
{
    public:
        spell_q12661_q12669_q12676_q12677_q12713_summon_stefan() : SpellScriptLoader("spell_q12661_q12669_q12676_q12677_q12713_summon_stefan") { }

        class spell_q12661_q12669_q12676_q12677_q12713_summon_stefan_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12661_q12669_q12676_q12677_q12713_summon_stefan_SpellScript);

            void SetDest(SpellDestination& dest)
            {
                // Adjust effect summon position
                Position const offset = { 0.0f, 0.0f, 20.0f, 0.0f };
                dest.RelocateOffset(offset);
            }

            void Register() override
            {
                OnDestinationTargetSelect += SpellDestinationTargetSelectFn(spell_q12661_q12669_q12676_q12677_q12713_summon_stefan_SpellScript::SetDest, EFFECT_0, TARGET_DEST_CASTER_BACK);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12661_q12669_q12676_q12677_q12713_summon_stefan_SpellScript();
        }
};

enum QuenchingMist
{
    SPELL_FLICKERING_FLAMES = 53504
};

class spell_q12730_quenching_mist : public SpellScriptLoader
{
    public:
        spell_q12730_quenching_mist() : SpellScriptLoader("spell_q12730_quenching_mist") { }

        class spell_q12730_quenching_mist_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_q12730_quenching_mist_AuraScript);

            bool Validate(SpellInfo const* /*spellInfo*/) override
            {
                return ValidateSpellInfo({ SPELL_FLICKERING_FLAMES });
            }

            void HandleEffectPeriodic(AuraEffect const* /*aurEff*/)
            {
                GetTarget()->RemoveAurasDueToSpell(SPELL_FLICKERING_FLAMES);
            }

            void Register() override
            {
                OnEffectPeriodic += AuraEffectPeriodicFn(spell_q12730_quenching_mist_AuraScript::HandleEffectPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_HEAL);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_q12730_quenching_mist_AuraScript();
        }
};

// 13291 - Borrowed Technology/13292 - The Solution Solution /Daily//13239 - Volatility/13261 - Volatiliy /Daily//
enum Quest13291_13292_13239_13261Data
{
    // NPCs
    NPC_SKYTALON       = 31583,
    NPC_DECOY          = 31578,
    // Spells
    SPELL_RIDE         = 59319
};

class spell_q13291_q13292_q13239_q13261_frostbrood_skytalon_grab_decoy : public SpellScriptLoader
{
    public:
        spell_q13291_q13292_q13239_q13261_frostbrood_skytalon_grab_decoy() : SpellScriptLoader("spell_q13291_q13292_q13239_q13261_frostbrood_skytalon_grab_decoy") { }

        class spell_q13291_q13292_q13239_q13261_frostbrood_skytalon_grab_decoy_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q13291_q13292_q13239_q13261_frostbrood_skytalon_grab_decoy_SpellScript);

            bool Validate(SpellInfo const* /*spell*/) override
            {
                return ValidateSpellInfo({ SPELL_RIDE });
            }

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                if (!GetHitCreature())
                    return;
                // TO DO: Being triggered is hack, but in checkcast it doesn't pass aurastate requirements.
                // Beside that the decoy won't keep it's freeze animation state when enter.
                GetHitCreature()->CastSpell(GetCaster(), SPELL_RIDE, true);
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q13291_q13292_q13239_q13261_frostbrood_skytalon_grab_decoy_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q13291_q13292_q13239_q13261_frostbrood_skytalon_grab_decoy_SpellScript();
        }
};

// 59303 - Summon Frost Wyrm
class spell_q13291_q13292_q13239_q13261_armored_decoy_summon_skytalon : public SpellScriptLoader
{
    public:
        spell_q13291_q13292_q13239_q13261_armored_decoy_summon_skytalon() : SpellScriptLoader("spell_q13291_q13292_q13239_q13261_armored_decoy_summon_skytalon") { }

        class spell_q13291_q13292_q13239_q13261_armored_decoy_summon_skytalon_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q13291_q13292_q13239_q13261_armored_decoy_summon_skytalon_SpellScript);

            void SetDest(SpellDestination& dest)
            {
                // Adjust effect summon position
                Position const offset = { 0.0f, 0.0f, 20.0f, 0.0f };
                dest.RelocateOffset(offset);
            }

            void Register() override
            {
                OnDestinationTargetSelect += SpellDestinationTargetSelectFn(spell_q13291_q13292_q13239_q13261_armored_decoy_summon_skytalon_SpellScript::SetDest, EFFECT_0, TARGET_DEST_CASTER_BACK);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q13291_q13292_q13239_q13261_armored_decoy_summon_skytalon_SpellScript();
        }
};

// 12601 - Second Chances: Summon Landgren's Soul Moveto Target Bunny
class spell_q12847_summon_soul_moveto_bunny : public SpellScriptLoader
{
    public:
        spell_q12847_summon_soul_moveto_bunny() : SpellScriptLoader("spell_q12847_summon_soul_moveto_bunny") { }

        class spell_q12847_summon_soul_moveto_bunny_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12847_summon_soul_moveto_bunny_SpellScript);

            void SetDest(SpellDestination& dest)
            {
                // Adjust effect summon position
                Position const offset = { 0.0f, 0.0f, 2.5f, 0.0f };
                dest.RelocateOffset(offset);
            }

            void Register() override
            {
                OnDestinationTargetSelect += SpellDestinationTargetSelectFn(spell_q12847_summon_soul_moveto_bunny_SpellScript::SetDest, EFFECT_0, TARGET_DEST_CASTER);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12847_summon_soul_moveto_bunny_SpellScript();
        }
};

enum BearFlankMaster
{
    SPELL_CREATE_BEAR_FLANK = 56566,
    SPELL_BEAR_FLANK_FAIL   = 56569
};

class spell_q13011_bear_flank_master : public SpellScriptLoader
{
    public:
        spell_q13011_bear_flank_master() : SpellScriptLoader("spell_q13011_bear_flank_master") { }

        class spell_q13011_bear_flank_master_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q13011_bear_flank_master_SpellScript);

            bool Validate(SpellInfo const* /*spellInfo*/) override
            {
                return ValidateSpellInfo(
                {
                    SPELL_CREATE_BEAR_FLANK,
                    SPELL_BEAR_FLANK_FAIL
                });
            }

            bool Load() override
            {
                return GetCaster()->GetTypeId() == TYPEID_UNIT;
            }

            void HandleScript(SpellEffIndex /*effIndex*/)
            {
                if (Player* player = GetHitPlayer())
                {
                    if (roll_chance_i(50))
                    {
                        Creature* creature = GetCaster()->ToCreature();
                        player->CastSpell(creature, SPELL_BEAR_FLANK_FAIL);
                        creature->AI()->Talk(0, player);
                    }
                    else
                        player->CastSpell(player, SPELL_CREATE_BEAR_FLANK);
                }
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q13011_bear_flank_master_SpellScript::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q13011_bear_flank_master_SpellScript();
        }
};

class spell_q13086_cannons_target : public SpellScriptLoader
{
    public:
        spell_q13086_cannons_target() : SpellScriptLoader("spell_q13086_cannons_target") { }

        class spell_q13086_cannons_target_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q13086_cannons_target_SpellScript);

            bool Validate(SpellInfo const* spellInfo) override
            {
                return !spellInfo->GetEffects().empty()
                    && ValidateSpellInfo({ static_cast<uint32>(spellInfo->GetEffect(EFFECT_0).CalcValue()) });
            }

            void HandleEffectDummy(SpellEffIndex /*effIndex*/)
            {
                if (WorldLocation const* pos = GetExplTargetDest())
                    GetCaster()->CastSpell(pos->GetPosition(), GetEffectValue(), true);
            }

            void Register() override
            {
                OnEffectHit += SpellEffectFn(spell_q13086_cannons_target_SpellScript::HandleEffectDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q13086_cannons_target_SpellScript();
        }
};

enum ThatsAbominable
{
    QUEST_THATS_ABOMINABLE                = 13264,

    NPC_ICY_GHOUL                         = 31142,
    NPC_RISEN_ALLIANCE_SOLDIERS           = 31205,
    NPC_VICIOUS_GEIST                     = 31147,
    NPC_RENIMATED_ABOMINATION             = 31692,

    SPELL_ICY_GHOUL_CREDIT                = 59591, // Credit for Icy Ghoul
    SPELL_VICIOUS_GEISTS_CREDIT           = 60042, // Credit for Vicious Geists
    SPELL_RISEN_ALLIANCE_SOLDIERS_CREDIT  = 60040, // Credit for Risen Alliance Soldiers
};

class spell_q13264_thats_abominable : public SpellScriptLoader
{
    public:
        spell_q13264_thats_abominable() : SpellScriptLoader("spell_q13264_thats_abominable") { }

        class spell_q13264_thats_abominable_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q13264_thats_abominable_SpellScript);

            bool Validate(SpellInfo const* /*spellInfo*/) override
            {
                return ValidateSpellInfo(
                {
                    SPELL_ICY_GHOUL_CREDIT,
                    SPELL_VICIOUS_GEISTS_CREDIT,
                    SPELL_RISEN_ALLIANCE_SOLDIERS_CREDIT,
                });
            }

            void HandleKnockBack(SpellEffIndex effIndex)
            {
                PreventHitDefaultEffect(effIndex);

                if (Creature* creature = GetHitCreature())
                    if (Unit* charmer = GetCaster()->GetCharmerOrOwner())
                        if (Player* player = charmer->ToPlayer())
                            if (player->GetQuestStatus(QUEST_THATS_ABOMINABLE) == QUEST_STATUS_INCOMPLETE)
                                if (GiveCreditIfValid(player, creature))
                                    creature->KillSelf();
            }

            bool GiveCreditIfValid(Player* player, Creature* creature)
            {
                uint32 entry = creature->GetEntry();

                switch(entry)
                {
                    case NPC_ICY_GHOUL:
                        player->CastSpell(player, SPELL_ICY_GHOUL_CREDIT, true);
                        return true;
                    case NPC_VICIOUS_GEIST:
                        player->CastSpell(player, SPELL_VICIOUS_GEISTS_CREDIT, true);
                        return true;
                    case NPC_RISEN_ALLIANCE_SOLDIERS:
                        player->CastSpell(player, SPELL_RISEN_ALLIANCE_SOLDIERS_CREDIT, true);
                        return true;
                }

                return false;
            }

            void HandleScript(SpellEffIndex /*effIndex*/)
            {
                if (Creature* creature = GetCaster()->ToCreature()) {
                    creature->KillSelf();
                    creature->DespawnOrUnsummon();
                }
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q13264_thats_abominable_SpellScript::HandleKnockBack, EFFECT_1, SPELL_EFFECT_KNOCK_BACK);
                OnEffectHitTarget += SpellEffectFn(spell_q13264_thats_abominable_SpellScript::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q13264_thats_abominable_SpellScript();
        }
};

enum BurstAtTheSeams
{
    NPC_DRAKKARI_CHIEFTAINK                 = 29099,

    QUEST_BURST_AT_THE_SEAMS                = 12690,

    SPELL_BURST_AT_THE_SEAMS                = 52510, // Burst at the Seams
    SPELL_BURST_AT_THE_SEAMS_DMG            = 52508, // Damage spell
    SPELL_BURST_AT_THE_SEAMS_DMG_2          = 59580, // Abomination self damage spell
    SPELL_BURST_AT_THE_SEAMS_BONE           = 52516, // Burst at the Seams:Bone
    SPELL_BURST_AT_THE_SEAMS_MEAT           = 52520, // Explode Abomination:Meat
    SPELL_BURST_AT_THE_SEAMS_BMEAT          = 52523, // Explode Abomination:Bloody Meat
    SPELL_DRAKKARI_SKULLCRUSHER_CREDIT      = 52590, // Credit for Drakkari Skullcrusher
    SPELL_SUMMON_DRAKKARI_CHIEFTAIN         = 52616, // Summon Drakkari Chieftain
    SPELL_DRAKKARI_CHIEFTAINK_KILL_CREDIT   = 52620  // Drakkari Chieftain Kill Credit
};

class spell_q12690_burst_at_the_seams : public SpellScriptLoader
{
    public:
        spell_q12690_burst_at_the_seams() : SpellScriptLoader("spell_q12690_burst_at_the_seams") { }

        class spell_q12690_burst_at_the_seams_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12690_burst_at_the_seams_SpellScript);

            bool Validate(SpellInfo const* /*spellInfo*/) override
            {
                return ValidateSpellInfo(
                {
                    SPELL_BURST_AT_THE_SEAMS,
                    SPELL_BURST_AT_THE_SEAMS_DMG,
                    SPELL_BURST_AT_THE_SEAMS_DMG_2,
                    SPELL_BURST_AT_THE_SEAMS_BONE,
                    SPELL_BURST_AT_THE_SEAMS_MEAT,
                    SPELL_BURST_AT_THE_SEAMS_BMEAT
                });
            }

            bool Load() override
            {
                return GetCaster()->GetTypeId() == TYPEID_UNIT;
            }

            void HandleKnockBack(SpellEffIndex /*effIndex*/)
            {
                if (Unit* creature = GetHitCreature())
                {
                    if (Unit* charmer = GetCaster()->GetCharmerOrOwner())
                    {
                        if (Player* player = charmer->ToPlayer())
                        {
                            if (player->GetQuestStatus(QUEST_BURST_AT_THE_SEAMS) == QUEST_STATUS_INCOMPLETE)
                            {
                                creature->CastSpell(creature, SPELL_BURST_AT_THE_SEAMS_BONE, true);
                                creature->CastSpell(creature, SPELL_BURST_AT_THE_SEAMS_MEAT, true);
                                creature->CastSpell(creature, SPELL_BURST_AT_THE_SEAMS_BMEAT, true);
                                creature->CastSpell(creature, SPELL_BURST_AT_THE_SEAMS_DMG, true);
                                creature->CastSpell(creature, SPELL_BURST_AT_THE_SEAMS_DMG_2, true);

                                player->CastSpell(player, SPELL_DRAKKARI_SKULLCRUSHER_CREDIT, true);
                                uint16 count = player->GetReqKillOrCastCurrentCount(QUEST_BURST_AT_THE_SEAMS, NPC_DRAKKARI_CHIEFTAINK);
                                if ((count % 20) == 0)
                                    player->CastSpell(player, SPELL_SUMMON_DRAKKARI_CHIEFTAIN, true);
                            }
                        }
                    }
                }
            }

            void HandleScript(SpellEffIndex /*effIndex*/)
            {
                GetCaster()->ToCreature()->DespawnOrUnsummon(2 * IN_MILLISECONDS);
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q12690_burst_at_the_seams_SpellScript::HandleKnockBack, EFFECT_1, SPELL_EFFECT_KNOCK_BACK);
                OnEffectHitTarget += SpellEffectFn(spell_q12690_burst_at_the_seams_SpellScript::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12690_burst_at_the_seams_SpellScript();
        }
};

enum EscapeFromSilverbrook
{
    SPELL_SUMMON_WORGEN = 48681
};

// 48682 - Escape from Silverbrook - Periodic Dummy
class spell_q12308_escape_from_silverbrook : public SpellScriptLoader
{
    public:
        spell_q12308_escape_from_silverbrook() : SpellScriptLoader("spell_q12308_escape_from_silverbrook") { }

        class spell_q12308_escape_from_silverbrook_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12308_escape_from_silverbrook_SpellScript);

            bool Validate(SpellInfo const* /*spellInfo*/) override
            {
                return ValidateSpellInfo({ SPELL_SUMMON_WORGEN });
            }

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                GetCaster()->CastSpell(GetCaster(), SPELL_SUMMON_WORGEN, true);
            }

            void Register() override
            {
                OnEffectHit += SpellEffectFn(spell_q12308_escape_from_silverbrook_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12308_escape_from_silverbrook_SpellScript();
        }
};

// 48681 - Summon Silverbrook Worgen
class spell_q12308_escape_from_silverbrook_summon_worgen : public SpellScriptLoader
{
    public:
        spell_q12308_escape_from_silverbrook_summon_worgen() : SpellScriptLoader("spell_q12308_escape_from_silverbrook_summon_worgen") { }

        class spell_q12308_escape_from_silverbrook_summon_worgen_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12308_escape_from_silverbrook_summon_worgen_SpellScript);

            void ModDest(SpellDestination& dest)
            {
                float dist = GetEffectInfo(EFFECT_0).CalcRadius(GetCaster());
                float angle = frand(0.75f, 1.25f) * float(M_PI);

                Position pos = GetCaster()->GetNearPosition(dist, angle);
                dest.Relocate(pos);
            }

            void Register() override
            {
                OnDestinationTargetSelect += SpellDestinationTargetSelectFn(spell_q12308_escape_from_silverbrook_summon_worgen_SpellScript::ModDest, EFFECT_0, TARGET_DEST_CASTER_SUMMON);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12308_escape_from_silverbrook_summon_worgen_SpellScript();
        }
};

enum DeathComesFromOnHigh
{
    SPELL_FORGE_CREDIT                  = 51974,
    SPELL_TOWN_HALL_CREDIT              = 51977,
    SPELL_SCARLET_HOLD_CREDIT           = 51980,
    SPELL_CHAPEL_CREDIT                 = 51982,

    NPC_NEW_AVALON_FORGE                = 28525,
    NPC_NEW_AVALON_TOWN_HALL            = 28543,
    NPC_SCARLET_HOLD                    = 28542,
    NPC_CHAPEL_OF_THE_CRIMSON_FLAME     = 28544
};

// 51858 - Siphon of Acherus
class spell_q12641_death_comes_from_on_high : public SpellScriptLoader
{
    public:
        spell_q12641_death_comes_from_on_high() : SpellScriptLoader("spell_q12641_death_comes_from_on_high") { }

        class spell_q12641_death_comes_from_on_high_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12641_death_comes_from_on_high_SpellScript);

            bool Validate(SpellInfo const* /*spellInfo*/) override
            {
                return ValidateSpellInfo(
                {
                    SPELL_FORGE_CREDIT,
                    SPELL_TOWN_HALL_CREDIT,
                    SPELL_SCARLET_HOLD_CREDIT,
                    SPELL_CHAPEL_CREDIT
                });
            }

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                uint32 spellId = 0;

                switch (GetHitCreature()->GetEntry())
                {
                    case NPC_NEW_AVALON_FORGE:
                        spellId = SPELL_FORGE_CREDIT;
                        break;
                    case NPC_NEW_AVALON_TOWN_HALL:
                        spellId = SPELL_TOWN_HALL_CREDIT;
                        break;
                    case NPC_SCARLET_HOLD:
                        spellId = SPELL_SCARLET_HOLD_CREDIT;
                        break;
                    case NPC_CHAPEL_OF_THE_CRIMSON_FLAME:
                        spellId = SPELL_CHAPEL_CREDIT;
                        break;
                    default:
                        return;
                }

                GetCaster()->CastSpell(nullptr, spellId, true);
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q12641_death_comes_from_on_high_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12641_death_comes_from_on_high_SpellScript();
        }
};

// 52694 - Recall Eye of Acherus
enum Recall_Eye_of_Acherus
{
    THE_EYE_OF_ACHERUS = 51852
};

class spell_q12641_recall_eye_of_acherus : public SpellScriptLoader
{
    public:
        spell_q12641_recall_eye_of_acherus() : SpellScriptLoader("spell_q12641_recall_eye_of_acherus") { }

        class spell_q12641_recall_eye_of_acherus_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12641_recall_eye_of_acherus_SpellScript);

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                if (Player* player = GetCaster()->GetCharmerOrOwner()->ToPlayer())
                {
                    player->StopCastingCharm();
                    player->StopCastingBindSight();
                    player->RemoveAura(THE_EYE_OF_ACHERUS);
                }
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q12641_recall_eye_of_acherus_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12641_recall_eye_of_acherus_SpellScript();
        }
};

// 51769 - Emblazon Runeblade
class spell_q12619_emblazon_runeblade : public SpellScriptLoader
{
    public:
        spell_q12619_emblazon_runeblade() : SpellScriptLoader("spell_q12619_emblazon_runeblade") { }

        class spell_q12619_emblazon_runeblade_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_q12619_emblazon_runeblade_AuraScript);

            void HandleEffectPeriodic(AuraEffect const* aurEff)
            {
                PreventDefaultAction();
                if (Unit* caster = GetCaster())
                    caster->CastSpell(caster, aurEff->GetSpellEffectInfo().TriggerSpell, aurEff);
            }

            void Register() override
            {
                OnEffectPeriodic += AuraEffectPeriodicFn(spell_q12619_emblazon_runeblade_AuraScript::HandleEffectPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_q12619_emblazon_runeblade_AuraScript();
        }
};

// 51770 - Emblazon Runeblade
class spell_q12619_emblazon_runeblade_effect : public SpellScriptLoader
{
    public:
        spell_q12619_emblazon_runeblade_effect() : SpellScriptLoader("spell_q12619_emblazon_runeblade_effect") { }

        class spell_q12619_emblazon_runeblade_effect_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12619_emblazon_runeblade_effect_SpellScript);

            void HandleScript(SpellEffIndex /*effIndex*/)
            {
                GetCaster()->CastSpell(GetCaster(), uint32(GetEffectValue()), false);
            }

            void Register() override
            {
                OnEffectHit += SpellEffectFn(spell_q12619_emblazon_runeblade_effect_SpellScript::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12619_emblazon_runeblade_effect_SpellScript();
        }
};

enum Quest_The_Storm_King
{
    SPELL_RIDE_GYMER            = 43671,
    SPELL_GRABBED               = 55424
};

class spell_q12919_gymers_grab : public SpellScriptLoader
{
    public:
        spell_q12919_gymers_grab() : SpellScriptLoader("spell_q12919_gymers_grab") { }

        class spell_q12919_gymers_grab_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12919_gymers_grab_SpellScript);

            bool Validate(SpellInfo const* /*spell*/) override
            {
                return ValidateSpellInfo({ SPELL_RIDE_GYMER });
            }

            void HandleScript(SpellEffIndex /*effIndex*/)
            {
                if (!GetHitCreature())
                    return;
                CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
                args.AddSpellBP0(2);
                GetHitCreature()->CastSpell(GetCaster(), SPELL_RIDE_GYMER, args);
                GetHitCreature()->CastSpell(GetHitCreature(), SPELL_GRABBED, true);
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q12919_gymers_grab_SpellScript::HandleScript, EFFECT_0,  SPELL_EFFECT_SCRIPT_EFFECT);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12919_gymers_grab_SpellScript();
        }
};

enum Quest_The_Storm_King_Throw
{
    SPELL_VARGUL_EXPLOSION      = 55569
};

class spell_q12919_gymers_throw : public SpellScriptLoader
{
    public:
        spell_q12919_gymers_throw() : SpellScriptLoader("spell_q12919_gymers_throw") { }

        class spell_q12919_gymers_throw_SpellScript : public SpellScript
        {
           PrepareSpellScript(spell_q12919_gymers_throw_SpellScript);

            void HandleScript(SpellEffIndex /*effIndex*/)
            {
                Unit* caster = GetCaster();
                if (caster->IsVehicle())
                    if (Unit* passenger = caster->GetVehicleKit()->GetPassenger(1))
                    {
                         passenger->ExitVehicle();
                         caster->CastSpell(passenger, SPELL_VARGUL_EXPLOSION, true);
                    }
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q12919_gymers_throw_SpellScript::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12919_gymers_throw_SpellScript();
        }
};

enum Quest_The_Hunter_And_The_Prince
{
    SPELL_ILLIDAN_KILL_CREDIT      = 61748
};

class spell_q13400_illidan_kill_master : public SpellScriptLoader
{
    public:
        spell_q13400_illidan_kill_master() : SpellScriptLoader("spell_q13400_illidan_kill_master") { }

        class spell_q13400_illidan_kill_master_SpellScript : public SpellScript
        {
           PrepareSpellScript(spell_q13400_illidan_kill_master_SpellScript);

            bool Validate(SpellInfo const* /*spellInfo*/) override
            {
                return ValidateSpellInfo({ SPELL_ILLIDAN_KILL_CREDIT });
            }

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                Unit* caster = GetCaster();
                if (caster->IsVehicle())
                    if (Unit* passenger = caster->GetVehicleKit()->GetPassenger(0))
                         passenger->CastSpell(passenger, SPELL_ILLIDAN_KILL_CREDIT, true);
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q13400_illidan_kill_master_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q13400_illidan_kill_master_SpellScript();
        }
};

enum RelicOfTheEarthenRing
{
    SPELL_TOTEM_OF_THE_EARTHEN_RING = 66747
};

// 66744 - Make Player Destroy Totems
class spell_q14100_q14111_make_player_destroy_totems : public SpellScriptLoader
{
    public:
        spell_q14100_q14111_make_player_destroy_totems() : SpellScriptLoader("spell_q14100_q14111_make_player_destroy_totems") { }

        class spell_q14100_q14111_make_player_destroy_totems_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q14100_q14111_make_player_destroy_totems_SpellScript);

            bool Validate(SpellInfo const* /*spellInfo*/) override
            {
                return ValidateSpellInfo({ SPELL_TOTEM_OF_THE_EARTHEN_RING });
            }

            void HandleScriptEffect(SpellEffIndex /*effIndex*/)
            {
                if (Player* player = GetHitPlayer())
                    player->CastSpell(player, SPELL_TOTEM_OF_THE_EARTHEN_RING, TRIGGERED_FULL_MASK); // ignore reagent cost, consumed by quest
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q14100_q14111_make_player_destroy_totems_SpellScript::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q14100_q14111_make_player_destroy_totems_SpellScript();
        }
};

enum Fumping
{
    SPELL_SUMMON_SAND_GNOME  = 39240,
    SPELL_SUMMON_BONE_SLICER = 39241
};

// 39238 - Fumping
class spell_q10929_fumping : SpellScriptLoader
{
    public:
        spell_q10929_fumping() : SpellScriptLoader("spell_q10929_fumping") { }

        class spell_q10929_fumpingAuraScript : public AuraScript
        {
            PrepareAuraScript(spell_q10929_fumpingAuraScript);

            bool Validate(SpellInfo const* /*spell*/) override
            {
                return ValidateSpellInfo({ SPELL_SUMMON_SAND_GNOME, SPELL_SUMMON_BONE_SLICER });
            }

            void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
            {
                if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
                    return;

                if (Unit* caster = GetCaster())
                    caster->CastSpell(caster, urand(SPELL_SUMMON_SAND_GNOME, SPELL_SUMMON_BONE_SLICER), true);
            }

        void Register() override
        {
            OnEffectRemove += AuraEffectRemoveFn(spell_q10929_fumpingAuraScript::HandleEffectRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        }
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_q10929_fumpingAuraScript();
    }
};

enum FearNoEvil
{
    SPELL_RENEWED_LIFE = 93097,
    NPC_INJURED_STORMWIND_INFANTRY = 50047
};

// 93072 - Get Our Boys Back Dummy
class spell_q28813_get_our_boys_back_dummy : public SpellScriptLoader
{
public:
    spell_q28813_get_our_boys_back_dummy() : SpellScriptLoader("spell_q28813_get_our_boys_back_dummy") { }

    class spell_q28813_get_our_boys_back_dummy_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_q28813_get_our_boys_back_dummy_SpellScript);

        bool Validate(SpellInfo const* /*spellInfo*/) override
        {
            return ValidateSpellInfo({ SPELL_RENEWED_LIFE });
        }

        void HandleDummyEffect()
        {
            Unit* caster = GetCaster();

            if (Creature* injuredStormwindInfantry = caster->FindNearestCreature(NPC_INJURED_STORMWIND_INFANTRY, 5.0f, true))
            {
                injuredStormwindInfantry->SetCreatorGUID(caster->GetGUID());
                injuredStormwindInfantry->CastSpell(injuredStormwindInfantry, SPELL_RENEWED_LIFE, true);
            }
        }

        void Register() override
        {
            OnCast += SpellCastFn(spell_q28813_get_our_boys_back_dummy_SpellScript::HandleDummyEffect);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_q28813_get_our_boys_back_dummy_SpellScript();
    }
};

class spell_q28813_set_health_random : public SpellScriptLoader
{
public:
    spell_q28813_set_health_random() : SpellScriptLoader("spell_q28813_set_health_random") { }

    class spell_q28813_set_health_random_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_q28813_set_health_random_SpellScript);

        void HandleDummyEffect()
        {
            Unit* caster = GetCaster();
            caster->SetHealth(caster->CountPctFromMaxHealth(urand(3, 5)*10));
        }

        void Register() override
        {
            OnCast += SpellCastFn(spell_q28813_set_health_random_SpellScript::HandleDummyEffect);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_q28813_set_health_random_SpellScript();
    }
};

class spell_q12414_hand_over_reins : public SpellScriptLoader
{
    public:
        spell_q12414_hand_over_reins() : SpellScriptLoader("spell_q12414_hand_over_reins") { }

        class spell_q12414_hand_over_reins_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q12414_hand_over_reins_SpellScript);

            void HandleScript(SpellEffIndex /*effIndex*/)
            {
                Creature* caster = GetCaster()->ToCreature();
                GetHitUnit()->ExitVehicle();

                if (caster)
                    caster->DespawnOrUnsummon();
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q12414_hand_over_reins_SpellScript::HandleScript, EFFECT_1, SPELL_EFFECT_SCRIPT_EFFECT);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q12414_hand_over_reins_SpellScript();
        }
};

// 13790 13793 13811 13814 - Among the Champions
// 13665 13745 13750 13756 13761 13767 13772 13777 13782  13787 - The Grand Melee
class spell_q13665_q13790_bested_trigger : public SpellScriptLoader
{
    public:
        spell_q13665_q13790_bested_trigger() : SpellScriptLoader("spell_q13665_q13790_bested_trigger") { }

        class spell_q13665_q13790_bested_trigger_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q13665_q13790_bested_trigger_SpellScript);

            void HandleScript(SpellEffIndex /*effIndex*/)
            {
                Unit* target = GetHitUnit()->GetCharmerOrOwnerOrSelf();
                target->CastSpell(target, uint32(GetEffectValue()), true);
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q13665_q13790_bested_trigger_SpellScript::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q13665_q13790_bested_trigger_SpellScript();
        }
};

// herald of war and life without regret portal spells
class spell_59064_59439_portals : public SpellScriptLoader
{
public:
    spell_59064_59439_portals() : SpellScriptLoader("spell_59064_59439_portals") { }

    class spell_59064_59439_portals_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_59064_59439_portals_SpellScript);

        void HandleScript(SpellEffIndex /*effIndex*/)
        {
            GetHitUnit()->CastSpell(GetHitUnit(), uint32(GetEffectValue()));
        }

        void Register() override
        {
            OnEffectHitTarget += SpellEffectFn(spell_59064_59439_portals_SpellScript::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_59064_59439_portals_SpellScript();
    }
};

enum ApplyHeatAndStir
{
    SPELL_SPURTS_AND_SMOKE    = 38594,
    SPELL_FAILED_MIX_1        = 43376,
    SPELL_FAILED_MIX_2        = 43378,
    SPELL_FAILED_MIX_3        = 43970,
    SPELL_SUCCESSFUL_MIX      = 43377,

    CREATURE_GENERIC_TRIGGER_LAB = 24042,

    TALK_0 = 0,
    TALK_1 = 1
};

class spell_q11306_mixing_blood : public SpellScriptLoader
{
public:
    spell_q11306_mixing_blood() : SpellScriptLoader("spell_q11306_mixing_blood") { }

    class spell_q11306_mixing_blood_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_q11306_mixing_blood_SpellScript);

        void HandleEffect(SpellEffIndex /*effIndex*/)
        {
            if (Unit* caster = GetCaster())
                if (Creature* trigger = caster->FindNearestCreature(CREATURE_GENERIC_TRIGGER_LAB, 100.0f))
                    trigger->AI()->DoCastSelf(SPELL_SPURTS_AND_SMOKE);
        }

        void Register() override
        {
            OnEffectHit += SpellEffectFn(spell_q11306_mixing_blood_SpellScript::HandleEffect, EFFECT_1, SPELL_EFFECT_SEND_EVENT);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_q11306_mixing_blood_SpellScript();
    }
};

class spell_q11306_mixing_vrykul_blood : public SpellScriptLoader
{
    public:
        spell_q11306_mixing_vrykul_blood() : SpellScriptLoader("spell_q11306_mixing_vrykul_blood") { }

        class spell_q11306_mixing_vrykul_blood_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_q11306_mixing_vrykul_blood_SpellScript);

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                if (Unit* caster = GetCaster())
                {
                    uint8 chance = urand(0, 99);
                    uint32 spellId = 0;

                    // 90% chance of getting one out of three failure effects
                    if (chance < 30)
                        spellId = SPELL_FAILED_MIX_1;
                    else if (chance < 60)
                        spellId = SPELL_FAILED_MIX_2;
                    else if (chance < 90)
                        spellId = SPELL_FAILED_MIX_3;
                    else // 10% chance of successful cast
                        spellId = SPELL_SUCCESSFUL_MIX;

                    caster->CastSpell(caster, spellId, true);
                }
            }

            void Register() override
            {
                OnEffectHitTarget += SpellEffectFn(spell_q11306_mixing_vrykul_blood_SpellScript::HandleDummy, EFFECT_1, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_q11306_mixing_vrykul_blood_SpellScript();
        }
};

class spell_q11306_failed_mix_43376 : public SpellScriptLoader
{
public:
    spell_q11306_failed_mix_43376() : SpellScriptLoader("spell_q11306_failed_mix_43376") { }

    class spell_q11306_failed_mix_43376_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_q11306_failed_mix_43376_SpellScript);

        void HandleEffect(SpellEffIndex /*effIndex*/)
        {
            if (Unit* caster = GetCaster())
                if (Creature* trigger = caster->FindNearestCreature(CREATURE_GENERIC_TRIGGER_LAB, 100.0f))
                    trigger->AI()->Talk(TALK_0, caster);
        }

        void Register() override
        {
            OnEffectHit += SpellEffectFn(spell_q11306_failed_mix_43376_SpellScript::HandleEffect, EFFECT_1, SPELL_EFFECT_SEND_EVENT);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_q11306_failed_mix_43376_SpellScript();
    }
};

class spell_q11306_failed_mix_43378 : public SpellScriptLoader
{
public:
    spell_q11306_failed_mix_43378() : SpellScriptLoader("spell_q11306_failed_mix_43378") { }

    class spell_q11306_failed_mix_43378_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_q11306_failed_mix_43378_SpellScript);

        void HandleEffect(SpellEffIndex /*effIndex*/)
        {
            if (Unit* caster = GetCaster())
                if (Creature* trigger = caster->FindNearestCreature(CREATURE_GENERIC_TRIGGER_LAB, 100.0f))
                    trigger->AI()->Talk(TALK_1, caster);
        }

        void Register() override
        {
            OnEffectHit += SpellEffectFn(spell_q11306_failed_mix_43378_SpellScript::HandleEffect, EFFECT_2, SPELL_EFFECT_SEND_EVENT);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_q11306_failed_mix_43378_SpellScript();
    }
};

/*######
## Quest 14386 Leader of the Pack
######*/

enum CallAttackMastiffs
{
    NPC_ATTACK_MASTIFF = 36405
};

// 68682 Call Attack Mastiffs
class spell_q14386_call_attack_mastiffs : public SpellScript
{
    PrepareSpellScript(spell_q14386_call_attack_mastiffs);

    void HandleEffect(SpellEffIndex /*eff*/)
    {
        Unit* caster = GetCaster();
        caster->SummonCreature(NPC_ATTACK_MASTIFF, -1944.573f, 2657.402f, 0.994939f, 1.691919f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 1000);
        caster->SummonCreature(NPC_ATTACK_MASTIFF, -2005.65f, 2663.526f, -2.086935f, 0.5942355f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 1000);
        caster->SummonCreature(NPC_ATTACK_MASTIFF, -1996.506f, 2651.347f, -1.011707f, 0.8185352f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 1000);
        caster->SummonCreature(NPC_ATTACK_MASTIFF, -1972.352f, 2640.07f, 1.080288f, 1.217854f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 1000);
        caster->SummonCreature(NPC_ATTACK_MASTIFF, -1949.322f, 2642.76f, 1.242482f, 1.58074f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 1000);
        caster->SummonCreature(NPC_ATTACK_MASTIFF, -1993.94f, 2672.535f, -2.322549f, 0.5766209f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 1000);
        caster->SummonCreature(NPC_ATTACK_MASTIFF, -1982.724f, 2662.8f, -1.773986f, 0.8628055f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 1000);
        caster->SummonCreature(NPC_ATTACK_MASTIFF, -1973.301f, 2655.475f, -0.7831049f, 1.098415f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 1000);
        caster->SummonCreature(NPC_ATTACK_MASTIFF, -1956.509f, 2650.655f, 1.350571f, 1.441473f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 1000);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_q14386_call_attack_mastiffs::HandleEffect, EFFECT_1, SPELL_EFFECT_SEND_EVENT);
    }
};
void AddSC_quest_spell_scripts()
{
    new spell_q55_sacred_cleansing();
    new spell_q2203_thaumaturgy_channel();
    new spell_q5206_test_fetid_skull();
    new spell_q6124_6129_apply_salve();
    new spell_q10255_administer_antidote();
    new spell_q11396_11399_force_shield_arcane_purple_x3();
    new spell_q11396_11399_scourging_crystal_controller();
    new spell_q11396_11399_scourging_crystal_controller_dummy();
    new spell_q11515_fel_siphon_dummy();
    new spell_q11587_arcane_prisoner_rescue();
    new spell_q11730_ultrasonic_screwdriver();
    new spell_q12459_seeds_of_natures_wrath();
    new spell_q12634_despawn_fruit_tosser();
    new spell_q12683_take_sputum_sample();
    new spell_q12851_going_bearback();
    new spell_q12937_relief_for_the_fallen();
    new spell_q10041_q10040_who_are_they();
    new spell_symbol_of_life_dummy();
    new spell_q12659_ahunaes_knife();
    new spell_q9874_liquid_fire();
    new spell_q12805_lifeblood_dummy();
    RegisterSpellScript(spell_q13280_13283_plant_battle_standard);
    RegisterSpellScript(spell_q13280_13283_jump_jets);
    new spell_q14112_14145_chum_the_water();
    new spell_q9452_cast_net();
    new spell_q12279_cast_net();
    new spell_q14076_14092_pound_drum();
    new spell_q12987_read_pronouncement();
    new spell_q12277_wintergarde_mine_explosion();
    new spell_q12066_bunny_kill_credit();
    new spell_q12735_song_of_cleansing();
    new spell_q12372_cast_from_gossip_trigger();
    new spell_q12372_destabilize_azure_dragonshrine_dummy();
    new spell_q11010_q11102_q11023_aggro_check_aura();
    new spell_q11010_q11102_q11023_aggro_check();
    new spell_q11010_q11102_q11023_aggro_burst();
    new spell_q11010_q11102_q11023_choose_loc();
    new spell_q11010_q11102_q11023_q11008_check_fly_mount();
    new spell_q12372_azure_on_death_force_whisper();
    new spell_q12527_zuldrak_rat();
    new spell_q12661_q12669_q12676_q12677_q12713_summon_stefan();
    new spell_q12730_quenching_mist();
    new spell_q13291_q13292_q13239_q13261_frostbrood_skytalon_grab_decoy();
    new spell_q13291_q13292_q13239_q13261_armored_decoy_summon_skytalon();
    new spell_q12847_summon_soul_moveto_bunny();
    new spell_q13011_bear_flank_master();
    new spell_q13086_cannons_target();
    new spell_q13264_thats_abominable();
    new spell_q12690_burst_at_the_seams();
    new spell_q12308_escape_from_silverbrook_summon_worgen();
    new spell_q12308_escape_from_silverbrook();
    new spell_q12641_death_comes_from_on_high();
    new spell_q12641_recall_eye_of_acherus();
    new spell_q12619_emblazon_runeblade();
    new spell_q12619_emblazon_runeblade_effect();
    new spell_q12919_gymers_grab();
    new spell_q12919_gymers_throw();
    new spell_q13400_illidan_kill_master();
    new spell_q14100_q14111_make_player_destroy_totems();
    new spell_q10929_fumping();
    new spell_q28813_get_our_boys_back_dummy();
    new spell_q28813_set_health_random();
    new spell_q12414_hand_over_reins();
    new spell_q13665_q13790_bested_trigger();
    new spell_59064_59439_portals();
    new spell_q11306_mixing_blood();
    new spell_q11306_mixing_vrykul_blood();
    new spell_q11306_failed_mix_43376();
    new spell_q11306_failed_mix_43378();
    RegisterSpellScript(spell_q14386_call_attack_mastiffs);
}
