#ifndef TURTLE_LUA_ENGINE_H
#define TURTLE_LUA_ENGINE_H

#include "Common.h"
#include "ObjectGuid.h"

#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

struct lua_State;
class Player;
class Creature;
class GameObject;
class Corpse;
class DynamicObject;
class Item;
class Unit;
class WorldObject;
class Quest;
class Group;
class Guild;
class Channel;
class Map;
class BattleGround;
class GmTicket;
class Spell;
class Roll;
class InstanceData;
class WorldPacket;
class WorldSession;
class SpellCastTargets;
class ObjectGuid;
class QueryNamedResult;
struct AuctionEntry;
struct ItemPrototype;

enum TurtleLuaPlayerEvents
{
    PLAYER_EVENT_ON_CHARACTER_CREATE = 1,
    PLAYER_EVENT_ON_CHARACTER_DELETE = 2,
    PLAYER_EVENT_ON_LOGIN = 3,
    PLAYER_EVENT_ON_LOGOUT = 4,
    PLAYER_EVENT_ON_SPELL_CAST = 5,
    PLAYER_EVENT_ON_KILL_PLAYER = 6,
    PLAYER_EVENT_ON_KILL_CREATURE = 7,
    PLAYER_EVENT_ON_KILLED_BY_CREATURE = 8,
    PLAYER_EVENT_ON_DUEL_REQUEST = 9,
    PLAYER_EVENT_ON_DUEL_START = 10,
    PLAYER_EVENT_ON_DUEL_END = 11,
    PLAYER_EVENT_ON_GIVE_XP = 12,
    PLAYER_EVENT_ON_LEVEL_CHANGE = 13,
    PLAYER_EVENT_ON_MONEY_CHANGE = 14,
    PLAYER_EVENT_ON_REPUTATION_CHANGE = 15,
    PLAYER_EVENT_ON_TALENTS_CHANGE = 16,
    PLAYER_EVENT_ON_TALENTS_RESET = 17,
    PLAYER_EVENT_ON_CHAT = 18,
    PLAYER_EVENT_ON_WHISPER = 19,
    PLAYER_EVENT_ON_GROUP_CHAT = 20,
    PLAYER_EVENT_ON_GUILD_CHAT = 21,
    PLAYER_EVENT_ON_CHANNEL_CHAT = 22,
    PLAYER_EVENT_ON_EMOTE = 23,
    PLAYER_EVENT_ON_TEXT_EMOTE = 24,
    PLAYER_EVENT_ON_SAVE = 25,
    PLAYER_EVENT_ON_BIND_TO_INSTANCE = 26,
    PLAYER_EVENT_ON_UPDATE_ZONE = 27,
    PLAYER_EVENT_ON_MAP_CHANGE = 28,
    PLAYER_EVENT_ON_EQUIP = 29,
    PLAYER_EVENT_ON_FIRST_LOGIN = 30,
    PLAYER_EVENT_ON_CAN_USE_ITEM = 31,
    PLAYER_EVENT_ON_LOOT_ITEM = 32,
    PLAYER_EVENT_ON_ENTER_COMBAT = 33,
    PLAYER_EVENT_ON_LEAVE_COMBAT = 34,
    PLAYER_EVENT_ON_REPOP = 35,
    PLAYER_EVENT_ON_RESURRECT = 36,
    PLAYER_EVENT_ON_LOOT_MONEY = 37,
    PLAYER_EVENT_ON_QUEST_ABANDON = 38,
    PLAYER_EVENT_ON_LEARN_TALENTS = 39,
    PLAYER_EVENT_ON_COMMAND = 42,
    PLAYER_EVENT_ON_PET_ADDED_TO_WORLD = 43,
    PLAYER_EVENT_ON_LEARN_SPELL = 44,
    PLAYER_EVENT_ON_ACHIEVEMENT_COMPLETE = 45,
    PLAYER_EVENT_ON_FFAPVP_CHANGE = 46,
    PLAYER_EVENT_ON_UPDATE_AREA = 47,
    PLAYER_EVENT_ON_CAN_INIT_TRADE = 48,
    PLAYER_EVENT_ON_CAN_SEND_MAIL = 49,
    PLAYER_EVENT_ON_CAN_JOIN_LFG = 50,
    PLAYER_EVENT_ON_QUEST_REWARD_ITEM = 51,
    PLAYER_EVENT_ON_CREATE_ITEM = 52,
    PLAYER_EVENT_ON_STORE_NEW_ITEM = 53,
    PLAYER_EVENT_ON_COMPLETE_QUEST = 54,
    PLAYER_EVENT_ON_CAN_GROUP_INVITE = 55,
    PLAYER_EVENT_ON_GROUP_ROLL_REWARD_ITEM = 56,
    PLAYER_EVENT_ON_BG_DESERTION = 57,
    PLAYER_EVENT_ON_PET_KILL = 58,
    PLAYER_EVENT_ON_CAN_RESURRECT = 59,
    PLAYER_EVENT_ON_CAN_UPDATE_SKILL = 60,
    PLAYER_EVENT_ON_BEFORE_UPDATE_SKILL = 61,
    PLAYER_EVENT_ON_UPDATE_SKILL = 62,
};

