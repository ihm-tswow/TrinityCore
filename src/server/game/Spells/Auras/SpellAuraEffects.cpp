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

#include "SpellAuraEffects.h"
#include "Battlefield.h"
#include "BattlefieldMgr.h"
#include "Battleground.h"
#include "BattlegroundPackets.h"
#include "CellImpl.h"
#include "Common.h"
#include "DB2Stores.h"
#include "GridNotifiersImpl.h"
#include "Item.h"
#include "Log.h"
#include "MiscPackets.h"
#include "MotionMaster.h"
#include "MovementPackets.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "OutdoorPvPMgr.h"
#include "Pet.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "ReputationMgr.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "ThreatManager.h"
#include "Unit.h"
#include "Util.h"
#include "Vehicle.h"
#include "Weather.h"
#include "WeatherMgr.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include <G3D/g3dmath.h>
#include <numeric>

class Aura;
//
// EFFECT HANDLER NOTES
//
// in aura handler there should be check for modes:
// AURA_EFFECT_HANDLE_REAL set
// AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK set
// AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK set - aura is recalculated or is just applied/removed - need to redo all things related to m_amount
// AURA_EFFECT_HANDLE_CHANGE_AMOUNT_SEND_FOR_CLIENT_MASK - logical or of above conditions
// AURA_EFFECT_HANDLE_STAT - set when stats are reapplied
// such checks will speedup trinity change amount/send for client operations
// because for change amount operation packets will not be send
// aura effect handlers shouldn't contain any AuraEffect or Aura object modifications

