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

#ifndef BattlePetMgr_h__
#define BattlePetMgr_h__

#include "BattlePetPackets.h"
#include "DatabaseEnvFwd.h"
#include "EnumFlag.h"
#include <unordered_map>

struct BattlePetSpeciesEntry;

enum BattlePetMisc
{
    MAX_PET_BATTLE_SLOTS                = 3,
    DEFAULT_MAX_BATTLE_PETS_PER_SPECIES = 3,
    BATTLE_PET_CAGE_ITEM_ID             = 82800,
    DEFAULT_SUMMON_BATTLE_PET_SPELL     = 118301,
    SPELL_VISUAL_UNCAGE_PET             = 222
};

enum class BattlePetBreedQuality : uint8
{
    Poor       = 0,
    Common     = 1,
    Uncommon   = 2,
    Rare       = 3,
    Epic       = 4,
    Legendary  = 5,

    Count
};

enum class BattlePetDbFlags : uint16
{
    None                = 0x000,
    Favorite            = 0x001,
    Converted           = 0x002,
    Revoked             = 0x004,
    LockedForConvert    = 0x008,
    Ability0Selection   = 0x010,
    Ability1Selection   = 0x020,
    Ability2Selection   = 0x040,
    FanfareNeeded       = 0x080,
    DisplayOverridden   = 0x100
};

DEFINE_ENUM_FLAG(BattlePetDbFlags);

// 6.2.4
enum FlagsControlType
{
    FLAGS_CONTROL_TYPE_APPLY        = 1,
    FLAGS_CONTROL_TYPE_REMOVE       = 2
};

// 6.2.4
enum BattlePetError
{
    BATTLEPETRESULT_CANT_HAVE_MORE_PETS_OF_THAT_TYPE = 3,
    BATTLEPETRESULT_CANT_HAVE_MORE_PETS              = 4,
    BATTLEPETRESULT_TOO_HIGH_LEVEL_TO_UNCAGE         = 7,

    // TODO: find correct values if possible and needed (also wrong order)
    BATTLEPETRESULT_DUPLICATE_CONVERTED_PET,
    BATTLEPETRESULT_NEED_TO_UNLOCK,
    BATTLEPETRESULT_BAD_PARAM,
    BATTLEPETRESULT_LOCKED_PET_ALREADY_EXISTS,
    BATTLEPETRESULT_OK,
    BATTLEPETRESULT_UNCAPTURABLE,
    BATTLEPETRESULT_CANT_INVALID_CHARACTER_GUID
};

// taken from BattlePetState.db2 - it seems to store some initial values for battle pets
// there are only values used in BattlePetSpeciesState.db2 and BattlePetBreedState.db2
// TODO: expand this enum if needed
enum BattlePetState
{
    STATE_MAX_HEALTH_BONUS          = 2,
    STATE_INTERNAL_INITIAL_LEVEL    = 17,
    STATE_STAT_POWER                = 18,
    STATE_STAT_STAMINA              = 19,
    STATE_STAT_SPEED                = 20,
    STATE_MOD_DAMAGE_DEALT_PERCENT  = 23,
    STATE_GENDER                    = 78, // 1 - male, 2 - female
    STATE_COSMETIC_WATER_BUBBLED    = 85,
    STATE_SPECIAL_IS_COCKROACH      = 93,
    STATE_COSMETIC_FLY_TIER         = 128,
    STATE_COSMETIC_BIGGLESWORTH     = 144,
    STATE_PASSIVE_ELITE             = 153,
    STATE_PASSIVE_BOSS              = 162,
    STATE_COSMETIC_TREASURE_GOBLIN  = 176,
    // these are not in BattlePetState.db2 but are used in BattlePetSpeciesState.db2
    STATE_START_WITH_BUFF           = 183,
    STATE_START_WITH_BUFF_2         = 184,
    //
    STATE_COSMETIC_SPECTRAL_BLUE    = 196
};

enum BattlePetSaveInfo
{
    BATTLE_PET_UNCHANGED = 0,
    BATTLE_PET_CHANGED   = 1,
    BATTLE_PET_NEW       = 2,
    BATTLE_PET_REMOVED   = 3
};

class BattlePetMgr
{
public:
    struct BattlePet
    {
        void CalculateStats();

        WorldPackets::BattlePet::BattlePet PacketInfo;
        std::unique_ptr<DeclinedName> DeclinedName;
        BattlePetSaveInfo SaveInfo = BATTLE_PET_UNCHANGED;
    };

    explicit BattlePetMgr(WorldSession* owner);

    static void Initialize();

    static uint16 RollPetBreed(uint32 species);
    static BattlePetBreedQuality GetDefaultPetQuality(uint32 species);
    static uint32 SelectPetDisplay(BattlePetSpeciesEntry const* speciesEntry);

    void LoadFromDB(PreparedQueryResult pets, PreparedQueryResult slots);
    void SaveToDB(LoginDatabaseTransaction& trans);

    BattlePet* GetPet(ObjectGuid guid);
    void AddPet(uint32 species, uint32 display, uint16 breed, BattlePetBreedQuality quality, uint16 level = 1);
    void RemovePet(ObjectGuid guid);
    void ClearFanfare(ObjectGuid guid);
    void ModifyName(ObjectGuid guid, std::string const& name, DeclinedName* declinedName);
    bool IsPetInSlot(ObjectGuid guid);

    uint8 GetPetCount(uint32 species) const;
    bool HasMaxPetCount(BattlePetSpeciesEntry const* speciesEntry) const;
    uint32 GetPetUniqueSpeciesCount() const;

    WorldPackets::BattlePet::BattlePetSlot* GetSlot(uint8 slot) { return slot < _slots.size() ? &_slots[slot] : nullptr; }
    void UnlockSlot(uint8 slot);

    WorldSession* GetOwner() const { return _owner; }

    uint16 GetTrapLevel() const { return _trapLevel; }
    uint16 GetMaxPetLevel() const;
    std::vector<WorldPackets::BattlePet::BattlePetSlot> const& GetSlots() const { return _slots; }

    void CageBattlePet(ObjectGuid guid);
    void HealBattlePetsPct(uint8 pct);

    void SummonPet(ObjectGuid guid);
    void DismissPet();

    void SendJournal();
    void SendUpdates(std::vector<std::reference_wrapper<BattlePet>> pets, bool petAdded);
    void SendError(BattlePetError error, uint32 creatureId);

    bool HasJournalLock() const { return true; }

private:
    WorldSession* _owner;
    uint16 _trapLevel = 0;
    std::unordered_map<uint64 /*battlePetGuid*/, BattlePet> _pets;
    std::vector<WorldPackets::BattlePet::BattlePetSlot> _slots;

    static void LoadAvailablePetBreeds();
    static void LoadDefaultPetQualities();

    // hash no longer required in C++14
    static std::unordered_map<uint16 /*BreedID*/, std::unordered_map<BattlePetState /*state*/, int32 /*value*/, std::hash<std::underlying_type<BattlePetState>::type> >> _battlePetBreedStates;
    static std::unordered_map<uint32 /*SpeciesID*/, std::unordered_map<BattlePetState /*state*/, int32 /*value*/, std::hash<std::underlying_type<BattlePetState>::type> >> _battlePetSpeciesStates;
    static std::unordered_map<uint32 /*SpeciesID*/, std::unordered_set<uint8 /*breed*/>> _availableBreedsPerSpecies;
    static std::unordered_map<uint32 /*SpeciesID*/, uint8 /*quality*/> _defaultQualityPerSpecies;
};

#endif // BattlePetMgr_h__