enum TurtleLuaServerEvents
{
    SERVER_EVENT_ON_PACKET_RECEIVE = 5,
    SERVER_EVENT_ON_PACKET_RECEIVE_UNKNOWN = 6,
    SERVER_EVENT_ON_PACKET_SEND = 7,
    WORLD_EVENT_ON_CONFIG_LOAD = 9,
    WORLD_EVENT_ON_SHUTDOWN_INIT = 11,
    WORLD_EVENT_ON_SHUTDOWN_CANCEL = 12,
    WORLD_EVENT_ON_UPDATE = 13,
    WORLD_EVENT_ON_STARTUP = 14,
    WORLD_EVENT_ON_SHUTDOWN = 15,
    ELUNA_EVENT_ON_LUA_STATE_CLOSE = 16,
    MAP_EVENT_ON_CREATE = 17,
    MAP_EVENT_ON_DESTROY = 18,
    MAP_EVENT_ON_GRID_LOAD = 19,
    MAP_EVENT_ON_GRID_UNLOAD = 20,
    MAP_EVENT_ON_PLAYER_ENTER = 21,
    MAP_EVENT_ON_PLAYER_LEAVE = 22,
    MAP_EVENT_ON_UPDATE = 23,
    TRIGGER_EVENT_ON_TRIGGER = 24,
    WEATHER_EVENT_ON_CHANGE = 25,
    AUCTION_EVENT_ON_ADD = 26,
    AUCTION_EVENT_ON_REMOVE = 27,
    AUCTION_EVENT_ON_SUCCESSFUL = 28,
    AUCTION_EVENT_ON_EXPIRE = 29,
    ADDON_EVENT_ON_MESSAGE = 30,
    WORLD_EVENT_ON_DELETE_CREATURE = 31,
    WORLD_EVENT_ON_DELETE_GAMEOBJECT = 32,
    ELUNA_EVENT_ON_LUA_STATE_OPEN = 33,
    GAME_EVENT_START = 34,
    GAME_EVENT_STOP = 35,
};

enum TurtleLuaPacketEvents
{
    PACKET_EVENT_ON_PACKET_RECEIVE = 5,
    PACKET_EVENT_ON_PACKET_RECEIVE_UNKNOWN = 6,
    PACKET_EVENT_ON_PACKET_SEND = 7,
};

enum TurtleLuaGuildEvents
{
    GUILD_EVENT_ON_ADD_MEMBER = 1,
    GUILD_EVENT_ON_REMOVE_MEMBER = 2,
    GUILD_EVENT_ON_MOTD_CHANGE = 3,
    GUILD_EVENT_ON_INFO_CHANGE = 4,
    GUILD_EVENT_ON_CREATE = 5,
    GUILD_EVENT_ON_DISBAND = 6,
    GUILD_EVENT_ON_MONEY_WITHDRAW = 7,
    GUILD_EVENT_ON_MONEY_DEPOSIT = 8,
    GUILD_EVENT_ON_ITEM_MOVE = 9,
    GUILD_EVENT_ON_EVENT = 10,
    GUILD_EVENT_ON_BANK_EVENT = 11,
};

enum TurtleLuaGroupEvents
{
    GROUP_EVENT_ON_MEMBER_ADD = 1,
    GROUP_EVENT_ON_MEMBER_INVITE = 2,
    GROUP_EVENT_ON_MEMBER_REMOVE = 3,
    GROUP_EVENT_ON_LEADER_CHANGE = 4,
    GROUP_EVENT_ON_DISBAND = 5,
    GROUP_EVENT_ON_CREATE = 6,
};

enum TurtleLuaBattleGroundEvents
{
    BG_EVENT_ON_START = 1,
    BG_EVENT_ON_END = 2,
    BG_EVENT_ON_CREATE = 3,
    BG_EVENT_ON_PRE_DESTROY = 4,
};

enum TurtleLuaTicketEvents
{
    TICKET_EVENT_ON_CREATE = 1,
    TICKET_EVENT_UPDATE_LAST_CHANGE = 2,
    TICKET_EVENT_ON_CLOSE = 3,
    TICKET_EVENT_ON_RESOLVE = 4,
};

enum TurtleLuaInstanceEvents
{
    INSTANCE_EVENT_ON_INITIALIZE = 1,
    INSTANCE_EVENT_ON_LOAD = 2,
    INSTANCE_EVENT_ON_UPDATE = 3,
    INSTANCE_EVENT_ON_PLAYER_ENTER = 4,
    INSTANCE_EVENT_ON_CREATURE_CREATE = 5,
    INSTANCE_EVENT_ON_GAMEOBJECT_CREATE = 6,
    INSTANCE_EVENT_ON_CHECK_ENCOUNTER_IN_PROGRESS = 7,
};