NonDefaultConstructible<pAuraEffectHandler> AuraEffectHandler[TOTAL_AURAS]=
{
    &AuraEffect::HandleNULL,                                      //  0 SPELL_AURA_NONE
    &AuraEffect::HandleBindSight,                                 //  1 SPELL_AURA_BIND_SIGHT
    &AuraEffect::HandleModPossess,                                //  2 SPELL_AURA_MOD_POSSESS
    &AuraEffect::HandleNoImmediateEffect,                         //  3 SPELL_AURA_PERIODIC_DAMAGE implemented in AuraEffect::PeriodicTick
    &AuraEffect::HandleAuraDummy,                                 //  4 SPELL_AURA_DUMMY
    &AuraEffect::HandleModConfuse,                                //  5 SPELL_AURA_MOD_CONFUSE
    &AuraEffect::HandleModCharm,                                  //  6 SPELL_AURA_MOD_CHARM
    &AuraEffect::HandleModFear,                                   //  7 SPELL_AURA_MOD_FEAR
    &AuraEffect::HandleNoImmediateEffect,                         //  8 SPELL_AURA_PERIODIC_HEAL implemented in AuraEffect::PeriodicTick
    &AuraEffect::HandleModAttackSpeed,                            //  9 SPELL_AURA_MOD_ATTACKSPEED
    &AuraEffect::HandleModThreat,                                 // 10 SPELL_AURA_MOD_THREAT
    &AuraEffect::HandleModTaunt,                                  // 11 SPELL_AURA_MOD_TAUNT
    &AuraEffect::HandleAuraModStun,                               // 12 SPELL_AURA_MOD_STUN
    &AuraEffect::HandleModDamageDone,                             // 13 SPELL_AURA_MOD_DAMAGE_DONE
    &AuraEffect::HandleNoImmediateEffect,                         // 14 SPELL_AURA_MOD_DAMAGE_TAKEN implemented in Unit::MeleeDamageBonus and Unit::SpellDamageBonus
    &AuraEffect::HandleNoImmediateEffect,                         // 15 SPELL_AURA_DAMAGE_SHIELD    implemented in Unit::DoAttackDamage
    &AuraEffect::HandleModStealth,                                // 16 SPELL_AURA_MOD_STEALTH
    &AuraEffect::HandleModStealthDetect,                          // 17 SPELL_AURA_MOD_DETECT
    &AuraEffect::HandleModInvisibility,                           // 18 SPELL_AURA_MOD_INVISIBILITY
    &AuraEffect::HandleModInvisibilityDetect,                     // 19 SPELL_AURA_MOD_INVISIBILITY_DETECT
    &AuraEffect::HandleNoImmediateEffect,                         // 20 SPELL_AURA_OBS_MOD_HEALTH implemented in AuraEffect::PeriodicTick
    &AuraEffect::HandleNoImmediateEffect,                         // 21 SPELL_AURA_OBS_MOD_POWER implemented in AuraEffect::PeriodicTick
    &AuraEffect::HandleAuraModResistance,                         // 22 SPELL_AURA_MOD_RESISTANCE
    &AuraEffect::HandleNoImmediateEffect,                         // 23 SPELL_AURA_PERIODIC_TRIGGER_SPELL implemented in AuraEffect::PeriodicTick
    &AuraEffect::HandleNoImmediateEffect,                         // 24 SPELL_AURA_PERIODIC_ENERGIZE implemented in AuraEffect::PeriodicTick
    &AuraEffect::HandleAuraModPacify,                             // 25 SPELL_AURA_MOD_PACIFY
    &AuraEffect::HandleAuraModRoot,                               // 26 SPELL_AURA_MOD_ROOT
    &AuraEffect::HandleAuraModSilence,                            // 27 SPELL_AURA_MOD_SILENCE
    &AuraEffect::HandleNoImmediateEffect,                         // 28 SPELL_AURA_REFLECT_SPELLS        implement in Unit::SpellHitResult
    &AuraEffect::HandleAuraModStat,                               // 29 SPELL_AURA_MOD_STAT
    &AuraEffect::HandleAuraModSkill,                              // 30 SPELL_AURA_MOD_SKILL
    &AuraEffect::HandleAuraModIncreaseSpeed,                      // 31 SPELL_AURA_MOD_INCREASE_SPEED
    &AuraEffect::HandleAuraModIncreaseMountedSpeed,               // 32 SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED
    &AuraEffect::HandleAuraModDecreaseSpeed,                      // 33 SPELL_AURA_MOD_DECREASE_SPEED
    &AuraEffect::HandleAuraModIncreaseHealth,                     // 34 SPELL_AURA_MOD_INCREASE_HEALTH
    &AuraEffect::HandleAuraModIncreaseEnergy,                     // 35 SPELL_AURA_MOD_INCREASE_ENERGY
    &AuraEffect::HandleAuraModShapeshift,                         // 36 SPELL_AURA_MOD_SHAPESHIFT
    &AuraEffect::HandleAuraModEffectImmunity,                     // 37 SPELL_AURA_EFFECT_IMMUNITY
    &AuraEffect::HandleAuraModStateImmunity,                      // 38 SPELL_AURA_STATE_IMMUNITY
    &AuraEffect::HandleAuraModSchoolImmunity,                     // 39 SPELL_AURA_SCHOOL_IMMUNITY
    &AuraEffect::HandleAuraModDmgImmunity,                        // 40 SPELL_AURA_DAMAGE_IMMUNITY
    &AuraEffect::HandleAuraModDispelImmunity,                     // 41 SPELL_AURA_DISPEL_IMMUNITY
    &AuraEffect::HandleNoImmediateEffect,                         // 42 SPELL_AURA_PROC_TRIGGER_SPELL  implemented in AuraEffect::HandleProc
    &AuraEffect::HandleNoImmediateEffect,                         // 43 SPELL_AURA_PROC_TRIGGER_DAMAGE implemented in AuraEffect::HandleProc
    &AuraEffect::HandleAuraTrackCreatures,                        // 44 SPELL_AURA_TRACK_CREATURES
    &AuraEffect::HandleAuraTrackResources,                        // 45 SPELL_AURA_TRACK_RESOURCES
    &AuraEffect::HandleNULL,                                      // 46 SPELL_AURA_46 (used in test spells 54054 and 54058, and spell 48050) (3.0.8a)
    &AuraEffect::HandleAuraModParryPercent,                       // 47 SPELL_AURA_MOD_PARRY_PERCENT
    &AuraEffect::HandleNULL,                                      // 48 SPELL_AURA_48 spell Napalm (area damage spell with additional delayed damage effect)
    &AuraEffect::HandleAuraModDodgePercent,                       // 49 SPELL_AURA_MOD_DODGE_PERCENT
    &AuraEffect::HandleNoImmediateEffect,                         // 50 SPELL_AURA_MOD_CRITICAL_HEALING_AMOUNT implemented in Unit::SpellCriticalHealingBonus
    &AuraEffect::HandleAuraModBlockPercent,                       // 51 SPELL_AURA_MOD_BLOCK_PERCENT
    &AuraEffect::HandleAuraModWeaponCritPercent,                  // 52 SPELL_AURA_MOD_WEAPON_CRIT_PERCENT
    &AuraEffect::HandleNoImmediateEffect,                         // 53 SPELL_AURA_PERIODIC_LEECH implemented in AuraEffect::PeriodicTick
    &AuraEffect::HandleNoImmediateEffect,                         // 54 SPELL_AURA_MOD_HIT_CHANCE implemented in Unit::MeleeSpellMissChance
    &AuraEffect::HandleModSpellHitChance,                         // 55 SPELL_AURA_MOD_SPELL_HIT_CHANCE
    &AuraEffect::HandleAuraTransform,                             // 56 SPELL_AURA_TRANSFORM
    &AuraEffect::HandleModSpellCritChance,                        // 57 SPELL_AURA_MOD_SPELL_CRIT_CHANCE
    &AuraEffect::HandleAuraModIncreaseSwimSpeed,                  // 58 SPELL_AURA_MOD_INCREASE_SWIM_SPEED
    &AuraEffect::HandleNoImmediateEffect,                         // 59 SPELL_AURA_MOD_DAMAGE_DONE_CREATURE implemented in Unit::MeleeDamageBonus and Unit::SpellDamageBonus
    &AuraEffect::HandleAuraModPacifyAndSilence,                   // 60 SPELL_AURA_MOD_PACIFY_SILENCE
    &AuraEffect::HandleAuraModScale,                              // 61 SPELL_AURA_MOD_SCALE
    &AuraEffect::HandleNoImmediateEffect,                         // 62 SPELL_AURA_PERIODIC_HEALTH_FUNNEL implemented in AuraEffect::PeriodicTick
    &AuraEffect::HandleNoImmediateEffect,                         // 63 SPELL_AURA_MOD_ADDITIONAL_POWER_COST implemented in SpellInfo::CalcPowerCost
    &AuraEffect::HandleNoImmediateEffect,                         // 64 SPELL_AURA_PERIODIC_MANA_LEECH implemented in AuraEffect::PeriodicTick
    &AuraEffect::HandleModCastingSpeed,                           // 65 SPELL_AURA_MOD_CASTING_SPEED_NOT_STACK
    &AuraEffect::HandleFeignDeath,                                // 66 SPELL_AURA_FEIGN_DEATH
    &AuraEffect::HandleAuraModDisarm,                             // 67 SPELL_AURA_MOD_DISARM
    &AuraEffect::HandleAuraModStalked,                            // 68 SPELL_AURA_MOD_STALKED
    &AuraEffect::HandleNoImmediateEffect,                         // 69 SPELL_AURA_SCHOOL_ABSORB implemented in Unit::CalcAbsorbResist
    &AuraEffect::HandleNoImmediateEffect,                         // 70 SPELL_AURA_PERIODIC_WEAPON_PERCENT_DAMAGE implemented in AuraEffect::PeriodicTick
    &AuraEffect::HandleStoreTeleportReturnPoint,                  // 71 SPELL_AURA_STORE_TELEPORT_RETURN_POINT
    &AuraEffect::HandleNoImmediateEffect,                         // 72 SPELL_AURA_MOD_POWER_COST_SCHOOL_PCT
    &AuraEffect::HandleModPowerCost,                              // 73 SPELL_AURA_MOD_POWER_COST_SCHOOL
    &AuraEffect::HandleNoImmediateEffect,                         // 74 SPELL_AURA_REFLECT_SPELLS_SCHOOL  implemented in Unit::SpellHitResult
    &AuraEffect::HandleNoImmediateEffect,                         // 75 SPELL_AURA_MOD_LANGUAGE
    &AuraEffect::HandleNoImmediateEffect,                         // 76 SPELL_AURA_FAR_SIGHT
    &AuraEffect::HandleModMechanicImmunity,                       // 77 SPELL_AURA_MECHANIC_IMMUNITY
    &AuraEffect::HandleAuraMounted,                               // 78 SPELL_AURA_MOUNTED
    &AuraEffect::HandleModDamagePercentDone,                      // 79 SPELL_AURA_MOD_DAMAGE_PERCENT_DONE
    &AuraEffect::HandleModPercentStat,                            // 80 SPELL_AURA_MOD_PERCENT_STAT
    &AuraEffect::HandleNoImmediateEffect,                         // 81 SPELL_AURA_SPLIT_DAMAGE_PCT implemented in Unit::CalcAbsorbResist
    &AuraEffect::HandleWaterBreathing,                            // 82 SPELL_AURA_WATER_BREATHING
    &AuraEffect::HandleModBaseResistance,                         // 83 SPELL_AURA_MOD_BASE_RESISTANCE
    &AuraEffect::HandleNoImmediateEffect,                         // 84 SPELL_AURA_MOD_REGEN implemented in Player::RegenerateHealth
    &AuraEffect::HandleModPowerRegen,                             // 85 SPELL_AURA_MOD_POWER_REGEN implemented in Player::Regenerate
    &AuraEffect::HandleChannelDeathItem,                          // 86 SPELL_AURA_CHANNEL_DEATH_ITEM
    &AuraEffect::HandleNoImmediateEffect,                         // 87 SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN implemented in Unit::MeleeDamageBonus and Unit::SpellDamageBonus
    &AuraEffect::HandleNoImmediateEffect,                         // 88 SPELL_AURA_MOD_HEALTH_REGEN_PERCENT implemented in Player::RegenerateHealth
    &AuraEffect::HandleNoImmediateEffect,                         // 89 SPELL_AURA_PERIODIC_DAMAGE_PERCENT
    &AuraEffect::HandleUnused,                                    // 90 unused (4.3.4) old SPELL_AURA_MOD_RESIST_CHANCE
    &AuraEffect::HandleNoImmediateEffect,                         // 91 SPELL_AURA_MOD_DETECT_RANGE implemented in Creature::GetAttackDistance
    &AuraEffect::HandlePreventFleeing,                            // 92 SPELL_AURA_PREVENTS_FLEEING
    &AuraEffect::HandleModUnattackable,                           // 93 SPELL_AURA_MOD_UNATTACKABLE
    &AuraEffect::HandleNoImmediateEffect,                         // 94 SPELL_AURA_INTERRUPT_REGEN implemented in Player::Regenerate
    &AuraEffect::HandleAuraGhost,                                 // 95 SPELL_AURA_GHOST
    &AuraEffect::HandleNoImmediateEffect,                         // 96 SPELL_AURA_SPELL_MAGNET implemented in Unit::GetMagicHitRedirectTarget
    &AuraEffect::HandleNoImmediateEffect,                         // 97 SPELL_AURA_MANA_SHIELD implemented in Unit::CalcAbsorbResist
    &AuraEffect::HandleAuraModSkill,                              // 98 SPELL_AURA_MOD_SKILL_TALENT
    &AuraEffect::HandleAuraModAttackPower,                        // 99 SPELL_AURA_MOD_ATTACK_POWER
    &AuraEffect::HandleUnused,                                    //100 SPELL_AURA_AURAS_VISIBLE obsolete? all player can see all auras now, but still have spells including GM-spell
    &AuraEffect::HandleModResistancePercent,                      //101 SPELL_AURA_MOD_RESISTANCE_PCT
    &AuraEffect::HandleNoImmediateEffect,                         //102 SPELL_AURA_MOD_MELEE_ATTACK_POWER_VERSUS implemented in Unit::MeleeDamageBonus
    &AuraEffect::HandleAuraModTotalThreat,                        //103 SPELL_AURA_MOD_TOTAL_THREAT
    &AuraEffect::HandleAuraWaterWalk,                             //104 SPELL_AURA_WATER_WALK
    &AuraEffect::HandleAuraFeatherFall,                           //105 SPELL_AURA_FEATHER_FALL
    &AuraEffect::HandleAuraHover,                                 //106 SPELL_AURA_HOVER
    &AuraEffect::HandleNoImmediateEffect,                         //107 SPELL_AURA_ADD_FLAT_MODIFIER implemented in AuraEffect::CalculateSpellMod()
    &AuraEffect::HandleNoImmediateEffect,                         //108 SPELL_AURA_ADD_PCT_MODIFIER implemented in AuraEffect::CalculateSpellMod()
    &AuraEffect::HandleNoImmediateEffect,                         //109 SPELL_AURA_ADD_TARGET_TRIGGER
    &AuraEffect::HandleModPowerRegenPCT,                          //110 SPELL_AURA_MOD_POWER_REGEN_PERCENT implemented in Player::Regenerate, Creature::Regenerate
    &AuraEffect::HandleNoImmediateEffect,                         //111 SPELL_AURA_INTERCEPT_MELEE_RANGED_ATTACKS implemented in Unit::GetMeleeHitRedirectTarget
    &AuraEffect::HandleNoImmediateEffect,                         //112 SPELL_AURA_OVERRIDE_CLASS_SCRIPTS
    &AuraEffect::HandleNoImmediateEffect,                         //113 SPELL_AURA_MOD_RANGED_DAMAGE_TAKEN implemented in Unit::MeleeDamageBonus
    &AuraEffect::HandleNoImmediateEffect,                         //114 SPELL_AURA_MOD_RANGED_DAMAGE_TAKEN_PCT implemented in Unit::MeleeDamageBonus
    &AuraEffect::HandleNoImmediateEffect,                         //115 SPELL_AURA_MOD_HEALING                 implemented in Unit::SpellBaseHealingBonusForVictim
    &AuraEffect::HandleNoImmediateEffect,                         //116 SPELL_AURA_MOD_REGEN_DURING_COMBAT
    &AuraEffect::HandleNoImmediateEffect,                         //117 SPELL_AURA_MOD_MECHANIC_RESISTANCE     implemented in Unit::MagicSpellHitResult
    &AuraEffect::HandleNoImmediateEffect,                         //118 SPELL_AURA_MOD_HEALING_PCT             implemented in Unit::SpellHealingBonus
    &AuraEffect::HandleAuraPvpTalents,                            //119 SPELL_AURA_PVP_TALENTS
    &AuraEffect::HandleAuraUntrackable,                           //120 SPELL_AURA_UNTRACKABLE
    &AuraEffect::HandleAuraEmpathy,                               //121 SPELL_AURA_EMPATHY
    &AuraEffect::HandleModOffhandDamagePercent,                   //122 SPELL_AURA_MOD_OFFHAND_DAMAGE_PCT
    &AuraEffect::HandleModTargetResistance,                       //123 SPELL_AURA_MOD_TARGET_RESISTANCE
    &AuraEffect::HandleAuraModRangedAttackPower,                  //124 SPELL_AURA_MOD_RANGED_ATTACK_POWER
    &AuraEffect::HandleNoImmediateEffect,                         //125 SPELL_AURA_MOD_MELEE_DAMAGE_TAKEN implemented in Unit::MeleeDamageBonus
    &AuraEffect::HandleNoImmediateEffect,                         //126 SPELL_AURA_MOD_MELEE_DAMAGE_TAKEN_PCT implemented in Unit::MeleeDamageBonus
    &AuraEffect::HandleNoImmediateEffect,                         //127 SPELL_AURA_RANGED_ATTACK_POWER_ATTACKER_BONUS implemented in Unit::MeleeDamageBonus
    &AuraEffect::HandleNULL,                                      //128 SPELL_AURA_MOD_FIXATE
    &AuraEffect::HandleAuraModIncreaseSpeed,                      //129 SPELL_AURA_MOD_SPEED_ALWAYS
    &AuraEffect::HandleAuraModIncreaseMountedSpeed,               //130 SPELL_AURA_MOD_MOUNTED_SPEED_ALWAYS
    &AuraEffect::HandleNoImmediateEffect,                         //131 SPELL_AURA_MOD_RANGED_ATTACK_POWER_VERSUS implemented in Unit::MeleeDamageBonus
    &AuraEffect::HandleAuraModIncreaseEnergyPercent,              //132 SPELL_AURA_MOD_INCREASE_ENERGY_PERCENT
    &AuraEffect::HandleAuraModIncreaseHealthPercent,              //133 SPELL_AURA_MOD_INCREASE_HEALTH_PERCENT
    &AuraEffect::HandleAuraModRegenInterrupt,                     //134 SPELL_AURA_MOD_MANA_REGEN_INTERRUPT
    &AuraEffect::HandleModHealingDone,                            //135 SPELL_AURA_MOD_HEALING_DONE
    &AuraEffect::HandleModHealingDonePct,                         //136 SPELL_AURA_MOD_HEALING_DONE_PERCENT
    &AuraEffect::HandleModTotalPercentStat,                       //137 SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE
    &AuraEffect::HandleModMeleeSpeedPct,                          //138 SPELL_AURA_MOD_MELEE_HASTE
    &AuraEffect::HandleForceReaction,                             //139 SPELL_AURA_FORCE_REACTION
    &AuraEffect::HandleAuraModRangedHaste,                        //140 SPELL_AURA_MOD_RANGED_HASTE
    &AuraEffect::HandleUnused,                                    //141 SPELL_AURA_141
    &AuraEffect::HandleAuraModBaseResistancePCT,                  //142 SPELL_AURA_MOD_BASE_RESISTANCE_PCT
    &AuraEffect::HandleNULL,                                      //143 SPELL_AURA_MOD_RECOVERY_RATE_BY_SPELL_LABEL
    &AuraEffect::HandleNoImmediateEffect,                         //144 SPELL_AURA_SAFE_FALL                         implemented in WorldSession::HandleMovementOpcodes
    &AuraEffect::HandleAuraModIncreaseHealthPercent,              //145 SPELL_AURA_MOD_INCREASE_HEALTH_PERCENT2
    &AuraEffect::HandleNoImmediateEffect,                         //146 SPELL_AURA_ALLOW_TAME_PET_TYPE
    &AuraEffect::HandleModMechanicImmunityMask,                   //147 SPELL_AURA_MECHANIC_IMMUNITY_MASK
    &AuraEffect::HandleNULL,                                      //148 SPELL_AURA_MOD_CHARGE_RECOVERY_RATE
    &AuraEffect::HandleNoImmediateEffect,                         //149 SPELL_AURA_REDUCE_PUSHBACK
    &AuraEffect::HandleShieldBlockValuePercent,                   //150 SPELL_AURA_MOD_SHIELD_BLOCKVALUE_PCT
    &AuraEffect::HandleAuraTrackStealthed,                        //151 SPELL_AURA_TRACK_STEALTHED
    &AuraEffect::HandleNoImmediateEffect,                         //152 SPELL_AURA_MOD_DETECTED_RANGE implemented in Creature::GetAttackDistance
    &AuraEffect::HandleNoImmediateEffect,                         //153 SPELL_AURA_MOD_AUTOATTACK_RANGE implemented in Unit::IsWithinMeleeRange
    &AuraEffect::HandleModStealthLevel,                           //154 SPELL_AURA_MOD_STEALTH_LEVEL
    &AuraEffect::HandleNoImmediateEffect,                         //155 SPELL_AURA_MOD_WATER_BREATHING
    &AuraEffect::HandleNoImmediateEffect,                         //156 SPELL_AURA_MOD_REPUTATION_GAIN
    &AuraEffect::HandleNULL,                                      //157 SPELL_AURA_PET_DAMAGE_MULTI
    &AuraEffect::HandleAuraAllowTalentSwapping,                   //158 SPELL_AURA_ALLOW_TALENT_SWAPPING
    &AuraEffect::HandleNoImmediateEffect,                         //159 SPELL_AURA_NO_PVP_CREDIT      only for Honorless Target spell
    &AuraEffect::HandleUnused,                                    //160 Unused (4.3.4) old SPELL_AURA_MOD_AOE_AVOIDANCE
    &AuraEffect::HandleNoImmediateEffect,                         //161 SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT
    &AuraEffect::HandleNoImmediateEffect,                         //162 SPELL_AURA_POWER_BURN implemented in AuraEffect::PeriodicTick
    &AuraEffect::HandleNoImmediateEffect,                         //163 SPELL_AURA_MOD_CRIT_DAMAGE_BONUS
    &AuraEffect::HandleNULL,                                      //164 SPELL_AURA_FORCE_BREATH_BAR
    &AuraEffect::HandleNoImmediateEffect,                         //165 SPELL_AURA_MELEE_ATTACK_POWER_ATTACKER_BONUS implemented in Unit::MeleeDamageBonus
    &AuraEffect::HandleAuraModAttackPowerPercent,                 //166 SPELL_AURA_MOD_ATTACK_POWER_PCT
    &AuraEffect::HandleAuraModRangedAttackPowerPercent,           //167 SPELL_AURA_MOD_RANGED_ATTACK_POWER_PCT
    &AuraEffect::HandleNoImmediateEffect,                         //168 SPELL_AURA_MOD_DAMAGE_DONE_VERSUS            implemented in Unit::SpellDamageBonus, Unit::MeleeDamageBonus
    &AuraEffect::HandleSetFFAPvP,                                 //169 SPELL_AURA_SET_FFA_PVP implemented in Player::UpdatePvPState
    &AuraEffect::HandleDetectAmore,                               //170 SPELL_AURA_DETECT_AMORE       used to detect various spells that change visual of units for aura target
    &AuraEffect::HandleAuraModIncreaseSpeed,                      //171 SPELL_AURA_MOD_SPEED_NOT_STACK
    &AuraEffect::HandleAuraModIncreaseMountedSpeed,               //172 SPELL_AURA_MOD_MOUNTED_SPEED_NOT_STACK
    &AuraEffect::HandleUnused,                                    //173 unused (4.3.4) no spells, old SPELL_AURA_ALLOW_CHAMPION_SPELLS  only for Proclaim Champion spell
    &AuraEffect::HandleModSpellDamagePercentFromStat,             //174 SPELL_AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT  implemented in Unit::SpellBaseDamageBonus
    &AuraEffect::HandleModSpellHealingPercentFromStat,            //175 SPELL_AURA_MOD_SPELL_HEALING_OF_STAT_PERCENT implemented in Unit::SpellBaseHealingBonus
    &AuraEffect::HandleSpiritOfRedemption,                        //176 SPELL_AURA_SPIRIT_OF_REDEMPTION   only for Spirit of Redemption spell, die at aura end
    &AuraEffect::HandleCharmConvert,                              //177 SPELL_AURA_AOE_CHARM
    &AuraEffect::HandleAuraModMaxPowerPct,                        //178 SPELL_AURA_MOD_MAX_POWER_PCT
    &AuraEffect::HandleAuraModPowerDisplay,                       //179 SPELL_AURA_MOD_POWER_DISPLAY
    &AuraEffect::HandleNoImmediateEffect,                         //180 SPELL_AURA_MOD_FLAT_SPELL_DAMAGE_VERSUS   implemented in Unit::SpellDamageBonus
    &AuraEffect::HandleNULL,                                      //181 SPELL_AURA_MOD_SPELL_CURRENCY_REAGENTS_COUNT_PCT
    &AuraEffect::HandleSuppressItemPassiveEffectBySpellLabel,     //182 SPELL_AURA_SUPPRESS_ITEM_PASSIVE_EFFECT_BY_SPELL_LABEL
    &AuraEffect::HandleNoImmediateEffect,                         //183 SPELL_AURA_MOD_CRIT_CHANCE_VERSUS_TARGET_HEALTH implemented in Unit::GetUnitCriticalChance, Unit::GetUnitSpellCriticalChance
    &AuraEffect::HandleNoImmediateEffect,                         //184 SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE  implemented in Unit::RollMeleeOutcomeAgainst
    &AuraEffect::HandleNoImmediateEffect,                         //185 SPELL_AURA_MOD_ATTACKER_RANGED_HIT_CHANCE implemented in Unit::RollMeleeOutcomeAgainst
    &AuraEffect::HandleNoImmediateEffect,                         //186 SPELL_AURA_MOD_ATTACKER_SPELL_HIT_CHANCE  implemented in Unit::MagicSpellHitResult
    &AuraEffect::HandleNoImmediateEffect,                         //187 SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_CHANCE  implemented in Unit::GetUnitCriticalChance
    &AuraEffect::HandleNoImmediateEffect,                         //188 SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_CHANCE implemented in Unit::GetUnitCriticalChance
    &AuraEffect::HandleModRating,                                 //189 SPELL_AURA_MOD_RATING
    &AuraEffect::HandleNoImmediateEffect,                         //190 SPELL_AURA_MOD_FACTION_REPUTATION_GAIN     implemented in Player::CalculateReputationGain
    &AuraEffect::HandleAuraModUseNormalSpeed,                     //191 SPELL_AURA_USE_NORMAL_MOVEMENT_SPEED
    &AuraEffect::HandleModMeleeRangedSpeedPct,                    //192 SPELL_AURA_MOD_MELEE_RANGED_HASTE
    &AuraEffect::HandleModCombatSpeedPct,                         //193 SPELL_AURA_MELEE_SLOW (in fact combat (any type attack) speed pct)
    &AuraEffect::HandleNoImmediateEffect,                         //194 SPELL_AURA_MOD_TARGET_ABSORB_SCHOOL implemented in Unit::CalcAbsorbResist
    &AuraEffect::HandleLearnSpell,                                //195 SPELL_AURA_LEARN_SPELL
    &AuraEffect::HandleNULL,                                      //196 SPELL_AURA_MOD_COOLDOWN - flat mod of spell cooldowns
    &AuraEffect::HandleNoImmediateEffect,                         //197 SPELL_AURA_MOD_ATTACKER_SPELL_AND_WEAPON_CRIT_CHANCE implemented in Unit::SpellCriticalBonus Unit::GetUnitCriticalChance
    &AuraEffect::HandleNoImmediateEffect,                         //198 SPELL_AURA_MOD_COMBAT_RATING_FROM_COMBAT_RATING implemented in Player::UpdateRating
    &AuraEffect::HandleUnused,                                    //199 unused (4.3.4) old SPELL_AURA_MOD_INCREASES_SPELL_PCT_TO_HIT
    &AuraEffect::HandleNoImmediateEffect,                         //200 SPELL_AURA_MOD_XP_PCT implemented in Player::RewardPlayerAndGroupAtKill
    &AuraEffect::HandleAuraAllowFlight,                           //201 SPELL_AURA_FLY                             this aura enable flight mode...
    &AuraEffect::HandleNoImmediateEffect,                         //202 SPELL_AURA_CANNOT_BE_DODGED                implemented in Unit::RollPhysicalOutcomeAgainst
    &AuraEffect::HandleNULL,                                      //203 SPELL_AURA_PREVENT_INTERRUPT
    &AuraEffect::HandleNULL,                                      //204 SPELL_AURA_PREVENT_CORPSE_RELEASE
    &AuraEffect::HandleNULL,                                      //205 SPELL_AURA_MOD_CHARGE_COOLDOWN
    &AuraEffect::HandleAuraModIncreaseFlightSpeed,                //206 SPELL_AURA_MOD_INCREASE_VEHICLE_FLIGHT_SPEED
    &AuraEffect::HandleAuraModIncreaseFlightSpeed,                //207 SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED
    &AuraEffect::HandleAuraModIncreaseFlightSpeed,                //208 SPELL_AURA_MOD_INCREASE_FLIGHT_SPEED
    &AuraEffect::HandleAuraModIncreaseFlightSpeed,                //209 SPELL_AURA_MOD_MOUNTED_FLIGHT_SPEED_ALWAYS
    &AuraEffect::HandleAuraModIncreaseFlightSpeed,                //210 SPELL_AURA_MOD_VEHICLE_SPEED_ALWAYS
    &AuraEffect::HandleAuraModIncreaseFlightSpeed,                //211 SPELL_AURA_MOD_FLIGHT_SPEED_NOT_STACK
    &AuraEffect::HandleNoImmediateEffect,                         //212 SPELL_AURA_MOD_HONOR_GAIN_PCT implemented in Player::RewardHonor
    &AuraEffect::HandleNoImmediateEffect,                         //213 SPELL_AURA_MOD_RAGE_FROM_DAMAGE_DEALT implemented in Player::RewardRage
    &AuraEffect::HandleNULL,                                      //214 Tamed Pet Passive
    &AuraEffect::HandleArenaPreparation,                          //215 SPELL_AURA_ARENA_PREPARATION
    &AuraEffect::HandleModCastingSpeed,                           //216 SPELL_AURA_HASTE_SPELLS
    &AuraEffect::HandleModMeleeSpeedPct,                          //217 SPELL_AURA_MOD_MELEE_HASTE_2
    &AuraEffect::HandleNoImmediateEffect,                         //218 SPELL_AURA_ADD_PCT_MODIFIER_BY_SPELL_LABEL implemented in AuraEffect::CalculateSpellMod()
    &AuraEffect::HandleNoImmediateEffect,                         //219 SPELL_AURA_ADD_FLAT_MODIFIER_BY_SPELL_LABEL implemented in AuraEffect::CalculateSpellMod()
    &AuraEffect::HandleNULL,                                      //220 SPELL_AURA_MOD_ABILITY_SCHOOL_MASK
    &AuraEffect::HandleModDetaunt,                                //221 SPELL_AURA_MOD_DETAUNT
    &AuraEffect::HandleNoImmediateEffect,                         //222 SPELL_AURA_REMOVE_TRANSMOG_COST implemented in WorldSession::HandleTransmogrifyItems
    &AuraEffect::HandleNoImmediateEffect,                         //223 SPELL_AURA_REMOVE_BARBER_SHOP_COST implemented in Player::GetBarberShopCost
    &AuraEffect::HandleNULL,                                      //224 SPELL_AURA_LEARN_TALENT
    &AuraEffect::HandleNULL,                                      //225 SPELL_AURA_MOD_VISIBILITY_RANGE
    &AuraEffect::HandleNoImmediateEffect,                         //226 SPELL_AURA_PERIODIC_DUMMY implemented in AuraEffect::PeriodicTick
    &AuraEffect::HandleNoImmediateEffect,                         //227 SPELL_AURA_PERIODIC_TRIGGER_SPELL_WITH_VALUE implemented in AuraEffect::PeriodicTick
    &AuraEffect::HandleNoImmediateEffect,                         //228 SPELL_AURA_DETECT_STEALTH stealth detection
    &AuraEffect::HandleNoImmediateEffect,                         //229 SPELL_AURA_MOD_AOE_DAMAGE_AVOIDANCE
    &AuraEffect::HandleAuraModIncreaseHealth,                     //230 SPELL_AURA_MOD_INCREASE_HEALTH_2
    &AuraEffect::HandleNoImmediateEffect,                         //231 SPELL_AURA_PROC_TRIGGER_SPELL_WITH_VALUE
    &AuraEffect::HandleNoImmediateEffect,                         //232 SPELL_AURA_MECHANIC_DURATION_MOD           implement in Unit::CalculateSpellDuration
    &AuraEffect::HandleUnused,                                    //233 set model id to the one of the creature with id GetMiscValue() - clientside
    &AuraEffect::HandleNoImmediateEffect,                         //234 SPELL_AURA_MECHANIC_DURATION_MOD_NOT_STACK implement in Unit::CalculateSpellDuration
    &AuraEffect::HandleNULL,                                      //235 SPELL_AURA_MOD_HOVER_NO_HEIGHT_OFFSET handled clientside
    &AuraEffect::HandleAuraControlVehicle,                        //236 SPELL_AURA_CONTROL_VEHICLE
    &AuraEffect::HandleNULL,                                      //237 SPELL_AURA_237
    &AuraEffect::HandleNULL,                                      //238 SPELL_AURA_238
    &AuraEffect::HandleAuraModScale,                              //239 SPELL_AURA_MOD_SCALE_2 only in Noggenfogger Elixir (16595) before 2.3.0 aura 61
    &AuraEffect::HandleAuraModExpertise,                          //240 SPELL_AURA_MOD_EXPERTISE
    &AuraEffect::HandleForceMoveForward,                          //241 SPELL_AURA_FORCE_MOVE_FORWARD Forces the caster to move forward
    &AuraEffect::HandleNULL,                                      //242 SPELL_AURA_MOD_SPELL_DAMAGE_FROM_HEALING - 2 test spells: 44183 and 44182
    &AuraEffect::HandleAuraModFaction,                            //243 SPELL_AURA_MOD_FACTION
    &AuraEffect::HandleComprehendLanguage,                        //244 SPELL_AURA_COMPREHEND_LANGUAGE
    &AuraEffect::HandleNoImmediateEffect,                         //245 SPELL_AURA_MOD_AURA_DURATION_BY_DISPEL
    &AuraEffect::HandleNoImmediateEffect,                         //246 SPELL_AURA_MOD_AURA_DURATION_BY_DISPEL_NOT_STACK implemented in Spell::EffectApplyAura
    &AuraEffect::HandleAuraCloneCaster,                           //247 SPELL_AURA_CLONE_CASTER
    &AuraEffect::HandleNoImmediateEffect,                         //248 SPELL_AURA_MOD_COMBAT_RESULT_CHANCE         implemented in Unit::RollMeleeOutcomeAgainst
    &AuraEffect::HandleNULL,                                      //249 SPELL_AURA_MOD_DAMAGE_PERCENT_DONE_BY_TARGET_AURA_MECHANIC
    &AuraEffect::HandleAuraModIncreaseHealth,                     //250 SPELL_AURA_MOD_INCREASE_HEALTH_2
    &AuraEffect::HandleNoImmediateEffect,                         //251 SPELL_AURA_MOD_ENEMY_DODGE                  implemented in Unit::GetUnitDodgeChance
    &AuraEffect::HandleModCombatSpeedPct,                         //252 SPELL_AURA_252 Is there any difference between this and SPELL_AURA_MELEE_SLOW ? maybe not stacking mod?
    &AuraEffect::HandleNoImmediateEffect,                         //253 SPELL_AURA_MOD_BLOCK_CRIT_CHANCE  implemented in Unit::isBlockCritical
    &AuraEffect::HandleAuraModDisarm,                             //254 SPELL_AURA_MOD_DISARM_OFFHAND
    &AuraEffect::HandleNoImmediateEffect,                         //255 SPELL_AURA_MOD_MECHANIC_DAMAGE_TAKEN_PERCENT    implemented in Unit::SpellDamageBonus
    &AuraEffect::HandleNoReagentUseAura,                          //256 SPELL_AURA_NO_REAGENT_USE Use SpellClassMask for spell select
    &AuraEffect::HandleNULL,                                      //257 SPELL_AURA_MOD_TARGET_RESIST_BY_SPELL_CLASS Use SpellClassMask for spell select
    &AuraEffect::HandleNoImmediateEffect,                         //258 SPELL_AURA_OVERRIDE_SUMMONED_OBJECT implemented in Spell::EffectTransmitted
    &AuraEffect::HandleNoImmediateEffect,                         //259 SPELL_AURA_MOD_HOT_PCT implemented in Unit::SpellHealingBonusTaken
    &AuraEffect::HandleNoImmediateEffect,                         //260 SPELL_AURA_SCREEN_EFFECT (miscvalue = id in ScreenEffect.dbc) not required any code
    &AuraEffect::HandlePhase,                                     //261 SPELL_AURA_PHASE
    &AuraEffect::HandleNoImmediateEffect,                         //262 SPELL_AURA_ABILITY_IGNORE_AURASTATE implemented in Spell::CheckCast
    &AuraEffect::HandleNoImmediateEffect,                         //263 SPELL_AURA_DISABLE_CASTING_EXCEPT_ABILITIES implemented in Spell::CheckCast
    &AuraEffect::HandleAuraDisableAttackingExceptAbilities,       //264 SPELL_AURA_DISABLE_ATTACKING_EXCEPT_ABILITIES implemented in Spell::CheckCast, Unit::Attack
    &AuraEffect::HandleUnused,                                    //265 unused (4.3.4)
    &AuraEffect::HandleNULL,                                      //266 SPELL_AURA_SET_VIGNETTE
    &AuraEffect::HandleNoImmediateEffect,                         //267 SPELL_AURA_MOD_IMMUNE_AURA_APPLY_SCHOOL         implemented in Unit::IsImmunedToSpellEffect
    &AuraEffect::HandleModArmorPctFromStat,                       //268 SPELL_AURA_MOD_ARMOR_PCT_FROM_STAT              also implemented in Player::UpdateArmor()
    &AuraEffect::HandleNoImmediateEffect,                         //269 SPELL_AURA_MOD_IGNORE_TARGET_RESIST implemented in Unit::CalcAbsorbResist and CalcArmorReducedDamage
    &AuraEffect::HandleNoImmediateEffect,                         //270 SPELL_AURA_MOD_SCHOOL_MASK_DAMAGE_FROM_CASTER    implemented in Unit::SpellDamageBonusTaken and Unit::MeleeDamageBonusTaken
    &AuraEffect::HandleNoImmediateEffect,                         //271 SPELL_AURA_MOD_SPELL_DAMAGE_FROM_CASTER    implemented in Unit::SpellDamageBonusTaken and Unit::MeleeDamageBonusTaken
    &AuraEffect::HandleNULL,                                      //272 SPELL_AURA_MOD_BLOCK_VALUE_PCT
    &AuraEffect::HandleUnused,                                    //273 clientside
    &AuraEffect::HandleNULL,                                      //274 SPELL_AURA_MOD_BLOCK_VALUE_FLAT
    &AuraEffect::HandleNoImmediateEffect,                         //275 SPELL_AURA_MOD_IGNORE_SHAPESHIFT Use SpellClassMask for spell select
    &AuraEffect::HandleNULL,                                      //276 mod damage % mechanic?
    &AuraEffect::HandleUnused,                                    //277 unused (4.3.4) old SPELL_AURA_MOD_MAX_AFFECTED_TARGETS
    &AuraEffect::HandleAuraModDisarm,                             //278 SPELL_AURA_MOD_DISARM_RANGED disarm ranged weapon
    &AuraEffect::HandleNoImmediateEffect,                         //279 SPELL_AURA_INITIALIZE_IMAGES
    &AuraEffect::HandleUnused,                                    //280 unused (4.3.4) old SPELL_AURA_MOD_ARMOR_PENETRATION_PCT
    &AuraEffect::HandleNoImmediateEffect,                         //281 SPELL_AURA_PROVIDE_SPELL_FOCUS implemented in Spell::CheckCast
    &AuraEffect::HandleAuraIncreaseBaseHealthPercent,             //282 SPELL_AURA_MOD_BASE_HEALTH_PCT
    &AuraEffect::HandleNoImmediateEffect,                         //283 SPELL_AURA_MOD_HEALING_RECEIVED       implemented in Unit::SpellHealingBonus
    &AuraEffect::HandleAuraLinked,                                //284 SPELL_AURA_LINKED
    &AuraEffect::HandleAuraLinked,                                //285 SPELL_AURA_LINKED_2
    &AuraEffect::HandleNULL,                                      //286 SPELL_AURA_MOD_RECOVERY_RATE
    &AuraEffect::HandleNoImmediateEffect,                         //287 SPELL_AURA_DEFLECT_SPELLS             implemented in Unit::MagicSpellHitResult and Unit::MeleeSpellHitResult
    &AuraEffect::HandleNoImmediateEffect,                         //288 SPELL_AURA_IGNORE_HIT_DIRECTION  implemented in Unit::MagicSpellHitResult and Unit::MeleeSpellHitResult Unit::RollMeleeOutcomeAgainst
    &AuraEffect::HandleNoImmediateEffect,                         //289 SPELL_AURA_PREVENT_DURABILITY_LOSS implemented in Player::DurabilityPointsLoss
    &AuraEffect::HandleAuraModCritPct,                            //290 SPELL_AURA_MOD_CRIT_PCT
    &AuraEffect::HandleNoImmediateEffect,                         //291 SPELL_AURA_MOD_XP_QUEST_PCT  implemented in Player::RewardQuest
    &AuraEffect::HandleAuraOpenStable,                            //292 SPELL_AURA_OPEN_STABLE
    &AuraEffect::HandleAuraOverrideSpells,                        //293 SPELL_AURA_OVERRIDE_SPELLS auras which probably add set of abilities to their target based on it's miscvalue
    &AuraEffect::HandleNoImmediateEffect,                         //294 SPELL_AURA_PREVENT_REGENERATE_POWER implemented in Player::Regenerate(Powers power)
    &AuraEffect::HandleNoImmediateEffect,                         //295 SPELL_AURA_MOD_PERIODIC_DAMAGE_TAKEN implemented in Unit::MeleeDamageBonusTaken, Unit::SpellDamageBonusTaken
    &AuraEffect::HandleAuraSetVehicle,                            //296 SPELL_AURA_SET_VEHICLE_ID sets vehicle on target
    &AuraEffect::HandleNULL,                                      //297 SPELL_AURA_MOD_ROOT_DISABLE_GRAVITY
    &AuraEffect::HandleNULL,                                      //298 SPELL_AURA_MOD_STUN_DISABLE_GRAVITY
    &AuraEffect::HandleUnused,                                    //299 unused (4.3.4)
    &AuraEffect::HandleNoImmediateEffect,                         //300 SPELL_AURA_SHARE_DAMAGE_PCT implemented in Unit::DealDamage
    &AuraEffect::HandleNoImmediateEffect,                         //301 SPELL_AURA_SCHOOL_HEAL_ABSORB implemented in Unit::CalcHealAbsorb
    &AuraEffect::HandleUnused,                                    //302 unused (4.3.4)
    &AuraEffect::HandleNoImmediateEffect,                         //303 SPELL_AURA_MOD_DAMAGE_DONE_VERSUS_AURASTATE implemented in Unit::SpellDamageBonus, Unit::MeleeDamageBonus
    &AuraEffect::HandleAuraModFakeInebriation,                    //304 SPELL_AURA_MOD_DRUNK
    &AuraEffect::HandleAuraModIncreaseSpeed,                      //305 SPELL_AURA_MOD_MINIMUM_SPEED
    &AuraEffect::HandleNoImmediateEffect,                         //306 SPELL_AURA_MOD_CRIT_CHANCE_FOR_CASTER implemented in Unit::GetUnitCriticalChance and Unit::GetUnitSpellCriticalChance
    &AuraEffect::HandleNoImmediateEffect,                         //307 SPELL_AURA_CAST_WHILE_WALKING_BY_SPELL_LABEL implemented in Unit::CanCastSpellWhileMoving
    &AuraEffect::HandleNoImmediateEffect,                         //308 SPELL_AURA_MOD_CRIT_CHANCE_FOR_CASTER_WITH_ABILITIES implemented in Unit::GetUnitSpellCriticalChance
    &AuraEffect::HandleNULL,                                      //309 SPELL_AURA_MOD_RESILIENCE
    &AuraEffect::HandleNoImmediateEffect,                         //310 SPELL_AURA_MOD_CREATURE_AOE_DAMAGE_AVOIDANCE implemented in Spell::CalculateDamageDone
    &AuraEffect::HandleNULL,                                      //311 SPELL_AURA_IGNORE_COMBAT
    &AuraEffect::HandleNULL,                                      //312 SPELL_AURA_ANIM_REPLACEMENT_SET implemented clientside
    &AuraEffect::HandleNULL,                                      //313 SPELL_AURA_MOUNT_ANIM_REPLACEMENT_SET implemented clientside
    &AuraEffect::HandlePreventResurrection,                       //314 SPELL_AURA_PREVENT_RESURRECTION todo
    &AuraEffect::HandleNoImmediateEffect,                         //315 SPELL_AURA_UNDERWATER_WALKING todo
    &AuraEffect::HandleNoImmediateEffect,                         //316 SPELL_AURA_SCHOOL_ABSORB_OVERKILL implemented in Unit::DealDamage()
    &AuraEffect::HandleNULL,                                      //317 SPELL_AURA_MOD_SPELL_POWER_PCT
    &AuraEffect::HandleMastery,                                   //318 SPELL_AURA_MASTERY
    &AuraEffect::HandleModMeleeSpeedPct,                          //319 SPELL_AURA_MOD_MELEE_HASTE_3
    &AuraEffect::HandleAuraModRangedHaste,                        //320 SPELL_AURA_MOD_RANGED_HASTE_2
    &AuraEffect::HandleAuraModNoActions,                          //321 SPELL_AURA_MOD_NO_ACTIONS
    &AuraEffect::HandleNoImmediateEffect,                         //322 SPELL_AURA_INTERFERE_TARGETTING implemented in Spell::CheckCast
    &AuraEffect::HandleUnused,                                    //323 unused (4.3.4)
    &AuraEffect::HandleNULL,                                      //324 SPELL_AURA_OVERRIDE_UNLOCKED_AZERITE_ESSENCE_RANK
    &AuraEffect::HandleUnused,                                    //325 SPELL_AURA_LEARN_PVP_TALENT
    &AuraEffect::HandlePhaseGroup,                                //326 SPELL_AURA_PHASE_GROUP
    &AuraEffect::HandlePhaseAlwaysVisible,                        //327 SPELL_AURA_PHASE_ALWAYS_VISIBLE
    &AuraEffect::HandleTriggerSpellOnPowerPercent,                //328 SPELL_AURA_TRIGGER_SPELL_ON_POWER_PCT
    &AuraEffect::HandleNULL,                                      //329 SPELL_AURA_MOD_POWER_GAIN_PCT
    &AuraEffect::HandleNoImmediateEffect,                         //330 SPELL_AURA_CAST_WHILE_WALKING implemented in Unit::CanCastSpellWhileMoving
    &AuraEffect::HandleAuraForceWeather,                          //331 SPELL_AURA_FORCE_WEATHER
    &AuraEffect::HandleNoImmediateEffect,                         //332 SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS implemented in Unit::GetCastSpellInfo
    &AuraEffect::HandleNoImmediateEffect,                         //333 SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS_TRIGGERED implemented in Unit::GetCastSpellInfo
    &AuraEffect::HandleNoImmediateEffect,                         //334 SPELL_AURA_MOD_AUTOATTACK_CRIT_CHANCE implemented in Unit::RollMeleeOutcomeAgainst
    &AuraEffect::HandleNULL,                                      //335 SPELL_AURA_335
    &AuraEffect::HandleNULL,                                      //336 SPELL_AURA_MOUNT_RESTRICTIONS implemented in Unit::GetMountCapability
    &AuraEffect::HandleNoImmediateEffect,                         //337 SPELL_AURA_MOD_VENDOR_ITEMS_PRICES
    &AuraEffect::HandleNoImmediateEffect,                         //338 SPELL_AURA_MOD_DURABILITY_LOSS
    &AuraEffect::HandleNoImmediateEffect,                         //339 SPELL_AURA_MOD_CRIT_CHANCE_FOR_CASTER implemented in Unit::GetUnitCriticalChance and Unit::GetUnitSpellCriticalChance
    &AuraEffect::HandleNULL,                                      //340 SPELL_AURA_MOD_RESURRECTED_HEALTH_BY_GUILD_MEMBER
    &AuraEffect::HandleModSpellCategoryCooldown,                  //341 SPELL_AURA_MOD_SPELL_CATEGORY_COOLDOWN
    &AuraEffect::HandleModMeleeRangedSpeedPct,                    //342 SPELL_AURA_MOD_MELEE_RANGED_HASTE_2
    &AuraEffect::HandleNULL,                                      //343 SPELL_AURA_MOD_MELEE_DAMAGE_FROM_CASTER
    &AuraEffect::HandleNoImmediateEffect,                         //344 SPELL_AURA_MOD_AUTOATTACK_DAMAGE - Implemented in Unit::MeleeDamageBonusDone
    &AuraEffect::HandleNoImmediateEffect,                         //345 SPELL_AURA_BYPASS_ARMOR_FOR_CASTER
    &AuraEffect::HandleEnableAltPower,                            //346 SPELL_AURA_ENABLE_ALT_POWER
    &AuraEffect::HandleNoImmediateEffect,                         //347 SPELL_AURA_MOD_SPELL_COOLDOWN_BY_HASTE  implemented in SpellHistory::StartCooldown
    &AuraEffect::HandleNoImmediateEffect,                         //348 SPELL_AURA_MOD_MONEY_GAIN implemented in WorldSession::HandleLootMoneyOpcode
    &AuraEffect::HandleNoImmediateEffect,                         //349 SPELL_AURA_MOD_CURRENCY_GAIN implemented in Player::ModifyCurrency
    &AuraEffect::HandleNULL,                                      //350 SPELL_AURA_350
    &AuraEffect::HandleNULL,                                      //351 SPELL_AURA_MOD_CURRENCY_CATEGORY_GAIN_PCT
    &AuraEffect::HandleNULL,                                      //352 SPELL_AURA_352
    &AuraEffect::HandleNULL,                                      //353 SPELL_AURA_MOD_CAMOUFLAGE
    &AuraEffect::HandleNoImmediateEffect,                         //354 SPELL_AURA_MOD_HEALING_DONE_PCT_VERSUS_TARGET_HEALTH implemented in Unit::SpellHealingPctDone
    &AuraEffect::HandleNULL,                                      //355 SPELL_AURA_MOD_CASTING_SPEED
    &AuraEffect::HandleNoImmediateEffect,                         //356 SPELL_AURA_PROVIDE_TOTEM_CATEGORY implemented in Player::HasItemTotemCategory
    &AuraEffect::HandleNULL,                                      //357 SPELL_AURA_ENABLE_BOSS1_UNIT_FRAME
    &AuraEffect::HandleNULL,                                      //358 SPELL_AURA_358
    &AuraEffect::HandleNoImmediateEffect,                         //359 SPELL_AURA_MOD_HEALING_DONE_VERSUS_AURASTATE implemented in Unit::SpellHealingPctDone
    &AuraEffect::HandleNULL,                                      //360 SPELL_AURA_PROC_TRIGGER_SPELL_COPY
    &AuraEffect::HandleNoImmediateEffect,                         //361 SPELL_AURA_OVERRIDE_AUTOATTACK_WITH_MELEE_SPELL implemented in Unit::AttackerStateUpdate
    &AuraEffect::HandleUnused,                                    //362 unused (4.3.4)
    &AuraEffect::HandleNULL,                                      //363 SPELL_AURA_MOD_NEXT_SPELL
    &AuraEffect::HandleUnused,                                    //364 unused (4.3.4)
    &AuraEffect::HandleNULL,                                      //365 SPELL_AURA_MAX_FAR_CLIP_PLANE
    &AuraEffect::HandleOverrideSpellPowerByAttackPower,           //366 SPELL_AURA_OVERRIDE_SPELL_POWER_BY_AP_PCT
    &AuraEffect::HandleNULL,                                      //367 SPELL_AURA_OVERRIDE_AUTOATTACK_WITH_RANGED_SPELL
    &AuraEffect::HandleUnused,                                    //368 unused (4.3.4)
    &AuraEffect::HandleNULL,                                      //369 SPELL_AURA_ENABLE_POWER_BAR_TIMER
    &AuraEffect::HandleNULL,                                      //370 SPELL_AURA_SPELL_OVERRIDE_NAME_GROUP
    &AuraEffect::HandleNULL,                                      //371
    &AuraEffect::HandleNULL,                                      //372
    &AuraEffect::HandleNULL,                                      //373 SPELL_AURA_MOD_SPEED_NO_CONTROL
    &AuraEffect::HandleNULL,                                      //374 SPELL_AURA_MODIFY_FALL_DAMAGE_PCT
    &AuraEffect::HandleNULL,                                      //375 SPELL_AURA_HIDE_MODEL_AND_EQUIPEMENT_SLOTS implemented clientside
    &AuraEffect::HandleNULL,                                      //376 SPELL_AURA_MOD_CURRENCY_GAIN_FROM_SOURCE
    &AuraEffect::HandleNoImmediateEffect,                         //377 SPELL_AURA_CAST_WHILE_WALKING_ALL implemented in Unit::CanCastSpellWhileMoving
    &AuraEffect::HandleModPossessPet,                             //378 SPELL_AURA_MOD_POSSESS_PET
    &AuraEffect::HandleModManaRegenPct,                           //379 SPELL_AURA_MOD_MANA_REGEN_PCT implemented in Player::UpdateManaRegen
    &AuraEffect::HandleNULL,                                      //380
    &AuraEffect::HandleNULL,                                      //381 SPELL_AURA_MOD_DAMAGE_TAKEN_FROM_CASTER_PET
    &AuraEffect::HandleNULL,                                      //382 SPELL_AURA_MOD_PET_STAT_PCT
    &AuraEffect::HandleNULL,                                      //383 SPELL_AURA_IGNORE_SPELL_COOLDOWN
    &AuraEffect::HandleNULL,                                      //384
    &AuraEffect::HandleNULL,                                      //385
    &AuraEffect::HandleNULL,                                      //386
    &AuraEffect::HandleNULL,                                      //387
    &AuraEffect::HandleNULL,                                      //388 SPELL_AURA_MOD_TAXI_FLIGHT_SPEED
    &AuraEffect::HandleNULL,                                      //389
    &AuraEffect::HandleNULL,                                      //390
    &AuraEffect::HandleNULL,                                      //391
    &AuraEffect::HandleNULL,                                      //392
    &AuraEffect::HandleNULL,                                      //393 SPELL_AURA_BLOCK_SPELLS_IN_FRONT
    &AuraEffect::HandleShowConfirmationPrompt,                    //394 SPELL_AURA_SHOW_CONFIRMATION_PROMPT
    &AuraEffect::HandleCreateAreaTrigger,                         //395 SPELL_AURA_AREA_TRIGGER
    &AuraEffect::HandleTriggerSpellOnPowerAmount,                 //396 SPELL_AURA_TRIGGER_SPELL_ON_POWER_AMOUNT
    &AuraEffect::HandleBattlegroundPlayerPosition,                //397 SPELL_AURA_BATTLEGROUND_PLAYER_POSITION_FACTIONAL
    &AuraEffect::HandleBattlegroundPlayerPosition,                //398 SPELL_AURA_BATTLEGROUND_PLAYER_POSITION
    &AuraEffect::HandleNULL,                                      //399 SPELL_AURA_MOD_TIME_RATE
    &AuraEffect::HandleAuraModSkill,                              //400 SPELL_AURA_MOD_SKILL_2
    &AuraEffect::HandleNULL,                                      //401
    &AuraEffect::HandleAuraModOverridePowerDisplay,               //402 SPELL_AURA_MOD_OVERRIDE_POWER_DISPLAY
    &AuraEffect::HandleNoImmediateEffect,                         //403 SPELL_AURA_OVERRIDE_SPELL_VISUAL implemented in Unit::GetCastSpellXSpellVisualId
    &AuraEffect::HandleOverrideAttackPowerBySpellPower,           //404 SPELL_AURA_OVERRIDE_ATTACK_POWER_BY_SP_PCT
    &AuraEffect::HandleModRatingPct,                              //405 SPELL_AURA_MOD_RATING_PCT
    &AuraEffect::HandleNULL,                                      //406 SPELL_AURA_KEYBOUND_OVERRIDE
    &AuraEffect::HandleNULL,                                      //407 SPELL_AURA_MOD_FEAR_2
    &AuraEffect::HandleUnused,                                    //408 SPELL_AURA_SET_ACTION_BUTTON_SPELL_COUNT clientside
    &AuraEffect::HandleAuraCanTurnWhileFalling,                   //409 SPELL_AURA_CAN_TURN_WHILE_FALLING
    &AuraEffect::HandleNULL,                                      //410
    &AuraEffect::HandleNoImmediateEffect,                         //411 SPELL_AURA_MOD_MAX_CHARGES implemented in SpellHistory::GetMaxCharges
    &AuraEffect::HandleNULL,                                      //412
    &AuraEffect::HandleNULL,                                      //413 SPELL_AURA_MOD_RANGED_ATTACK_DEFLECT_CHANCE
    &AuraEffect::HandleNULL,                                      //414 SPELL_AURA_MOD_RANGED_ATTACK_BLOCK_CHANCE_IN_FRONT
    &AuraEffect::HandleNULL,                                      //415
    &AuraEffect::HandleNoImmediateEffect,                         //416 SPELL_AURA_MOD_COOLDOWN_BY_HASTE_REGEN implemented in SpellHistory::StartCooldown
    &AuraEffect::HandleNoImmediateEffect,                         //417 SPELL_AURA_MOD_GLOBAL_COOLDOWN_BY_HASTE_REGEN implemented in Spell::TriggerGlobalCooldown
    &AuraEffect::HandleAuraModMaxPower,                           //418 SPELL_AURA_MOD_MAX_POWER
    &AuraEffect::HandleAuraModIncreaseBaseManaPercent,            //419 SPELL_AURA_MOD_BASE_MANA_PCT
    &AuraEffect::HandleNULL,                                      //420 SPELL_AURA_MOD_BATTLE_PET_XP_PCT
    &AuraEffect::HandleNULL,                                      //421 SPELL_AURA_MOD_ABSORB_EFFECTS_DONE_PCT
    &AuraEffect::HandleNULL,                                      //422 SPELL_AURA_MOD_ABSORB_EFFECTS_TAKEN_PCT
    &AuraEffect::HandleModManaCostPct,                            //423 SPELL_AURA_MOD_MANA_COST_PCT
    &AuraEffect::HandleNULL,                                      //424 SPELL_AURA_CASTER_IGNORE_LOS
    &AuraEffect::HandleNULL,                                      //425
    &AuraEffect::HandleNULL,                                      //426
    &AuraEffect::HandleNULL,                                      //427 SPELL_AURA_SCALE_PLAYER_LEVEL
    &AuraEffect::HandleLinkedSummon,                              //428 SPELL_AURA_LINKED_SUMMON
    &AuraEffect::HandleNULL,                                      //429 SPELL_AURA_MOD_SUMMON_DAMAGE
    &AuraEffect::HandlePlayScene,                                 //430 SPELL_AURA_PLAY_SCENE
    &AuraEffect::HandleModOverrideZonePVPType,                    //431 SPELL_AURA_MOD_OVERRIDE_ZONE_PVP_TYPE
    &AuraEffect::HandleNULL,                                      //432
    &AuraEffect::HandleNULL,                                      //433
    &AuraEffect::HandleNULL,                                      //434
    &AuraEffect::HandleNULL,                                      //435
    &AuraEffect::HandleNULL,                                      //436 SPELL_AURA_MOD_ENVIRONMENTAL_DAMAGE_TAKEN
    &AuraEffect::HandleAuraModMinimumSpeedRate,                   //437 SPELL_AURA_MOD_MINIMUM_SPEED_RATE
    &AuraEffect::HandleNULL,                                      //438 SPELL_AURA_PRELOAD_PHASE
    &AuraEffect::HandleNULL,                                      //439
    &AuraEffect::HandleNULL,                                      //440 SPELL_AURA_MOD_MULTISTRIKE_DAMAGE
    &AuraEffect::HandleNULL,                                      //441 SPELL_AURA_MOD_MULTISTRIKE_CHANCE
    &AuraEffect::HandleNULL,                                      //442 SPELL_AURA_MOD_READINESS
    &AuraEffect::HandleNULL,                                      //443 SPELL_AURA_MOD_LEECH
    &AuraEffect::HandleNULL,                                      //444
    &AuraEffect::HandleNULL,                                      //445
    &AuraEffect::HandleNULL,                                      //446
    &AuraEffect::HandleNoImmediateEffect,                         //447 SPELL_AURA_MOD_XP_FROM_CREATURE_TYPE implemented in KillRewarder::_RewardXP
    &AuraEffect::HandleNULL,                                      //448
    &AuraEffect::HandleNULL,                                      //449
    &AuraEffect::HandleNULL,                                      //450
    &AuraEffect::HandleOverridePetSpecs,                          //451 SPELL_AURA_OVERRIDE_PET_SPECS
    &AuraEffect::HandleNULL,                                      //452
    &AuraEffect::HandleNoImmediateEffect,                         //453 SPELL_AURA_CHARGE_RECOVERY_MOD implemented in SpellHistory::GetChargeRecoveryTime
    &AuraEffect::HandleNoImmediateEffect,                         //454 SPELL_AURA_CHARGE_RECOVERY_MULTIPLIER implemented in SpellHistory::GetChargeRecoveryTime
    &AuraEffect::HandleAuraModRoot,                               //455 SPELL_AURA_MOD_ROOT_2
    &AuraEffect::HandleNoImmediateEffect,                         //456 SPELL_AURA_CHARGE_RECOVERY_AFFECTED_BY_HASTE implemented in SpellHistory::GetChargeRecoveryTime
    &AuraEffect::HandleNoImmediateEffect,                         //457 SPELL_AURA_CHARGE_RECOVERY_AFFECTED_BY_HASTE_REGEN implemented in SpellHistory::GetChargeRecoveryTime
    &AuraEffect::HandleNULL,                                      //458 SPELL_AURA_IGNORE_DUAL_WIELD_HIT_PENALTY
    &AuraEffect::HandleIgnoreMovementForces,                      //459 SPELL_AURA_IGNORE_MOVEMENT_FORCES
    &AuraEffect::HandleNULL,                                      //460 SPELL_AURA_RESET_COOLDOWNS_ON_DUEL_START
    &AuraEffect::HandleNULL,                                      //461
    &AuraEffect::HandleNULL,                                      //462 SPELL_AURA_MOD_HEALING_AND_ABSORB_FROM_CASTER
    &AuraEffect::HandleNULL,                                      //463 SPELL_AURA_CONVERT_CRIT_RATING_PCT_TO_PARRY_RATING used by Riposte
    &AuraEffect::HandleNULL,                                      //464 SPELL_AURA_MOD_ATTACK_POWER_OF_BONUS_ARMOR
    &AuraEffect::HandleNULL,                                      //465 SPELL_AURA_MOD_BONUS_ARMOR
    &AuraEffect::HandleNULL,                                      //466 SPELL_AURA_MOD_BONUS_ARMOR_PCT
    &AuraEffect::HandleModStatBonusPercent,                       //467 SPELL_AURA_MOD_STAT_BONUS_PCT
    &AuraEffect::HandleNULL,                                      //468 SPELL_AURA_TRIGGER_SPELL_ON_HEALTH_PCT
    &AuraEffect::HandleShowConfirmationPrompt,                    //469 SPELL_AURA_SHOW_CONFIRMATION_PROMPT_WITH_DIFFICULTY
    &AuraEffect::HandleNULL,                                      //470 SPELL_AURA_MOD_AURA_TIME_RATE_BY_SPELL_LABEL
    &AuraEffect::HandleModVersatilityByPct,                       //471 SPELL_AURA_MOD_VERSATILITY
    &AuraEffect::HandleNULL,                                      //472
    &AuraEffect::HandleNoImmediateEffect,                         //473 SPELL_AURA_PREVENT_DURABILITY_LOSS_FROM_COMBAT implemented in Player::DurabilityPointLossForEquipSlot
    &AuraEffect::HandleNULL,                                      //474 SPELL_AURA_REPLACE_ITEM_BONUS_TREE
    &AuraEffect::HandleAllowUsingGameobjectsWhileMounted,         //475 SPELL_AURA_ALLOW_USING_GAMEOBJECTS_WHILE_MOUNTED
    &AuraEffect::HandleNULL,                                      //476 SPELL_AURA_MOD_CURRENCY_GAIN_LOOTED
    &AuraEffect::HandleNULL,                                      //477
    &AuraEffect::HandleNULL,                                      //478
    &AuraEffect::HandleNULL,                                      //479
    &AuraEffect::HandleNULL,                                      //480 SPELL_AURA_MOD_ARTIFACT_ITEM_LEVEL
    &AuraEffect::HandleNoImmediateEffect,                         //481 SPELL_AURA_CONVERT_CONSUMED_RUNE implemented in Spell::TakeRunePower
    &AuraEffect::HandleNULL,                                      //482
    &AuraEffect::HandleNULL,                                      //483 SPELL_AURA_SUPPRESS_TRANSFORMS
    &AuraEffect::HandleNULL,                                      //484 SPELL_AURA_ALLOW_INTERRUPT_SPELL
    &AuraEffect::HandleModMovementForceMagnitude,                 //485 SPELL_AURA_MOD_MOVEMENT_FORCE_MAGNITUDE
    &AuraEffect::HandleNULL,                                      //486
    &AuraEffect::HandleCosmeticMounted,                           //487 SPELL_AURA_COSMETIC_MOUNTED
    &AuraEffect::HandleNULL,                                      //488
    &AuraEffect::HandleNULL,                                      //489 SPELL_AURA_MOD_ALTERNATIVE_DEFAULT_LANGUAGE
    &AuraEffect::HandleNULL,                                      //490
    &AuraEffect::HandleNULL,                                      //491
    &AuraEffect::HandleNULL,                                      //492
    &AuraEffect::HandleNULL,                                      //493
    &AuraEffect::HandleNULL,                                      //494 SPELL_AURA_SET_POWER_POINT_CHARGE
    &AuraEffect::HandleNULL,                                      //495 SPELL_AURA_TRIGGER_SPELL_ON_EXPIRE
    &AuraEffect::HandleNULL,                                      //496 SPELL_AURA_ALLOW_CHANGING_EQUIPMENT_IN_TORGHAST
    &AuraEffect::HandleNULL,                                      //497 SPELL_AURA_MOD_ANIMA_GAIN
    &AuraEffect::HandleNULL,                                      //498 SPELL_AURA_CURRENCY_LOSS_PCT_ON_DEATH
    &AuraEffect::HandleNULL,                                      //499 SPELL_AURA_MOD_RESTED_XP_CONSUMPTION
    &AuraEffect::HandleNULL,                                      //500 SPELL_AURA_IGNORE_SPELL_CHARGE_COOLDOWN
    &AuraEffect::HandleNULL,                                      //501 SPELL_AURA_MOD_CRITICAL_DAMAGE_TAKEN_FROM_CASTER
    &AuraEffect::HandleNULL,                                      //502 SPELL_AURA_MOD_VERSATILITY_DAMAGE_DONE_BENEFIT
    &AuraEffect::HandleNULL,                                      //503 SPELL_AURA_MOD_VERSATILITY_HEALING_DONE_BENEFIT
    &AuraEffect::HandleNoImmediateEffect,                         //504 SPELL_AURA_MOD_HEALING_TAKEN_FROM_CASTER implemented in Unit::SpellHealingBonusTaken
    &AuraEffect::HandleNULL,                                      //503 SPELL_AURA_MOD_PLAYER_CHOICE_REROLLS
};

AuraEffect::AuraEffect(Aura* base, SpellEffectInfo const& spellEfffectInfo, int32 const* baseAmount, Unit* caster) :
m_base(base), m_spellInfo(base->GetSpellInfo()), m_effectInfo(spellEfffectInfo), m_spellmod(nullptr),
m_baseAmount(baseAmount ? *baseAmount : spellEfffectInfo.CalcBaseValue(caster, base->GetType() == UNIT_AURA_TYPE ? base->GetOwner()->ToUnit() : nullptr, base->GetCastItemId(), base->GetCastItemLevel())),
_amount(), _periodicTimer(0), _period(0), _ticksDone(0),
m_canBeRecalculated(true), m_isPeriodic(false)
{
    CalculatePeriodic(caster, true, false);

    _amount = CalculateAmount(caster);

    CalculateSpellMod();
}

AuraEffect::~AuraEffect()
{
    delete m_spellmod;
}

template <typename Container>
void AuraEffect::GetTargetList(Container& targetContainer) const
{
    Aura::ApplicationMap const& targetMap = GetBase()->GetApplicationMap();
    // remove all targets which were not added to new list - they no longer deserve area aura
    for (auto appIter = targetMap.begin(); appIter != targetMap.end(); ++appIter)
    {
        if (appIter->second->HasEffect(GetEffIndex()))
            targetContainer.push_back(appIter->second->GetTarget());
    }
}

template <typename Container>
void AuraEffect::GetApplicationList(Container& applicationContainer) const
{
    Aura::ApplicationMap const& targetMap = GetBase()->GetApplicationMap();
    for (auto appIter = targetMap.begin(); appIter != targetMap.end(); ++appIter)
    {
        if (appIter->second->HasEffect(GetEffIndex()))
            applicationContainer.push_back(appIter->second);
    }
}

int32 AuraEffect::CalculateAmount(Unit* caster)
{
    // default amount calculation
    int32 amount = 0;

    if (!m_spellInfo->HasAttribute(SPELL_ATTR8_MASTERY_SPECIALIZATION) || G3D::fuzzyEq(GetSpellEffectInfo().BonusCoefficient, 0.0f))
        amount = GetSpellEffectInfo().CalcValue(caster, &m_baseAmount, GetBase()->GetOwner()->ToUnit(), nullptr, GetBase()->GetCastItemId(), GetBase()->GetCastItemLevel());
    else if (caster && caster->GetTypeId() == TYPEID_PLAYER)
        amount = int32(caster->ToPlayer()->m_activePlayerData->Mastery * GetSpellEffectInfo().BonusCoefficient);

    // custom amount calculations go here
    switch (GetAuraType())
    {
        // crowd control auras
        case SPELL_AURA_MOD_CONFUSE:
        case SPELL_AURA_MOD_FEAR:
        case SPELL_AURA_MOD_STUN:
        case SPELL_AURA_MOD_ROOT:
        case SPELL_AURA_TRANSFORM:
        case SPELL_AURA_MOD_ROOT_2:
            m_canBeRecalculated = false;
            if (!m_spellInfo->ProcFlags)
                break;
            amount = int32(GetBase()->GetUnitOwner()->CountPctFromMaxHealth(10));
            break;
        case SPELL_AURA_SCHOOL_ABSORB:
        case SPELL_AURA_MANA_SHIELD:
            m_canBeRecalculated = false;
            break;
        case SPELL_AURA_MOUNTED:
        {
            uint32 mountType = uint32(GetMiscValueB());
            if (MountEntry const* mountEntry = sDB2Manager.GetMount(GetId()))
                mountType = mountEntry->MountTypeID;

            if (MountCapabilityEntry const* mountCapability = GetBase()->GetUnitOwner()->GetMountCapability(mountType))
            {
                amount = mountCapability->ID;
                m_canBeRecalculated = false;
            }
            break;
        }
        case SPELL_AURA_SHOW_CONFIRMATION_PROMPT_WITH_DIFFICULTY:
            if (caster)
                amount = caster->GetMap()->GetDifficultyID();
            m_canBeRecalculated = false;
            break;
        default:
            break;
    }

    if (GetSpellInfo()->HasAttribute(SPELL_ATTR10_ROLLING_PERIODIC))
    {
        Unit::AuraEffectList const& periodicAuras = GetBase()->GetUnitOwner()->GetAuraEffectsByType(GetAuraType());
        amount = std::accumulate(std::begin(periodicAuras), std::end(periodicAuras), amount, [this](int32 val, AuraEffect const* aurEff)
        {
            if (aurEff->GetCasterGUID() == GetCasterGUID() && aurEff->GetId() == GetId() && aurEff->GetEffIndex() == GetEffIndex() && aurEff->GetTotalTicks() > 0)
                val += aurEff->GetAmount() * static_cast<int32>(aurEff->GetRemainingTicks()) / static_cast<int32>(aurEff->GetTotalTicks());
            return val;
        });
    }

    GetBase()->CallScriptEffectCalcAmountHandlers(this, amount, m_canBeRecalculated);
    if (!GetSpellEffectInfo().EffectAttributes.HasFlag(SpellEffectAttributes::NoScaleWithStack))
        amount *= GetBase()->GetStackAmount();

    if (caster && GetBase()->GetType() == UNIT_AURA_TYPE)
    {
        uint32 stackAmountForBonuses = !GetSpellEffectInfo().EffectAttributes.HasFlag(SpellEffectAttributes::NoScaleWithStack) ? GetBase()->GetStackAmount() : 1;

        switch (GetAuraType())
        {
            case SPELL_AURA_PERIODIC_DAMAGE:
            case SPELL_AURA_PERIODIC_LEECH:
                _estimatedAmount = caster->SpellDamageBonusDone(GetBase()->GetUnitOwner(), GetSpellInfo(), amount, DOT, GetSpellEffectInfo(), stackAmountForBonuses);
                break;
            case SPELL_AURA_PERIODIC_HEAL:
                _estimatedAmount = caster->SpellHealingBonusDone(GetBase()->GetUnitOwner(), GetSpellInfo(), amount, DOT, GetSpellEffectInfo(), stackAmountForBonuses);
                break;
            default:
                break;
        }
    }

    return amount;
}

uint32 AuraEffect::GetTotalTicks() const
{
    uint32 totalTicks = 0;
    if (_period && !GetBase()->IsPermanent())
    {
        totalTicks = static_cast<uint32>(GetBase()->GetMaxDuration() / _period);
        if (m_spellInfo->HasAttribute(SPELL_ATTR5_START_PERIODIC_AT_APPLY))
            ++totalTicks;
    }

    return totalTicks;
}

void AuraEffect::ResetPeriodic(bool resetPeriodicTimer /*= false*/)
{
    _ticksDone = 0;
    if (resetPeriodicTimer)
    {
        _periodicTimer = 0;
        // Start periodic on next tick or at aura apply
        if (m_spellInfo->HasAttribute(SPELL_ATTR5_START_PERIODIC_AT_APPLY))
            _periodicTimer = _period;
    }
}

void AuraEffect::CalculatePeriodic(Unit* caster, bool resetPeriodicTimer /*= true*/, bool load /*= false*/)
{
    _period = GetSpellEffectInfo().ApplyAuraPeriod;

    // prepare periodics
    switch (GetAuraType())
    {
        case SPELL_AURA_OBS_MOD_POWER:
        case SPELL_AURA_PERIODIC_DAMAGE:
        case SPELL_AURA_PERIODIC_HEAL:
        case SPELL_AURA_OBS_MOD_HEALTH:
        case SPELL_AURA_PERIODIC_TRIGGER_SPELL:
        case SPELL_AURA_PERIODIC_ENERGIZE:
        case SPELL_AURA_PERIODIC_LEECH:
        case SPELL_AURA_PERIODIC_HEALTH_FUNNEL:
        case SPELL_AURA_PERIODIC_MANA_LEECH:
        case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
        case SPELL_AURA_POWER_BURN:
        case SPELL_AURA_PERIODIC_DUMMY:
        case SPELL_AURA_PERIODIC_TRIGGER_SPELL_WITH_VALUE:
            m_isPeriodic = true;
            break;
        default:
            break;
    }

    GetBase()->CallScriptEffectCalcPeriodicHandlers(this, m_isPeriodic, _period);

    if (!m_isPeriodic)
        return;

    Player* modOwner = caster ? caster->GetSpellModOwner() : nullptr;
    // Apply casting time mods
    if (_period)
    {
        // Apply periodic time mod
        if (modOwner)
            modOwner->ApplySpellMod(GetSpellInfo(), SpellModOp::Period, _period);

        if (caster)
        {
            // Haste modifies periodic time of channeled spells
            if (m_spellInfo->IsChanneled())
                caster->ModSpellDurationTime(m_spellInfo, _period);
            else if (m_spellInfo->HasAttribute(SPELL_ATTR5_HASTE_AFFECT_DURATION))
                _period = int32(_period * caster->m_unitData->ModCastingSpeed);
        }
    }
    else // prevent infinite loop on Update
        m_isPeriodic = false;

    if (load) // aura loaded from db
    {
        if (_period && !GetBase()->IsPermanent())
        {
            uint32 elapsedTime = GetBase()->GetMaxDuration() - GetBase()->GetDuration();
            _ticksDone = elapsedTime / uint32(_period);
            _periodicTimer = elapsedTime % uint32(_period);
        }

        if (m_spellInfo->HasAttribute(SPELL_ATTR5_START_PERIODIC_AT_APPLY))
            ++_ticksDone;
    }
    else // aura just created or reapplied
    {
        // reset periodic timer on aura create or reapply
        // we don't reset periodic timers when aura is triggered by proc
        ResetPeriodic(resetPeriodicTimer);
    }
}

void AuraEffect::CalculateSpellMod()
{
    switch (GetAuraType())
    {
        case SPELL_AURA_ADD_FLAT_MODIFIER:
        case SPELL_AURA_ADD_PCT_MODIFIER:
            if (!m_spellmod)
            {
                SpellModifierByClassMask* spellmod = new SpellModifierByClassMask(GetBase());
                spellmod->op = SpellModOp(GetMiscValue());

                spellmod->type = GetAuraType() == SPELL_AURA_ADD_PCT_MODIFIER ? SPELLMOD_PCT : SPELLMOD_FLAT;
                spellmod->spellId = GetId();
                spellmod->mask = GetSpellEffectInfo().SpellClassMask;
                m_spellmod = spellmod;
            }
            static_cast<SpellModifierByClassMask*>(m_spellmod)->value = GetAmount();
            break;
        case SPELL_AURA_ADD_FLAT_MODIFIER_BY_SPELL_LABEL:
            if (!m_spellmod)
            {
                SpellFlatModifierByLabel* spellmod = new SpellFlatModifierByLabel(GetBase());
                spellmod->op = SpellModOp(GetMiscValue());

                spellmod->type = SPELLMOD_LABEL_FLAT;
                spellmod->spellId = GetId();
                spellmod->value.ModIndex = GetMiscValue();
                spellmod->value.LabelID = GetMiscValueB();
                m_spellmod = spellmod;
            }
            static_cast<SpellFlatModifierByLabel*>(m_spellmod)->value.ModifierValue = GetAmount();
            break;
        case SPELL_AURA_ADD_PCT_MODIFIER_BY_SPELL_LABEL:
            if (!m_spellmod)
            {
                SpellPctModifierByLabel* spellmod = new SpellPctModifierByLabel(GetBase());
                spellmod->op = SpellModOp(GetMiscValue());

                spellmod->type = SPELLMOD_LABEL_PCT;
                spellmod->spellId = GetId();
                spellmod->value.ModIndex = GetMiscValue();
                spellmod->value.LabelID = GetMiscValueB();
                m_spellmod = spellmod;
            }
            static_cast<SpellPctModifierByLabel*>(m_spellmod)->value.ModifierValue = 1.0f + CalculatePct(1.0f, GetAmount());
            break;
        default:
            break;
    }
    GetBase()->CallScriptEffectCalcSpellModHandlers(this, m_spellmod);
}

void AuraEffect::ChangeAmount(int32 newAmount, bool mark, bool onStackOrReapply, AuraEffect const* triggeredBy /* = nullptr */)
{
    // Reapply if amount change
    uint8 handleMask = 0;
    if (newAmount != GetAmount())
        handleMask |= AURA_EFFECT_HANDLE_CHANGE_AMOUNT;
    if (onStackOrReapply)
        handleMask |= AURA_EFFECT_HANDLE_REAPPLY;

    if (!handleMask)
        return;

    std::vector<AuraApplication*> effectApplications;
    GetApplicationList(effectApplications);

    for (AuraApplication* aurApp : effectApplications)
    {
        aurApp->GetTarget()->_RegisterAuraEffect(this, false);
        HandleEffect(aurApp, handleMask, false, triggeredBy);
    }

    if (handleMask & AURA_EFFECT_HANDLE_CHANGE_AMOUNT)
    {
        if (!mark)
            _amount = newAmount;
        else
            SetAmount(newAmount);
        CalculateSpellMod();
    }

    for (AuraApplication* aurApp : effectApplications)
    {
        if (aurApp->GetRemoveMode() != AURA_REMOVE_NONE)
            continue;

        aurApp->GetTarget()->_RegisterAuraEffect(this, true);
        HandleEffect(aurApp, handleMask, true, triggeredBy);
    }

    if (GetSpellInfo()->HasAttribute(SPELL_ATTR8_AURA_SEND_AMOUNT) || Aura::EffectTypeNeedsSendingAmount(GetAuraType()))
        GetBase()->SetNeedClientUpdateForTargets();
}

void AuraEffect::HandleEffect(AuraApplication * aurApp, uint8 mode, bool apply, AuraEffect const* triggeredBy /*= nullptr*/)
{
    // check if call is correct, we really don't want using bitmasks here (with 1 exception)
    ASSERT(mode == AURA_EFFECT_HANDLE_REAL
        || mode == AURA_EFFECT_HANDLE_SEND_FOR_CLIENT
        || mode == AURA_EFFECT_HANDLE_CHANGE_AMOUNT
        || mode == AURA_EFFECT_HANDLE_STAT
        || mode == AURA_EFFECT_HANDLE_SKILL
        || mode == AURA_EFFECT_HANDLE_REAPPLY
        || mode == (AURA_EFFECT_HANDLE_CHANGE_AMOUNT | AURA_EFFECT_HANDLE_REAPPLY));

    // register/unregister effect in lists in case of real AuraEffect apply/remove
    // registration/unregistration is done always before real effect handling (some effect handlers code is depending on this)
    if (mode & AURA_EFFECT_HANDLE_REAL)
        aurApp->GetTarget()->_RegisterAuraEffect(this, apply);

    // real aura apply/remove, handle modifier
    if (mode & AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK)
        ApplySpellMod(aurApp->GetTarget(), apply, triggeredBy);

    // call scripts helping/replacing effect handlers
    bool prevented = false;
    if (apply)
        prevented = GetBase()->CallScriptEffectApplyHandlers(this, aurApp, (AuraEffectHandleModes)mode);
    else
        prevented = GetBase()->CallScriptEffectRemoveHandlers(this, aurApp, (AuraEffectHandleModes)mode);

    // check if script events have removed the aura already
    if (apply && aurApp->GetRemoveMode())
        return;

    // call default effect handler if it wasn't prevented
    if (!prevented)
        (*this.*AuraEffectHandler[GetAuraType()].Value)(aurApp, mode, apply);

    // check if the default handler reemoved the aura
    if (apply && aurApp->GetRemoveMode())
        return;

    // call scripts triggering additional events after apply/remove
    if (apply)
        GetBase()->CallScriptAfterEffectApplyHandlers(this, aurApp, (AuraEffectHandleModes)mode);
    else
        GetBase()->CallScriptAfterEffectRemoveHandlers(this, aurApp, (AuraEffectHandleModes)mode);
}

void AuraEffect::HandleEffect(Unit* target, uint8 mode, bool apply, AuraEffect const* triggeredBy /*= nullptr*/)
{
    AuraApplication* aurApp = GetBase()->GetApplicationOfTarget(target->GetGUID());
    ASSERT(aurApp);
    HandleEffect(aurApp, mode, apply, triggeredBy);
}

void AuraEffect::ApplySpellMod(Unit* target, bool apply, AuraEffect const* triggeredBy /*= nullptr*/)
{
    if (!m_spellmod || target->GetTypeId() != TYPEID_PLAYER)
        return;

    target->ToPlayer()->AddSpellMod(m_spellmod, apply);

    // Auras with charges do not mod amount of passive auras
    if (GetBase()->IsUsingCharges())
        return;

    // reapply some passive spells after add/remove related spellmods
    // Warning: it is a dead loop if 2 auras each other amount-shouldn't happen
    std::bitset<MAX_SPELL_EFFECTS> recalculateEffectMask;
    switch (SpellModOp(GetMiscValue()))
    {
        case SpellModOp::Points:
            recalculateEffectMask.set();
            break;
        case SpellModOp::PointsIndex0:
            recalculateEffectMask.set(EFFECT_0);
            break;
        case SpellModOp::PointsIndex1:
            recalculateEffectMask.set(EFFECT_1);
            break;
        case SpellModOp::PointsIndex2:
            recalculateEffectMask.set(EFFECT_2);
            break;
        case SpellModOp::PointsIndex3:
            recalculateEffectMask.set(EFFECT_3);
            break;
        case SpellModOp::PointsIndex4:
            recalculateEffectMask.set(EFFECT_4);
            break;
        default:
            break;
    }

    if (recalculateEffectMask.any())
    {
        if (!triggeredBy)
            triggeredBy = this;

        ObjectGuid guid = target->GetGUID();
        Unit::AuraApplicationMap& auras = target->GetAppliedAuras();
        for (auto iter = auras.begin(); iter != auras.end(); ++iter)
        {
            Aura* aura = iter->second->GetBase();
            // only passive and permament auras-active auras should have amount set on spellcast and not be affected
            // if aura is cast by others, it will not be affected
            if ((aura->IsPassive() || aura->IsPermanent()) && aura->GetCasterGUID() == guid && aura->GetSpellInfo()->IsAffectedBySpellMod(m_spellmod))
                for (size_t i = 0; i < recalculateEffectMask.size(); ++i)
                    if (recalculateEffectMask[i])
                        if (AuraEffect* aurEff = aura->GetEffect(i))
                            if (aurEff != triggeredBy)
                                aurEff->RecalculateAmount(triggeredBy);
        }
    }
}

void AuraEffect::Update(uint32 diff, Unit* caster)
{
    if (!m_isPeriodic || (GetBase()->GetDuration() < 0 && !GetBase()->IsPassive() && !GetBase()->IsPermanent()))
        return;

    uint32 totalTicks = GetTotalTicks();

    _periodicTimer += diff;
    while (_periodicTimer >= _period)
    {
        _periodicTimer -= _period;

        if (!GetBase()->IsPermanent() && (_ticksDone + 1) > totalTicks)
            break;

        ++_ticksDone;

        GetBase()->CallScriptEffectUpdatePeriodicHandlers(this);

        std::vector<AuraApplication*> effectApplications;
        GetApplicationList(effectApplications);

        // tick on targets of effects
        for (AuraApplication* aurApp : effectApplications)
            PeriodicTick(aurApp, caster);
    }
}

float AuraEffect::GetCritChanceFor(Unit const* caster, Unit const* target) const
{
    return target->SpellCritChanceTaken(caster, nullptr, this, GetSpellInfo()->GetSchoolMask(), CalcPeriodicCritChance(caster), GetSpellInfo()->GetAttackType());
}

bool AuraEffect::IsAffectingSpell(SpellInfo const* spell) const
{
    if (!spell)
        return false;

    // Check family name and EffectClassMask
    if (!spell->IsAffected(m_spellInfo->SpellFamilyName, GetSpellEffectInfo().SpellClassMask))
        return false;

    return true;
}

void AuraEffect::SendTickImmune(Unit* target, Unit* caster) const
{
    if (caster)
        caster->SendSpellDamageImmune(target, m_spellInfo->Id, true);
}

void AuraEffect::PeriodicTick(AuraApplication* aurApp, Unit* caster) const
{
    bool prevented = GetBase()->CallScriptEffectPeriodicHandlers(this, aurApp);
    if (prevented)
        return;

    Unit* target = aurApp->GetTarget();

    switch (GetAuraType())
    {
        case SPELL_AURA_PERIODIC_DUMMY:
            // handled via scripts
            break;
        case SPELL_AURA_PERIODIC_TRIGGER_SPELL:
            HandlePeriodicTriggerSpellAuraTick(target, caster);
            break;
        case SPELL_AURA_PERIODIC_TRIGGER_SPELL_WITH_VALUE:
            HandlePeriodicTriggerSpellWithValueAuraTick(target, caster);
            break;
        case SPELL_AURA_PERIODIC_DAMAGE:
        case SPELL_AURA_PERIODIC_WEAPON_PERCENT_DAMAGE:
        case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
            HandlePeriodicDamageAurasTick(target, caster);
            break;
        case SPELL_AURA_PERIODIC_LEECH:
            HandlePeriodicHealthLeechAuraTick(target, caster);
            break;
        case SPELL_AURA_PERIODIC_HEALTH_FUNNEL:
            HandlePeriodicHealthFunnelAuraTick(target, caster);
            break;
        case SPELL_AURA_PERIODIC_HEAL:
        case SPELL_AURA_OBS_MOD_HEALTH:
            HandlePeriodicHealAurasTick(target, caster);
            break;
        case SPELL_AURA_PERIODIC_MANA_LEECH:
            HandlePeriodicManaLeechAuraTick(target, caster);
            break;
        case SPELL_AURA_OBS_MOD_POWER:
            HandleObsModPowerAuraTick(target, caster);
            break;
        case SPELL_AURA_PERIODIC_ENERGIZE:
            HandlePeriodicEnergizeAuraTick(target, caster);
            break;
        case SPELL_AURA_POWER_BURN:
            HandlePeriodicPowerBurnAuraTick(target, caster);
            break;
        default:
            break;
    }
}