enum TurtleLuaCreatureEvents
{
    CREATURE_EVENT_ON_ENTER_COMBAT = 1,
    CREATURE_EVENT_ON_LEAVE_COMBAT = 2,
    CREATURE_EVENT_ON_TARGET_DIED = 3,
    CREATURE_EVENT_ON_DIED = 4,
    CREATURE_EVENT_ON_SPAWN = 5,
    CREATURE_EVENT_ON_REACH_WP = 6,
    CREATURE_EVENT_ON_RECEIVE_EMOTE = 8,
    CREATURE_EVENT_ON_DAMAGE_TAKEN = 9,
    CREATURE_EVENT_ON_PRE_COMBAT = 10,
    CREATURE_EVENT_ON_OWNER_ATTACKED = 12,
    CREATURE_EVENT_ON_OWNER_ATTACKED_AT = 13,
    CREATURE_EVENT_ON_HIT_BY_SPELL = 14,
    CREATURE_EVENT_ON_SPELL_HIT_TARGET = 15,
    CREATURE_EVENT_ON_JUST_SUMMONED_CREATURE = 19,
    CREATURE_EVENT_ON_SUMMONED_CREATURE_DESPAWN = 20,
    CREATURE_EVENT_ON_SUMMONED_CREATURE_DIED = 21,
    CREATURE_EVENT_ON_SUMMONED = 22,
    CREATURE_EVENT_ON_RESET = 23,
    CREATURE_EVENT_ON_REACH_HOME = 24,
    CREATURE_EVENT_ON_CORPSE_REMOVED = 26,
    CREATURE_EVENT_ON_MOVE_IN_LOS = 27,
    CREATURE_EVENT_ON_AIUPDATE = 7,
    CREATURE_EVENT_ON_DUMMY_EFFECT = 30,
    CREATURE_EVENT_ON_QUEST_ACCEPT = 31,
    CREATURE_EVENT_ON_QUEST_REWARD = 34,
    CREATURE_EVENT_ON_DIALOG_STATUS = 35,
    CREATURE_EVENT_ON_ADD = 36,
    CREATURE_EVENT_ON_REMOVE = 37,
};

enum TurtleLuaGameObjectEvents
{
    GAMEOBJECT_EVENT_ON_AIUPDATE = 1,
    GAMEOBJECT_EVENT_ON_SPAWN = 2,
    GAMEOBJECT_EVENT_ON_DUMMY_EFFECT = 3,
    GAMEOBJECT_EVENT_ON_QUEST_ACCEPT = 4,
    GAMEOBJECT_EVENT_ON_QUEST_REWARD = 5,
    GAMEOBJECT_EVENT_ON_DIALOG_STATUS = 6,
    GAMEOBJECT_EVENT_ON_DESTROYED = 7,
    GAMEOBJECT_EVENT_ON_DAMAGED = 8,
    GAMEOBJECT_EVENT_ON_LOOT_STATE_CHANGE = 9,
    GAMEOBJECT_EVENT_ON_GO_STATE_CHANGED = 10,
    GAMEOBJECT_EVENT_ON_ADD = 12,
    GAMEOBJECT_EVENT_ON_REMOVE = 13,
    GAMEOBJECT_EVENT_ON_USE = 14,
};

enum TurtleLuaItemEvents
{
    ITEM_EVENT_ON_DUMMY_EFFECT = 1,
    ITEM_EVENT_ON_USE = 2,
    ITEM_EVENT_ON_QUEST_ACCEPT = 3,
    ITEM_EVENT_ON_EXPIRE = 4,
    ITEM_EVENT_ON_REMOVE = 5,
};

enum TurtleLuaGossipEvents
{
    GOSSIP_EVENT_ON_HELLO = 1,
    GOSSIP_EVENT_ON_SELECT = 2,
};

enum TurtleLuaSpellEvents
{
    SPELL_EVENT_ON_PREPARE = 1,
    SPELL_EVENT_ON_CAST = 2,
    SPELL_EVENT_ON_CAST_CANCEL = 3,
};

class TurtleLuaEngine
{
public:
    TurtleLuaEngine();
    ~TurtleLuaEngine();

    void Initialize();
    void Shutdown();
    void Reload();
    void Update(uint32 diff);
    void OnConfigLoad(bool reload);
    void OnShutdownInit(uint32 exitCode, uint32 shutdownMask);
    void OnShutdownCancel();
    void OnGameEventStart(uint32 gameEventId);
    void OnGameEventStop(uint32 gameEventId);
    bool OnAreaTrigger(Player* player, uint32 triggerId);
    void OnWeatherChange(uint32 zoneId, uint32 state, float grade);
    void OnAuctionEvent(uint32 eventId, AuctionEntry* entry, Item* item);