bool AuraEffect::CheckEffectProc(AuraApplication* aurApp, ProcEventInfo& eventInfo) const
{
    bool result = GetBase()->CallScriptCheckEffectProcHandlers(this, aurApp, eventInfo);
    if (!result)
        return false;

    SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
    switch (GetAuraType())
    {
        case SPELL_AURA_MOD_CONFUSE:
        case SPELL_AURA_MOD_FEAR:
        case SPELL_AURA_MOD_STUN:
        case SPELL_AURA_MOD_ROOT:
        case SPELL_AURA_TRANSFORM:
        {
            DamageInfo* damageInfo = eventInfo.GetDamageInfo();
            if (!damageInfo || !damageInfo->GetDamage())
                return false;

            // Spell own damage at apply won't break CC
            if (spellInfo && spellInfo == GetSpellInfo())
            {
                Aura* aura = GetBase();
                // called from spellcast, should not have ticked yet
                if (aura->GetDuration() == aura->GetMaxDuration())
                    return false;
            }
            break;
        }
        case SPELL_AURA_MECHANIC_IMMUNITY:
        case SPELL_AURA_MOD_MECHANIC_RESISTANCE:
            // compare mechanic
            if (!spellInfo || !(spellInfo->GetAllEffectsMechanicMask() & (1 << GetMiscValue())))
                return false;
            break;
        case SPELL_AURA_MOD_CASTING_SPEED_NOT_STACK:
            // skip melee hits and instant cast spells
            if (!eventInfo.GetProcSpell() || !eventInfo.GetProcSpell()->GetCastTime())
                return false;
            break;
        case SPELL_AURA_MOD_SCHOOL_MASK_DAMAGE_FROM_CASTER:
        case SPELL_AURA_MOD_SPELL_DAMAGE_FROM_CASTER:
            // Compare casters
            if (GetCasterGUID() != eventInfo.GetActor()->GetGUID())
                return false;
            break;
        case SPELL_AURA_MOD_POWER_COST_SCHOOL:
        case SPELL_AURA_MOD_POWER_COST_SCHOOL_PCT:
        {
            // Skip melee hits and spells with wrong school or zero cost
            if (!spellInfo || !(spellInfo->GetSchoolMask() & GetMiscValue()) // School Check
                || !eventInfo.GetProcSpell())
                return false;

            // Costs Check
            std::vector<SpellPowerCost> const& costs = eventInfo.GetProcSpell()->GetPowerCost();
            auto m = std::find_if(costs.begin(), costs.end(), [](SpellPowerCost const& cost) { return cost.Amount > 0; });
            if (m == costs.end())
                return false;
            break;
        }
        case SPELL_AURA_REFLECT_SPELLS_SCHOOL:
            // Skip melee hits and spells with wrong school
            if (!spellInfo || !(spellInfo->GetSchoolMask() & GetMiscValue()))
                return false;
            break;
        case SPELL_AURA_PROC_TRIGGER_SPELL:
        case SPELL_AURA_PROC_TRIGGER_SPELL_WITH_VALUE:
        {
            // Don't proc extra attacks while already processing extra attack spell
            uint32 triggerSpellId = GetSpellEffectInfo().TriggerSpell;
            if (SpellInfo const* triggeredSpellInfo = sSpellMgr->GetSpellInfo(triggerSpellId, GetBase()->GetCastDifficulty()))
                if (aurApp->GetTarget()->m_extraAttacks && triggeredSpellInfo->HasEffect(SPELL_EFFECT_ADD_EXTRA_ATTACKS))
                    return false;
            break;
        }
        case SPELL_AURA_MOD_SPELL_CRIT_CHANCE:
            // skip spells that can't crit
            if (!spellInfo || !spellInfo->HasAttribute(SPELL_ATTR0_CU_CAN_CRIT))
                return false;
            break;
        default:
            break;
    }

    return result;
}

void AuraEffect::HandleProc(AuraApplication* aurApp, ProcEventInfo& eventInfo)
{
    bool prevented = GetBase()->CallScriptEffectProcHandlers(this, aurApp, eventInfo);
    if (prevented)
        return;

    switch (GetAuraType())
    {
        // CC Auras which use their amount to drop
        // Are there any more auras which need this?
        case SPELL_AURA_MOD_CONFUSE:
        case SPELL_AURA_MOD_FEAR:
        case SPELL_AURA_MOD_STUN:
        case SPELL_AURA_MOD_ROOT:
        case SPELL_AURA_TRANSFORM:
        case SPELL_AURA_MOD_ROOT_2:
            HandleBreakableCCAuraProc(aurApp, eventInfo);
            break;
        case SPELL_AURA_DUMMY:
        case SPELL_AURA_PROC_TRIGGER_SPELL:
            HandleProcTriggerSpellAuraProc(aurApp, eventInfo);
            break;
        case SPELL_AURA_PROC_TRIGGER_SPELL_WITH_VALUE:
            HandleProcTriggerSpellWithValueAuraProc(aurApp, eventInfo);
            break;
        case SPELL_AURA_PROC_TRIGGER_DAMAGE:
            HandleProcTriggerDamageAuraProc(aurApp, eventInfo);
            break;
        default:
            break;
    }

    GetBase()->CallScriptAfterEffectProcHandlers(this, aurApp, eventInfo);
}

void AuraEffect::HandleShapeshiftBoosts(Unit* target, bool apply) const
{
    uint32 spellId = 0;
    uint32 spellId2 = 0;
    uint32 spellId3 = 0;
    uint32 spellId4 = 0;

    switch (GetMiscValue())
    {
        case FORM_CAT_FORM:
            spellId = 3025;
            spellId2 = 48629;
            spellId3 = 106840;
            spellId4 = 113636;
            break;
        case FORM_TREE_OF_LIFE:
            spellId = 5420;
            spellId2 = 81097;
            break;
        case FORM_TRAVEL_FORM:
            spellId = 5419;
            break;
        case FORM_AQUATIC_FORM:
            spellId = 5421;
            break;
        case FORM_BEAR_FORM:
            spellId = 1178;
            spellId2 = 21178;
            spellId3 = 106829;
            spellId4 = 106899;
            break;
        case FORM_FLIGHT_FORM:
            spellId = 33948;
            spellId2 = 34764;
            break;
        case FORM_FLIGHT_FORM_EPIC:
            spellId  = 40122;
            spellId2 = 40121;
            break;
        case FORM_SPIRIT_OF_REDEMPTION:
            spellId  = 27792;
            spellId2 = 27795;
            spellId3 = 62371;
            break;
        case FORM_SHADOWFORM:
            if (target->HasAura(107906)) // Glyph of Shadow
                spellId = 107904;
            else if (target->HasAura(126745)) // Glyph of Shadowy Friends
                spellId = 142024;
            else
                spellId = 107903;
            break;
        case FORM_GHOST_WOLF:
            if (target->HasAura(58135)) // Glyph of Spectral Wolf
                spellId = 160942;
            break;
        case FORM_GHOUL:
        case FORM_AMBIENT:
        case FORM_STEALTH:
        case FORM_BATTLE_STANCE:
        case FORM_DEFENSIVE_STANCE:
        case FORM_BERSERKER_STANCE:
        case FORM_MOONKIN_FORM:
        case FORM_METAMORPHOSIS:
            break;
        default:
            break;
    }

    if (apply)
    {
        if (spellId)
            target->CastSpell(target, spellId, this);

        if (spellId2)
            target->CastSpell(target, spellId2, this);

        if (spellId3)
            target->CastSpell(target, spellId3, this);

        if (spellId4)
            target->CastSpell(target, spellId4, this);

        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            Player* plrTarget = target->ToPlayer();

            PlayerSpellMap const& sp_list = plrTarget->GetSpellMap();
            for (auto itr = sp_list.begin(); itr != sp_list.end(); ++itr)
            {
                if (itr->second->state == PLAYERSPELL_REMOVED || itr->second->disabled)
                    continue;

                if (itr->first == spellId || itr->first == spellId2 || itr->first == spellId3 || itr->first == spellId4)
                    continue;

                SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(itr->first, DIFFICULTY_NONE);
                if (!spellInfo || !(spellInfo->IsPassive() || spellInfo->HasAttribute(SPELL_ATTR0_HIDDEN_CLIENTSIDE)))
                    continue;

                // always valid?
                if ((spellInfo->HasAttribute(SPELL_ATTR8_MASTERY_SPECIALIZATION)) && !plrTarget->IsCurrentSpecMasterySpell(spellInfo))
                    continue;

                if (spellInfo->Stances & (UI64LIT(1) << (GetMiscValue() - 1)))
                    target->CastSpell(target, itr->first, this);
            }
        }
    }
    else
    {
        if (spellId)
            target->RemoveOwnedAura(spellId, target->GetGUID());
        if (spellId2)
            target->RemoveOwnedAura(spellId2, target->GetGUID());
        if (spellId3)
            target->RemoveOwnedAura(spellId3, target->GetGUID());
        if (spellId4)
            target->RemoveOwnedAura(spellId4, target->GetGUID());

        Unit::AuraEffectList const& shapeshifts = target->GetAuraEffectsByType(SPELL_AURA_MOD_SHAPESHIFT);
        AuraEffect const* newAura = nullptr;
        // Iterate through all the shapeshift auras that the target has, if there is another aura with SPELL_AURA_MOD_SHAPESHIFT, then this aura is being removed due to that one being applied
        for (AuraEffect const* aurEff : shapeshifts)
        {
            if (aurEff != this)
            {
                newAura = aurEff;
                break;
            }
        }

        Unit::AuraApplicationMap& tAuras = target->GetAppliedAuras();
        for (auto itr = tAuras.begin(); itr != tAuras.end();)
        {
            // Use the new aura to see on what stance the target will be
            uint64 newStance = newAura ? (UI64LIT(1) << (newAura->GetMiscValue() - 1)) : 0;

            // If the stances are not compatible with the spell, remove it
            if (itr->second->GetBase()->IsRemovedOnShapeLost(target) && !(itr->second->GetBase()->GetSpellInfo()->Stances & newStance))
                target->RemoveAura(itr);
            else
                ++itr;
        }
    }
}

/*********************************************************/
/***               AURA EFFECT HANDLERS                ***/
/*********************************************************/

/**************************************/
/***       VISIBILITY & PHASES      ***/
/**************************************/

void AuraEffect::HandleModInvisibilityDetect(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK))
        return;

    Unit* target = aurApp->GetTarget();
    InvisibilityType type = InvisibilityType(GetMiscValue());

    if (apply)
    {
        target->m_invisibilityDetect.AddFlag(type);
        target->m_invisibilityDetect.AddValue(type, GetAmount());
    }
    else
    {
        if (!target->HasAuraType(SPELL_AURA_MOD_INVISIBILITY_DETECT))
            target->m_invisibilityDetect.DelFlag(type);

        target->m_invisibilityDetect.AddValue(type, -GetAmount());
    }

    // call functions which may have additional effects after chainging state of unit
    target->UpdateObjectVisibility();
}

void AuraEffect::HandleModInvisibility(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_CHANGE_AMOUNT_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();
    InvisibilityType type = InvisibilityType(GetMiscValue());

    if (apply)
    {
        // apply glow vision
        if (Player* playerTarget = target->ToPlayer())
            playerTarget->AddAuraVision(PLAYER_FIELD_BYTE2_INVISIBILITY_GLOW);

        target->m_invisibility.AddFlag(type);
        target->m_invisibility.AddValue(type, GetAmount());
    }
    else
    {
        if (!target->HasAuraType(SPELL_AURA_MOD_INVISIBILITY))
        {
            // if not have different invisibility auras.
            // remove glow vision
            if (Player * playerTarget = target->ToPlayer())
                playerTarget->RemoveAuraVision(PLAYER_FIELD_BYTE2_INVISIBILITY_GLOW);

            target->m_invisibility.DelFlag(type);
        }
        else
        {
            bool found = false;
            Unit::AuraEffectList const& invisAuras = target->GetAuraEffectsByType(SPELL_AURA_MOD_INVISIBILITY);
            for (Unit::AuraEffectList::const_iterator i = invisAuras.begin(); i != invisAuras.end(); ++i)
            {
                if (GetMiscValue() == (*i)->GetMiscValue())
                {
                    found = true;
                    break;
                }
            }
            if (!found)
                target->m_invisibility.DelFlag(type);
        }

        target->m_invisibility.AddValue(type, -GetAmount());
    }

    // call functions which may have additional effects after chainging state of unit
    if (apply && (mode & AURA_EFFECT_HANDLE_REAL))
    {
        // drop flag at invisibiliy in bg
        target->RemoveAurasWithInterruptFlags(SpellAuraInterruptFlags::StealthOrInvis);
    }
    target->UpdateObjectVisibility();
}

void AuraEffect::HandleModStealthDetect(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK))
        return;

    Unit* target = aurApp->GetTarget();
    StealthType type = StealthType(GetMiscValue());

    if (apply)
    {
        target->m_stealthDetect.AddFlag(type);
        target->m_stealthDetect.AddValue(type, GetAmount());
    }
    else
    {
        if (!target->HasAuraType(SPELL_AURA_MOD_STEALTH_DETECT))
            target->m_stealthDetect.DelFlag(type);

        target->m_stealthDetect.AddValue(type, -GetAmount());
    }

    // call functions which may have additional effects after chainging state of unit
    target->UpdateObjectVisibility();
}

void AuraEffect::HandleModStealth(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_CHANGE_AMOUNT_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();
    StealthType type = StealthType(GetMiscValue());

    if (apply)
    {
        target->m_stealth.AddFlag(type);
        target->m_stealth.AddValue(type, GetAmount());

        target->AddVisFlags(UNIT_VIS_FLAGS_CREEP);
        if (Player * playerTarget = target->ToPlayer())
            playerTarget->AddAuraVision(PLAYER_FIELD_BYTE2_STEALTH);
    }
    else
    {
        target->m_stealth.AddValue(type, -GetAmount());

        if (!target->HasAuraType(SPELL_AURA_MOD_STEALTH)) // if last SPELL_AURA_MOD_STEALTH
        {
            target->m_stealth.DelFlag(type);

            target->RemoveVisFlags(UNIT_VIS_FLAGS_CREEP);
            if (Player * playerTarget = target->ToPlayer())
                playerTarget->RemoveAuraVision(PLAYER_FIELD_BYTE2_STEALTH);
        }
    }

    // call functions which may have additional effects after chainging state of unit
    if (apply && (mode & AURA_EFFECT_HANDLE_REAL))
    {
        // drop flag at stealth in bg
        target->RemoveAurasWithInterruptFlags(SpellAuraInterruptFlags::StealthOrInvis);
    }
    target->UpdateObjectVisibility();
}

void AuraEffect::HandleModStealthLevel(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK))
        return;

    Unit* target = aurApp->GetTarget();
    StealthType type = StealthType(GetMiscValue());

    if (apply)
        target->m_stealth.AddValue(type, GetAmount());
    else
        target->m_stealth.AddValue(type, -GetAmount());

    // call functions which may have additional effects after chainging state of unit
    target->UpdateObjectVisibility();
}

void AuraEffect::HandleDetectAmore(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    if (target->GetTypeId() != TYPEID_PLAYER)
        return;

    if (apply)
    {
        if (Player* playerTarget = target->ToPlayer())
        {
            playerTarget->AddAuraVision(PlayerFieldByte2Flags(1 << (GetMiscValue() - 1)));
        }
    }
    else
    {
        if (target->HasAuraType(SPELL_AURA_DETECT_AMORE))
        {
            Unit::AuraEffectList const& amoreAuras = target->GetAuraEffectsByType(SPELL_AURA_DETECT_AMORE);
            for (Unit::AuraEffectList::const_iterator i = amoreAuras.begin(); i != amoreAuras.end(); ++i)
            {
                if (GetMiscValue() == (*i)->GetMiscValue())
                    return;
            }
        }

        if (Player* playerTarget = target->ToPlayer())
            playerTarget->RemoveAuraVision(PlayerFieldByte2Flags(1 << (GetMiscValue() - 1)));
    }
}

void AuraEffect::HandleSpiritOfRedemption(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    if (target->GetTypeId() != TYPEID_PLAYER)
        return;

    // prepare spirit state
    if (apply)
    {
        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            // set stand state (expected in this form)
            if (!target->IsStandState())
                target->SetStandState(UNIT_STAND_STATE_STAND);
        }
    }
    // die at aura end
    else if (target->IsAlive())
        // call functions which may have additional effects after chainging state of unit
        target->setDeathState(JUST_DIED);
}

void AuraEffect::HandleAuraGhost(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Player* target = aurApp->GetTarget()->ToPlayer();
    if (!target)
        return;

    if (apply)
    {
        target->AddPlayerFlag(PLAYER_FLAGS_GHOST);
        target->m_serverSideVisibility.SetValue(SERVERSIDE_VISIBILITY_GHOST, GHOST_VISIBILITY_GHOST);
        target->m_serverSideVisibilityDetect.SetValue(SERVERSIDE_VISIBILITY_GHOST, GHOST_VISIBILITY_GHOST);
    }
    else
    {
        if (target->HasAuraType(SPELL_AURA_GHOST))
            return;

        target->RemovePlayerFlag(PLAYER_FLAGS_GHOST);
        target->m_serverSideVisibility.SetValue(SERVERSIDE_VISIBILITY_GHOST, GHOST_VISIBILITY_ALIVE);
        target->m_serverSideVisibilityDetect.SetValue(SERVERSIDE_VISIBILITY_GHOST, GHOST_VISIBILITY_ALIVE);
    }
}

void AuraEffect::HandlePhase(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    if (apply)
        PhasingHandler::AddPhase(target, uint32(GetMiscValueB()), true);
    else
        PhasingHandler::RemovePhase(target, uint32(GetMiscValueB()), true);
}

void AuraEffect::HandlePhaseGroup(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    if (apply)
        PhasingHandler::AddPhaseGroup(target, uint32(GetMiscValueB()), true);
    else
        PhasingHandler::RemovePhaseGroup(target, uint32(GetMiscValueB()), true);
}

void AuraEffect::HandlePhaseAlwaysVisible(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    if (!apply)
        PhasingHandler::SetAlwaysVisible(target, true, true);
    else
    {
        if (target->HasAuraType(SPELL_AURA_PHASE_ALWAYS_VISIBLE) || (target->IsPlayer() && target->ToPlayer()->IsGameMaster()))
            return;

        PhasingHandler::SetAlwaysVisible(target, false, true);
    }
}

/**********************/
/***   UNIT MODEL   ***/
/**********************/

void AuraEffect::HandleAuraModShapeshift(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK))
        return;

    SpellShapeshiftFormEntry const* shapeInfo = sSpellShapeshiftFormStore.LookupEntry(GetMiscValue());
    ASSERT(shapeInfo, "Spell %u uses unknown ShapeshiftForm (%u).", GetId(), GetMiscValue());

    Unit* target = aurApp->GetTarget();

    ShapeshiftForm form = ShapeshiftForm(GetMiscValue());
    uint32 modelid = target->GetModelForForm(form, GetId());

    if (apply)
    {
        // remove polymorph before changing display id to keep new display id
        switch (form)
        {
            case FORM_CAT_FORM:
            case FORM_TREE_OF_LIFE:
            case FORM_TRAVEL_FORM:
            case FORM_AQUATIC_FORM:
            case FORM_BEAR_FORM:
            case FORM_FLIGHT_FORM_EPIC:
            case FORM_FLIGHT_FORM:
            case FORM_MOONKIN_FORM:
            {
                // remove movement affects
                target->RemoveAurasByShapeShift();

                // and polymorphic affects
                if (target->IsPolymorphed())
                    target->RemoveAurasDueToSpell(target->GetTransformSpell());
                break;
            }
            default:
               break;
        }

        // remove other shapeshift before applying a new one
        target->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT, ObjectGuid::Empty, GetBase());

        // stop handling the effect if it was removed by linked event
        if (aurApp->GetRemoveMode())
            return;

        ShapeshiftForm prevForm = target->GetShapeshiftForm();
        target->SetShapeshiftForm(form);
        // add the shapeshift aura's boosts
        if (prevForm != form)
            HandleShapeshiftBoosts(target, true);

        if (modelid > 0)
        {
            SpellInfo const* transformSpellInfo = sSpellMgr->GetSpellInfo(target->GetTransformSpell(), GetBase()->GetCastDifficulty());
            if (!transformSpellInfo || !GetSpellInfo()->IsPositive())
                target->SetDisplayId(modelid);
        }

        if (!shapeInfo->GetFlags().HasFlag(SpellShapeshiftFormFlags::Stance))
            target->RemoveAurasWithInterruptFlags(SpellAuraInterruptFlags::Shapeshifting, GetId());
    }
    else
    {
        // reset model id if no other auras present
        // may happen when aura is applied on linked event on aura removal
        if (!target->HasAuraType(SPELL_AURA_MOD_SHAPESHIFT))
        {
            target->SetShapeshiftForm(FORM_NONE);
            if (target->getClass() == CLASS_DRUID)
            {
                // Remove movement impairing effects also when shifting out
                target->RemoveAurasByShapeShift();
            }
        }

        if (modelid > 0)
            target->RestoreDisplayId(target->IsMounted());

        switch (form)
        {
            // Nordrassil Harness - bonus
            case FORM_BEAR_FORM:
            case FORM_CAT_FORM:
                if (AuraEffect* dummy = target->GetAuraEffect(37315, 0))
                    target->CastSpell(target, 37316, dummy);
                break;
            // Nordrassil Regalia - bonus
            case FORM_MOONKIN_FORM:
                if (AuraEffect* dummy = target->GetAuraEffect(37324, 0))
                    target->CastSpell(target, 37325, dummy);
                break;
            default:
                break;
        }

        // remove the shapeshift aura's boosts
        HandleShapeshiftBoosts(target, false);
    }

    if (Player* playerTarget = target->ToPlayer())
    {
        playerTarget->SendMovementSetCollisionHeight(playerTarget->GetCollisionHeight(), WorldPackets::Movement::UpdateCollisionHeightReason::Force);
        playerTarget->InitDataForForm();
    }
    else
        target->UpdateDisplayPower();

    if (target->getClass() == CLASS_DRUID)
    {
        // Dash
        if (AuraEffect* aurEff = target->GetAuraEffect(SPELL_AURA_MOD_INCREASE_SPEED, SPELLFAMILY_DRUID, flag128(0, 0, 0x8)))
            aurEff->RecalculateAmount();

        // Disarm handling
        // If druid shifts while being disarmed we need to deal with that since forms aren't affected by disarm
        // and also HandleAuraModDisarm is not triggered
        if (!target->CanUseAttackType(BASE_ATTACK))
        {
            if (Item* pItem = target->ToPlayer()->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND))
                target->ToPlayer()->_ApplyWeaponDamage(EQUIPMENT_SLOT_MAINHAND, pItem, apply);
        }
    }

    // stop handling the effect if it was removed by linked event
    if (apply && aurApp->GetRemoveMode())
        return;

    if (target->GetTypeId() == TYPEID_PLAYER)
    {
        // Learn spells for shapeshift form - no need to send action bars or add spells to spellbook
        for (uint8 i = 0; i < MAX_SHAPESHIFT_SPELLS; ++i)
        {
            if (!shapeInfo->PresetSpellID[i])
                continue;
            if (apply)
                target->ToPlayer()->AddTemporarySpell(shapeInfo->PresetSpellID[i]);
            else
                target->ToPlayer()->RemoveTemporarySpell(shapeInfo->PresetSpellID[i]);
        }
    }
}

void AuraEffect::HandleAuraTransform(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    if (apply)
    {
        // update active transform spell only when transform not set or not overwriting negative by positive case
        SpellInfo const* transformSpellInfo = sSpellMgr->GetSpellInfo(target->GetTransformSpell(), GetBase()->GetCastDifficulty());
        if (!transformSpellInfo || !GetSpellInfo()->IsPositive() || transformSpellInfo->IsPositive())
        {
            target->SetTransformSpell(GetId());
            // special case (spell specific functionality)
            if (GetMiscValue() == 0)
            {
                switch (GetId())
                {
                    // Orb of Deception
                    case 16739:
                    {
                        if (target->GetTypeId() != TYPEID_PLAYER)
                            return;

                        switch (target->getRace())
                        {
                            // Blood Elf
                            case RACE_BLOODELF:
                                target->SetDisplayId(target->getGender() == GENDER_FEMALE ? 17830 : 17829);
                                break;
                            // Orc
                            case RACE_ORC:
                                target->SetDisplayId(target->getGender() == GENDER_FEMALE ? 10140 : 10139);
                                break;
                            // Troll
                            case RACE_TROLL:
                                target->SetDisplayId(target->getGender() == GENDER_FEMALE ? 10134 : 10135);
                                break;
                            // Tauren
                            case RACE_TAUREN:
                                target->SetDisplayId(target->getGender() == GENDER_FEMALE ? 10147 : 10136);
                                break;
                            // Undead
                            case RACE_UNDEAD_PLAYER:
                                target->SetDisplayId(target->getGender() == GENDER_FEMALE ? 10145 : 10146);
                                break;
                            // Draenei
                            case RACE_DRAENEI:
                                target->SetDisplayId(target->getGender() == GENDER_FEMALE ? 17828 : 17827);
                                break;
                            // Dwarf
                            case RACE_DWARF:
                                target->SetDisplayId(target->getGender() == GENDER_FEMALE ? 10142 : 10141);
                                break;
                            // Gnome
                            case RACE_GNOME:
                                target->SetDisplayId(target->getGender() == GENDER_FEMALE ? 10149 : 10148);
                                break;
                            // Human
                            case RACE_HUMAN:
                                target->SetDisplayId(target->getGender() == GENDER_FEMALE ? 10138 : 10137);
                                break;
                            // Night Elf
                            case RACE_NIGHTELF:
                                target->SetDisplayId(target->getGender() == GENDER_FEMALE ? 10144 : 10143);
                                break;
                            default:
                                break;
                        }
                        break;
                    }
                    // Murloc costume
                    case 42365:
                        target->SetDisplayId(21723);
                        break;
                    // Dread Corsair
                    case 50517:
                    // Corsair Costume
                    case 51926:
                    {
                        if (target->GetTypeId() != TYPEID_PLAYER)
                            return;

                        switch (target->getRace())
                        {
                            // Blood Elf
                            case RACE_BLOODELF:
                                target->SetDisplayId(target->getGender() == GENDER_FEMALE ? 25043 : 25032);
                                break;
                            // Orc
                            case RACE_ORC:
                                target->SetDisplayId(target->getGender() == GENDER_FEMALE ? 25050 : 25039);
                                break;
                            // Troll
                            case RACE_TROLL:
                                target->SetDisplayId(target->getGender() == GENDER_FEMALE ? 25052 : 25041);
                                break;
                            // Tauren
                            case RACE_TAUREN:
                                target->SetDisplayId(target->getGender() == GENDER_FEMALE ? 25051 : 25040);
                                break;
                            // Undead
                            case RACE_UNDEAD_PLAYER:
                                target->SetDisplayId(target->getGender() == GENDER_FEMALE ? 25053 : 25042);
                                break;
                            // Draenei
                            case RACE_DRAENEI:
                                target->SetDisplayId(target->getGender() == GENDER_FEMALE ? 25044 : 25033);
                                break;
                            // Dwarf
                            case RACE_DWARF:
                                target->SetDisplayId(target->getGender() == GENDER_FEMALE ? 25045 : 25034);
                                break;
                            // Gnome
                            case RACE_GNOME:
                                target->SetDisplayId(target->getGender() == GENDER_FEMALE ? 25035 : 25046);
                                break;
                            // Human
                            case RACE_HUMAN:
                                target->SetDisplayId(target->getGender() == GENDER_FEMALE ? 25037 : 25048);
                                break;
                            // Night Elf
                            case RACE_NIGHTELF:
                                target->SetDisplayId(target->getGender() == GENDER_FEMALE ? 25038 : 25049);
                                break;
                            default:
                                break;
                        }
                        break;
                    }
                    // Pygmy Oil
                    case 53806:
                        target->SetDisplayId(22512);
                        break;
                    // Honor the Dead
                    case 65386:
                    case 65495:
                        target->SetDisplayId(target->getGender() == GENDER_MALE ? 29203 : 29204);
                        break;
                    // Darkspear Pride
                    case 75532:
                        target->SetDisplayId(target->getGender() == GENDER_MALE ? 31737 : 31738);
                        break;
                    // Gnomeregan Pride
                    case 75531:
                        target->SetDisplayId(target->getGender() == GENDER_MALE ? 31654 : 31655);
                        break;
                    default:
                        break;
                }
            }
            else
            {
                CreatureTemplate const* ci = sObjectMgr->GetCreatureTemplate(GetMiscValue());
                if (!ci)
                {
                    target->SetDisplayId(16358);              // pig pink ^_^
                    TC_LOG_ERROR("spells", "Auras: unknown creature id = %d (only need its modelid) From Spell Aura Transform in Spell ID = %d", GetMiscValue(), GetId());
                }
                else
                {
                    uint32 model_id = 0;

                    // choose a model, based on trigger flag
                    if (uint32 modelid = ObjectMgr::ChooseDisplayId(ci)->CreatureDisplayID)
                        model_id = modelid;

                    target->SetDisplayId(model_id);

                    // Dragonmaw Illusion (set mount model also)
                    if (GetId() == 42016 && target->GetMountDisplayId() && !target->GetAuraEffectsByType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED).empty())
                        target->SetMountDisplayId(16314);
                }
            }
        }

        // polymorph case
        if ((mode & AURA_EFFECT_HANDLE_REAL) && target->GetTypeId() == TYPEID_PLAYER && target->IsPolymorphed())
        {
            // for players, start regeneration after 1s (in polymorph fast regeneration case)
            // only if caster is Player (after patch 2.4.2)
            if (GetCasterGUID().IsPlayer())
                target->ToPlayer()->setRegenTimerCount(1*IN_MILLISECONDS);

            //dismount polymorphed target (after patch 2.4.2)
            if (target->IsMounted())
                target->RemoveAurasByType(SPELL_AURA_MOUNTED);
        }
    }
    else
    {
        // HandleEffect(this, AURA_EFFECT_HANDLE_SEND_FOR_CLIENT, true) will reapply it if need
        if (target->GetTransformSpell() == GetId())
            target->SetTransformSpell(0);

        target->RestoreDisplayId(target->IsMounted());

        // Dragonmaw Illusion (restore mount model)
        if (GetId() == 42016 && target->GetMountDisplayId() == 16314)
        {
            if (!target->GetAuraEffectsByType(SPELL_AURA_MOUNTED).empty())
            {
                uint32 cr_id = target->GetAuraEffectsByType(SPELL_AURA_MOUNTED).front()->GetMiscValue();
                if (CreatureTemplate const* ci = sObjectMgr->GetCreatureTemplate(cr_id))
                {
                    CreatureModel model = *ObjectMgr::ChooseDisplayId(ci);
                    sObjectMgr->GetCreatureModelRandomGender(&model, ci);

                    target->SetMountDisplayId(model.CreatureDisplayID);
                }
            }
        }
    }
}

void AuraEffect::HandleAuraModScale(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_CHANGE_AMOUNT_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    float scale = target->GetObjectScale();
    scale += CalculatePct(1.0f, apply ? GetAmount() : -GetAmount());
    target->SetObjectScale(scale);
}

void AuraEffect::HandleAuraCloneCaster(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    if (apply)
    {
        Unit* caster = GetCaster();
        if (!caster || caster == target)
            return;

        // What must be cloned? at least display and scale
        target->SetDisplayId(caster->GetDisplayId());
        //target->SetObjectScale(caster->GetObjectScale()); // we need retail info about how scaling is handled (aura maybe?)
        target->AddUnitFlag2(UNIT_FLAG2_MIRROR_IMAGE);
    }
    else
    {
        target->SetDisplayId(target->GetNativeDisplayId());
        target->RemoveUnitFlag2(UNIT_FLAG2_MIRROR_IMAGE);
    }
}

/************************/
/***      FIGHT       ***/
/************************/

void AuraEffect::HandleFeignDeath(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    if (apply)
    {
        UnitList targets;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(target, target, target->GetMap()->GetVisibilityRange());
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(target, targets, u_check);
        Cell::VisitAllObjects(target, searcher, target->GetMap()->GetVisibilityRange());
        for (UnitList::iterator iter = targets.begin(); iter != targets.end(); ++iter)
        {
            if (!(*iter)->HasUnitState(UNIT_STATE_CASTING))
                continue;

            for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; i++)
            {
                if ((*iter)->GetCurrentSpell(i)
                && (*iter)->GetCurrentSpell(i)->m_targets.GetUnitTargetGUID() == target->GetGUID())
                {
                    (*iter)->InterruptSpell(CurrentSpellTypes(i), false);
                }
            }
        }

        if (target->GetMap()->IsDungeon()) // feign death does not remove combat in dungeons
        {
            target->AttackStop();
            if (Player* targetPlayer = target->ToPlayer())
                targetPlayer->SendAttackSwingCancelAttack();
        }
        else
            target->CombatStop(false, false);

        // prevent interrupt message
        if (GetCasterGUID() == target->GetGUID() && target->GetCurrentSpell(CURRENT_GENERIC_SPELL))
            target->FinishSpell(CURRENT_GENERIC_SPELL, false);
        target->InterruptNonMeleeSpells(true);

        // stop handling the effect if it was removed by linked event
        if (aurApp->GetRemoveMode())
            return;

        target->AddUnitFlag(UNIT_FLAG_UNK_29);
        target->AddUnitFlag2(UNIT_FLAG2_FEIGN_DEATH);
        target->AddDynamicFlag(UNIT_DYNFLAG_DEAD);
        target->AddUnitState(UNIT_STATE_DIED);

        if (Creature* creature = target->ToCreature())
            creature->SetReactState(REACT_PASSIVE);
    }
    else
    {
        target->RemoveUnitFlag(UNIT_FLAG_UNK_29);
        target->RemoveUnitFlag2(UNIT_FLAG2_FEIGN_DEATH);
        target->RemoveDynamicFlag(UNIT_DYNFLAG_DEAD);
        target->ClearUnitState(UNIT_STATE_DIED);

        if (Creature* creature = target->ToCreature())
            creature->InitializeReactState();
    }
}

void AuraEffect::HandleModUnattackable(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    // do not remove unit flag if there are more than this auraEffect of that kind on unit on unit
    if (!apply && target->HasAuraType(SPELL_AURA_MOD_UNATTACKABLE))
        return;

    if (apply)
        target->AddUnitFlag(UNIT_FLAG_NON_ATTACKABLE_2);
    else
        target->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE_2);

    // call functions which may have additional effects after chainging state of unit
    if (apply && (mode & AURA_EFFECT_HANDLE_REAL))
    {
        if (target->GetMap()->IsDungeon())
        {
            target->AttackStop();
            if (Player* targetPlayer = target->ToPlayer())
                targetPlayer->SendAttackSwingCancelAttack();
        }
        else
            target->CombatStop();
    }
}

void AuraEffect::HandleAuraModDisarm(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    //Prevent handling aura twice
    AuraType type = GetAuraType();
    if (apply ? target->GetAuraEffectsByType(type).size() > 1 : target->HasAuraType(type))
        return;

    void(*flagChangeFunc)(Unit* u) = nullptr;

    uint32 slot;
    WeaponAttackType attType;
    switch (type)
    {
        case SPELL_AURA_MOD_DISARM:
            if (apply)
                flagChangeFunc = [](Unit* u) { u->AddUnitFlag(UNIT_FLAG_DISARMED); };
            else
                flagChangeFunc = [](Unit* u) { u->RemoveUnitFlag(UNIT_FLAG_DISARMED); };
            slot = EQUIPMENT_SLOT_MAINHAND;
            attType = BASE_ATTACK;
            break;
        case SPELL_AURA_MOD_DISARM_OFFHAND:
            if (apply)
                flagChangeFunc = [](Unit* u) { u->AddUnitFlag2(UNIT_FLAG2_DISARM_OFFHAND); };
            else
                flagChangeFunc = [](Unit* u) { u->RemoveUnitFlag2(UNIT_FLAG2_DISARM_OFFHAND); };
            slot = EQUIPMENT_SLOT_OFFHAND;
            attType = OFF_ATTACK;
            break;
        case SPELL_AURA_MOD_DISARM_RANGED:
            if (apply)
                flagChangeFunc = [](Unit* u) { u->AddUnitFlag2(UNIT_FLAG2_DISARM_RANGED); };
            else
                flagChangeFunc = [](Unit* u) { u->RemoveUnitFlag2(UNIT_FLAG2_DISARM_RANGED); };
            slot = EQUIPMENT_SLOT_MAINHAND;
            attType = RANGED_ATTACK;
            break;
        default:
            return;
    }

    // set/remove flag before weapon bonuses so it's properly reflected in CanUseAttackType
    if (flagChangeFunc)
        flagChangeFunc(target);

    // Handle damage modification, shapeshifted druids are not affected
    if (target->GetTypeId() == TYPEID_PLAYER && !target->IsInFeralForm())
    {
        Player* player = target->ToPlayer();
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            WeaponAttackType const attackType = Player::GetAttackBySlot(slot, item->GetTemplate()->GetInventoryType());

            player->ApplyItemDependentAuras(item, !apply);
            if (attackType != MAX_ATTACK)
            {
                player->_ApplyWeaponDamage(slot, item, !apply);
                if (!apply) // apply case already handled on item dependent aura removal (if any)
                    player->UpdateWeaponDependentAuras(attackType);
            }
        }
    }

    if (target->GetTypeId() == TYPEID_UNIT && target->ToCreature()->GetCurrentEquipmentId())
        target->UpdateDamagePhysical(attType);
}

void AuraEffect::HandleAuraModSilence(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    if (apply)
    {
        target->AddUnitFlag(UNIT_FLAG_SILENCED);

        // call functions which may have additional effects after chainging state of unit
        // Stop cast only spells vs PreventionType & SPELL_PREVENTION_TYPE_SILENCE
        for (uint32 i = CURRENT_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
            if (Spell* spell = target->GetCurrentSpell(CurrentSpellTypes(i)))
                if (spell->m_spellInfo->PreventionType & SPELL_PREVENTION_TYPE_SILENCE)
                    // Stop spells on prepare or casting state
                    target->InterruptSpell(CurrentSpellTypes(i), false);
    }
    else
    {
        // do not remove unit flag if there are more than this auraEffect of that kind on unit on unit
        if (target->HasAuraType(SPELL_AURA_MOD_SILENCE) || target->HasAuraType(SPELL_AURA_MOD_PACIFY_SILENCE))
            return;

        target->RemoveUnitFlag(UNIT_FLAG_SILENCED);
    }
}

void AuraEffect::HandleAuraModPacify(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    if (apply)
    {
        target->AddUnitFlag(UNIT_FLAG_PACIFIED);
        target->AttackStop();
    }
    else
    {
        // do not remove unit flag if there are more than this auraEffect of that kind on unit on unit
        if (target->HasAuraType(SPELL_AURA_MOD_PACIFY) || target->HasAuraType(SPELL_AURA_MOD_PACIFY_SILENCE))
            return;
        target->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
    }
}

void AuraEffect::HandleAuraModPacifyAndSilence(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    // Vengeance of the Blue Flight (@todo REMOVE THIS!)
    /// @workaround
    if (m_spellInfo->Id == 45839)
    {
        if (apply)
            target->AddUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
        else
            target->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
    }
    if (!(apply))
    {
        // do not remove unit flag if there are more than this auraEffect of that kind on unit on unit
        if (target->HasAuraType(SPELL_AURA_MOD_PACIFY_SILENCE))
            return;
    }
    HandleAuraModPacify(aurApp, mode, apply);
    HandleAuraModSilence(aurApp, mode, apply);
}

void AuraEffect::HandleAuraDisableAttackingExceptAbilities(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    if (apply)
        aurApp->GetTarget()->AttackStop();
}

void AuraEffect::HandleAuraModNoActions(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    if (apply)
    {
        target->AddUnitFlag2(UNIT_FLAG2_NO_ACTIONS);

        // call functions which may have additional effects after chainging state of unit
        // Stop cast only spells vs PreventionType & SPELL_PREVENTION_TYPE_SILENCE
        for (uint32 i = CURRENT_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
            if (Spell* spell = target->GetCurrentSpell(CurrentSpellTypes(i)))
                if (spell->m_spellInfo->PreventionType & SPELL_PREVENTION_TYPE_NO_ACTIONS)
                    // Stop spells on prepare or casting state
                    target->InterruptSpell(CurrentSpellTypes(i), false);
    }
    else
    {
        // do not remove unit flag if there are more than this auraEffect of that kind on unit on unit
        if (target->HasAuraType(SPELL_AURA_MOD_NO_ACTIONS))
            return;

        target->RemoveUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
    }
}

/****************************/
/***      TRACKING        ***/
/****************************/

void AuraEffect::HandleAuraTrackCreatures(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Player* target = aurApp->GetTarget()->ToPlayer();
    if (!target)
        return;

    if (apply)
        target->AddTrackCreatureFlag(uint32(1) << (GetMiscValue() - 1));
    else
        target->RemoveTrackCreatureFlag(uint32(1) << (GetMiscValue() - 1));
}

void AuraEffect::HandleAuraTrackResources(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Player* target = aurApp->GetTarget()->ToPlayer();
    if (!target)
        return;

    uint32 bitIndex = GetMiscValue() - 1;
    uint32 index = bitIndex / 32;
    uint32 flag = 1 << (bitIndex % 32);
    if (apply)
        target->AddTrackResourceFlag(index, flag);
    else
        target->RemoveTrackResourceFlag(index, flag);
}

void AuraEffect::HandleAuraTrackStealthed(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Player* target = aurApp->GetTarget()->ToPlayer();
    if (!target)
        return;

    if (!(apply))
    {
        // do not remove unit flag if there are more than this auraEffect of that kind on unit on unit
        if (target->HasAuraType(GetAuraType()))
            return;
    }
    if (apply)
        target->AddPlayerLocalFlag(PLAYER_LOCAL_FLAG_TRACK_STEALTHED);
    else
        target->RemovePlayerLocalFlag(PLAYER_LOCAL_FLAG_TRACK_STEALTHED);
}

void AuraEffect::HandleAuraModStalked(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    // used by spells: Hunter's Mark, Mind Vision, Syndicate Tracker (MURP) DND
    if (apply)
        target->AddDynamicFlag(UNIT_DYNFLAG_TRACK_UNIT);
    else
    {
        // do not remove unit flag if there are more than this auraEffect of that kind on unit on unit
        if (!target->HasAuraType(GetAuraType()))
            target->RemoveDynamicFlag(UNIT_DYNFLAG_TRACK_UNIT);
    }

    // call functions which may have additional effects after chainging state of unit
    target->UpdateObjectVisibility();
}

void AuraEffect::HandleAuraUntrackable(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    if (apply)
        target->AddVisFlags(UNIT_VIS_FLAGS_UNTRACKABLE);
    else
    {
        // do not remove unit flag if there are more than this auraEffect of that kind on unit on unit
        if (target->HasAuraType(GetAuraType()))
            return;
        target->RemoveVisFlags(UNIT_VIS_FLAGS_UNTRACKABLE);
    }
}

/****************************/
/***  SKILLS & TALENTS    ***/
/****************************/

void AuraEffect::HandleAuraModSkill(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_SKILL)))
        return;

    Player* target = aurApp->GetTarget()->ToPlayer();
    if (!target)
        return;

    uint32 prot = GetMiscValue();
    int32 points = GetAmount();

    if (prot == SKILL_DEFENSE)
        return;

    target->ModifySkillBonus(prot, (apply ? points : -points), GetAuraType() == SPELL_AURA_MOD_SKILL_TALENT);
}

void AuraEffect::HandleAuraAllowTalentSwapping(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_REAL)))
        return;

    Player* target = aurApp->GetTarget()->ToPlayer();
    if (!target)
        return;

    if (apply)
        target->AddUnitFlag2(UNIT_FLAG2_ALLOW_CHANGING_TALENTS);
    else if (!target->HasAuraType(GetAuraType()))
        target->RemoveUnitFlag2(UNIT_FLAG2_ALLOW_CHANGING_TALENTS);
}

/****************************/
/***       MOVEMENT       ***/
/****************************/

void AuraEffect::HandleAuraMounted(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    if (apply)
    {
        uint32 creatureEntry = GetMiscValue();
        uint32 displayId = 0;
        uint32 vehicleId = 0;

        if (MountEntry const* mountEntry = sDB2Manager.GetMount(GetId()))
        {
            if (DB2Manager::MountXDisplayContainer const* mountDisplays = sDB2Manager.GetMountDisplays(mountEntry->ID))
            {
                if (mountEntry->IsSelfMount())
                {
                    displayId = DISPLAYID_HIDDEN_MOUNT;
                }
                else
                {
                    DB2Manager::MountXDisplayContainer usableDisplays;
                    std::copy_if(mountDisplays->begin(), mountDisplays->end(), std::back_inserter(usableDisplays), [target](MountXDisplayEntry const* mountDisplay)
                    {
                        if (Player* playerTarget = target->ToPlayer())
                            if (PlayerConditionEntry const* playerCondition = sPlayerConditionStore.LookupEntry(mountDisplay->PlayerConditionID))
                                return sConditionMgr->IsPlayerMeetingCondition(playerTarget, playerCondition);

                        return true;
                    });

                    if (!usableDisplays.empty())
                        displayId = Trinity::Containers::SelectRandomContainerElement(usableDisplays)->CreatureDisplayInfoID;
                }
            }
            // TODO: CREATE TABLE mount_vehicle (mountId, vehicleCreatureId) for future mounts that are vehicles (new mounts no longer have proper data in MiscValue)
            //if (MountVehicle const* mountVehicle = sObjectMgr->GetMountVehicle(mountEntry->Id))
            //    creatureEntry = mountVehicle->VehicleCreatureId;
        }

        if (CreatureTemplate const* creatureInfo = sObjectMgr->GetCreatureTemplate(creatureEntry))
        {
            vehicleId = creatureInfo->VehicleId;

            if (!displayId)
            {
                CreatureModel model = *ObjectMgr::ChooseDisplayId(creatureInfo);
                sObjectMgr->GetCreatureModelRandomGender(&model, creatureInfo);
                displayId = model.CreatureDisplayID;
            }

            //some spell has one aura of mount and one of vehicle
            for (SpellEffectInfo const& effect : GetSpellInfo()->GetEffects())
                if (effect.IsEffect(SPELL_EFFECT_SUMMON) && effect.MiscValue == GetMiscValue())
                    displayId = 0;
        }

        target->Mount(displayId, vehicleId, creatureEntry);

        // cast speed aura
        if (mode & AURA_EFFECT_HANDLE_REAL)
            if (MountCapabilityEntry const* mountCapability = sMountCapabilityStore.LookupEntry(GetAmount()))
                target->CastSpell(target, mountCapability->ModSpellAuraID, this);
    }
    else
    {
        target->Dismount();
        //some mounts like Headless Horseman's Mount or broom stick are skill based spell
        // need to remove ALL arura related to mounts, this will stop client crash with broom stick
        // and never endless flying after using Headless Horseman's Mount
        if (mode & AURA_EFFECT_HANDLE_REAL)
        {
            target->RemoveAurasByType(SPELL_AURA_MOUNTED);

            // remove speed aura
            if (MountCapabilityEntry const* mountCapability = sMountCapabilityStore.LookupEntry(GetAmount()))
                target->RemoveAurasDueToSpell(mountCapability->ModSpellAuraID, target->GetGUID());
        }
    }
}

void AuraEffect::HandleAuraAllowFlight(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    if (!apply)
    {
        // do not remove unit flag if there are more than this auraEffect of that kind on unit on unit
        if (target->HasAuraType(GetAuraType()) || target->HasAuraType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED))
            return;
    }

    target->SetCanTransitionBetweenSwimAndFly(apply);

    if (target->SetCanFly(apply))
    {
        if (!apply && !target->IsLevitating())
            target->GetMotionMaster()->MoveFall();
    }
}

void AuraEffect::HandleAuraWaterWalk(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    if (!apply)
    {
        // do not remove unit flag if there are more than this auraEffect of that kind on unit on unit
        if (target->HasAuraType(GetAuraType()))
            return;
    }

    target->SetWaterWalking(apply);
}

void AuraEffect::HandleAuraFeatherFall(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    if (!apply)
    {
        // do not remove unit flag if there are more than this auraEffect of that kind on unit on unit
        if (target->HasAuraType(GetAuraType()))
            return;
    }

    target->SetFeatherFall(apply);

    // start fall from current height
    if (!apply && target->GetTypeId() == TYPEID_PLAYER)
        target->ToPlayer()->SetFallInformation(0, target->GetPositionZ());
}

void AuraEffect::HandleAuraHover(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    if (!apply)
    {
        // do not remove unit flag if there are more than this auraEffect of that kind on unit on unit
        if (target->HasAuraType(GetAuraType()))
            return;
    }

    target->SetHover(apply);    //! Sets movementflags
}

void AuraEffect::HandleWaterBreathing(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    // update timers in client
    if (target->GetTypeId() == TYPEID_PLAYER)
        target->ToPlayer()->UpdateMirrorTimers();
}

void AuraEffect::HandleForceMoveForward(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    if (apply)
        target->AddUnitFlag2(UNIT_FLAG2_FORCE_MOVEMENT);
    else
    {
        // do not remove unit flag if there are more than this auraEffect of that kind on unit on unit
        if (target->HasAuraType(GetAuraType()))
            return;
        target->RemoveUnitFlag2(UNIT_FLAG2_FORCE_MOVEMENT);
    }
}

void AuraEffect::HandleAuraCanTurnWhileFalling(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    if (!apply)
    {
        // do not remove unit flag if there are more than this auraEffect of that kind on unit on unit
        if (target->HasAuraType(GetAuraType()))
            return;
    }

    target->SetCanTurnWhileFalling(apply);
}

void AuraEffect::HandleIgnoreMovementForces(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    if (!apply)
    {
        // do not remove unit flag if there are more than this auraEffect of that kind on unit on unit
        if (target->HasAuraType(GetAuraType()))
            return;
    }

    target->SetIgnoreMovementForces(apply);
}

/****************************/
/***        THREAT        ***/
/****************************/

void AuraEffect::HandleModThreat(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK))
        return;

    aurApp->GetTarget()->GetThreatManager().UpdateMySpellSchoolModifiers();
}

void AuraEffect::HandleAuraModTotalThreat(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    if (!target->IsAlive() || target->GetTypeId() != TYPEID_PLAYER)
        return;

    Unit* caster = GetCaster();
    if (caster && caster->IsAlive())
        caster->GetThreatManager().UpdateMyTempModifiers();
}

void AuraEffect::HandleModTaunt(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    if (!target->IsAlive() || !target->CanHaveThreatList())
        return;

    target->GetThreatManager().TauntUpdate();
}

void AuraEffect::HandleModDetaunt(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* caster = GetCaster();
    Unit* target = aurApp->GetTarget();

    if (!caster || !caster->IsAlive() || !target->IsAlive() || !caster->CanHaveThreatList())
        return;

    caster->GetThreatManager().TauntUpdate();
}

/*****************************/
/***        CONTROL        ***/
/*****************************/

void AuraEffect::HandleModConfuse(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    target->SetControlled(apply, UNIT_STATE_CONFUSED);
    if (apply)
        target->GetThreatManager().EvaluateSuppressed();
}

void AuraEffect::HandleModFear(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    target->SetControlled(apply, UNIT_STATE_FLEEING);
}

void AuraEffect::HandleAuraModStun(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    target->SetControlled(apply, UNIT_STATE_STUNNED);
    if (apply)
        target->GetThreatManager().EvaluateSuppressed();
}

void AuraEffect::HandleAuraModRoot(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    target->SetControlled(apply, UNIT_STATE_ROOT);
}

void AuraEffect::HandlePreventFleeing(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    // Since patch 3.0.2 this mechanic no longer affects fear effects. It will ONLY prevent humanoids from fleeing due to low health.
    if (!apply || target->HasAuraType(SPELL_AURA_MOD_FEAR))
        return;
    /// TODO: find a way to cancel fleeing for assistance.
    /// Currently this will only stop creatures fleeing due to low health that could not find nearby allies to flee towards.
    target->SetControlled(false, UNIT_STATE_FLEEING);
}

/***************************/
/***        CHARM        ***/
/***************************/

void AuraEffect::HandleModPossess(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    Unit* caster = GetCaster();

    // no support for posession AI yet
    if (caster && caster->GetTypeId() == TYPEID_UNIT)
    {
        HandleModCharm(aurApp, mode, apply);
        return;
    }

    if (apply)
        target->SetCharmedBy(caster, CHARM_TYPE_POSSESS, aurApp);
    else
        target->RemoveCharmedBy(caster);
}

void AuraEffect::HandleModPossessPet(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* caster = GetCaster();
    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
        return;

    // seems it may happen that when removing it is no longer owner's pet
    //if (caster->ToPlayer()->GetPet() != target)
    //    return;

    Unit* target = aurApp->GetTarget();
    if (target->GetTypeId() != TYPEID_UNIT || !target->IsPet())
        return;

    Pet* pet = target->ToPet();
    if (apply)
    {
        if (caster->ToPlayer()->GetPet() != pet)
            return;

        pet->SetCharmedBy(caster, CHARM_TYPE_POSSESS, aurApp);
    }
    else
    {
        pet->RemoveCharmedBy(caster);

        if (!pet->IsWithinDistInMap(caster, pet->GetMap()->GetVisibilityRange()))
            pet->Remove(PET_SAVE_NOT_IN_SLOT, true);
        else
        {
            // Reinitialize the pet bar or it will appear greyed out
            caster->ToPlayer()->PetSpellInitialize();

            // TODO: remove this
            if (!pet->GetVictim() && !pet->GetCharmInfo()->HasCommandState(COMMAND_STAY))
                pet->GetMotionMaster()->MoveFollow(caster, PET_FOLLOW_DIST, pet->GetFollowAngle());
        }
    }
}

void AuraEffect::HandleModCharm(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    Unit* caster = GetCaster();

    if (apply)
        target->SetCharmedBy(caster, CHARM_TYPE_CHARM, aurApp);
    else
        target->RemoveCharmedBy(caster);
}

void AuraEffect::HandleCharmConvert(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    Unit* caster = GetCaster();

    if (apply)
        target->SetCharmedBy(caster, CHARM_TYPE_CONVERT, aurApp);
    else
        target->RemoveCharmedBy(caster);
}

/**
 * Such auras are applied from a caster(=player) to a vehicle.
 * This has been verified using spell #49256
 */
void AuraEffect::HandleAuraControlVehicle(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK))
        return;

    Unit* target = aurApp->GetTarget();
    if (!target->IsVehicle())
        return;

    Unit* caster = GetCaster();
    if (!caster || caster == target)
        return;

    if (apply)
    {
        // Currently spells that have base points  0 and DieSides 0 = "0/0" exception are pushed to -1,
        // however the idea of 0/0 is to ingore flag VEHICLE_SEAT_FLAG_CAN_ENTER_OR_EXIT and -1 checks for it,
        // so this break such spells or most of them.
        // Current formula about m_amount: effect base points + dieside - 1
        // TO DO: Reasearch more about 0/0 and fix it.
        caster->_EnterVehicle(target->GetVehicleKit(), GetAmount() - 1, aurApp);
    }
    else
    {
        // Remove pending passengers before exiting vehicle - might cause an Uninstall
        target->GetVehicleKit()->RemovePendingEventsForPassenger(caster);

        if (GetId() == 53111) // Devour Humanoid
        {
            Unit::Kill(target, caster);
            if (caster->GetTypeId() == TYPEID_UNIT)
                caster->ToCreature()->DespawnOrUnsummon();
        }

        bool seatChange = (mode & AURA_EFFECT_HANDLE_CHANGE_AMOUNT)                             // Seat change on the same direct vehicle
            || target->HasAuraTypeWithCaster(SPELL_AURA_CONTROL_VEHICLE, caster->GetGUID());    // Seat change to a proxy vehicle (for example turret mounted on a siege engine)

        if (!seatChange)
            caster->_ExitVehicle();
        else
            target->GetVehicleKit()->RemovePassenger(caster);  // Only remove passenger from vehicle without launching exit movement or despawning the vehicle

        // some SPELL_AURA_CONTROL_VEHICLE auras have a dummy effect on the player - remove them
        caster->RemoveAurasDueToSpell(GetId());
    }
}

/*********************************************************/
/***                  MODIFY SPEED                     ***/
/*********************************************************/
void AuraEffect::HandleAuraModIncreaseSpeed(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    target->UpdateSpeed(MOVE_RUN);
}

void AuraEffect::HandleAuraModIncreaseMountedSpeed(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    HandleAuraModIncreaseSpeed(aurApp, mode, apply);
}

void AuraEffect::HandleAuraModIncreaseFlightSpeed(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_CHANGE_AMOUNT_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();
    if (mode & AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK)
        target->UpdateSpeed(MOVE_FLIGHT);

    //! Update ability to fly
    if (GetAuraType() == SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED)
    {
        // do not remove unit flag if there are more than this auraEffect of that kind on unit on unit
        if (mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK && (apply || (!target->HasAuraType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED) && !target->HasAuraType(SPELL_AURA_FLY))))
        {
            target->SetCanTransitionBetweenSwimAndFly(apply);

            if (target->SetCanFly(apply))
                if (!apply && !target->IsLevitating())
                    target->GetMotionMaster()->MoveFall();
        }

        //! Someone should clean up these hacks and remove it from this function. It doesn't even belong here.
        if (mode & AURA_EFFECT_HANDLE_REAL)
        {
            //Players on flying mounts must be immune to polymorph
            if (target->GetTypeId() == TYPEID_PLAYER)
                target->ApplySpellImmune(GetId(), IMMUNITY_MECHANIC, MECHANIC_POLYMORPH, apply);

            // Dragonmaw Illusion (overwrite mount model, mounted aura already applied)
            if (apply && target->HasAuraEffect(42016, 0) && target->GetMountDisplayId())
                target->SetMountDisplayId(16314);
        }
    }
}

void AuraEffect::HandleAuraModIncreaseSwimSpeed(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    target->UpdateSpeed(MOVE_SWIM);
}

void AuraEffect::HandleAuraModDecreaseSpeed(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    target->UpdateSpeed(MOVE_RUN);
    target->UpdateSpeed(MOVE_SWIM);
    target->UpdateSpeed(MOVE_FLIGHT);
    target->UpdateSpeed(MOVE_RUN_BACK);
    target->UpdateSpeed(MOVE_SWIM_BACK);
    target->UpdateSpeed(MOVE_FLIGHT_BACK);
}

void AuraEffect::HandleAuraModUseNormalSpeed(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    target->UpdateSpeed(MOVE_RUN);
    target->UpdateSpeed(MOVE_SWIM);
    target->UpdateSpeed(MOVE_FLIGHT);
}

void AuraEffect::HandleAuraModMinimumSpeedRate(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    target->UpdateSpeed(MOVE_RUN);
}

void AuraEffect::HandleModMovementForceMagnitude(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK))
        return;

    aurApp->GetTarget()->UpdateMovementForcesModMagnitude();
}

/*********************************************************/
/***                     IMMUNITY                      ***/
/*********************************************************/

void AuraEffect::HandleModMechanicImmunityMask(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();
    m_spellInfo->ApplyAllSpellImmunitiesTo(target, GetSpellEffectInfo(), apply);
}

void AuraEffect::HandleModMechanicImmunity(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();
    m_spellInfo->ApplyAllSpellImmunitiesTo(target, GetSpellEffectInfo(), apply);
}

void AuraEffect::HandleAuraModEffectImmunity(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();
    m_spellInfo->ApplyAllSpellImmunitiesTo(target, GetSpellEffectInfo(), apply);

    // when removing flag aura, handle flag drop
    // TODO: this should be handled in aura script for flag spells using AfterEffectRemove hook
    Player* player = target->ToPlayer();
    if (!apply && player && GetSpellInfo()->HasAuraInterruptFlag(SpellAuraInterruptFlags::StealthOrInvis))
    {
        if (player->InBattleground())
        {
            if (Battleground* bg = player->GetBattleground())
                bg->EventPlayerDroppedFlag(player);
        }
        else
            sOutdoorPvPMgr->HandleDropFlag(player, GetSpellInfo()->Id);
    }
}

void AuraEffect::HandleAuraModStateImmunity(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();
    m_spellInfo->ApplyAllSpellImmunitiesTo(target, GetSpellEffectInfo(), apply);
}

void AuraEffect::HandleAuraModSchoolImmunity(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();
    m_spellInfo->ApplyAllSpellImmunitiesTo(target, GetSpellEffectInfo(), apply);

    if (GetSpellInfo()->Mechanic == MECHANIC_BANISH)
    {
        if (apply)
            target->AddUnitState(UNIT_STATE_ISOLATED);
        else
        {
            bool banishFound = false;
            Unit::AuraEffectList const& banishAuras = target->GetAuraEffectsByType(GetAuraType());
            for (AuraEffect const* aurEff : banishAuras)
            {
                if (aurEff->GetSpellInfo()->Mechanic == MECHANIC_BANISH)
                {
                    banishFound = true;
                    break;
                }
            }

            if (!banishFound)
                target->ClearUnitState(UNIT_STATE_ISOLATED);
        }
    }

    // TODO: should be changed to a proc script on flag spell (they have "Taken positive" proc flags in db2)
    {
        if (apply && GetMiscValue() == SPELL_SCHOOL_MASK_NORMAL)
            target->RemoveAurasWithInterruptFlags(SpellAuraInterruptFlags::StealthOrInvis);

        // remove all flag auras (they are positive, but they must be removed when you are immune)
        if (GetSpellInfo()->HasAttribute(SPELL_ATTR1_DISPEL_AURAS_ON_IMMUNITY)
            && GetSpellInfo()->HasAttribute(SPELL_ATTR2_DAMAGE_REDUCED_SHIELD))
            target->RemoveAurasWithInterruptFlags(SpellAuraInterruptFlags::StealthOrInvis);
    }

    if (apply)
        target->GetThreatManager().EvaluateSuppressed();
}

void AuraEffect::HandleAuraModDmgImmunity(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();
    m_spellInfo->ApplyAllSpellImmunitiesTo(target, GetSpellEffectInfo(), apply);

    if (apply)
        target->GetThreatManager().EvaluateSuppressed();
}

void AuraEffect::HandleAuraModDispelImmunity(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();
    m_spellInfo->ApplyAllSpellImmunitiesTo(target, GetSpellEffectInfo(), apply);
}

/*********************************************************/
/***                  MODIFY STATS                     ***/
/*********************************************************/

/********************************/
/***        RESISTANCE        ***/
/********************************/

void AuraEffect::HandleAuraModResistance(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    for (uint8 x = SPELL_SCHOOL_NORMAL; x < MAX_SPELL_SCHOOL; ++x)
        if (GetMiscValue() & (1 << x))
            target->HandleStatFlatModifier(UnitMods(UNIT_MOD_RESISTANCE_START + x), TOTAL_VALUE, float(GetAmount()), apply);
}

void AuraEffect::HandleAuraModBaseResistancePCT(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    // only players have base stats
    if (target->GetTypeId() != TYPEID_PLAYER)
    {
        //pets only have base armor
        if (target->IsPet() && (GetMiscValue() & SPELL_SCHOOL_MASK_NORMAL))
        {
            if (apply)
                target->ApplyStatPctModifier(UNIT_MOD_ARMOR, BASE_PCT, float(GetAmount()));
            else
            {
                float amount = target->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_BASE_RESISTANCE_PCT, SPELL_SCHOOL_MASK_NORMAL);
                target->SetStatPctModifier(UNIT_MOD_ARMOR, BASE_PCT, amount);
            }
        }
    }
    else
    {
        for (uint8 x = SPELL_SCHOOL_NORMAL; x < MAX_SPELL_SCHOOL; ++x)
        {
            if (GetMiscValue() & (1 << x))
            {
                if (apply)
                    target->ApplyStatPctModifier(UnitMods(UNIT_MOD_RESISTANCE_START + x), BASE_PCT, float(GetAmount()));
                else
                {
                    float amount = target->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_BASE_RESISTANCE_PCT, 1 << x);
                    target->SetStatPctModifier(UnitMods(UNIT_MOD_RESISTANCE_START + x), BASE_PCT, amount);
                }
            }
        }
    }
}