    void OnPlayerLogin(Player* player);
    void OnPlayerLogout(Player* player);
    void OnPlayerCharacterCreate(Player* player);
    void OnPlayerCharacterDelete(uint32 guidLow);
    void OnPlayerSpellCast(Player* player, Spell* spell, bool skipCheck);
    void OnPlayerKillPlayer(Player* killer, Player* killed);
    void OnPlayerKillCreature(Player* killer, Creature* killed);
    void OnPlayerKilledByCreature(Creature* killer, Player* killed);
    void OnPlayerDuelRequest(Player* target, Player* challenger);
    void OnPlayerDuelStart(Player* player1, Player* player2);
    void OnPlayerDuelEnd(Player* winner, Player* loser, uint32 type);
    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* victim, uint8 source);
    void OnPlayerLevelChange(Player* player, uint32 oldLevel);
    void OnPlayerMoneyChange(Player* player, int32& amount);
    bool OnPlayerReputationChange(Player* player, uint32 factionId, int32& standing, bool incremental);
    void OnPlayerTalentsChange(Player* player, uint32 points);
    void OnPlayerTalentsReset(Player* player, bool noCost);
    void OnPlayerLearnTalents(Player* player, uint32 talentId, uint32 talentRank, uint32 spellId);
    void OnPlayerFirstLogin(Player* player);
    void OnPlayerRepop(Player* player);
    void OnPlayerResurrect(Player* player);
    void OnPlayerEquip(Player* player, Item* item, uint8 bag, uint8 slot);
    uint32 OnPlayerCanUseItem(Player const* player, uint32 itemEntry);
    void OnPlayerLootItem(Player* player, Item* item, uint32 count, ObjectGuid const& guid);
    void OnPlayerLootMoney(Player* player, uint32 amount);
    void OnPlayerQuestAbandon(Player* player, uint32 questId);
    void OnPetAddedToWorld(Player* player, Creature* pet);
    void OnPlayerLearnSpell(Player* player, uint32 spellId);
    void OnPlayerAchievementComplete(Player* player, uint32 achievementId);
    void OnPlayerFFAPvPChange(Player* player, bool hasFfaPvp);
    void OnPlayerPetKill(Player* player, Creature* killed);
    bool OnPlayerCommand(Player* player, std::string const& command);
    bool OnPlayerCanInitTrade(Player* player, Player* target);
    bool OnPlayerCanSendMail(Player* player, ObjectGuid const& receiverGuid, ObjectGuid const& mailboxGuid, std::string const& subject, std::string const& body, uint32 money, uint32 cod, Item* item);
    bool OnPlayerCanJoinLFG(Player* player, uint32 areaId);
    void OnPlayerEnterCombat(Player* player, Unit* enemy);
    void OnPlayerLeaveCombat(Player* player);
    void OnPlayerQuestRewardItem(Player* player, Item* item, uint32 count);
    void OnPlayerCreateItem(Player* player, Item* item, uint32 count);
    void OnPlayerStoreNewItem(Player* player, Item* item, uint32 count);
    void OnPlayerCompleteQuest(Player* player, Quest const* quest);
    void OnPlayerGroupRollRewardItem(Player* player, Item* item, uint32 count, uint32 voteType, Roll const* roll);
    bool OnPacketReceive(WorldSession* session, WorldPacket& packet);
    bool OnPacketSend(WorldSession* session, WorldPacket& packet);
    void OnPlayerBGDesertion(Player* player, uint32 type);
    bool OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& message);
    bool OnPlayerWhisper(Player* player, uint32 type, uint32 lang, std::string& message, Player* receiver);
    bool OnPlayerGroupChat(Player* player, uint32 type, uint32 lang, std::string& message, Group* group);
    bool OnPlayerGuildChat(Player* player, uint32 type, uint32 lang, std::string& message, Guild* guild);
    bool OnPlayerChannelChat(Player* player, uint32 type, uint32 lang, std::string& message, Channel* channel);
    bool OnAddonMessage(Player* sender, uint32 type, std::string const& message, Player* receiver, Guild* guild, Group* group, uint32 channelId, bool hasChannelTarget);
    void OnPlayerEmote(Player* player, uint32 emote);
    bool OnPlayerCanGroupInvite(Player* player, std::string const& memberName);
    bool OnPlayerCanResurrect(Player* player);
    bool OnPlayerCanUpdateSkill(Player* player, uint32 skillId);
    void OnPlayerBeforeUpdateSkill(Player* player, uint32 skillId, uint32& value, uint32 max, uint32 step);
    void OnPlayerUpdateSkill(Player* player, uint32 skillId, uint32 value, uint32 max, uint32 step, uint32 newValue);
    void OnPlayerTextEmote(Player* player, uint32 textEmote, uint32 emoteNum, ObjectGuid const& guid);
    void OnPlayerSave(Player* player);
    void OnPlayerBindToInstance(Player* player, uint32 difficulty, uint32 mapId, bool permanent);
    void OnPlayerUpdateZone(Player* player, uint32 newZone, uint32 newArea);
    void OnPlayerMapChanged(Player* player);
    void OnPlayerUpdateArea(Player* player, uint32 oldArea, uint32 newArea);
    void OnMapCreate(Map* map);
    void OnMapDestroy(Map* map);
    void OnMapPlayerEnter(Map* map, Player* player);
    void OnMapPlayerLeave(Map* map, Player* player);
    void OnMapUpdate(Map* map, uint32 diff);
    void OnInstanceInitialize(Map* map);
    void OnInstanceLoad(Map* map);
    void OnInstanceUpdate(Map* map, uint32 diff);
    void OnInstancePlayerEnter(Map* map, Player* player);
    void OnBattleGroundCreate(BattleGround* bg);
    void OnBattleGroundStart(BattleGround* bg);
    void OnBattleGroundEnd(BattleGround* bg, uint32 winner);
    void OnBattleGroundPreDestroy(BattleGround* bg);
    void OnTicketCreate(GmTicket* ticket);
    void OnTicketUpdateLastChange(GmTicket* ticket, std::string const& message);
    void OnTicketClose(GmTicket* ticket);
    void OnTicketResolve(GmTicket* ticket);
    void OnCreatureEnterCombat(Creature* creature, Unit* target);
    void OnCreatureLeaveCombat(Creature* creature);
    void OnCreatureTargetDied(Creature* creature, Unit* victim);
    void OnCreatureDied(Creature* creature, Unit* killer);
    void OnCreatureSpawn(Creature* creature);
    void OnCreatureAdd(Creature* creature);
    void OnCreatureRemove(Creature* creature);
    bool OnCreatureReachWP(Creature* creature, uint32 type, uint32 id);
    void OnCreatureReset(Creature* creature);
    bool OnCreatureReachHome(Creature* creature);
    void OnCreatureAIUpdate(Creature* creature, uint32 diff);
    bool OnCreatureDamageTaken(Creature* creature, Unit* attacker, uint32& damage);
    bool OnCreaturePreCombat(Creature* creature, Unit* target);
    bool OnCreatureOwnerAttacked(Creature* creature, Unit* target);
    bool OnCreatureOwnerAttackedAt(Creature* creature, Unit* attacker);
    bool OnCreatureHitBySpell(Creature* creature, WorldObject* caster, uint32 spellId);
    bool OnCreatureSpellHitTarget(Creature* creature, Unit* target, uint32 spellId);
    bool OnCreatureJustSummoned(Creature* creature, Creature* summon);
    bool OnCreatureSummonedCreatureDespawn(Creature* creature, Creature* summon);
    bool OnCreatureSummonedCreatureDied(Creature* creature, Creature* summon, Unit* killer);
    bool OnCreatureSummoned(Creature* creature, Unit* summoner);
    bool OnCreatureMoveInLOS(Creature* creature, Unit* who);
    bool OnCreatureReceiveEmote(Creature* creature, Player* player, uint32 emoteId);
    bool OnCreatureCorpseRemoved(Creature* creature, uint32& respawnDelay);
    bool OnCreatureDummyEffect(WorldObject* caster, uint32 spellId, uint32 effIndex, Creature* target);

    void RegisterPlayerEvent(uint32 eventId, int functionRef);
    void RegisterServerEvent(uint32 eventId, int functionRef);
    void RegisterGroupEvent(uint32 eventId, int functionRef);
    void RegisterGuildEvent(uint32 eventId, int functionRef);
    void RegisterBattleGroundEvent(uint32 eventId, int functionRef);
    void RegisterTicketEvent(uint32 eventId, int functionRef);
    void RegisterMapEvent(uint32 mapId, uint32 eventId, int functionRef);
    void RegisterInstanceEvent(uint32 instanceId, uint32 eventId, int functionRef);
    void RegisterCreatureEvent(uint32 entry, uint32 eventId, int functionRef);
    void RegisterUniqueCreatureEvent(ObjectGuid const& guid, uint32 instanceId, uint32 eventId, int functionRef);
    void RegisterGameObjectEvent(uint32 entry, uint32 eventId, int functionRef);
    void RegisterItemEvent(uint32 entry, uint32 eventId, int functionRef);
    void RegisterSpellEvent(uint32 entry, uint32 eventId, int functionRef);
    void RegisterPacketEvent(uint32 opcode, uint32 eventId, int functionRef);
    void RegisterCreatureGossipEvent(uint32 entry, uint32 eventId, int functionRef);
    void RegisterGameObjectGossipEvent(uint32 entry, uint32 eventId, int functionRef);
    void RegisterItemGossipEvent(uint32 entry, uint32 eventId, int functionRef);
    void RegisterPlayerGossipEvent(uint32 entry, uint32 eventId, int functionRef);
    void ClearPlayerEvents(uint32 eventId, bool allEvents);
    void ClearServerEvents(uint32 eventId, bool allEvents);
    void ClearGroupEvents(uint32 eventId, bool allEvents);
    void ClearGuildEvents(uint32 eventId, bool allEvents);
    void ClearBattleGroundEvents(uint32 eventId, bool allEvents);
    void ClearTicketEvents(uint32 eventId, bool allEvents);
    void ClearMapEvents(uint32 mapId, uint32 eventId, bool allEvents);
    void ClearInstanceEvents(uint32 instanceId, uint32 eventId, bool allEvents);
    void ClearCreatureEvents(uint32 entry, uint32 eventId, bool allEvents);
    void ClearUniqueCreatureEvents(ObjectGuid const& guid, uint32 instanceId, uint32 eventId, bool allEvents);
    void ClearGameObjectEvents(uint32 entry, uint32 eventId, bool allEvents);
    void ClearItemEvents(uint32 entry, uint32 eventId, bool allEvents);
    void ClearSpellEvents(uint32 entry, uint32 eventId, bool allEvents);
    void ClearPacketEvents(uint32 opcode, uint32 eventId, bool allEvents);
    void ClearCreatureGossipEvents(uint32 entry, uint32 eventId, bool allEvents);
    void ClearGameObjectGossipEvents(uint32 entry, uint32 eventId, bool allEvents);
    void ClearItemGossipEvents(uint32 entry, uint32 eventId, bool allEvents);
    void ClearPlayerGossipEvents(uint32 entry, uint32 eventId, bool allEvents);

    bool OnCreatureGossipHello(Player* player, Creature* creature);
    bool OnCreatureGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action, char const* code);
    bool OnGameObjectGossipHello(Player* player, GameObject* go);
    bool OnGameObjectGossipSelect(Player* player, GameObject* go, uint32 sender, uint32 action, char const* code);
    bool OnItemGossipHello(Player* player, Item* item, SpellCastTargets& targets);
    bool OnItemGossipSelect(Player* player, Item* item, uint32 sender, uint32 action, char const* code);
    bool OnPlayerGossipSelect(Player* player, uint32 sender, uint32 action, char const* code);
    void OnGroupCreate(Group* group, ObjectGuid const& leaderGuid, uint32 groupType);
    void OnGroupMemberAdd(Group* group, ObjectGuid const& guid);
    void OnGroupMemberInvite(Group* group, ObjectGuid const& guid);
    void OnGroupMemberRemove(Group* group, ObjectGuid const& guid, uint8 removeMethod);
    void OnGroupLeaderChange(Group* group, ObjectGuid const& newLeaderGuid, ObjectGuid const& oldLeaderGuid);
    void OnGroupDisband(Group* group);
    void OnGuildCreate(Guild* guild, Player* leader, std::string const& name);
    void OnGuildMemberAdd(Guild* guild, Player* player, uint32 rank);
    void OnGuildMemberRemove(Guild* guild, Player* player, bool isDisbanding);
    void OnGuildMOTDChange(Guild* guild, std::string const& motd);
    void OnGuildInfoChange(Guild* guild, std::string const& info);
    void OnGuildDisband(Guild* guild);
    void OnGuildMoneyWithdraw(Guild* guild, Player* player, uint32& amount, bool isRepair);
    void OnGuildMoneyDeposit(Guild* guild, Player* player, uint32& amount);
    void OnGuildItemMove(Guild* guild, Player* player, Item* item, bool isSrcBank, uint8 srcContainer, uint8 srcSlotId, bool isDestBank, uint8 destContainer, uint8 destSlotId);
    void OnGuildEvent(Guild* guild, uint8 eventType, uint32 playerGuid1, uint32 playerGuid2, uint8 newRank);
    void OnGuildBankEvent(Guild* guild, uint8 eventType, uint8 tabId, uint32 playerGuid, uint32 itemOrMoney, uint16 itemStackCount, uint8 destTabId);
    bool OnGameObjectUse(Player* player, GameObject* go);
    void OnGameObjectLootStateChanged(GameObject* go, uint32 state);
    void OnGameObjectGoStateChanged(GameObject* go, uint32 state);
    void OnGameObjectAIUpdate(GameObject* go, uint32 diff);
    void OnGameObjectSpawn(GameObject* go);
    void OnGameObjectAdd(GameObject* go);
    void OnGameObjectRemove(GameObject* go);
    void OnGameObjectDestroyed(GameObject* go, WorldObject* attacker);
    void OnGameObjectDamaged(GameObject* go, WorldObject* attacker);
    bool OnGameObjectDummyEffect(WorldObject* caster, uint32 spellId, uint32 effIndex, GameObject* target);
    bool OnItemUse(Player* player, Item* item, SpellCastTargets& targets);
    bool OnItemDummyEffect(WorldObject* caster, uint32 spellId, uint32 effIndex, Item* target);
    bool OnItemExpire(Player* player, ItemPrototype const* proto);
    bool OnItemRemove(Player* player, Item* item);
    void OnSpellPrepare(WorldObject* caster, Spell* spell);
    void OnSpellCast(WorldObject* caster, Spell* spell, bool skipCheck);
    void OnSpellCastCancel(WorldObject* caster, Spell* spell, bool bySelf);
    bool OnCreatureQuestAccept(Player* player, Creature* creature, Quest const* quest);
    bool OnCreatureQuestReward(Player* player, Creature* creature, Quest const* quest);
    bool OnGameObjectQuestAccept(Player* player, GameObject* go, Quest const* quest);
    bool OnGameObjectQuestReward(Player* player, GameObject* go, Quest const* quest);
    bool OnItemQuestAccept(Player* player, Item* item, Quest const* quest);
    bool IsEnabled() const { return _enabled && _state; }

    void PushPlayer(Player* player);
    void PushCreature(Creature* creature);
    void PushGameObject(GameObject* go);
    void PushCorpse(Corpse* corpse);
    void PushDynamicObject(DynamicObject* dynObj);
    void PushItem(Item* item);
    void PushUnit(Unit* unit);
    void PushGroup(Group* group);
    void PushGuild(Guild* guild);
    void PushMap(Map* map);
    void PushBattleGround(BattleGround* bg);
    void PushTicket(GmTicket* ticket);
    void PushSpell(Spell* spell);
    void PushObjectGuid(ObjectGuid const& guid);

    uint64 GetStateId() const { return _stateId; }
    void EnqueueAsyncQueryResult(uint64 stateId, int functionRef, QueryNamedResult* result);
    void EnqueueHttpResponse(uint64 stateId, int functionRef, int statusCode, std::string body, std::vector<std::pair<std::string, std::string>> headers);

    uint32 CreateTimedEvent(int functionRef, uint32 delay, uint32 repeats);
    uint32 CreateTimedEvent(int functionRef, uint32 minDelay, uint32 maxDelay, uint32 repeats, WorldObject* object);
    bool RemoveTimedEvent(uint32 eventId);
    bool RemoveTimedEventForObject(uint32 eventId, ObjectGuid const& objectGuid);
    uint32 RemoveTimedEventsForObject(ObjectGuid const& objectGuid);
    uint32 RemoveTimedEvents(bool allEvents);