void AuraEffect::HandleModResistancePercent(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    for (uint8 i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
    {
        if (GetMiscValue() & (1 << i))
        {
            float amount = target->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_RESISTANCE_PCT, 1 << i);
            if (target->GetPctModifierValue(UnitMods(UNIT_MOD_RESISTANCE_START + i), TOTAL_PCT) == amount)
                continue;

            target->SetStatPctModifier(UnitMods(UNIT_MOD_RESISTANCE_START + i), TOTAL_PCT, amount);
        }
    }
}

void AuraEffect::HandleModBaseResistance(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    // only players have base stats
    if (target->GetTypeId() != TYPEID_PLAYER)
    {
        //pets only have base armor
        if (target->IsPet() && (GetMiscValue() & SPELL_SCHOOL_MASK_NORMAL))
            target->HandleStatFlatModifier(UNIT_MOD_ARMOR, TOTAL_VALUE, float(GetAmount()), apply);
    }
    else
    {
        for (uint8 i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
            if (GetMiscValue() & (1 << i))
                target->HandleStatFlatModifier(UnitMods(UNIT_MOD_RESISTANCE_START + i), TOTAL_VALUE, float(GetAmount()), apply);
    }
}

void AuraEffect::HandleModTargetResistance(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Player* target = aurApp->GetTarget()->ToPlayer();
    if (!target)
        return;

    // applied to damage as HandleNoImmediateEffect in Unit::CalcAbsorbResist and Unit::CalcArmorReducedDamage

    // show armor penetration
    if (target->GetTypeId() == TYPEID_PLAYER && (GetMiscValue() & SPELL_SCHOOL_MASK_NORMAL))
        target->ApplyModTargetPhysicalResistance(GetAmount(), apply);

    // show as spell penetration only full spell penetration bonuses (all resistances except armor and holy
    if (target->GetTypeId() == TYPEID_PLAYER && (GetMiscValue() & SPELL_SCHOOL_MASK_SPELL) == SPELL_SCHOOL_MASK_SPELL)
        target->ApplyModTargetResistance(GetAmount(), apply);
}

/********************************/
/***           STAT           ***/
/********************************/

void AuraEffect::HandleAuraModStat(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    if (GetMiscValue() < -2 || GetMiscValue() > 4)
    {
        TC_LOG_ERROR("spells", "WARNING: Spell %u effect %u has an unsupported misc value (%i) for SPELL_AURA_MOD_STAT ", GetId(), GetEffIndex(), GetMiscValue());
        return;
    }

    Unit* target = aurApp->GetTarget();
    int32 spellGroupVal = target->GetHighestExclusiveSameEffectSpellGroupValue(this, SPELL_AURA_MOD_STAT, true, GetMiscValue());
    if (abs(spellGroupVal) >= abs(GetAmount()))
        return;

    for (int32 i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        // -1 or -2 is all stats (misc < -2 checked in function beginning)
        if (GetMiscValue() < 0 || GetMiscValue() == i)
        {
            if (spellGroupVal)
            {
                target->HandleStatFlatModifier(UnitMods(UNIT_MOD_STAT_START + i), TOTAL_VALUE, float(spellGroupVal), !apply);
                if (target->GetTypeId() == TYPEID_PLAYER || target->IsPet())
                    target->UpdateStatBuffMod(Stats(i));
            }

            target->HandleStatFlatModifier(UnitMods(UNIT_MOD_STAT_START + i), TOTAL_VALUE, float(GetAmount()), apply);
            if (target->GetTypeId() == TYPEID_PLAYER || target->IsPet())
                target->UpdateStatBuffMod(Stats(i));
        }
    }
}

void AuraEffect::HandleModPercentStat(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if (GetMiscValue() < -1 || GetMiscValue() > 4)
    {
        TC_LOG_ERROR("spells", "WARNING: Misc Value for SPELL_AURA_MOD_PERCENT_STAT not valid");
        return;
    }

    // only players have base stats
    if (target->GetTypeId() != TYPEID_PLAYER)
        return;

    for (int32 i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        if (GetMiscValue() == i || GetMiscValue() == -1)
        {
            if (apply)
                target->ApplyStatPctModifier(UnitMods(UNIT_MOD_STAT_START + i), BASE_PCT, float(GetAmount()));
            else
            {
                float amount = target->GetTotalAuraMultiplier(SPELL_AURA_MOD_PERCENT_STAT, [i](AuraEffect const* aurEff) -> bool
                    {
                        if (aurEff->GetMiscValue() == i || aurEff->GetMiscValue() == -1)
                            return true;
                        return false;
                    });
                target->SetStatPctModifier(UnitMods(UNIT_MOD_STAT_START + i), BASE_PCT, amount);
            }
        }
    }
}

void AuraEffect::HandleModSpellDamagePercentFromStat(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if (target->GetTypeId() != TYPEID_PLAYER)
        return;

    // Magic damage modifiers implemented in Unit::SpellDamageBonus
    // This information for client side use only
    // Recalculate bonus
    target->ToPlayer()->UpdateSpellDamageAndHealingBonus();
}

void AuraEffect::HandleModSpellHealingPercentFromStat(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if (target->GetTypeId() != TYPEID_PLAYER)
        return;

    // Recalculate bonus
    target->ToPlayer()->UpdateSpellDamageAndHealingBonus();
}

void AuraEffect::HandleModHealingDone(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if (target->GetTypeId() != TYPEID_PLAYER)
        return;
    // implemented in Unit::SpellHealingBonus
    // this information is for client side only
    target->ToPlayer()->UpdateSpellDamageAndHealingBonus();
}

void AuraEffect::HandleModHealingDonePct(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    if (Player* player = aurApp->GetTarget()->ToPlayer())
        player->UpdateHealingDonePercentMod();
}

void AuraEffect::HandleModTotalPercentStat(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    // save current health state
    float healthPct = target->GetHealthPct();
    bool zeroHealth = !target->IsAlive();

    // players in corpse state may mean two different states:
   /// 1. player just died but did not release (in this case health == 0)
   /// 2. player is corpse running (ie ghost) (in this case health == 1)
    if (target->getDeathState() == CORPSE)
        zeroHealth = (target->GetHealth() == 0);

    for (int32 i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        if (GetMiscValueB() & 1 << i || !GetMiscValueB()) // 0 is also used for all stats
        {
            float amount = target->GetTotalAuraMultiplier(SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE, [i](AuraEffect const* aurEff) -> bool
            {
                if (aurEff->GetMiscValueB() & 1 << i || !aurEff->GetMiscValueB())
                    return true;
                return false;
            });

            if (target->GetPctModifierValue(UnitMods(UNIT_MOD_STAT_START + i), TOTAL_PCT) == amount)
                continue;

            target->SetStatPctModifier(UnitMods(UNIT_MOD_STAT_START + i), TOTAL_PCT, amount);
            if (target->GetTypeId() == TYPEID_PLAYER || target->IsPet())
                target->UpdateStatBuffMod(Stats(i));
        }
    }

    // recalculate current HP/MP after applying aura modifications (only for spells with SPELL_ATTR0_ABILITY 0x00000010 flag)
    // this check is total bullshit i think
    if ((GetMiscValueB() & 1 << STAT_STAMINA || !GetMiscValueB()) && (m_spellInfo->HasAttribute(SPELL_ATTR0_ABILITY)))
        target->SetHealth(std::max<uint32>(CalculatePct(target->GetMaxHealth(), healthPct), (zeroHealth ? 0 : 1)));
}

void AuraEffect::HandleAuraModExpertise(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if (target->GetTypeId() != TYPEID_PLAYER)
        return;

    target->ToPlayer()->UpdateExpertise(BASE_ATTACK);
    target->ToPlayer()->UpdateExpertise(OFF_ATTACK);
}

// Increase armor by <AuraEffect.BasePoints> % of your <primary stat>
void AuraEffect::HandleModArmorPctFromStat(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    // only players have primary stats
    Player* player = aurApp->GetTarget()->ToPlayer();
    if (!player)
        return;

    player->UpdateArmor();
}

void AuraEffect::HandleModStatBonusPercent(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if (GetMiscValue() < -1 || GetMiscValue() > 4)
    {
        TC_LOG_ERROR("spells", "WARNING: Misc Value for SPELL_AURA_MOD_STAT_BONUS_PCT not valid");
        return;
    }

    // only players have base stats
    if (target->GetTypeId() != TYPEID_PLAYER)
        return;

    for (int32 i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        if (GetMiscValue() == i || GetMiscValue() == -1)
        {
            target->HandleStatFlatModifier(UnitMods(UNIT_MOD_STAT_START + i), BASE_PCT_EXCLUDE_CREATE, float(GetAmount()), apply);
            target->UpdateStatBuffMod(Stats(i));
        }
    }
}

void AuraEffect::HandleOverrideSpellPowerByAttackPower(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Player* target = aurApp->GetTarget()->ToPlayer();
    if (!target)
        return;

    target->ApplyModOverrideSpellPowerByAPPercent(float(GetAmount()), apply);
    target->UpdateSpellDamageAndHealingBonus();
}

void AuraEffect::HandleOverrideAttackPowerBySpellPower(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Player* target = aurApp->GetTarget()->ToPlayer();
    if (!target)
        return;

    target->ApplyModOverrideAPBySpellPowerPercent(float(GetAmount()), apply);
    target->UpdateAttackPowerAndDamage();
    target->UpdateAttackPowerAndDamage(true);
}

void AuraEffect::HandleModVersatilityByPct(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    if (Player* target = aurApp->GetTarget()->ToPlayer())
    {
        target->SetVersatilityBonus(target->GetTotalAuraModifier(SPELL_AURA_MOD_VERSATILITY));
        target->UpdateHealingDonePercentMod();
        target->UpdateVersatilityDamageDone();
    }
}

void AuraEffect::HandleAuraModMaxPower(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    Powers power = Powers(GetMiscValue());
    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + power);

    target->HandleStatFlatModifier(unitMod, TOTAL_VALUE, float(GetAmount()), apply);
}

/********************************/
/***      HEAL & ENERGIZE     ***/
/********************************/
void AuraEffect::HandleModPowerRegen(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if (target->GetTypeId() != TYPEID_PLAYER)
        return;

    // Update manaregen value
    if (GetMiscValue() == POWER_MANA)
        target->ToPlayer()->UpdateManaRegen();
    else if (GetMiscValue() == POWER_RUNES)
        target->ToPlayer()->UpdateAllRunesRegen();
    // other powers are not immediate effects - implemented in Player::Regenerate, Creature::Regenerate
}

void AuraEffect::HandleModPowerRegenPCT(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    HandleModPowerRegen(aurApp, mode, apply);
}

void AuraEffect::HandleModManaRegenPct(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if (!target->IsPlayer())
        return;

    target->ToPlayer()->UpdateManaRegen();
}

void AuraEffect::HandleAuraModIncreaseHealth(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    int32 const amt = apply ? GetAmount() : -GetAmount();
    if (amt < 0)
        target->ModifyHealth(std::max<int32>(1 - target->GetHealth(), amt));

    target->HandleStatFlatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, GetAmount(), apply);

    if (amt > 0)
        target->ModifyHealth(amt);
}

void AuraEffect::HandleAuraModIncreaseMaxHealth(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    float percent = target->GetHealthPct();

    target->HandleStatFlatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, float(GetAmount()), apply);

    // refresh percentage
    if (target->GetHealth() > 0)
    {
        uint32 newHealth = std::max<uint32>(target->CountPctFromMaxHealth(int32(percent)), 1);
        target->SetHealth(newHealth);
    }
}

void AuraEffect::HandleAuraModIncreaseEnergy(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();
    Powers powerType = Powers(GetMiscValue());
    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + powerType);

    target->HandleStatFlatModifier(unitMod, TOTAL_VALUE, float(GetAmount()), apply);
}

void AuraEffect::HandleAuraModIncreaseEnergyPercent(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    Powers powerType = Powers(GetMiscValue());
    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + powerType);

    // Save old powers for further calculation
    int32 oldPower = target->GetPower(powerType);
    int32 oldMaxPower = target->GetMaxPower(powerType);

    // Handle aura effect for max power
    if (apply)
        target->ApplyStatPctModifier(unitMod, TOTAL_PCT, float(GetAmount()));
    else
    {
        float amount = target->GetTotalAuraMultiplier(SPELL_AURA_MOD_INCREASE_ENERGY_PERCENT, [powerType](AuraEffect const* aurEff) -> bool
            {
                if (aurEff->GetMiscValue() == powerType)
                    return true;
                return false;
            });

        amount *= target->GetTotalAuraMultiplier(SPELL_AURA_MOD_MAX_POWER_PCT, [powerType](AuraEffect const* aurEff) -> bool
            {
                if (aurEff->GetMiscValue() == powerType)
                    return true;
                return false;
            });

        target->SetStatPctModifier(unitMod, TOTAL_PCT, amount);
    }

    // Calculate the current power change
    int32 change = target->GetMaxPower(powerType) - oldMaxPower;
    change = (oldPower + change) - target->GetPower(powerType);
    target->ModifyPower(powerType, change);
}

void AuraEffect::HandleAuraModIncreaseHealthPercent(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    // Unit will keep hp% after MaxHealth being modified if unit is alive.
    float percent = target->GetHealthPct();
    if (apply)
        target->ApplyStatPctModifier(UNIT_MOD_HEALTH, TOTAL_PCT, float(GetAmount()));
    else
    {
        float amount = target->GetTotalAuraMultiplier(SPELL_AURA_MOD_INCREASE_HEALTH_PERCENT)
            * target->GetTotalAuraMultiplier(SPELL_AURA_MOD_INCREASE_HEALTH_PERCENT2);
        target->SetStatPctModifier(UNIT_MOD_HEALTH, TOTAL_PCT, amount);
    }

    if (target->GetHealth() > 0)
    {
        uint32 newHealth = std::max<uint32>(CalculatePct(target->GetMaxHealth(), percent), 1);
        target->SetHealth(newHealth);
    }
}

void AuraEffect::HandleAuraIncreaseBaseHealthPercent(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if (apply)
        target->ApplyStatPctModifier(UNIT_MOD_HEALTH, BASE_PCT, float(GetAmount()));
    else
    {
        float amount = target->GetTotalAuraMultiplier(SPELL_AURA_MOD_BASE_HEALTH_PCT);
        target->SetStatPctModifier(UNIT_MOD_HEALTH, BASE_PCT, amount);
    }
}

void AuraEffect::HandleAuraModIncreaseBaseManaPercent(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if (apply)
        target->ApplyStatPctModifier(UNIT_MOD_MANA, BASE_PCT, float(GetAmount()));
    else
    {
        float amount = target->GetTotalAuraMultiplier(SPELL_AURA_MOD_BASE_MANA_PCT);
        target->SetStatPctModifier(UNIT_MOD_MANA, BASE_PCT, amount);
    }
}

void AuraEffect::HandleModManaCostPct(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    aurApp->GetTarget()->ApplyModManaCostMultiplier(GetAmount() / 100.0f, apply);
}

void AuraEffect::HandleAuraModPowerDisplay(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK))
        return;

    if (GetMiscValue() >= MAX_POWERS)
        return;

    if (apply)
        aurApp->GetTarget()->RemoveAurasByType(GetAuraType(), ObjectGuid::Empty, GetBase());

    aurApp->GetTarget()->UpdateDisplayPower();
}

void AuraEffect::HandleAuraModOverridePowerDisplay(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    PowerDisplayEntry const* powerDisplay = sPowerDisplayStore.LookupEntry(GetMiscValue());
    if (!powerDisplay)
        return;

    Unit* target = aurApp->GetTarget();
    if (target->GetPowerIndex(Powers(powerDisplay->ActualType)) == MAX_POWERS)
        return;

    if (apply)
    {
        target->RemoveAurasByType(GetAuraType(), ObjectGuid::Empty, GetBase());
        target->SetOverrideDisplayPowerId(powerDisplay->ID);
    }
    else
        target->SetOverrideDisplayPowerId(0);
}

void AuraEffect::HandleAuraModMaxPowerPct(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit * target = aurApp->GetTarget();
    if (!target->IsPlayer())
        return;

    Powers powerType = Powers(GetMiscValue());
    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + powerType);

    // Save old powers for further calculation
    int32 oldPower = target->GetPower(powerType);
    int32 oldMaxPower = target->GetMaxPower(powerType);

    // Handle aura effect for max power
    if (apply)
        target->ApplyStatPctModifier(unitMod, TOTAL_PCT, float(GetAmount()));
    else
    {
        float amount = target->GetTotalAuraMultiplier(SPELL_AURA_MOD_MAX_POWER_PCT, [powerType](AuraEffect const* aurEff) -> bool
            {
                if (aurEff->GetMiscValue() == powerType)
                    return true;
                return false;
            });

        amount *= target->GetTotalAuraMultiplier(SPELL_AURA_MOD_INCREASE_ENERGY_PERCENT, [powerType](AuraEffect const* aurEff) -> bool
            {
                if (aurEff->GetMiscValue() == powerType)
                    return true;
                return false;
            });

        target->SetStatPctModifier(unitMod, TOTAL_PCT, amount);
    }

    // Calculate the current power change
    int32 change = target->GetMaxPower(powerType) - oldMaxPower;
    change = (oldPower + change) - target->GetPower(powerType);
    target->ModifyPower(powerType, change);
}

/********************************/
/***          FIGHT           ***/
/********************************/

void AuraEffect::HandleAuraModParryPercent(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if (target->GetTypeId() != TYPEID_PLAYER)
        return;

    target->ToPlayer()->UpdateParryPercentage();
}

void AuraEffect::HandleAuraModDodgePercent(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if (target->GetTypeId() != TYPEID_PLAYER)
        return;

    target->ToPlayer()->UpdateDodgePercentage();
}

void AuraEffect::HandleAuraModBlockPercent(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if (target->GetTypeId() != TYPEID_PLAYER)
        return;

    target->ToPlayer()->UpdateBlockPercentage();
}

void AuraEffect::HandleAuraModRegenInterrupt(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if (!target->IsPlayer())
        return;

    target->ToPlayer()->UpdateManaRegen();
}

void AuraEffect::HandleAuraModWeaponCritPercent(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Player* target = aurApp->GetTarget()->ToPlayer();
    if (!target)
        return;

    target->UpdateAllWeaponDependentCritAuras();
}

void AuraEffect::HandleModSpellHitChance(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if (target->GetTypeId() == TYPEID_PLAYER)
        target->ToPlayer()->UpdateSpellHitChances();
    else
        target->m_modSpellHitChance += (apply) ? GetAmount() : (-GetAmount());
}

void AuraEffect::HandleModSpellCritChance(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if (target->GetTypeId() == TYPEID_PLAYER)
        target->ToPlayer()->UpdateSpellCritChance();
    else
        target->m_baseSpellCritChance += (apply) ? GetAmount() : -GetAmount();
}

void AuraEffect::HandleAuraModCritPct(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if (target->GetTypeId() != TYPEID_PLAYER)
    {
        target->m_baseSpellCritChance += (apply) ? GetAmount() : -GetAmount();
        return;
    }

    target->ToPlayer()->UpdateAllWeaponDependentCritAuras();

    // included in Player::UpdateSpellCritChance calculation
    target->ToPlayer()->UpdateSpellCritChance();
}

/********************************/
/***         ATTACK SPEED     ***/
/********************************/

void AuraEffect::HandleModCastingSpeed(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    // Do not apply such auras in normal way
    if (GetAmount() >= 1000)
    {
        if (apply)
            target->SetInstantCast(true);
        else
        {
            // only SPELL_AURA_MOD_CASTING_SPEED_NOT_STACK can have this high amount
            // it's some rare case that you have 2 auras like that, but just in case ;)

            bool remove = true;
            Unit::AuraEffectList const& castingSpeedNotStack = target->GetAuraEffectsByType(SPELL_AURA_MOD_CASTING_SPEED_NOT_STACK);
            for (AuraEffect const* aurEff : castingSpeedNotStack)
            {
                if (aurEff != this && aurEff->GetAmount() >= 1000)
                {
                    remove = false;
                    break;
                }
            }

            if (remove)
                target->SetInstantCast(false);
        }

        return;
    }

    target->ApplyCastTimePercentMod((float)GetAmount(), apply);
}

void AuraEffect::HandleModMeleeRangedSpeedPct(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    //! ToDo: Haste auras with the same handler _CAN'T_ stack together
    Unit* target = aurApp->GetTarget();

    target->ApplyAttackTimePercentMod(BASE_ATTACK, (float)GetAmount(), apply);
    target->ApplyAttackTimePercentMod(OFF_ATTACK, (float)GetAmount(), apply);
    target->ApplyAttackTimePercentMod(RANGED_ATTACK, (float)GetAmount(), apply);
}

void AuraEffect::HandleModCombatSpeedPct(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();
    int32 spellGroupVal = target->GetHighestExclusiveSameEffectSpellGroupValue(this, SPELL_AURA_MELEE_SLOW);
    if (abs(spellGroupVal) >= abs(GetAmount()))
        return;

    if (spellGroupVal)
    {
        target->ApplyCastTimePercentMod(float(spellGroupVal), !apply);
        target->ApplyAttackTimePercentMod(BASE_ATTACK, float(spellGroupVal), !apply);
        target->ApplyAttackTimePercentMod(OFF_ATTACK, float(spellGroupVal), !apply);
        target->ApplyAttackTimePercentMod(RANGED_ATTACK, float(spellGroupVal), !apply);
    }
    target->ApplyCastTimePercentMod(float(GetAmount()), apply);
    target->ApplyAttackTimePercentMod(BASE_ATTACK, float(GetAmount()), apply);
    target->ApplyAttackTimePercentMod(OFF_ATTACK, float(GetAmount()), apply);
    target->ApplyAttackTimePercentMod(RANGED_ATTACK, float(GetAmount()), apply);
}

void AuraEffect::HandleModAttackSpeed(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    target->ApplyAttackTimePercentMod(BASE_ATTACK, float(GetAmount()), apply);
    target->UpdateDamagePhysical(BASE_ATTACK);
}

void AuraEffect::HandleModMeleeSpeedPct(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    //! ToDo: Haste auras with the same handler _CAN'T_ stack together
    Unit* target = aurApp->GetTarget();
    int32 spellGroupVal = target->GetHighestExclusiveSameEffectSpellGroupValue(this, SPELL_AURA_MOD_MELEE_HASTE);
    if (abs(spellGroupVal) >= abs(GetAmount()))
        return;

    if (spellGroupVal)
    {
        target->ApplyAttackTimePercentMod(BASE_ATTACK, float(spellGroupVal), !apply);
        target->ApplyAttackTimePercentMod(OFF_ATTACK, float(spellGroupVal), !apply);
    }
    target->ApplyAttackTimePercentMod(BASE_ATTACK, float(GetAmount()), apply);
    target->ApplyAttackTimePercentMod(OFF_ATTACK,  float(GetAmount()), apply);
}

void AuraEffect::HandleAuraModRangedHaste(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    //! ToDo: Haste auras with the same handler _CAN'T_ stack together
    Unit* target = aurApp->GetTarget();

    target->ApplyAttackTimePercentMod(RANGED_ATTACK, (float)GetAmount(), apply);
}

/********************************/
/***       COMBAT RATING      ***/
/********************************/

void AuraEffect::HandleModRating(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if (target->GetTypeId() != TYPEID_PLAYER)
        return;

    for (uint32 rating = 0; rating < MAX_COMBAT_RATING; ++rating)
        if (GetMiscValue() & (1 << rating))
            target->ToPlayer()->ApplyRatingMod(CombatRating(rating), GetAmount(), apply);
}

void AuraEffect::HandleModRatingPct(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if (target->GetTypeId() != TYPEID_PLAYER)
        return;

    // Just recalculate ratings
    for (uint32 rating = 0; rating < MAX_COMBAT_RATING; ++rating)
        if (GetMiscValue() & (1 << rating))
            target->ToPlayer()->UpdateRating(CombatRating(rating));
}

/********************************/
/***        ATTACK POWER      ***/
/********************************/

void AuraEffect::HandleAuraModAttackPower(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    target->HandleStatFlatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, float(GetAmount()), apply);
}

void AuraEffect::HandleAuraModRangedAttackPower(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if ((target->getClassMask() & CLASSMASK_WAND_USERS) != 0)
        return;

    target->HandleStatFlatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(GetAmount()), apply);
}

void AuraEffect::HandleAuraModAttackPowerPercent(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    //UNIT_FIELD_ATTACK_POWER_MULTIPLIER = multiplier - 1
    if (apply)
        target->ApplyStatPctModifier(UNIT_MOD_ATTACK_POWER, TOTAL_PCT, float(GetAmount()));
    else
    {
        float amount = target->GetTotalAuraMultiplier(SPELL_AURA_MOD_ATTACK_POWER_PCT);
        target->SetStatPctModifier(UNIT_MOD_ATTACK_POWER, TOTAL_PCT, amount);
    }
}

void AuraEffect::HandleAuraModRangedAttackPowerPercent(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if ((target->getClassMask() & CLASSMASK_WAND_USERS) != 0)
        return;

    //UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER = multiplier - 1
    if (apply)
        target->ApplyStatPctModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_PCT, float(GetAmount()));
    else
    {
        float amount = target->GetTotalAuraMultiplier(SPELL_AURA_MOD_RANGED_ATTACK_POWER_PCT);
        target->SetStatPctModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_PCT, amount);
    }
}

/********************************/
/***        DAMAGE BONUS      ***/
/********************************/
void AuraEffect::HandleModDamageDone(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    if (GetMiscValue() & SPELL_SCHOOL_MASK_NORMAL)
        target->UpdateAllDamageDoneMods();

    // Magic damage modifiers implemented in Unit::SpellBaseDamageBonusDone
    // This information for client side use only
    if (Player* playerTarget = target->ToPlayer())
    {
        for (uint16 i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
            if (GetMiscValue() & (1 << i))
            {
                if (GetAmount() >= 0)
                    playerTarget->ApplyModDamageDonePos(SpellSchools(i), GetAmount(), apply);
                else
                    playerTarget->ApplyModDamageDoneNeg(SpellSchools(i), GetAmount(), apply);
            }

        if (Guardian* pet = playerTarget->GetGuardianPet())
            pet->UpdateAttackPowerAndDamage();
    }
}

void AuraEffect::HandleModDamagePercentDone(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    // also handles spell group stacks
    if (GetMiscValue() & SPELL_SCHOOL_MASK_NORMAL)
        target->UpdateAllDamagePctDoneMods();

    if (Player* thisPlayer = target->ToPlayer())
    {
        for (uint8 i = 0; i < MAX_SPELL_SCHOOL; ++i)
        {
            if (GetMiscValue() & (1 << i))
            {
                // only aura type modifying PLAYER_FIELD_MOD_DAMAGE_DONE_PCT
                float amount = thisPlayer->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE, 1 << i);
                thisPlayer->SetModDamageDonePercent(i, amount);
            }
        }
    }
}

void AuraEffect::HandleModOffhandDamagePercent(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Unit* target = aurApp->GetTarget();

    // also handles spell group stacks
    target->UpdateDamagePctDoneMods(OFF_ATTACK);
}

void AuraEffect::HandleShieldBlockValue(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    if (Player* player = aurApp->GetTarget()->ToPlayer())
        player->HandleBaseModFlatValue(SHIELD_BLOCK_VALUE, float(GetAmount()), apply);
}

void AuraEffect::HandleShieldBlockValuePercent(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_STAT)))
        return;

    Player* target = aurApp->GetTarget()->ToPlayer();
    if (!target)
        return;

    if (apply)
        target->ApplyBaseModPctValue(SHIELD_BLOCK_VALUE, float(GetAmount()));
    else
    {
        float amount = target->GetTotalAuraMultiplier(SPELL_AURA_MOD_SHIELD_BLOCKVALUE_PCT);
        target->SetBaseModPctValue(SHIELD_BLOCK_VALUE, amount);
    }
}

/********************************/
/***        POWER COST        ***/
/********************************/

void AuraEffect::HandleModPowerCost(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK))
        return;

    // handled in SpellInfo::CalcPowerCost, this is only for client UI
    if (!(GetMiscValueB() & (1 << POWER_MANA)))
        return;

    Unit* target = aurApp->GetTarget();

    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
        if (GetMiscValue() & (1 << i))
            target->ApplyModManaCostModifier(SpellSchools(i), GetAmount(), apply);
}

void AuraEffect::HandleArenaPreparation(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    if (apply)
        target->AddUnitFlag(UNIT_FLAG_PREPARATION);
    else
    {
        // do not remove unit flag if there are more than this auraEffect of that kind on unit on unit
        if (target->HasAuraType(GetAuraType()))
            return;
        target->RemoveUnitFlag(UNIT_FLAG_PREPARATION);
    }

    target->ModifyAuraState(AURA_STATE_ARENA_PREPARATION, apply);
}

void AuraEffect::HandleNoReagentUseAura(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    if (target->GetTypeId() != TYPEID_PLAYER)
        return;

    flag128 mask;
    Unit::AuraEffectList const& noReagent = target->GetAuraEffectsByType(SPELL_AURA_NO_REAGENT_USE);
    for (Unit::AuraEffectList::const_iterator i = noReagent.begin(); i != noReagent.end(); ++i)
        mask |= (*i)->GetSpellEffectInfo().SpellClassMask;

    target->ToPlayer()->SetNoRegentCostMask(mask);
}

/*********************************************************/
/***                    OTHERS                         ***/
/*********************************************************/

void AuraEffect::HandleAuraDummy(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & (AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK | AURA_EFFECT_HANDLE_REAPPLY)))
        return;

    Unit* target = aurApp->GetTarget();

    Unit* caster = GetCaster();

    // pet auras
    if (target->GetTypeId() == TYPEID_PLAYER && (mode & AURA_EFFECT_HANDLE_REAL))
    {
        if (PetAura const* petSpell = sSpellMgr->GetPetAura(GetId(), GetEffIndex()))
        {
            if (apply)
                target->ToPlayer()->AddPetAura(petSpell);
            else
                target->ToPlayer()->RemovePetAura(petSpell);
        }
    }

    if (mode & (AURA_EFFECT_HANDLE_REAL | AURA_EFFECT_HANDLE_REAPPLY))
    {
        // AT APPLY
        if (apply)
        {
            switch (GetId())
            {
                case 1515:                                      // Tame beast
                    // FIX_ME: this is 2.0.12 threat effect replaced in 2.1.x by dummy aura, must be checked for correctness
                    if (caster && target->CanHaveThreatList())
                        target->GetThreatManager().AddThreat(caster, 10.0f);
                    break;
                case 13139:                                     // net-o-matic
                    // root to self part of (root_target->charge->root_self sequence
                    if (caster)
                        caster->CastSpell(caster, 13138, this);
                    break;
                case 34026:   // kill command
                {
                    Unit* pet = target->GetGuardianPet();
                    if (!pet)
                        break;

                    target->CastSpell(target, 34027, this);

                    // set 3 stacks and 3 charges (to make all auras not disappear at once)
                    Aura* owner_aura = target->GetAura(34027, GetCasterGUID());
                    Aura* pet_aura  = pet->GetAura(58914, GetCasterGUID());
                    if (owner_aura)
                    {
                        owner_aura->SetStackAmount(owner_aura->GetSpellInfo()->StackAmount);
                        if (pet_aura)
                        {
                            pet_aura->SetCharges(0);
                            pet_aura->SetStackAmount(owner_aura->GetSpellInfo()->StackAmount);
                        }
                    }
                    break;
                }
                case 37096:                                     // Blood Elf Illusion
                {
                    if (caster)
                    {
                        if (caster->getGender() == GENDER_FEMALE)
                            caster->CastSpell(target, 37095, this); // Blood Elf Disguise
                        else
                            caster->CastSpell(target, 37093, this);
                    }
                    break;
                }
                case 39850:                                     // Rocket Blast
                    if (roll_chance_i(20))                       // backfire stun
                        target->CastSpell(target, 51581, this);
                    break;
                case 43873:                                     // Headless Horseman Laugh
                    target->PlayDistanceSound(11965);
                    break;
                case 46354:                                     // Blood Elf Illusion
                    if (caster)
                    {
                        if (caster->getGender() == GENDER_FEMALE)
                            caster->CastSpell(target, 46356, this);
                        else
                            caster->CastSpell(target, 46355, this);
                    }
                    break;
                case 46361:                                     // Reinforced Net
                    if (caster)
                        target->GetMotionMaster()->MoveFall();
                    break;
            }
        }
        // AT REMOVE
        else
        {
            if ((GetSpellInfo()->IsQuestTame()) && caster && caster->IsAlive() && target->IsAlive())
            {
                uint32 finalSpelId = 0;
                switch (GetId())
                {
                    case 19548: finalSpelId = 19597; break;
                    case 19674: finalSpelId = 19677; break;
                    case 19687: finalSpelId = 19676; break;
                    case 19688: finalSpelId = 19678; break;
                    case 19689: finalSpelId = 19679; break;
                    case 19692: finalSpelId = 19680; break;
                    case 19693: finalSpelId = 19684; break;
                    case 19694: finalSpelId = 19681; break;
                    case 19696: finalSpelId = 19682; break;
                    case 19697: finalSpelId = 19683; break;
                    case 19699: finalSpelId = 19685; break;
                    case 19700: finalSpelId = 19686; break;
                    case 30646: finalSpelId = 30647; break;
                    case 30653: finalSpelId = 30648; break;
                    case 30654: finalSpelId = 30652; break;
                    case 30099: finalSpelId = 30100; break;
                    case 30102: finalSpelId = 30103; break;
                    case 30105: finalSpelId = 30104; break;
                }

                if (finalSpelId)
                    caster->CastSpell(target, finalSpelId, this);
            }

            switch (m_spellInfo->SpellFamilyName)
            {
                case SPELLFAMILY_GENERIC:
                    switch (GetId())
                    {
                        case 2584: // Waiting to Resurrect
                            // Waiting to resurrect spell cancel, we must remove player from resurrect queue
                            if (target->GetTypeId() == TYPEID_PLAYER)
                            {
                                if (Battleground* bg = target->ToPlayer()->GetBattleground())
                                    bg->RemovePlayerFromResurrectQueue(target->GetGUID());
                                if (Battlefield* bf = sBattlefieldMgr->GetBattlefieldToZoneId(target->GetZoneId()))
                                    bf->RemovePlayerFromResurrectQueue(target->GetGUID());
                            }
                            break;
                        case 36730:                                     // Flame Strike
                        {
                            target->CastSpell(target, 36731, this);
                            break;
                        }
                        case 44191:                                     // Flame Strike
                        {
                            if (target->GetMap()->IsDungeon())
                            {
                                uint32 spellId = target->GetMap()->IsHeroic() ? 46163 : 44190;

                                target->CastSpell(target, spellId, this);
                            }
                            break;
                        }
                        case 43681: // Inactive
                        {
                            if (target->GetTypeId() != TYPEID_PLAYER || aurApp->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
                                return;

                            if (target->GetMap()->IsBattleground())
                                target->ToPlayer()->LeaveBattleground();
                            break;
                        }
                        case 42783: // Wrath of the Astromancer
                            target->CastSpell(target, GetAmount(), this);
                            break;
                        case 46308: // Burning Winds cast only at creatures at spawn
                            target->CastSpell(target, 47287, this);
                            break;
                        case 52172:  // Coyote Spirit Despawn Aura
                        case 60244:  // Blood Parrot Despawn Aura
                            target->CastSpell(nullptr, GetAmount(), this);
                            break;
                        case 91604: // Restricted Flight Area
                            if (aurApp->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE)
                                target->CastSpell(target, 58601, this);
                            break;
                    }
                    break;
                case SPELLFAMILY_DEATHKNIGHT:
                    // Summon Gargoyle (Dismiss Gargoyle at remove)
                    if (GetId() == 61777)
                        target->CastSpell(target, GetAmount(), this);
                    break;
                default:
                    break;
            }
        }
    }

    // AT APPLY & REMOVE

    switch (m_spellInfo->SpellFamilyName)
    {
        case SPELLFAMILY_GENERIC:
        {
            if (!(mode & AURA_EFFECT_HANDLE_REAL))
                break;
            switch (GetId())
            {
                // Recently Bandaged
                case 11196:
                    target->ApplySpellImmune(GetId(), IMMUNITY_MECHANIC, GetMiscValue(), apply);
                    break;
                // Unstable Power
                case 24658:
                {
                    uint32 spellId = 24659;
                    if (apply && caster)
                    {
                        SpellInfo const* spell = sSpellMgr->AssertSpellInfo(spellId, GetBase()->GetCastDifficulty());
                        CastSpellExtraArgs args;
                        args.TriggerFlags = TRIGGERED_FULL_MASK;
                        args.OriginalCaster = GetCasterGUID();
                        args.OriginalCastId = GetBase()->GetCastId();
                        args.CastDifficulty = GetBase()->GetCastDifficulty();

                        for (uint32 i = 0; i < spell->StackAmount; ++i)
                            caster->CastSpell(target, spell->Id, args);
                        break;
                    }
                    target->RemoveAurasDueToSpell(spellId);
                    break;
                }
                // Restless Strength
                case 24661:
                {
                    uint32 spellId = 24662;
                    if (apply && caster)
                    {
                        SpellInfo const* spell = sSpellMgr->AssertSpellInfo(spellId, GetBase()->GetCastDifficulty());
                        CastSpellExtraArgs args;
                        args.TriggerFlags = TRIGGERED_FULL_MASK;
                        args.OriginalCaster = GetCasterGUID();
                        args.OriginalCastId = GetBase()->GetCastId();
                        args.CastDifficulty = GetBase()->GetCastDifficulty();

                        for (uint32 i = 0; i < spell->StackAmount; ++i)
                            caster->CastSpell(target, spell->Id, args);
                        break;
                    }
                    target->RemoveAurasDueToSpell(spellId);
                    break;
                }
                // Tag Murloc
                case 30877:
                {
                    // Tag/untag Blacksilt Scout
                    target->SetEntry(apply ? 17654 : 17326);
                    break;
                }
                case 57819: // Argent Champion
                case 57820: // Ebon Champion
                case 57821: // Champion of the Kirin Tor
                case 57822: // Wyrmrest Champion
                {
                    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
                        break;

                    uint32 FactionID = 0;

                    if (apply)
                    {
                        switch (m_spellInfo->Id)
                        {
                            case 57819: FactionID = 1106; break; // Argent Crusade
                            case 57820: FactionID = 1098; break; // Knights of the Ebon Blade
                            case 57821: FactionID = 1090; break; // Kirin Tor
                            case 57822: FactionID = 1091; break; // The Wyrmrest Accord
                        }
                    }
                    caster->ToPlayer()->SetChampioningFaction(FactionID);
                    break;
                }
                // LK Intro VO (1)
                case 58204:
                    if (target->GetTypeId() == TYPEID_PLAYER)
                    {
                        // Play part 1
                        if (apply)
                            target->PlayDirectSound(14970, target->ToPlayer());
                        // continue in 58205
                        else
                            target->CastSpell(target, 58205, this);
                    }
                    break;
                // LK Intro VO (2)
                case 58205:
                    if (target->GetTypeId() == TYPEID_PLAYER)
                    {
                        // Play part 2
                        if (apply)
                            target->PlayDirectSound(14971, target->ToPlayer());
                        // Play part 3
                        else
                            target->PlayDirectSound(14972, target->ToPlayer());
                    }
                    break;
            }

            break;
        }
        case SPELLFAMILY_MAGE:
        {
            //if (!(mode & AURA_EFFECT_HANDLE_REAL))
                //break;
            break;
        }
        case SPELLFAMILY_PRIEST:
        {
            //if (!(mode & AURA_EFFECT_HANDLE_REAL))
                //break;
            break;
        }
        case SPELLFAMILY_DRUID:
        {
            //if (!(mode & AURA_EFFECT_HANDLE_REAL))
                //break;
            break;
        }
        case SPELLFAMILY_SHAMAN:
        {
            //if (!(mode & AURA_EFFECT_HANDLE_REAL))
                //break;
            break;
        }
        case SPELLFAMILY_PALADIN:
            // if (!(mode & AURA_EFFECT_HANDLE_REAL))
            //    break;
            break;
        case SPELLFAMILY_DEATHKNIGHT:
        {
            //if (!(mode & AURA_EFFECT_HANDLE_REAL))
            //    break;
            break;
        }
    }
}

void AuraEffect::HandleChannelDeathItem(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    if (apply || aurApp->GetRemoveMode() != AURA_REMOVE_BY_DEATH)
        return;

    Unit* caster = GetCaster();

    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
        return;

    Player* plCaster = caster->ToPlayer();
    Unit* target = aurApp->GetTarget();

    // Item amount
    if (GetAmount() <= 0)
        return;

    if (GetSpellEffectInfo().ItemType == 0)
        return;

    // Soul Shard
    if (GetSpellEffectInfo().ItemType == 6265)
    {
        // Soul Shard only from units that grant XP or honor
        if (!plCaster->isHonorOrXPTarget(target) ||
            (target->GetTypeId() == TYPEID_UNIT && !target->ToCreature()->isTappedBy(plCaster)))
            return;
    }

    //Adding items
    uint32 noSpaceForCount = 0;
    uint32 count = GetAmount();

    ItemPosCountVec dest;
    InventoryResult msg = plCaster->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, GetSpellEffectInfo().ItemType, count, &noSpaceForCount);
    if (msg != EQUIP_ERR_OK)
    {
        count -= noSpaceForCount;
        plCaster->SendEquipError(msg, nullptr, nullptr, GetSpellEffectInfo().ItemType);
        if (count == 0)
            return;
    }

    Item* newitem = plCaster->StoreNewItem(dest, GetSpellEffectInfo().ItemType, true);
    if (!newitem)
    {
        plCaster->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr, nullptr);
        return;
    }
    plCaster->SendNewItem(newitem, count, true, true);
}

void AuraEffect::HandleBindSight(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    Unit* caster = GetCaster();

    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
        return;

    caster->ToPlayer()->SetViewpoint(target, apply);
}

void AuraEffect::HandleForceReaction(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    Player* player = target->ToPlayer();
    if (!player)
        return;

    uint32 factionId = GetMiscValue();
    ReputationRank factionRank = ReputationRank(GetAmount());

    player->GetReputationMgr().ApplyForceReaction(factionId, factionRank, apply);
    player->GetReputationMgr().SendForceReactions();

    // stop fighting at apply (if forced rank friendly) or at remove (if real rank friendly)
    if ((apply && factionRank >= REP_FRIENDLY) || (!apply && player->GetReputationRank(factionId) >= REP_FRIENDLY))
        player->StopAttackFaction(factionId);
}

void AuraEffect::HandleAuraEmpathy(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();
    if (!apply)
    {
        // do not remove unit flag if there are more than this auraEffect of that kind on unit on unit
        if (target->HasAuraType(GetAuraType()))
            return;
    }

    if (target->GetCreatureType() == CREATURE_TYPE_BEAST)
    {
        if (apply)
            target->AddDynamicFlag(UNIT_DYNFLAG_SPECIALINFO);
        else
            target->RemoveDynamicFlag(UNIT_DYNFLAG_SPECIALINFO);
    }
}

void AuraEffect::HandleAuraModFaction(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    if (apply)
    {
        target->SetFaction(GetMiscValue());
        if (target->GetTypeId() == TYPEID_PLAYER)
            target->RemoveUnitFlag(UNIT_FLAG_PLAYER_CONTROLLED);
    }
    else
    {
        target->RestoreFaction();
        if (target->GetTypeId() == TYPEID_PLAYER)
            target->AddUnitFlag(UNIT_FLAG_PLAYER_CONTROLLED);
    }
}

void AuraEffect::HandleLearnSpell(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Player* player = aurApp->GetTarget()->ToPlayer();
    if (!player)
        return;

    if (apply)
        player->LearnSpell(GetMiscValue(), true, 0, true);
    else
        player->RemoveSpell(GetMiscValue(), false, false, true);
}

void AuraEffect::HandleComprehendLanguage(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    if (apply)
        target->AddUnitFlag2(UNIT_FLAG2_COMPREHEND_LANG);
    else
    {
        if (target->HasAuraType(GetAuraType()))
            return;

        target->RemoveUnitFlag2(UNIT_FLAG2_COMPREHEND_LANG);
    }
}

void AuraEffect::HandleAuraLinked(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    Unit* target = aurApp->GetTarget();

    uint32 triggeredSpellId = GetSpellEffectInfo().TriggerSpell;
    SpellInfo const* triggeredSpellInfo = sSpellMgr->GetSpellInfo(triggeredSpellId, GetBase()->GetCastDifficulty());
    if (!triggeredSpellInfo)
        return;

    Unit* caster = triggeredSpellInfo->NeedsToBeTriggeredByCaster(m_spellInfo) ? GetCaster() : target;
    if (!caster)
        return;

    if (mode & AURA_EFFECT_HANDLE_REAL)
    {
        if (apply)
        {
            CastSpellExtraArgs args(this);
            if (GetAmount()) // If amount avalible cast with basepoints (Crypt Fever for example)
                args.AddSpellMod(SPELLVALUE_BASE_POINT0, GetAmount());

            caster->CastSpell(target, triggeredSpellId, args);
        }
        else
        {
            ObjectGuid casterGUID = triggeredSpellInfo->NeedsToBeTriggeredByCaster(m_spellInfo) ? GetCasterGUID() : target->GetGUID();
            target->RemoveAura(triggeredSpellId, casterGUID);
        }
    }
    else if (mode & AURA_EFFECT_HANDLE_REAPPLY && apply)
    {
        ObjectGuid casterGUID = triggeredSpellInfo->NeedsToBeTriggeredByCaster(m_spellInfo) ? GetCasterGUID() : target->GetGUID();
        // change the stack amount to be equal to stack amount of our aura
        if (Aura* triggeredAura = target->GetAura(triggeredSpellId, casterGUID))
            triggeredAura->ModStackAmount(GetBase()->GetStackAmount() - triggeredAura->GetStackAmount());
    }
}

void AuraEffect::HandleTriggerSpellOnPowerPercent(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL) || !apply)
        return;

    Unit* target = aurApp->GetTarget();

    int32 effectAmount = GetAmount();
    uint32 triggerSpell = GetSpellEffectInfo().TriggerSpell;
    float powerAmountPct = GetPctOf(target->GetPower(Powers(GetMiscValue())), target->GetMaxPower(Powers(GetMiscValue())));

    switch (AuraTriggerOnPowerChangeDirection(GetMiscValueB()))
    {
        case AuraTriggerOnPowerChangeDirection::Gain:
            if (powerAmountPct < effectAmount)
                return;
            break;
        case AuraTriggerOnPowerChangeDirection::Loss:
            if (powerAmountPct > effectAmount)
                return;
            break;
        default:
            break;
    }

    target->CastSpell(target, triggerSpell, this);
}

void AuraEffect::HandleTriggerSpellOnPowerAmount(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL) || !apply)
        return;

    Unit* target = aurApp->GetTarget();

    int32 effectAmount = GetAmount();
    uint32 triggerSpell = GetSpellEffectInfo().TriggerSpell;
    float powerAmount = target->GetPower(Powers(GetMiscValue()));

    switch (AuraTriggerOnPowerChangeDirection(GetMiscValueB()))
    {
        case AuraTriggerOnPowerChangeDirection::Gain:
            if (powerAmount < effectAmount)
                return;
            break;
        case AuraTriggerOnPowerChangeDirection::Loss:
            if (powerAmount > effectAmount)
                return;
            break;
        default:
            break;
    }

    target->CastSpell(target, triggerSpell, this);
}

void AuraEffect::HandleAuraOpenStable(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    if (target->GetTypeId() != TYPEID_PLAYER || !target->IsInWorld())
        return;

    if (apply)
        target->ToPlayer()->GetSession()->SendStablePet(target->GetGUID());

     // client auto close stable dialog at !apply aura
}

void AuraEffect::HandleAuraModFakeInebriation(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK))
        return;

    Unit* target = aurApp->GetTarget();

    if (apply)
    {
        target->m_invisibilityDetect.AddFlag(INVISIBILITY_DRUNK);
        target->m_invisibilityDetect.AddValue(INVISIBILITY_DRUNK, GetAmount());

        if (Player* playerTarget = target->ToPlayer())
            playerTarget->ApplyModFakeInebriation(GetAmount(), true);
    }
    else
    {
        bool removeDetect = !target->HasAuraType(SPELL_AURA_MOD_FAKE_INEBRIATE);

        target->m_invisibilityDetect.AddValue(INVISIBILITY_DRUNK, -GetAmount());

        if (Player* playerTarget = target->ToPlayer())
        {
            playerTarget->ApplyModFakeInebriation(GetAmount(), false);

            if (removeDetect)
                removeDetect = !playerTarget->GetDrunkValue();
        }

        if (removeDetect)
            target->m_invisibilityDetect.DelFlag(INVISIBILITY_DRUNK);
    }

    // call functions which may have additional effects after chainging state of unit
    target->UpdateObjectVisibility();
}

void AuraEffect::HandleAuraOverrideSpells(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Player* target = aurApp->GetTarget()->ToPlayer();

    if (!target || !target->IsInWorld())
        return;

    uint32 overrideId = uint32(GetMiscValue());

    if (apply)
    {
        target->SetOverrideSpellsId(overrideId);
        if (OverrideSpellDataEntry const* overrideSpells = sOverrideSpellDataStore.LookupEntry(overrideId))
            for (uint8 i = 0; i < MAX_OVERRIDE_SPELL; ++i)
                if (uint32 spellId = overrideSpells->Spells[i])
                    target->AddTemporarySpell(spellId);
    }
    else
    {
        target->SetOverrideSpellsId(0);
        if (OverrideSpellDataEntry const* overrideSpells = sOverrideSpellDataStore.LookupEntry(overrideId))
            for (uint8 i = 0; i < MAX_OVERRIDE_SPELL; ++i)
                if (uint32 spellId = overrideSpells->Spells[i])
                    target->RemoveTemporarySpell(spellId);
    }
}

void AuraEffect::HandleAuraSetVehicle(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    if (!target->IsInWorld())
        return;

    uint32 vehicleId = GetMiscValue();

    if (apply)
    {
        if (!target->CreateVehicleKit(vehicleId, 0))
            return;
    }
    else if (target->GetVehicleKit())
        target->RemoveVehicleKit();

    if (target->GetTypeId() != TYPEID_PLAYER)
        return;

    if (apply)
        target->ToPlayer()->SendOnCancelExpectedVehicleRideAura();
}

void AuraEffect::HandlePreventResurrection(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Player* target = aurApp->GetTarget()->ToPlayer();
    if (!target)
        return;

    if (apply)
        target->RemovePlayerLocalFlag(PLAYER_LOCAL_FLAG_RELEASE_TIMER);
    else if (!target->GetMap()->Instanceable())
        target->AddPlayerLocalFlag(PLAYER_LOCAL_FLAG_RELEASE_TIMER);
}

void AuraEffect::HandleMastery(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Player* target = aurApp->GetTarget()->ToPlayer();
    if (!target)
        return;

    target->UpdateMastery();
}

void AuraEffect::HandlePeriodicTriggerSpellAuraTick(Unit* target, Unit* caster) const
{
    uint32 triggerSpellId = GetSpellEffectInfo().TriggerSpell;
    if (SpellInfo const* triggeredSpellInfo = sSpellMgr->GetSpellInfo(triggerSpellId, GetBase()->GetCastDifficulty()))
    {
        if (Unit* triggerCaster = triggeredSpellInfo->NeedsToBeTriggeredByCaster(m_spellInfo) ? caster : target)
        {
            triggerCaster->CastSpell(target, triggerSpellId, this);
            TC_LOG_DEBUG("spells", "AuraEffect::HandlePeriodicTriggerSpellAuraTick: Spell %u Trigger %u", GetId(), triggeredSpellInfo->Id);
        }
    }
    else
        TC_LOG_WARN("spells", "AuraEffect::HandlePeriodicTriggerSpellAuraTick: Spell %u has non-existent spell %u in EffectTriggered[%d] and is therefore not triggered.", GetId(), triggerSpellId, GetEffIndex());
}