private:
    struct UniqueCreatureEventKey
    {
        UniqueCreatureEventKey(ObjectGuid const& guid_, uint32 instanceId_) : guid(guid_), instanceId(instanceId_) {}

        bool operator<(UniqueCreatureEventKey const& other) const
        {
            if (guid != other.guid)
                return guid < other.guid;

            return instanceId < other.instanceId;
        }

        ObjectGuid guid;
        uint32 instanceId;
    };

    struct TimedEvent
    {
        uint32 id;
        int functionRef;
        uint32 delay;
        uint32 minDelay;
        uint32 maxDelay;
        uint32 elapsed;
        int32 remaining;
        bool hasObject;
        ObjectGuid objectGuid;
        uint32 mapId;
        uint32 instanceId;
    };

    struct PendingAsyncQuery
    {
        int functionRef;
        QueryNamedResult* result;
    };

    struct PendingHttpResponse
    {
        int functionRef;
        int statusCode;
        std::string body;
        std::vector<std::pair<std::string, std::string>> headers;
    };

    void OpenState();
    void CloseState();
    void LoadScripts();
    void LoadScriptFile(std::string const& path);
    void RegisterGlobals();
    void RegisterPlayerMetatable();
    void RegisterCreatureMetatable();
    void RegisterGameObjectMetatable();
    void RegisterCorpseMetatable();
    void RegisterDynamicObjectMetatable();
    void RegisterGameObjectTemplateMetatable();
    void RegisterItemMetatable();
    void RegisterItemTemplateMetatable();
    void RegisterCreatureTemplateMetatable();
    void RegisterQuestMetatable();
    void RegisterGroupMetatable();
    void RegisterGuildMetatable();
    void RegisterMapMetatable();
    void RegisterInstanceDataMetatable();
    void RegisterBattleGroundMetatable();
    void RegisterTicketMetatable();
    void RegisterAuraMetatable();
    void RegisterAchievementMetatable();
    void RegisterSpellMetatable();
    void RegisterSpellInfoMetatable();
    void RegisterSpellTargetsMetatable();
    void RegisterGemPropertiesMetatable();
    void RegisterVehicleMetatable();
    void RegisterWorldPacketMetatable();
    void RegisterQueryMetatable();
    void RegisterObjectGuidMetatable();
    void RegisterChatHandlerMetatable();
    void RegisterChannelMetatable();
    void RegisterRollMetatable();
    void RegisterUnitMetatable();

    void LogError(char const* context);
    uint32 GenerateTimedEventDelay(TimedEvent const& event) const;
    WorldObject* ResolveTimedEventObject(TimedEvent const& event);
    bool CallPlayerEvent(uint32 eventId, Player* player);
    bool CallPlayerChatEvent(uint32 eventId, char const* context, Player* player, uint32 type, uint32 lang, std::string& message, Player* receiver, Group* group, Guild* guild, Channel* channel);
    void CallPlayerItemEvent(uint32 eventId, char const* context, Player* player, Item* item, uint32 count);
    bool CallServerPacketEvent(uint32 eventId, WorldPacket& packet, Player* player, char const* context);
    bool CallPacketEvent(uint32 opcode, uint32 eventId, WorldPacket& packet, Player* player, char const* context);
    bool CallPacketFunctionRefs(std::vector<int> const& functionRefs, uint32 eventId, WorldPacket& packet, Player* player, char const* context);
    void CallServerEvent(uint32 eventId);
    void CallServerEvent(uint32 eventId, uint32 arg);
    void CallServerEvent(uint32 eventId, uint32 arg1, uint32 arg2);
    void CallServerMapEvent(uint32 eventId, Map* map, Player* player, uint32 diff);
    void CallBattleGroundEvent(uint32 eventId, BattleGround* bg, int argCount);
    void CallTicketEvent(uint32 eventId, GmTicket* ticket, int argCount);
    std::vector<int> CollectInstanceEventRefs(Map* map, uint32 eventId);
    bool CallInstanceEvent(Map* map, uint32 eventId, int argCount);
    std::vector<int> CollectCreatureEventRefs(Creature* creature, uint32 eventId);
    bool CallCreatureEvent(Creature* creature, uint32 eventId, int argCount);
    bool CallEntryEvent(std::map<uint32, std::map<uint32, std::vector<int>>>& store, uint32 entry, uint32 eventId, int argCount);
    bool CallEntryEventForBoolean(std::map<uint32, std::map<uint32, std::vector<int>>>& store, uint32 entry, uint32 eventId, int argCount, bool expectedValue);
    void CallEntryEventIgnoreResult(std::map<uint32, std::map<uint32, std::vector<int>>>& store, uint32 entry, uint32 eventId, int argCount);
    void UpdateAsyncQueries();
    void ClearAsyncQueries();
    void UpdateHttpResponses();
    void ClearHttpResponses();
    void UpdateTimedEvents(uint32 diff);

    lua_State* _state;
    bool _enabled;
    bool _reloadPending;
    uint64 _stateId;
    uint32 _nextTimedEventId;
    std::string _scriptPath;
    std::recursive_mutex _lock;
    std::mutex _asyncQueryLock;
    std::mutex _httpResponseLock;
    std::map<uint32, std::vector<int>> _serverEvents;
    std::map<uint32, std::vector<int>> _playerEvents;
    std::map<uint32, std::vector<int>> _groupEvents;
    std::map<uint32, std::vector<int>> _guildEvents;
    std::map<uint32, std::vector<int>> _battleGroundEvents;
    std::map<uint32, std::vector<int>> _ticketEvents;
    std::map<uint32, std::map<uint32, std::vector<int>>> _mapEvents;
    std::map<uint32, std::map<uint32, std::vector<int>>> _instanceEvents;
    std::map<uint32, std::map<uint32, std::vector<int>>> _creatureEvents;
    std::map<UniqueCreatureEventKey, std::map<uint32, std::vector<int>>> _uniqueCreatureEvents;
    std::map<uint32, std::map<uint32, std::vector<int>>> _gameObjectEvents;
    std::map<uint32, std::map<uint32, std::vector<int>>> _itemEvents;
    std::map<uint32, std::map<uint32, std::vector<int>>> _spellEvents;
    std::map<uint32, std::map<uint32, std::vector<int>>> _packetEvents;
    std::map<uint32, std::map<uint32, std::vector<int>>> _creatureGossipEvents;
    std::map<uint32, std::map<uint32, std::vector<int>>> _gameObjectGossipEvents;
    std::map<uint32, std::map<uint32, std::vector<int>>> _itemGossipEvents;
    std::map<uint32, std::map<uint32, std::vector<int>>> _playerGossipEvents;
    std::vector<PendingAsyncQuery> _asyncQueries;
    std::vector<PendingHttpResponse> _httpResponses;
    std::vector<TimedEvent> _timedEvents;
};

extern TurtleLuaEngine sTurtleLuaEngine;

#endif