void AuraEffect::HandlePeriodicTriggerSpellWithValueAuraTick(Unit* target, Unit* caster) const
{
    uint32 triggerSpellId = GetSpellEffectInfo().TriggerSpell;
    if (SpellInfo const* triggeredSpellInfo = sSpellMgr->GetSpellInfo(triggerSpellId, GetBase()->GetCastDifficulty()))
    {
        if (Unit* triggerCaster = triggeredSpellInfo->NeedsToBeTriggeredByCaster(m_spellInfo) ? caster : target)
        {
            CastSpellExtraArgs args(this);
            for (uint32 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                args.AddSpellMod(SpellValueMod(SPELLVALUE_BASE_POINT0 + i), GetAmount());
            triggerCaster->CastSpell(target, triggerSpellId, args);
            TC_LOG_DEBUG("spells", "AuraEffect::HandlePeriodicTriggerSpellWithValueAuraTick: Spell %u Trigger %u", GetId(), triggeredSpellInfo->Id);
        }
    }
    else
        TC_LOG_WARN("spells","AuraEffect::HandlePeriodicTriggerSpellWithValueAuraTick: Spell %u has non-existent spell %u in EffectTriggered[%d] and is therefore not triggered.", GetId(), triggerSpellId, GetEffIndex());
}

void AuraEffect::HandlePeriodicDamageAurasTick(Unit* target, Unit* caster) const
{
    if (!target->IsAlive())
        return;

    if (target->HasUnitState(UNIT_STATE_ISOLATED) || target->IsImmunedToDamage(GetSpellInfo()))
    {
        SendTickImmune(target, caster);
        return;
    }

    // Consecrate ticks can miss and will not show up in the combat log
    // dynobj auras must always have a caster
    if (GetSpellEffectInfo().IsEffect(SPELL_EFFECT_PERSISTENT_AREA_AURA) &&
        ASSERT_NOTNULL(caster)->SpellHitResult(target, GetSpellInfo(), false) != SPELL_MISS_NONE)
        return;

    CleanDamage cleanDamage = CleanDamage(0, 0, BASE_ATTACK, MELEE_HIT_NORMAL);

    uint32 stackAmountForBonuses = !GetSpellEffectInfo().EffectAttributes.HasFlag(SpellEffectAttributes::NoScaleWithStack) ? GetBase()->GetStackAmount() : 1;

    // ignore negative values (can be result apply spellmods to aura damage
    uint32 damage = std::max(GetAmount(), 0);

    // Script Hook For HandlePeriodicDamageAurasTick -- Allow scripts to change the Damage pre class mitigation calculations
    sScriptMgr->ModifyPeriodicDamageAurasTick(target, caster, damage);

    switch (GetAuraType())
    {
        case SPELL_AURA_PERIODIC_DAMAGE:
        {
            if (caster)
                damage = caster->SpellDamageBonusDone(target, GetSpellInfo(), damage, DOT, GetSpellEffectInfo(), stackAmountForBonuses);
            damage = target->SpellDamageBonusTaken(caster, GetSpellInfo(), damage, DOT);

            // There is a Chance to make a Soul Shard when Drain soul does damage
            if (caster && GetSpellInfo()->SpellFamilyName == SPELLFAMILY_WARLOCK && (GetSpellInfo()->SpellFamilyFlags[0] & 0x00004000))
            {
                if (caster->GetTypeId() == TYPEID_PLAYER && caster->ToPlayer()->isHonorOrXPTarget(target))
                    caster->CastSpell(caster, 95810, this);
            }
            else if (GetSpellInfo()->SpellFamilyName == SPELLFAMILY_GENERIC)
            {
                switch (GetId())
                {
                    case 70911: // Unbound Plague
                    case 72854: // Unbound Plague
                    case 72855: // Unbound Plague
                    case 72856: // Unbound Plague
                        damage *= uint32(pow(1.25f, int32(_ticksDone)));
                        break;
                    default:
                        break;
                }
            }
            break;
        }
        case SPELL_AURA_PERIODIC_WEAPON_PERCENT_DAMAGE:
        {
            WeaponAttackType attackType = GetSpellInfo()->GetAttackType();

            damage = CalculatePct(caster->CalculateDamage(attackType, false, true), GetAmount());

            // Add melee damage bonuses (also check for negative)
            if (caster)
                damage = caster->MeleeDamageBonusDone(target, damage, attackType, DOT, GetSpellInfo());

            damage = target->MeleeDamageBonusTaken(caster, damage, attackType, DOT, GetSpellInfo());
            break;
        }
        case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
            // ceil obtained value, it may happen that 10 ticks for 10% damage may not kill owner
            damage = uint32(ceil(CalculatePct<float, float>(target->GetMaxHealth(), damage)));
            damage = target->SpellDamageBonusTaken(caster, GetSpellInfo(), damage, DOT);
            break;
        default:
            break;
    }

    bool crit = roll_chance_f(GetCritChanceFor(caster, target));
    if (crit)
        damage = Unit::SpellCriticalDamageBonus(caster, m_spellInfo, damage, target);

    // Calculate armor mitigation
    if (Unit::IsDamageReducedByArmor(GetSpellInfo()->GetSchoolMask(), GetSpellInfo()))
    {
        uint32 damageReducedArmor = Unit::CalcArmorReducedDamage(caster, target, damage, GetSpellInfo(), GetSpellInfo()->GetAttackType(), GetBase()->GetCasterLevel());
        cleanDamage.mitigated_damage += damage - damageReducedArmor;
        damage = damageReducedArmor;
    }

    if (!GetSpellInfo()->HasAttribute(SPELL_ATTR4_FIXED_DAMAGE))
    {
        if (GetSpellEffectInfo().IsTargetingArea() || GetSpellEffectInfo().IsAreaAuraEffect() || GetSpellEffectInfo().IsEffect(SPELL_EFFECT_PERSISTENT_AREA_AURA))
            damage = target->CalculateAOEAvoidance(damage, m_spellInfo->SchoolMask, GetBase()->GetCasterGUID());
    }

    int32 dmg = damage;
    if (!GetSpellInfo()->HasAttribute(SPELL_ATTR4_FIXED_DAMAGE) && caster && caster->CanApplyResilience())
        Unit::ApplyResilience(target, &dmg);
    damage = dmg;

    DamageInfo damageInfo(caster, target, damage, GetSpellInfo(), GetSpellInfo()->GetSchoolMask(), DOT, BASE_ATTACK);
    Unit::CalcAbsorbResist(damageInfo);
    damage = damageInfo.GetDamage();

    uint32 absorb = damageInfo.GetAbsorb();
    uint32 resist = damageInfo.GetResist();
    TC_LOG_DEBUG("spells.periodic", "PeriodicTick: %s attacked %s for %u dmg inflicted by %u absorb is %u",
        GetCasterGUID().ToString().c_str(), target->GetGUID().ToString().c_str(), damage, GetId(), absorb);

    Unit::DealDamageMods(caster, target, damage, &absorb);

    // Set trigger flag
    uint32 procAttacker = PROC_FLAG_DONE_PERIODIC;
    uint32 procVictim   = PROC_FLAG_TAKEN_PERIODIC;
    uint32 hitMask = damageInfo.GetHitMask();
    if (damage)
    {
        hitMask |= crit ? PROC_HIT_CRITICAL : PROC_HIT_NORMAL;
        procVictim |= PROC_FLAG_TAKEN_DAMAGE;
    }

    int32 overkill = damage - target->GetHealth();
    if (overkill < 0)
        overkill = 0;

    SpellPeriodicAuraLogInfo pInfo(this, damage, dmg, overkill, absorb, resist, 0.0f, crit);

    Unit::DealDamage(caster, target, damage, &cleanDamage, DOT, GetSpellInfo()->GetSchoolMask(), GetSpellInfo(), true);

    Unit::ProcSkillsAndAuras(caster, target, procAttacker, procVictim, PROC_SPELL_TYPE_DAMAGE, PROC_SPELL_PHASE_HIT, hitMask, nullptr, &damageInfo, nullptr);

    target->SendPeriodicAuraLog(&pInfo);
}

bool AuraEffect::IsAreaAuraEffect() const
{
    return GetSpellEffectInfo().IsAreaAuraEffect();
}

void AuraEffect::HandlePeriodicHealthLeechAuraTick(Unit* target, Unit* caster) const
{
    if (!target->IsAlive())
        return;

    if (target->HasUnitState(UNIT_STATE_ISOLATED) || target->IsImmunedToDamage(GetSpellInfo()))
    {
        SendTickImmune(target, caster);
        return;
    }

    // dynobj auras must always have a caster
    if (GetSpellEffectInfo().IsEffect(SPELL_EFFECT_PERSISTENT_AREA_AURA) &&
        ASSERT_NOTNULL(caster)->SpellHitResult(target, GetSpellInfo(), false) != SPELL_MISS_NONE)
        return;

    CleanDamage cleanDamage = CleanDamage(0, 0, GetSpellInfo()->GetAttackType(), MELEE_HIT_NORMAL);

    uint32 stackAmountForBonuses = !GetSpellEffectInfo().EffectAttributes.HasFlag(SpellEffectAttributes::NoScaleWithStack) ? GetBase()->GetStackAmount() : 1;

    // ignore negative values (can be result apply spellmods to aura damage
    uint32 damage = std::max(GetAmount(), 0);

    if (caster)
        damage = caster->SpellDamageBonusDone(target, GetSpellInfo(), damage, DOT, GetSpellEffectInfo(), stackAmountForBonuses);
    damage = target->SpellDamageBonusTaken(caster, GetSpellInfo(), damage, DOT);

    bool crit = roll_chance_f(GetCritChanceFor(caster, target));
    if (crit)
        damage = Unit::SpellCriticalDamageBonus(caster, m_spellInfo, damage, target);

    // Calculate armor mitigation
    if (Unit::IsDamageReducedByArmor(GetSpellInfo()->GetSchoolMask(), GetSpellInfo()))
    {
        uint32 damageReducedArmor = Unit::CalcArmorReducedDamage(caster, target, damage, GetSpellInfo(), GetSpellInfo()->GetAttackType(), GetBase()->GetCasterLevel());
        cleanDamage.mitigated_damage += damage - damageReducedArmor;
        damage = damageReducedArmor;
    }

    if (!GetSpellInfo()->HasAttribute(SPELL_ATTR4_FIXED_DAMAGE))
    {
        if (GetSpellEffectInfo().IsTargetingArea() || GetSpellEffectInfo().IsAreaAuraEffect() || GetSpellEffectInfo().IsEffect(SPELL_EFFECT_PERSISTENT_AREA_AURA))
            damage = target->CalculateAOEAvoidance(damage, m_spellInfo->SchoolMask, GetBase()->GetCasterGUID());
    }

    int32 dmg = damage;
    if (!GetSpellInfo()->HasAttribute(SPELL_ATTR4_FIXED_DAMAGE) && caster && caster->CanApplyResilience())
        Unit::ApplyResilience(target, &dmg);
    damage = dmg;

    DamageInfo damageInfo(caster, target, damage, GetSpellInfo(), GetSpellInfo()->GetSchoolMask(), DOT, GetSpellInfo()->GetAttackType());
    Unit::CalcAbsorbResist(damageInfo);

    uint32 absorb = damageInfo.GetAbsorb();
    uint32 resist = damageInfo.GetResist();
    TC_LOG_DEBUG("spells.periodic", "PeriodicTick: %s health leech of %s for %u dmg inflicted by %u abs is %u",
        GetCasterGUID().ToString().c_str(), target->GetGUID().ToString().c_str(), damage, GetId(), absorb);

    // SendSpellNonMeleeDamageLog expects non-absorbed/non-resisted damage
    SpellNonMeleeDamage log(caster, target, GetSpellInfo(), GetBase()->GetSpellVisual(), GetSpellInfo()->GetSchoolMask(), GetBase()->GetCastId());
    log.damage = damage;
    log.originalDamage = dmg;
    log.absorb = absorb;
    log.resist = resist;
    log.periodicLog = true;
    if (crit)
        log.HitInfo |= SPELL_HIT_TYPE_CRIT;

    // Set trigger flag
    uint32 procAttacker = PROC_FLAG_DONE_PERIODIC;
    uint32 procVictim   = PROC_FLAG_TAKEN_PERIODIC;
    uint32 hitMask = damageInfo.GetHitMask();
    if (damage)
    {
        hitMask |= crit ? PROC_HIT_CRITICAL : PROC_HIT_NORMAL;
        procVictim |= PROC_FLAG_TAKEN_DAMAGE;
    }

    int32 new_damage = Unit::DealDamage(caster, target, damage, &cleanDamage, DOT, GetSpellInfo()->GetSchoolMask(), GetSpellInfo(), false);
    Unit::ProcSkillsAndAuras(caster, target, procAttacker, procVictim, PROC_SPELL_TYPE_DAMAGE, PROC_SPELL_PHASE_HIT, hitMask, nullptr, &damageInfo, nullptr);

    // process caster heal from now on (must be in world)
    if (!caster || !caster->IsAlive())
        return;

    float gainMultiplier = GetSpellEffectInfo().CalcValueMultiplier(caster);

    uint32 heal = caster->SpellHealingBonusDone(caster, GetSpellInfo(), uint32(new_damage * gainMultiplier), DOT, GetSpellEffectInfo(), stackAmountForBonuses);
    heal = caster->SpellHealingBonusTaken(caster, GetSpellInfo(), heal, DOT);

    HealInfo healInfo(caster, caster, heal, GetSpellInfo(), GetSpellInfo()->GetSchoolMask());
    caster->HealBySpell(healInfo);

    caster->GetThreatManager().ForwardThreatForAssistingMe(caster, healInfo.GetEffectiveHeal() * 0.5f, GetSpellInfo());
    Unit::ProcSkillsAndAuras(caster, caster, PROC_FLAG_DONE_PERIODIC, PROC_FLAG_TAKEN_PERIODIC, PROC_SPELL_TYPE_HEAL, PROC_SPELL_PHASE_HIT, hitMask, nullptr, nullptr, &healInfo);

    caster->SendSpellNonMeleeDamageLog(&log);
}

void AuraEffect::HandlePeriodicHealthFunnelAuraTick(Unit* target, Unit* caster) const
{
    if (!caster || !caster->IsAlive() || !target->IsAlive())
        return;

    if (target->HasUnitState(UNIT_STATE_ISOLATED))
    {
        SendTickImmune(target, caster);
        return;
    }

    uint32 damage = std::max(GetAmount(), 0);
    // do not kill health donator
    if (caster->GetHealth() < damage)
        damage = caster->GetHealth() - 1;
    if (!damage)
        return;

    caster->ModifyHealth(-(int32)damage);
    TC_LOG_DEBUG("spells", "PeriodicTick: donator %u target %u damage %u.", caster->GetEntry(), target->GetEntry(), damage);

    float gainMultiplier = GetSpellEffectInfo().CalcValueMultiplier(caster);

    damage = int32(damage * gainMultiplier);

    HealInfo healInfo(caster, target, damage, GetSpellInfo(), GetSpellInfo()->GetSchoolMask());
    caster->HealBySpell(healInfo);
    Unit::ProcSkillsAndAuras(caster, target, PROC_FLAG_DONE_PERIODIC, PROC_FLAG_TAKEN_PERIODIC, PROC_SPELL_TYPE_HEAL, PROC_SPELL_PHASE_HIT, PROC_HIT_NORMAL, nullptr, nullptr, &healInfo);
}

void AuraEffect::HandlePeriodicHealAurasTick(Unit* target, Unit* caster) const
{
    if (!target->IsAlive())
        return;

    if (target->HasUnitState(UNIT_STATE_ISOLATED))
    {
        SendTickImmune(target, caster);
        return;
    }

    // heal for caster damage (must be alive)
    if (target != caster && GetSpellInfo()->HasAttribute(SPELL_ATTR2_HEALTH_FUNNEL) && (!caster || !caster->IsAlive()))
        return;

    // don't regen when permanent aura target has full power
    if (GetBase()->IsPermanent() && target->IsFullHealth())
        return;

    uint32 stackAmountForBonuses = !GetSpellEffectInfo().EffectAttributes.HasFlag(SpellEffectAttributes::NoScaleWithStack) ? GetBase()->GetStackAmount() : 1;

    // ignore negative values (can be result apply spellmods to aura damage
    uint32 damage = std::max(GetAmount(), 0);

    if (GetAuraType() == SPELL_AURA_OBS_MOD_HEALTH)
        damage = uint32(target->CountPctFromMaxHealth(damage));
    else if (caster)
        damage = caster->SpellHealingBonusDone(target, GetSpellInfo(), damage, DOT, GetSpellEffectInfo(), stackAmountForBonuses);

    damage = target->SpellHealingBonusTaken(caster, GetSpellInfo(), damage, DOT);

    bool crit = roll_chance_f(GetCritChanceFor(caster, target));
    if (crit)
        damage = Unit::SpellCriticalHealingBonus(caster, m_spellInfo, damage, target);

    TC_LOG_DEBUG("spells.periodic", "PeriodicTick: %s heal of %s for %u health inflicted by %u",
        GetCasterGUID().ToString().c_str(), target->GetGUID().ToString().c_str(), damage, GetId());

    uint32 heal = damage;

    HealInfo healInfo(caster, target, damage, GetSpellInfo(), GetSpellInfo()->GetSchoolMask());
    Unit::CalcHealAbsorb(healInfo);
    Unit::DealHeal(healInfo);

    SpellPeriodicAuraLogInfo pInfo(this, heal, damage, heal - healInfo.GetEffectiveHeal(), healInfo.GetAbsorb(), 0, 0.0f, crit);
    target->SendPeriodicAuraLog(&pInfo);

    if (caster)
        target->GetThreatManager().ForwardThreatForAssistingMe(caster, healInfo.GetEffectiveHeal() * 0.5f, GetSpellInfo());

    // %-based heal - does not proc auras
    if (GetAuraType() == SPELL_AURA_OBS_MOD_HEALTH)
        return;

    uint32 procAttacker = PROC_FLAG_DONE_PERIODIC;
    uint32 procVictim   = PROC_FLAG_TAKEN_PERIODIC;
    uint32 hitMask = crit ? PROC_HIT_CRITICAL : PROC_HIT_NORMAL;
    // ignore item heals
    if (GetBase()->GetCastItemGUID().IsEmpty())
        Unit::ProcSkillsAndAuras(caster, target, procAttacker, procVictim, PROC_SPELL_TYPE_HEAL, PROC_SPELL_PHASE_HIT, hitMask, nullptr, nullptr, &healInfo);
}

void AuraEffect::HandlePeriodicManaLeechAuraTick(Unit* target, Unit* caster) const
{
    Powers powerType = Powers(GetMiscValue());

    if (!caster || !caster->IsAlive() || !target->IsAlive() || target->GetPowerType() != powerType)
        return;

    if (target->HasUnitState(UNIT_STATE_ISOLATED) || target->IsImmunedToDamage(GetSpellInfo()))
    {
        SendTickImmune(target, caster);
        return;
    }

    if (GetSpellEffectInfo().IsEffect(SPELL_EFFECT_PERSISTENT_AREA_AURA) &&
        caster->SpellHitResult(target, GetSpellInfo(), false) != SPELL_MISS_NONE)
        return;

    // ignore negative values (can be result apply spellmods to aura damage
    int32 drainAmount = std::max(GetAmount(), 0);

    TC_LOG_DEBUG("spells.periodic", "PeriodicTick: %s power leech of %s for %u dmg inflicted by %u",
        GetCasterGUID().ToString().c_str(), target->GetGUID().ToString().c_str(), drainAmount, GetId());

    int32 drainedAmount = -target->ModifyPower(powerType, -drainAmount);

    float gainMultiplier = GetSpellEffectInfo().CalcValueMultiplier(caster);

    SpellPeriodicAuraLogInfo pInfo(this, drainedAmount, drainAmount, 0, 0, 0, gainMultiplier, false);

    int32 gainAmount = int32(drainedAmount * gainMultiplier);
    int32 gainedAmount = 0;
    if (gainAmount)
    {
        gainedAmount = caster->ModifyPower(powerType, gainAmount);
        // energize is not modified by threat modifiers
        target->GetThreatManager().AddThreat(caster, float(gainedAmount) * 0.5f, GetSpellInfo(), true);
    }

    // Drain Mana - Mana Feed effect
    if (caster->GetGuardianPet() && m_spellInfo->SpellFamilyName == SPELLFAMILY_WARLOCK && m_spellInfo->SpellFamilyFlags[0] & 0x00000010)
    {
        int32 manaFeedVal = 0;
        if (AuraEffect const* aurEff = GetBase()->GetEffect(EFFECT_1))
            manaFeedVal = aurEff->GetAmount();

        if (manaFeedVal > 0)
        {
            int32 feedAmount = CalculatePct(gainedAmount, manaFeedVal);

            CastSpellExtraArgs args(this);
            args.AddSpellMod(SPELLVALUE_BASE_POINT0, feedAmount);
            caster->CastSpell(caster, 32554, args);
        }
    }

    target->SendPeriodicAuraLog(&pInfo);
}

void AuraEffect::HandleObsModPowerAuraTick(Unit* target, Unit* caster) const
{
    Powers powerType;
    if (GetMiscValue() == POWER_ALL)
        powerType = target->GetPowerType();
    else
        powerType = Powers(GetMiscValue());

    if (!target->IsAlive() || !target->GetMaxPower(powerType))
        return;

    if (target->HasUnitState(UNIT_STATE_ISOLATED))
    {
        SendTickImmune(target, caster);
        return;
    }

    // don't regen when permanent aura target has full power
    if (GetBase()->IsPermanent() && target->GetPower(powerType) == target->GetMaxPower(powerType))
        return;

    // ignore negative values (can be result apply spellmods to aura damage
    uint32 amount = std::max(GetAmount(), 0) * target->GetMaxPower(powerType) /100;
    TC_LOG_DEBUG("spells.periodic", "PeriodicTick: %s energize %s for %u dmg inflicted by %u",
        GetCasterGUID().ToString().c_str(), target->GetGUID().ToString().c_str(), amount, GetId());

    SpellPeriodicAuraLogInfo pInfo(this, amount, amount, 0, 0, 0, 0.0f, false);
    int32 gain = target->ModifyPower(powerType, amount);

    if (caster)
        target->GetThreatManager().ForwardThreatForAssistingMe(caster, float(gain)*0.5f, GetSpellInfo(), true);

    target->SendPeriodicAuraLog(&pInfo);
}

void AuraEffect::HandlePeriodicEnergizeAuraTick(Unit* target, Unit* caster) const
{
    Powers powerType = Powers(GetMiscValue());
    if (!target->IsAlive() || !target->GetMaxPower(powerType))
        return;

    if (target->HasUnitState(UNIT_STATE_ISOLATED))
    {
        SendTickImmune(target, caster);
        return;
    }

    // don't regen when permanent aura target has full power
    if (GetBase()->IsPermanent() && target->GetPower(powerType) == target->GetMaxPower(powerType))
        return;

    // ignore negative values (can be result apply spellmods to aura damage
    int32 amount = std::max(GetAmount(), 0);

    SpellPeriodicAuraLogInfo pInfo(this, amount, amount, 0, 0, 0, 0.0f, false);

    TC_LOG_DEBUG("spells.periodic", "PeriodicTick: %s energize %s for %u dmg inflicted by %u",
        GetCasterGUID().ToString().c_str(), target->GetGUID().ToString().c_str(), amount, GetId());

    int32 gain = target->ModifyPower(powerType, amount);
    target->SendPeriodicAuraLog(&pInfo);

    if (caster)
        target->GetThreatManager().ForwardThreatForAssistingMe(caster, float(gain)*0.5f, GetSpellInfo(), true);
}

void AuraEffect::HandlePeriodicPowerBurnAuraTick(Unit* target, Unit* caster) const
{
    Powers powerType = Powers(GetMiscValue());

    if (!caster || !target->IsAlive() || target->GetPowerType() != powerType)
        return;

    if (target->HasUnitState(UNIT_STATE_ISOLATED) || target->IsImmunedToDamage(GetSpellInfo()))
    {
        SendTickImmune(target, caster);
        return;
    }

    // ignore negative values (can be result apply spellmods to aura damage
    int32 damage = std::max(GetAmount(), 0);

    uint32 gain = uint32(-target->ModifyPower(powerType, -damage));

    float dmgMultiplier = GetSpellEffectInfo().CalcValueMultiplier(caster);

    SpellInfo const* spellProto = GetSpellInfo();
    // maybe has to be sent different to client, but not by SMSG_PERIODICAURALOG
    SpellNonMeleeDamage damageInfo(caster, target, spellProto, GetBase()->GetSpellVisual(), spellProto->SchoolMask, GetBase()->GetCastId());
    // no SpellDamageBonus for burn mana
    caster->CalculateSpellDamageTaken(&damageInfo, int32(gain * dmgMultiplier), spellProto);

    Unit::DealDamageMods(damageInfo.attacker, damageInfo.target, damageInfo.damage, &damageInfo.absorb);

    // Set trigger flag
    uint32 procAttacker = PROC_FLAG_DONE_PERIODIC;
    uint32 procVictim   = PROC_FLAG_TAKEN_PERIODIC;
    uint32 hitMask      = createProcHitMask(&damageInfo, SPELL_MISS_NONE);
    uint32 spellTypeMask = PROC_SPELL_TYPE_NO_DMG_HEAL;
    if (damageInfo.damage)
    {
        procVictim |= PROC_FLAG_TAKEN_DAMAGE;
        spellTypeMask |= PROC_SPELL_TYPE_DAMAGE;
    }

    caster->DealSpellDamage(&damageInfo, true);

    DamageInfo dotDamageInfo(damageInfo, DOT, BASE_ATTACK, hitMask);
    Unit::ProcSkillsAndAuras(caster, target, procAttacker, procVictim, spellTypeMask, PROC_SPELL_PHASE_HIT, hitMask, nullptr, &dotDamageInfo, nullptr);

    caster->SendSpellNonMeleeDamageLog(&damageInfo);
}

bool AuraEffect::CanPeriodicTickCrit() const
{
    if (GetSpellInfo()->HasAttribute(SPELL_ATTR2_CANT_CRIT))
        return false;

    return true;
}

float AuraEffect::CalcPeriodicCritChance(Unit const* caster) const
{
    if (!caster || !CanPeriodicTickCrit())
        return 0.0f;

    Player* modOwner = caster->GetSpellModOwner();
    if (!modOwner)
        return 0.0f;

    float critChance = modOwner->SpellCritChanceDone(nullptr, this, GetSpellInfo()->GetSchoolMask(), GetSpellInfo()->GetAttackType());
    return std::max(0.0f, critChance);
}

void AuraEffect::HandleBreakableCCAuraProc(AuraApplication* aurApp, ProcEventInfo& eventInfo)
{
    int32 const damageLeft = GetAmount() - static_cast<int32>(eventInfo.GetDamageInfo()->GetDamage());

    if (damageLeft <= 0)
        aurApp->GetTarget()->RemoveAura(aurApp);
    else
        ChangeAmount(damageLeft);
}

void AuraEffect::HandleProcTriggerSpellAuraProc(AuraApplication* aurApp, ProcEventInfo& eventInfo)
{
    Unit* triggerCaster = aurApp->GetTarget();
    Unit* triggerTarget = eventInfo.GetProcTarget();

    uint32 triggerSpellId = GetSpellEffectInfo().TriggerSpell;
    if (SpellInfo const* triggeredSpellInfo = sSpellMgr->GetSpellInfo(triggerSpellId, GetBase()->GetCastDifficulty()))
    {
        TC_LOG_DEBUG("spells", "AuraEffect::HandleProcTriggerSpellAuraProc: Triggering spell %u from aura %u proc", triggeredSpellInfo->Id, GetId());
        triggerCaster->CastSpell(triggerTarget, triggeredSpellInfo->Id, this);
    }
    else if (triggerSpellId && GetAuraType() != SPELL_AURA_DUMMY)
        TC_LOG_ERROR("spells","AuraEffect::HandleProcTriggerSpellAuraProc: Could not trigger spell %u from aura %u proc, because the spell does not have an entry in Spell.dbc.", triggerSpellId, GetId());
}

void AuraEffect::HandleProcTriggerSpellWithValueAuraProc(AuraApplication* aurApp, ProcEventInfo& eventInfo)
{
    Unit* triggerCaster = aurApp->GetTarget();
    Unit* triggerTarget = eventInfo.GetProcTarget();

    uint32 triggerSpellId = GetSpellEffectInfo().TriggerSpell;
    if (SpellInfo const* triggeredSpellInfo = sSpellMgr->GetSpellInfo(triggerSpellId, GetBase()->GetCastDifficulty()))
    {
        CastSpellExtraArgs args(this);
        args.AddSpellMod(SPELLVALUE_BASE_POINT0, GetAmount());
        triggerCaster->CastSpell(triggerTarget, triggerSpellId, args);
        TC_LOG_DEBUG("spells", "AuraEffect::HandleProcTriggerSpellWithValueAuraProc: Triggering spell %u with value %d from aura %u proc", triggeredSpellInfo->Id, GetAmount(), GetId());
    }
    else
        TC_LOG_ERROR("spells","AuraEffect::HandleProcTriggerSpellWithValueAuraProc: Could not trigger spell %u from aura %u proc, because the spell does not have an entry in Spell.dbc.", triggerSpellId, GetId());
}

void AuraEffect::HandleProcTriggerDamageAuraProc(AuraApplication* aurApp, ProcEventInfo& eventInfo)
{
    Unit* target = aurApp->GetTarget();
    Unit* triggerTarget = eventInfo.GetProcTarget();
    if (triggerTarget->HasUnitState(UNIT_STATE_ISOLATED) || triggerTarget->IsImmunedToDamage(GetSpellInfo()))
    {
        SendTickImmune(triggerTarget, target);
        return;
    }

    SpellNonMeleeDamage damageInfo(target, triggerTarget, GetSpellInfo(), GetBase()->GetSpellVisual(), GetSpellInfo()->SchoolMask, GetBase()->GetCastId());
    uint32 damage = target->SpellDamageBonusDone(triggerTarget, GetSpellInfo(), GetAmount(), SPELL_DIRECT_DAMAGE, GetSpellEffectInfo());
    damage = triggerTarget->SpellDamageBonusTaken(target, GetSpellInfo(), damage, SPELL_DIRECT_DAMAGE);
    target->CalculateSpellDamageTaken(&damageInfo, damage, GetSpellInfo());
    Unit::DealDamageMods(damageInfo.attacker, damageInfo.target, damageInfo.damage, &damageInfo.absorb);
    TC_LOG_DEBUG("spells", "AuraEffect::HandleProcTriggerDamageAuraProc: Triggering %u spell damage from aura %u proc", damage, GetId());
    target->DealSpellDamage(&damageInfo, true);
    target->SendSpellNonMeleeDamageLog(&damageInfo);
}

void AuraEffect::HandleAuraForceWeather(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Player* target = aurApp->GetTarget()->ToPlayer();

    if (!target)
        return;

    if (apply)
        target->SendDirectMessage(WorldPackets::Misc::Weather(WeatherState(GetMiscValue()), 1.0f).Write());
    else
        target->GetMap()->SendZoneWeather(target->GetZoneId(), target);
}

void AuraEffect::HandleEnableAltPower(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    uint32 altPowerId = GetMiscValue();
    UnitPowerBarEntry const* powerEntry = sUnitPowerBarStore.LookupEntry(altPowerId);
    if (!powerEntry)
        return;

    if (apply)
        aurApp->GetTarget()->SetMaxPower(POWER_ALTERNATE_POWER, powerEntry->MaxPower);
    else
        aurApp->GetTarget()->SetMaxPower(POWER_ALTERNATE_POWER, 0);
}

void AuraEffect::HandleModSpellCategoryCooldown(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    if (Player* player = aurApp->GetTarget()->ToPlayer())
        player->SendSpellCategoryCooldowns();
}

void AuraEffect::HandleShowConfirmationPrompt(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Player* player = aurApp->GetTarget()->ToPlayer();
    if (!player)
        return;

    if (apply)
        player->AddTemporarySpell(GetSpellEffectInfo().TriggerSpell);
    else
        player->RemoveTemporarySpell(GetSpellEffectInfo().TriggerSpell);
}

void AuraEffect::HandleOverridePetSpecs(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Player* player = aurApp->GetTarget()->ToPlayer();
    if (!player)
        return;

    if (player->getClass() != CLASS_HUNTER)
        return;

    Pet* pet = player->GetPet();
    if (!pet)
        return;

    ChrSpecializationEntry const* currSpec = sChrSpecializationStore.LookupEntry(pet->GetSpecialization());
    if (!currSpec)
        return;

    pet->SetSpecialization(sDB2Manager.GetChrSpecializationByIndex(apply ? PET_SPEC_OVERRIDE_CLASS_INDEX : 0, currSpec->OrderIndex)->ID);
}

void AuraEffect::HandleAllowUsingGameobjectsWhileMounted(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Player* target = aurApp->GetTarget()->ToPlayer();
    if (!target)
        return;

    if (apply)
        target->AddPlayerLocalFlag(PLAYER_LOCAL_FLAG_CAN_USE_OBJECTS_MOUNTED);
    else if (!target->HasAuraType(SPELL_AURA_ALLOW_USING_GAMEOBJECTS_WHILE_MOUNTED))
        target->RemovePlayerLocalFlag(PLAYER_LOCAL_FLAG_CAN_USE_OBJECTS_MOUNTED);
}

void AuraEffect::HandlePlayScene(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Player* player = aurApp->GetTarget()->ToPlayer();
    if (!player)
        return;

    if (apply)
        player->GetSceneMgr().PlayScene(GetMiscValue());
    else
        player->GetSceneMgr().CancelSceneBySceneId(GetMiscValue());
}

void AuraEffect::HandleCreateAreaTrigger(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();

    if (apply)
    {
        AreaTrigger::CreateAreaTrigger(GetMiscValue(), GetCaster(), target, GetSpellInfo(), *target, GetBase()->GetDuration(), GetBase()->GetSpellVisual(), ObjectGuid::Empty, this);
    }
    else
    {
        if (Unit* caster = GetCaster())
            caster->RemoveAreaTrigger(this);
    }
}

void AuraEffect::HandleAuraPvpTalents(AuraApplication const* auraApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    if (Player* target = auraApp->GetTarget()->ToPlayer())
    {
        if (apply)
            target->TogglePvpTalents(true);
        else if (!target->HasAuraType(SPELL_AURA_PVP_TALENTS))
            target->TogglePvpTalents(false);
    }
}

void AuraEffect::HandleLinkedSummon(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Unit* target = aurApp->GetTarget();
    SpellInfo const* triggerSpellInfo = sSpellMgr->GetSpellInfo(GetSpellEffectInfo().TriggerSpell, GetBase()->GetCastDifficulty());
    if (!triggerSpellInfo)
        return;

    // on apply cast summon spell
    if (apply)
    {
        CastSpellExtraArgs args(this);
        args.CastDifficulty = triggerSpellInfo->Difficulty;
        target->CastSpell(target, triggerSpellInfo->Id, args);
    }
    // on unapply we need to search for and remove the summoned creature
    else
    {
        std::vector<uint32> summonedEntries;
        for (SpellEffectInfo const& spellEffect : triggerSpellInfo->GetEffects())
            if (spellEffect.IsEffect(SPELL_EFFECT_SUMMON))
                if (uint32 summonEntry = spellEffect.MiscValue)
                    summonedEntries.push_back(summonEntry);

        // we don't know if there can be multiple summons for the same effect, so consider only 1 summon for each effect
        // most of the spells have multiple effects with the same summon spell id for multiple spawns, so right now it's safe to assume there's only 1 spawn per effect
        for (uint32 summonEntry : summonedEntries)
        {
            std::list<Creature*> nearbyEntries;
            target->GetCreatureListWithEntryInGrid(nearbyEntries, summonEntry);
            for (auto creature : nearbyEntries)
            {
                if (creature->GetOwner() == target)
                {
                    creature->DespawnOrUnsummon();
                    break;
                }
                else if (TempSummon* tempSummon = creature->ToTempSummon())
                {
                    if (tempSummon->GetSummoner() == target)
                    {
                        tempSummon->DespawnOrUnsummon();
                        break;
                    }
                }
            }
        }
    }
}

void AuraEffect::HandleSetFFAPvP(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Player* target = aurApp->GetTarget()->ToPlayer();
    if (!target)
        return;

    target->UpdatePvPState(true);
}

void AuraEffect::HandleModOverrideZonePVPType(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Player* target = aurApp->GetTarget()->ToPlayer();
    if (!target)
        return;

    if (apply)
        target->SetOverrideZonePVPType(ZonePVPTypeOverride(GetMiscValue()));
    else if (target->HasAuraType(SPELL_AURA_MOD_OVERRIDE_ZONE_PVP_TYPE))
        target->SetOverrideZonePVPType(ZonePVPTypeOverride(target->GetAuraEffectsByType(SPELL_AURA_MOD_OVERRIDE_ZONE_PVP_TYPE).back()->GetMiscValue()));
    else
        target->SetOverrideZonePVPType(ZonePVPTypeOverride::None);

    target->UpdateHostileAreaState(sAreaTableStore.LookupEntry(target->GetZoneId()));
    target->UpdatePvPState();
}

void AuraEffect::HandleBattlegroundPlayerPosition(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Player* target = aurApp->GetTarget()->ToPlayer();
    if (!target)
        return;

    BattlegroundMap* battlegroundMap = target->GetMap()->ToBattlegroundMap();
    if (!battlegroundMap)
        return;

    Battleground* bg = battlegroundMap->GetBG();
    if (!bg)
        return;

    if (apply)
    {
        WorldPackets::Battleground::BattlegroundPlayerPosition playerPosition;
        playerPosition.Guid = target->GetGUID();
        playerPosition.ArenaSlot = static_cast<uint8>(GetMiscValue());
        playerPosition.Pos = target->GetPosition();

        if (GetAuraType() == SPELL_AURA_BATTLEGROUND_PLAYER_POSITION_FACTIONAL)
            playerPosition.IconID = target->GetTeam() == ALLIANCE ? PLAYER_POSITION_ICON_HORDE_FLAG : PLAYER_POSITION_ICON_ALLIANCE_FLAG;
        else if (GetAuraType() == SPELL_AURA_BATTLEGROUND_PLAYER_POSITION)
            playerPosition.IconID = target->GetTeam() == ALLIANCE ? PLAYER_POSITION_ICON_ALLIANCE_FLAG : PLAYER_POSITION_ICON_HORDE_FLAG;
        else
            TC_LOG_WARN("spell.auras", "Unknown aura effect %u handled by HandleBattlegroundPlayerPosition.", GetAuraType());

        bg->AddPlayerPosition(playerPosition);
    }
    else
        bg->RemovePlayerPosition(target->GetGUID());
}

void AuraEffect::HandleStoreTeleportReturnPoint(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    Player* playerTarget = aurApp->GetTarget()->ToPlayer();
    if (!playerTarget)
        return;

    if (apply)
        playerTarget->AddStoredAuraTeleportLocation(GetSpellInfo()->Id);
    else if (!playerTarget->GetSession()->isLogingOut())
        playerTarget->RemoveStoredAuraTeleportLocation(GetSpellInfo()->Id);
}

void AuraEffect::HandleCosmeticMounted(AuraApplication const* aurApp, uint8 mode, bool apply) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    if (apply)
        aurApp->GetTarget()->SetCosmeticMountDisplayId(GetMiscValue());
    else
        aurApp->GetTarget()->SetCosmeticMountDisplayId(0); // set cosmetic mount to 0, even if multiple auras are active; tested with zandalari racial + divine steed

    Player* playerTarget = aurApp->GetTarget()->ToPlayer();
    if (!playerTarget)
        return;

    playerTarget->SendMovementSetCollisionHeight(playerTarget->GetCollisionHeight(), WorldPackets::Movement::UpdateCollisionHeightReason::Force);
}

void AuraEffect::HandleSuppressItemPassiveEffectBySpellLabel(AuraApplication const* aurApp, uint8 mode, bool /*apply*/) const
{
    if (!(mode & AURA_EFFECT_HANDLE_REAL))
        return;

    std::vector<Aura*> suppressedAuras;
    for (Unit::AuraMap::value_type const& appliedAura : aurApp->GetTarget()->GetOwnedAuras())
        if (appliedAura.second->GetSpellInfo()->HasLabel(GetMiscValue()))
            suppressedAuras.push_back(appliedAura.second);

    // Refresh applications
    for (Aura* aura : suppressedAuras)
        aura->ApplyForTargets();
}

template TC_GAME_API void AuraEffect::GetTargetList(std::list<Unit*>&) const;
template TC_GAME_API void AuraEffect::GetTargetList(std::deque<Unit*>&) const;
template TC_GAME_API void AuraEffect::GetTargetList(std::vector<Unit*>&) const;

template TC_GAME_API void AuraEffect::GetApplicationList(std::list<AuraApplication*>&) const;
template TC_GAME_API void AuraEffect::GetApplicationList(std::deque<AuraApplication*>&) const;
template TC_GAME_API void AuraEffect::GetApplicationList(std::vector<AuraApplication*>&) const;
