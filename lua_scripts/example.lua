print("Turtle Lua compatibility layer loaded")

RegisterServerEvent(14, function(event)
    print("Lua server startup event fired")
end)

RegisterServerEvent(13, function(event, diff)
    -- World update event is intentionally quiet; use CreateLuaEvent for normal timers.
end)

-- Map/Instance 事件示例。打开注释后，把 mapId 换成目标副本地图 ID。
-- RegisterMapEvent(409, 4, function(event, instanceData, map, player)
--     player:SendBroadcastMessage("Lua saw you enter instance map " .. map:GetMapId())
-- end)
--
-- RegisterServerEvent(21, function(event, map, player)
--     -- 玩家进入任意地图时触发；正式脚本里建议按 map:GetMapId() 做过滤。
-- end)

-- 战场事件示例。默认注释掉，避免测试服没有开战时刷日志。
-- RegisterBGEvent(3, function(event, bg, bgId, instanceId)
--     print("BG created: " .. bg:GetName() .. " type=" .. bgId .. " instance=" .. instanceId)
-- end)
--
-- RegisterBGEvent(1, function(event, bg, bgId, instanceId)
--     print("BG started: " .. bg:GetName() .. " status=" .. bg:GetStatus())
-- end)
--
-- RegisterBGEvent(2, function(event, bg, bgId, instanceId, winner)
--     print("BG ended: " .. bg:GetName() .. " winner=" .. winner)
-- end)
--
-- RegisterBGEvent(4, function(event, bg, bgId, instanceId)
--     print("BG pre-destroy: " .. bg:GetName() .. " instance=" .. instanceId)
-- end)

-- 工单事件示例。只有玩家创建/更新/关闭工单时才会触发。
-- RegisterTicketEvent(1, function(event, ticket)
--     print("Ticket created: #" .. ticket:GetId() .. " player=" .. ticket:GetPlayerName())
-- end)
--
-- RegisterTicketEvent(2, function(event, ticket, message)
--     print("Ticket updated: #" .. ticket:GetId() .. " message=" .. message)
-- end)
--
-- RegisterTicketEvent(3, function(event, ticket)
--     print("Ticket closed: #" .. ticket:GetId() .. " closed=" .. tostring(ticket:IsClosed()))
-- end)

-- 自定义飞行路径示例。把坐标换成角色附近坐标后，可在脚本里调用 player:StartTaxi(pathId)。
-- local sampleTaxiPath = AddTaxiPath({
--     {0, -8949.95, -132.49, 83.53},
--     {0, -8935.00, -180.00, 120.00},
--     {0, -8900.00, -220.00, 90.00},
-- }, 541, 2224)
--
-- RegisterPlayerEvent(18, function(event, player, message)
--     if message == "#taxi" and sampleTaxiPath then
--         player:StartTaxi(sampleTaxiPath)
--         return false
--     end
-- end)

CreateLuaEvent(function(eventId)
    print("Lua timed event " .. eventId .. " is alive")
end, 60000, 0)

RegisterPlayerEvent(3, function(event, player)
    player:SendBroadcastMessage("Lua engine is online. Welcome, " .. player:GetName() .. "!")
end)

-- 角色创建 / 删除事件。创建时角色还没有真正进入世界；删除事件按 Eluna 兼容形式传 guidLow。
-- RegisterPlayerEvent(1, function(event, player)
--     print("Created character " .. player:GetName() .. " guidLow=" .. player:GetGUIDLow())
-- end)
--
-- RegisterPlayerEvent(2, function(event, guidLow)
--     local guid = CreatePlayerGuid(guidLow)
--     print("Deleting character guid=" .. guid:ToString())
-- end)

RegisterPlayerEvent(18, function(event, player, message, chatType, language)
    if message == "#lua" then
        player:SendBroadcastMessage("Lua says hello from map " .. player:GetMapId())
        player:SendSysMessage("Lua SendSysMessage alias is available")
        player:SendNotification("Lua notification is available")
        player:SendAreaTriggerMessage("Lua area trigger message is available")
        player:SendAddonMessage("TURTLE_LUA", "Ping:" .. player:GetName())
        player:SendBroadcastMessage("XP=" .. player:GetXP() .. ", money=" .. player:GetMoney())
        player:SendBroadcastMessage("Area=" .. player:GetAreaId() .. ", instance=" .. player:GetInstanceId())
        player:SendBroadcastMessage("Alive=" .. tostring(player:IsAlive()) .. ", mounted=" .. tostring(player:IsMounted()))
        player:SendBroadcastMessage("Type=" .. player:GetTypeId() .. ", isPlayer=" .. tostring(player:IsPlayer()))
        local guid = player:GetGUID()
        player:SendBroadcastMessage("GUID=" .. guid:ToString() .. ", low=" .. guid:GetGUIDLow() .. ", type=" .. guid:GetTypeName())
        player:SendBroadcastMessage("GUID helpers: low=" .. GetGUIDLow(guid) .. ", high=" .. GetGUIDType(guid) .. ", entry=" .. GetGUIDEntry(guid))
        local rebuiltPlayer = GetPlayerByGUID(GetPlayerGUID(player:GetGUIDLow()))
        local rebuiltPlayerLow = GetPlayerByGUIDLow(player:GetGUIDLow())
        player:SendBroadcastMessage("GUID lookup self=" .. tostring(rebuiltPlayer ~= nil) .. ", lowAlias=" .. tostring(rebuiltPlayerLow ~= nil))
        local selectedGuid = player:GetSelectionGUID()
        local selectedUnit = player:GetSelection()
        local selectedGo = player:GetSelectedGO()
        local selectedText = selectedUnit and selectedUnit:GetName() or "none"
        player:SendBroadcastMessage("Selection unit=" .. selectedText .. ", unitGuidEmpty=" .. tostring(selectedGuid:IsEmpty()) .. ", go=" .. tostring(selectedGo ~= nil))
        player:SendBroadcastMessage("RunSpeed=" .. player:GetSpeed(1) .. ", rooted=" .. tostring(player:IsRooted()))
        player:SendBroadcastMessage("HP%=" .. math.floor(player:GetHealthPct()) .. ", power%=" .. math.floor(player:GetPowerPct()) .. ", fullHP=" .. tostring(player:IsFullHealth()))
        player:SendBroadcastMessage("RaceMask=" .. player:GetRaceMask() .. ", classMask=" .. player:GetClassMask() .. ", str=" .. math.floor(player:GetStat(0)) .. ", pvp=" .. tostring(player:IsPvPFlagged()))
        player:SendBroadcastMessage("RaceClass=" .. (player:GetRaceAsString() or "nil") .. "/" .. (player:GetClassAsString() or "nil") .. ", dying=" .. tostring(player:IsDying()) .. ", charmed=" .. tostring(player:IsCharmed()))
        player:SendBroadcastMessage("OwnerGUID=" .. player:GetOwnerGUID() .. ", creatorGUID=" .. player:GetCreatorGUID() .. ", petGUID=" .. player:GetPetGUID())
        player:SendBroadcastMessage("CharmGUID=" .. player:GetCharmGUID() .. ", controllerGUID=" .. player:GetControllerGUID() .. ", controllerOrSelf=" .. player:GetControllerGUIDS())

        local rx, ry, rz = player:GetRelativePoint(3, 0)
        player:SendBroadcastMessage("Point ahead: " .. math.floor(rx) .. ", " .. math.floor(ry) .. ", " .. math.floor(rz))
        player:SendBroadcastMessage("Exact self distance: " .. player:GetExactDistance(player))
        local lx, ly, lz, lo = player:GetLocation()
        player:SendBroadcastMessage("Location=" .. math.floor(lx) .. "," .. math.floor(ly) .. "," .. math.floor(lz) .. " o=" .. math.floor(lo * 100) / 100 .. ", phase=" .. player:GetPhaseMask())
        local nearbyPlayers = player:GetPlayersInRange(40)
        local nearbyCreatures = player:GetCreaturesInRange(30, 0, 0, 1)
        local nearbyObjects = player:GetGameObjectsInRange(30)
        local nearAny = player:GetNearObject(30, 0, 0, 0, 1)
        local nearObjects = player:GetNearObjects(30, 0, 0, 0, 1)
        local nearestPlayer = player:GetNearestPlayer(40)
        local nearestCreature = player:GetNearestCreature(30, 0, 0, 1)
        player:SendBroadcastMessage("Nearby players=" .. #nearbyPlayers .. ", creatures=" .. #nearbyCreatures .. ", gameobjects=" .. #nearbyObjects)
        player:SendBroadcastMessage("NearObjects any=" .. (nearAny and nearAny:GetGUIDLow() or "none") .. ", total=" .. #nearObjects)
        player:SendBroadcastMessage("Nearest player=" .. (nearestPlayer and nearestPlayer:GetName() or "none") .. ", nearest creature=" .. (nearestCreature and nearestCreature:GetName() or "none"))
        if selectedUnit then
            player:SendBroadcastMessage("Selected inMap=" .. tostring(player:IsInMap(selectedUnit)) .. ", inRange40=" .. tostring(player:IsInRange(selectedUnit, 0, 40)) .. ", inFront=" .. tostring(player:IsInFront(selectedUnit)))
            player:SendBroadcastMessage("Selected cast types unit=" .. tostring(selectedUnit:ToUnit() ~= nil) .. ", creature=" .. tostring(selectedUnit:ToCreature() ~= nil) .. ", player=" .. tostring(selectedUnit:ToPlayer() ~= nil))
            player:SendBroadcastMessage("Selected creatureType=" .. selectedUnit:GetCreatureType() .. ", movement=" .. selectedUnit:GetMovementType() .. ", attackers=" .. #selectedUnit:GetAttackers())
            local threatList = selectedUnit:GetThreatList()
            player:SendBroadcastMessage("Selected threatList=" .. (threatList and #threatList or 0) .. ", threatOnMe=" .. math.floor(selectedUnit:GetThreat(player)))
            player:SendBroadcastMessage("Selected attackingPlayer=" .. tostring(selectedUnit:IsAttackingPlayer()) .. ", charmed=" .. tostring(selectedUnit:IsCharmed()) .. ", baseFirePower=" .. selectedUnit:GetBaseSpellPower(2))
            player:SendBroadcastMessage("Selected casting=" .. tostring(selectedUnit:IsCasting()) .. ", water=" .. tostring(selectedUnit:IsInWater()) .. "/" .. tostring(selectedUnit:IsUnderWater()))
            player:SendBroadcastMessage("Selected npcFlags vendor=" .. tostring(selectedUnit:IsVendor()) .. ", trainer=" .. tostring(selectedUnit:IsTrainer()) .. ", quest=" .. tostring(selectedUnit:IsQuestGiver()))
            local friendlyUnits = selectedUnit:GetFriendlyUnitsInRange(30)
            local unfriendlyUnits = selectedUnit:GetUnfriendlyUnitsInRange(30)
            player:SendBroadcastMessage("Selected nearby friend=" .. #friendlyUnits .. ", enemy=" .. #unfriendlyUnits .. ", onVehicle=" .. tostring(selectedUnit:IsOnVehicle()))
            local currentSpell = selectedUnit:GetCurrentSpell(1)
            player:SendBroadcastMessage("Selected currentSpell=" .. (currentSpell and currentSpell:GetEntry() or "none"))
            if currentSpell then
                local tx, ty, tz = currentSpell:GetTargetDest()
                local reagentTotal = 0
                for _, count in pairs(currentSpell:GetReagentCost()) do
                    reagentTotal = reagentTotal + count
                end
                player:SendBroadcastMessage("CurrentSpell autoRepeat=" .. tostring(currentSpell:IsAutoRepeat()) .. ", duration=" .. currentSpell:GetDuration() .. ", reagents=" .. reagentTotal .. ", destX=" .. tostring(tx))
            end
            if selectedUnit:IsCreature() then
                player:SendBroadcastMessage("Selected accessibleForSelfCreature=" .. tostring(selectedUnit:IsInAccessiblePlaceFor(selectedUnit)))
            end
        end
        if selectedGo then
            player:SendBroadcastMessage("SelectedGO entry=" .. selectedGo:GetEntry() .. ", spawned=" .. tostring(selectedGo:IsSpawned()) .. ", active=" .. tostring(selectedGo:IsActive()) .. ", transport=" .. tostring(selectedGo:IsTransport()))
            player:SendBroadcastMessage("SelectedGO cast type gameobject=" .. tostring(selectedGo:ToGameObject() ~= nil) .. ", unit=" .. tostring(selectedGo:ToUnit() ~= nil))
            player:SendBroadcastMessage("SelectedGO state=" .. selectedGo:GetGoState() .. ", lootState=" .. selectedGo:GetLootState() .. ", respawnDelay=" .. selectedGo:GetRespawnDelay() .. ", destructible=" .. tostring(selectedGo:IsDestructible()))
            player:SendBroadcastMessage("SelectedGO hasQuest783=" .. tostring(selectedGo:HasQuest(783)) .. ", lootRecipient=" .. tostring(selectedGo:GetLootRecipient()) .. ", lootGroup=" .. tostring(selectedGo:GetLootRecipientGroup()))
        end

        local map = player:GetMap()
        if map then
            player:SendBroadcastMessage("Map " .. map:GetName() .. " has " .. map:GetPlayerCount() .. " online player(s)")
            local playerFromMap = map:GetPlayer(guid)
            local objectFromMap = map:GetWorldObjectOrPlayer(guid)
            player:SendBroadcastMessage("Map GUID lookup player=" .. tostring(playerFromMap ~= nil) .. ", object=" .. tostring(objectFromMap ~= nil))
            player:SendBroadcastMessage("Map empty=" .. tostring(map:IsEmpty()) .. ", difficulty=" .. map:GetDifficulty() .. ", heroic=" .. tostring(map:IsHeroic()) .. ", arena=" .. tostring(map:IsArena()))
            local terrainHeight = map:GetHeight(player:GetX(), player:GetY(), player:GetZ())
            local mapAreaId = map:GetAreaId(player:GetX(), player:GetY(), player:GetZ())
            local creaturesInArea = map:GetCreaturesByAreaId(player:GetAreaId())
            player:SendBroadcastMessage("Map terrain height=" .. tostring(terrainHeight) .. ", areaId=" .. mapAreaId .. ", creaturesInArea=" .. #creaturesInArea)
        end

        local hearthstone = player:GetItemByEntry(6948)
        if hearthstone then
            local itemGuid = hearthstone:GetGUID()
            local sameItem = player:GetItemByGUID(GetItemGUID(hearthstone:GetGUIDLow()))
            player:SendBroadcastMessage("Item lookup " .. hearthstone:GetName() .. " low=" .. itemGuid:GetGUIDLow() .. ", found=" .. tostring(sameItem ~= nil))
            player:SendBroadcastMessage("Item link=" .. hearthstone:GetItemLink())
            player:SendBroadcastMessage("Item suffix=" .. hearthstone:GetRandomSuffix() .. ", currency=" .. tostring(hearthstone:IsCurrencyToken()) .. ", weaponVellum=" .. tostring(hearthstone:IsWeaponVellum()) .. ", armorVellum=" .. tostring(hearthstone:IsArmorVellum()))
            local template = hearthstone:GetTemplate()
            if template then
                player:SendBroadcastMessage("Item instance template level=" .. template:GetItemLevel() .. ", class=" .. template:GetClass())
            end
        end

        local group = player:GetGroup()
        if group then
            player:SendBroadcastMessage("Group members: " .. group:GetMembersCount())
            local leaderGuid = group:GetLeaderGUID()
            player:SendBroadcastMessage("Group type=" .. group:GetGroupType() .. ", leaderLow=" .. leaderGuid:GetGUIDLow() .. ", raid=" .. tostring(group:IsRaidGroup()) .. ", lfg=" .. tostring(group:IsLFGGroup()))
            player:SendBroadcastMessage("Group member=" .. tostring(group:IsMember(player)) .. ", assistant=" .. tostring(group:IsAssistant(player)) .. ", subGroup=" .. group:GetMemberGroup(player) .. ", freeSub0=" .. tostring(group:HasFreeSlotSubGroup(0)))
        end
        local originalGroup = player:GetOriginalGroup()
        local inviteGroup = player:GetGroupInvite()
        player:SendBroadcastMessage("InGroup=" .. tostring(player:IsInGroup()) .. ", inGuild=" .. tostring(player:IsInGuild()))
        player:SendBroadcastMessage("SubGroup=" .. player:GetSubGroup() .. ", originalSub=" .. player:GetOriginalSubGroup() .. ", originalGroup=" .. tostring(originalGroup ~= nil) .. ", inviteGroup=" .. tostring(inviteGroup ~= nil))

        local guild = player:GetGuild()
        if guild then
            player:SendBroadcastMessage("Guild: " .. guild:GetName() .. ", online=" .. #guild:GetOnlineMembers())
            local guildLeaderGuid = guild:GetLeaderGUID()
            player:SendBroadcastMessage("Guild members=" .. guild:GetMemberCount() .. ", leaderLow=" .. guildLeaderGuid:GetGUIDLow() .. ", created=" .. guild:GetCreatedDate())
            player:SendBroadcastMessage("Guild rank=" .. guild:GetRank(player) .. ", rankName=" .. guild:GetRankName(guild:GetRank(player)) .. ", bankMoney=" .. guild:GetTotalBankMoney())
        end
        player:SendBroadcastMessage("GuildName=" .. tostring(player:GetGuildName()) .. ", horde=" .. tostring(player:IsHorde()) .. ", alliance=" .. tostring(player:IsAlliance()))
        local homebind = player:GetHomebind()
        player:SendBroadcastMessage("Trader=" .. tostring(player:GetTrader() ~= nil) .. ", spells=" .. #player:GetSpells() .. ", inGameTime=" .. player:GetInGameTime())
        player:SendBroadcastMessage("Homebind map=" .. homebind.mapId .. ", area=" .. homebind.areaId)
        player:SendBroadcastMessage("Played total=" .. player:GetTotalPlayedTime() .. "s, level=" .. player:GetLevelPlayedTime() .. "s")
        player:SendBroadcastMessage("Hearthstone count=" .. player:GetItemCount(6948, true) .. ", defense skill=" .. player:GetSkillValue(95))
        player:SendBroadcastMessage("Stormwind reputation=" .. player:GetReputation(72) .. ", rank=" .. player:GetReputationRank(72))
        player:SendBroadcastMessage("Moving=" .. tostring(player:IsMoving()) .. ", flying=" .. tostring(player:IsFlying()) .. ", falling=" .. tostring(player:IsFalling()) .. ", water=" .. tostring(player:IsInWater()))
        player:SendBroadcastMessage("AFK=" .. tostring(player:IsAFK()) .. ", DND=" .. tostring(player:IsDND()) .. ", rested=" .. tostring(player:IsRested()) .. ", talents=" .. player:GetFreeTalentPoints())
        player:SendBroadcastMessage("Combo=" .. player:GetComboPoints() .. ", canBlock=" .. tostring(player:CanBlock()) .. ", canParry=" .. tostring(player:CanParry()) .. ", immuneDamage=" .. tostring(player:IsImmuneToDamage()))
        player:SendBroadcastMessage("Latency=" .. player:GetLatency() .. "ms, locale=" .. player:GetDbLocaleIndex() .. "/" .. player:GetDbcLocale() .. ", chatTag=" .. player:GetChatTag())
        player:SendBroadcastMessage("CanSpeak=" .. tostring(player:CanSpeak()) .. ", acceptsWhispers=" .. tostring(player:IsAcceptingWhispers()) .. ", atLoginRename=" .. tostring(player:HasAtLoginFlag(1)))
        player:SendBroadcastMessage("BG arena=" .. tostring(player:InArena()) .. ", battleground=" .. tostring(player:InBattleground()) .. ", queue=" .. tostring(player:InBattlegroundQueue()) .. ", bgId=" .. player:GetBattlegroundId())
        player:SendBroadcastMessage("LifetimeHK=" .. player:GetLifetimeKills() .. ", canUseHearth=" .. tostring(player:CanUseItem(6948)) .. ", canEquipHearthMainHand=" .. tostring(player:CanEquipItem(6948, 0)))
        player:SendBroadcastMessage("Eluna Player compat quests=" .. player:GetCompletedQuestsCount() .. ", mail=" .. player:GetMailCount() .. ", honor=" .. player:GetHonorPoints())
        player:SendBroadcastMessage("Classic compat spec=" .. player:GetActiveSpec() .. "/" .. player:GetSpecsCount() .. ", titanGrip=" .. tostring(player:CanTitanGrip()) .. ", title0=" .. tostring(player:HasTitle(0)))
        player:SendBroadcastMessage("Stat bonuses hp=" .. math.floor(player:GetHealthBonusFromStamina()) .. ", mana=" .. math.floor(player:GetManaBonusFromIntellect()) .. ", block=" .. player:GetShieldBlockValue())
        player:SendBroadcastMessage("FireballCooldown=" .. tostring(player:HasSpellCooldown(133)) .. ", delay=" .. player:GetSpellCooldownDelay(133))
        local sampleAura = player:GetAura(2479)
        if sampleAura then
            player:SendBroadcastMessage("Aura " .. sampleAura:GetAuraId() .. " duration=" .. sampleAura:GetDuration() .. ", stack=" .. sampleAura:GetStackAmount() .. ", casterLow=" .. sampleAura:GetCasterGUIDLow())
        else
            player:SendBroadcastMessage("Aura 2479 present=false")
        end

        local spell = GetSpellInfo(133)
        if spell then
            player:SendBroadcastMessage("SpellInfo 133: " .. spell:GetName() .. ", mana=" .. spell:GetManaCost())
            player:SendBroadcastMessage("SpellInfo flags area=" .. tostring(spell:IsAffectingArea()) .. ", combat=" .. tostring(spell:CanBeUsedInCombat()) .. ", positive0=" .. tostring(spell:IsPositiveEffect(0)))
            player:SendBroadcastMessage("SpellInfo cast channeled=" .. tostring(spell:IsChanneled()) .. ", combo=" .. tostring(spell:NeedsComboPoints()) .. ", explicitTarget=" .. tostring(spell:NeedsExplicitUnitTarget()))
            player:SendBroadcastMessage("SpellInfo masks mechanic=" .. spell:GetAllEffectsMechanicMask() .. ", target=" .. spell:GetExplicitTargetMask() .. ", specific=" .. spell:GetSpellSpecific())
            player:SendBroadcastMessage("SpellInfo misc ranged=" .. tostring(spell:IsRangedWeaponSpell()) .. ", autoRepeat=" .. tostring(spell:IsAutoRepeatRangedSpell()) .. ", breakStealth=" .. tostring(spell:IsBreakingStealth()))
            player:SendBroadcastMessage("SpellInfo check selfTarget=" .. spell:CheckTarget(player, player, false) .. ", creatureType=" .. tostring(spell:CheckTargetCreatureType(player)))
        end

        local hearthTemplate = GetItemTemplate(6948)
        if hearthTemplate then
            player:SendBroadcastMessage("ItemTemplate 6948: " .. hearthTemplate:GetName() .. ", quality=" .. hearthTemplate:GetQuality() .. ", stack=" .. hearthTemplate:GetMaxStackSize())
            player:SendBroadcastMessage("ItemTemplate misc display=" .. hearthTemplate:GetDisplayId() .. ", flags=" .. hearthTemplate:GetFlags() .. ", extraFlags=" .. hearthTemplate:GetExtraFlags() .. ", icon='" .. hearthTemplate:GetIcon() .. "'")
        end

        local koboldTemplate = GetCreatureTemplate(6)
        if koboldTemplate then
            player:SendBroadcastMessage("CreatureTemplate 6: " .. koboldTemplate:GetName() .. ", level=" .. koboldTemplate:GetMinLevel() .. "-" .. koboldTemplate:GetMaxLevel() .. ", model=" .. koboldTemplate:GetDisplayId(1))
        end

        local goTemplate = GetGameObjectTemplate(1000167)
        if goTemplate then
            player:SendBroadcastMessage("GameObjectTemplate 1000167: " .. goTemplate:GetName() .. ", type=" .. goTemplate:GetType() .. ", data1=" .. goTemplate:GetData(1))
        end

        local testQuest = GetQuest(783)
        if testQuest then
            player:SendBroadcastMessage("Quest 783 active=" .. tostring(player:HasQuest(testQuest)) .. ", completeable=" .. tostring(player:CanCompleteQuest(testQuest)) .. ", share=" .. tostring(player:CanShareQuest(testQuest)))
            player:SendBroadcastMessage("Quest 783 level=" .. player:GetQuestLevel(testQuest) .. ", repeatableComplete=" .. tostring(player:CanCompleteRepeatableQuest(testQuest)) .. ", reqCount=" .. player:GetReqKillOrCastCurrentCount(testQuest, 6))
            player:SendBroadcastMessage("Quest chain next=" .. testQuest:GetNextQuestId() .. ", prev=" .. testQuest:GetPrevQuestId() .. ", nextChain=" .. testQuest:GetNextQuestInChain() .. ", type=" .. testQuest:GetType() .. ", daily=" .. tostring(testQuest:IsDaily()) .. ", hasFlags=" .. tostring(testQuest:HasFlag(testQuest:GetFlags())))
        end
        player:SendBroadcastMessage("Quest checks: GO1000167=" .. tostring(player:HasQuestForGO(1000167)) .. ", hearthItem=" .. tostring(player:HasQuestForItem(6948)))

        player:SendBroadcastMessage("Online players from Lua: " .. GetPlayerCount())

        return false
    end
end)

-- 播放过场镜头会打断玩家当前画面，所以示例里不在 #lua 命令中自动执行。
-- player:SendCinematicStart(2)
--
-- 以下接口会改玩家状态或造成明显游戏内效果，示例只保留注释，避免输入 #lua 时误改角色。
-- player:SetSkill(95, 0, 300, 300)
-- player:SetReputation(72, 42000)
-- player:SetFreeTalentPoints(1)
-- player:LearnTalent(0, 0)
-- player:ResetTalents(true)
-- player:AdvanceSkillsToMax()
-- player:AdvanceSkill(95, 1)
-- player:AdvanceAllSkills(1)
-- player:SetDrunkValue(0)
-- player:SetRestBonus(0)
-- player:SetAcceptWhispers(true)
-- player:SetGMVisible(true)
-- player:SetGMChat(true)
-- player:SetTaxiCheat(false)
-- player:ToggleAFK()
-- player:ToggleDND()
-- player:Say("Lua say test", 0)
-- player:Yell("Lua yell test", 0)
-- player:TextEmote("Lua emote test")
-- player:SendUpdateWorldState(1, 1)
-- player:SetQuestStatus(783, 3)
-- player:FailQuest(783)
-- player:RemoveQuest(783)
-- player:SetBindPoint(player:GetX(), player:GetY(), player:GetZ(), player:GetMapId(), player:GetAreaId())
-- player:AddComboPoints(target, 1)
-- player:ClearComboPoints()
-- player:ResetAllCooldowns()
-- player:RemoveArenaSpellCooldowns()
-- player:DurabilityRepairAll(true, 1.0)
-- player:DurabilityLossAll(0.1, false)
-- player:KillPlayer()
-- player:SpawnBones()
-- player:PlayDirectSound(1204)
-- player:PlayDistanceSound(1204)
-- player:PlayMusic(1171)
-- player:SpawnCreature(6, player:GetX(), player:GetY(), player:GetZ(), player:GetO(), 8, 60000)
-- player:SetUInt32Value(index, value)
-- player:SetInt32Value(index, value)
-- player:SetFloatValue(index, value)
-- player:SetByteValue(index, offset, value)
-- player:SetUInt16Value(index, offset, value)
-- player:SetInt16Value(index, offset, value)
-- player:SetUInt64Value(index, value)
-- player:SetMaxPower(player:GetPowerType(), player:GetMaxPower())
-- player:ModifyPower(10)
-- player:SetRooted(false)
-- player:SetConfused(false)
-- player:SetFeared(false)
-- player:SetFacing(player:GetO())
-- player:SetFacingToObject(player)
-- player:SetSheath(1)
-- player:PerformEmote(1)
-- player:EmoteState(0)
-- player:StopSpellCast()
-- local aura = player:AddAura(2479, player)
-- if aura then
--     aura:SetDuration(60000)
--     aura:SetMaxDuration(60000)
--     aura:SetStackAmount(1)
--     aura:Remove()
-- end
-- player:RemoveAllAuras()
-- selectedUnit:AddThreat(player, 100.0)
-- selectedUnit:ModifyThreatPct(player, -50)
-- selectedUnit:ClearThreat(player)
-- selectedUnit:ResetAllThreat()
-- selectedUnit:ClearThreatList()
-- player:SetPowerType(0)
-- player:SetSpeed(1, 7.0)
-- player:SetSpeedRate(1, 1.0)
-- player:SetWaterWalk(true)
-- player:SetFFA(true)
-- player:SetSanctuary(true)
-- player:SetImmuneTo(5, true)
-- player:SetName(player:GetName())
-- selectedUnit:SetOwnerGUID(player:GetGUID())
-- selectedUnit:SetCreatorGUID(player:GetGUID())
-- selectedUnit:SetPetGUID(player:GetGUID())
-- selectedUnit:SetCritterGUID(player:GetGUID())
-- selectedUnit:SetInCombatWith(player)
-- selectedUnit:MoveStop()
-- selectedUnit:MoveChase(player)
-- selectedUnit:MoveFollow(player, 3.0, 0.0)
-- selectedUnit:MoveRandom(5.0)
-- selectedUnit:MoveTo(1, player:GetX(), player:GetY(), player:GetZ(), true)
-- selectedUnit:MoveJump(player:GetX(), player:GetY(), player:GetZ(), 10.0, 5.0, 1)
-- selectedUnit:CastSpell(player, 133, true)
-- selectedUnit:CastCustomSpell(player, 133, true, 50, nil, nil)
-- selectedUnit:CastSpellAoF(player:GetX(), player:GetY(), player:GetZ(), 133, true)
-- selectedUnit:NearTeleport(player:GetX(), player:GetY(), player:GetZ(), player:GetO())
-- selectedUnit:SendUnitSay("Lua say", 0)
-- selectedUnit:SendUnitYell("Lua yell", 0)
-- selectedUnit:SendUnitWhisper("Lua whisper", 0, player, false)
-- selectedUnit:SendUnitEmote("Lua emote", player, false)
-- selectedUnit:SendChatMessageToPlayer(0, 0, "Direct chat packet", player)
-- selectedUnit:RemoveArenaAuras(false)
-- selectedUnit:DealDamage(player, 1, false)
-- selectedUnit:DealHeal(player, 2050, 1, false)
-- selectedUnit:HandleStatModifier(0, 2, 1.0, true)
-- selectedUnit:RegisterEvent(function(eventId, delay, repeats, object) end, 1000, 1)
-- selectedUnit:RemoveEventById(1)
-- selectedUnit:RemoveEvents()
-- group:SetLeader(player)
-- group:AddMember(otherPlayer)
-- group:RemoveMember(player:GetGUID(), 0)
-- group:SetMembersGroup(player:GetGUID(), 0)
-- group:SetTargetIcon(0, selectedUnit:GetGUID())
-- group:SetMemberFlag(player:GetGUID(), true, 0x01)
-- guild:SetLeader(player)
-- guild:AddMember(player, guild:GetRank(player))
-- guild:DeleteMember(player, false)
-- guild:SetMemberRank(player, 1)
-- guild:SetName("New Guild Name")
-- guild:SetBankTabText(1, "Tab name")
-- guild:ModifyBankMoney(10000, true)
-- selectedGo:AddLoot(6948, 1)
-- selectedGo:UseDoorOrButton(0)
-- selectedGo:Despawn()
-- selectedGo:SetRespawnTime(60)
-- selectedGo:SaveToDB()
-- selectedGo:RemoveFromWorld(false)
-- local map = player:GetMap()
-- if map then
--     map:SetWeather(player:GetZoneId(), 1, 0.5, false)
--     map:SaveInstanceData()
-- end

-- 命令事件。返回 false 会阻止核心继续解析，所以可以做 Lua 自定义命令。
-- chatHandler 当前还没有封装，暂时为 nil；控制台执行时 player 也是 nil。
-- RegisterPlayerEvent(42, function(event, player, command, chatHandler)
--     if command == "luaping" then
--         if player then
--             player:SendBroadcastMessage("pong from Lua")
--         else
--             print("pong from Lua console command")
--         end
--         return false
--     end
--     return true
-- end)

-- 聊天分流事件。19-22 和 18 一样可以 return false 拦截，也可以 return "新内容" 改写消息。
-- RegisterPlayerEvent(19, function(event, player, message, chatType, language, receiver)
--     print(player:GetName() .. " whispers " .. receiver:GetName() .. ": " .. message)
-- end)
--
-- RegisterPlayerEvent(20, function(event, player, message, chatType, language, group)
--     print(player:GetName() .. " group chat type=" .. chatType .. ", members=" .. group:GetMembersCount())
-- end)
--
-- RegisterPlayerEvent(21, function(event, player, message, chatType, language, guild)
--     print(player:GetName() .. " guild chat in " .. guild:GetName())
-- end)
--
-- RegisterPlayerEvent(22, function(event, player, message, chatType, language, channelId, channel)
--     print(player:GetName() .. " channel " .. channel:GetName() .. " id=" .. channelId .. ": " .. message)
-- end)
--
-- 动作表情包事件。这里是客户端 CMSG_EMOTE 的数字 emote，不是 /me 文本。
-- RegisterPlayerEvent(23, function(event, player, emote)
--     print(player:GetName() .. " emote=" .. emote)
-- end)

-- 玩家施法事件。spell 是本次施法中的动态 Spell 对象，只能在当前回调里使用。
-- RegisterPlayerEvent(5, function(event, player, spell, skipCheck)
--     local spellInfo = spell:GetSpellInfo()
--     local spellName = spellInfo and spellInfo:GetName() or spell:GetEntry()
--     print(player:GetName() .. " cast spell " .. spellName .. ", skipCheck=" .. tostring(skipCheck))
-- end)
--
-- 玩家击杀 / 被击杀事件。
-- RegisterPlayerEvent(6, function(event, killer, killed)
--     print(killer:GetName() .. " killed player " .. killed:GetName())
-- end)
--
-- RegisterPlayerEvent(7, function(event, killer, killed)
--     print(killer:GetName() .. " killed creature " .. killed:GetEntry())
-- end)
--
-- RegisterPlayerEvent(8, function(event, killer, killed)
--     print(killed:GetName() .. " was killed by creature " .. killer:GetEntry())
-- end)
--
-- 决斗事件。type 是核心的 DuelCompleteType 数值，中断时不代表真实胜负。
-- RegisterPlayerEvent(9, function(event, target, challenger)
--     print(challenger:GetName() .. " challenged " .. target:GetName())
-- end)
--
-- RegisterPlayerEvent(10, function(event, player1, player2)
--     print("Duel started: " .. player1:GetName() .. " vs " .. player2:GetName())
-- end)
--
-- RegisterPlayerEvent(11, function(event, winner, loser, type)
--     print("Duel ended: winner=" .. winner:GetName() .. ", loser=" .. loser:GetName() .. ", type=" .. type)
-- end)
--
-- 经验、金币、声望变化事件。返回数字可以改写本次变化；声望返回 -1 或 false 可以阻止变化。
-- RegisterPlayerEvent(12, function(event, player, amount, victim, source)
--     return amount
-- end)
--
-- RegisterPlayerEvent(14, function(event, player, amount)
--     return amount
-- end)
--
-- RegisterPlayerEvent(15, function(event, player, factionId, standing, incremental)
--     return standing
-- end)
--
-- 天赋事件。
-- RegisterPlayerEvent(16, function(event, player, points)
--     print(player:GetName() .. " now has " .. points .. " free talent point(s)")
-- end)
--
-- RegisterPlayerEvent(17, function(event, player, noCost)
--     print(player:GetName() .. " reset talents, noCost=" .. tostring(noCost))
-- end)
--
-- RegisterPlayerEvent(39, function(event, player, talentId, talentRank, spellId)
--     print(player:GetName() .. " learned talent " .. talentId .. " rank=" .. talentRank .. " spell=" .. spellId)
-- end)
--
-- 玩家等级变化事件。触发时玩家已经是新等级，oldLevel 是变化前等级。
-- RegisterPlayerEvent(13, function(event, player, oldLevel)
--     print(player:GetName() .. " level changed: " .. oldLevel .. " -> " .. player:GetLevel())
-- end)
--
-- 角色首次登录事件。正常情况下同一角色只触发一次。
-- RegisterPlayerEvent(30, function(event, player)
--     player:SendBroadcastMessage("这是你的第一次登录，欢迎来到服务器。")
-- end)

-- 玩家文字表情事件。guid 是目标对象的 ObjectGuid。
-- RegisterPlayerEvent(24, function(event, player, textEmote, emoteNum, guid)
--     print("Text emote from " .. player:GetName() .. ": text=" .. textEmote .. ", emote=" .. emoteNum .. ", target=" .. guid:ToString())
-- end)
--
-- 玩家保存前触发。回调里可以读取或更新玩家数据；如果调用 SaveToDB，核心有重入保护。
-- RegisterPlayerEvent(25, function(event, player)
--     print("Saving player " .. player:GetName())
-- end)
--
-- 玩家绑定到副本进度时触发。Turtle 1.12 没有副本难度系统，difficulty 当前固定为 0。
-- RegisterPlayerEvent(26, function(event, player, difficulty, mapId, permanent)
--     print(player:GetName() .. " bound to instance map=" .. mapId .. ", permanent=" .. tostring(permanent))
-- end)
--
-- Zone / Map / Area 变化事件。
-- RegisterPlayerEvent(27, function(event, player, newZone, newArea)
--     print(player:GetName() .. " moved to zone=" .. newZone .. ", area=" .. newArea)
-- end)
--
-- RegisterPlayerEvent(28, function(event, player)
--     print(player:GetName() .. " changed map to " .. player:GetMapId())
-- end)
--
-- 玩家装备物品事件。bag 和 slot 是核心装备位置拆出来的背包/槽位编号。
-- RegisterPlayerEvent(29, function(event, player, item, bag, slot)
--     print(player:GetName() .. " equipped " .. item:GetName() .. " at " .. bag .. ":" .. slot)
-- end)
--
-- 使用物品前的核心可用性检查。返回非 0 的 InventoryResult 可以阻止。
-- RegisterPlayerEvent(31, function(event, player, itemEntry)
--     return 0
-- end)
--
-- 拾取物品事件。guid 是战利品来源对象的 ObjectGuid。
-- RegisterPlayerEvent(32, function(event, player, item, count, guid)
--     local itemName = item and item:GetName() or "unknown"
--     print(player:GetName() .. " looted " .. itemName .. " x" .. count .. ", source=" .. guid:ToString() .. ", low=" .. guid:GetGUIDLow())
-- end)
--
-- 玩家进战 / 脱战事件。
-- RegisterPlayerEvent(33, function(event, player, enemy)
--     local target = enemy and enemy:GetGUIDLow() or 0
--     print(player:GetName() .. " entered combat, enemy guidLow=" .. target)
-- end)
--
-- RegisterPlayerEvent(34, function(event, player)
--     print(player:GetName() .. " left combat")
-- end)
--
-- 释放灵魂 / 复活 / 拾取金币事件。
-- RegisterPlayerEvent(35, function(event, player)
--     print(player:GetName() .. " released spirit")
-- end)
--
-- RegisterPlayerEvent(36, function(event, player)
--     print(player:GetName() .. " resurrected")
-- end)
--
-- RegisterPlayerEvent(37, function(event, player, amount)
--     print(player:GetName() .. " looted money: " .. amount .. " copper")
-- end)
--
-- 战场逃亡事件。type=0 是离开战场，type=5 是离开竞技场。
-- RegisterPlayerEvent(57, function(event, player, type)
--     print(player:GetName() .. " got battleground desertion type=" .. type)
-- end)
--
-- 任务放弃 / 完成事件。
-- RegisterPlayerEvent(38, function(event, player, questId)
--     print(player:GetName() .. " abandoned quest " .. questId)
-- end)
--
-- RegisterPlayerEvent(54, function(event, player, quest)
--     print(player:GetName() .. " completed quest " .. quest:GetId() .. ": " .. quest:GetTitle())
-- end)
--
-- 组队邀请前检查。返回 false 可以阻止邀请。
-- RegisterPlayerEvent(55, function(event, player, memberName)
--     return true
-- end)
--
-- 队伍掷骰获胜并拿到物品后触发。roll 是本次掷骰的 Roll 对象。
-- RegisterPlayerEvent(56, function(event, player, item, count, voteType, roll)
--     print(player:GetName() .. " won roll item " .. item:GetEntry() .. " x" .. count .. ", voteType=" .. voteType)
-- end)
--
-- 接受复活请求前检查。返回 false 可以阻止本次复活。
-- RegisterPlayerEvent(59, function(event, player)
--     return true
-- end)
--
-- 技能增长事件。60 可以拦截，61 可以改写参与计算的当前值，62 在实际增长成功后通知。
-- RegisterPlayerEvent(60, function(event, player, skillId)
--     return true
-- end)
--
-- RegisterPlayerEvent(61, function(event, player, skillId, value, max, step)
--     return value
-- end)
--
-- RegisterPlayerEvent(62, function(event, player, skillId, value, max, step, newValue)
--     print(player:GetName() .. " skill " .. skillId .. " changed: " .. value .. " -> " .. newValue .. "/" .. max)
-- end)
--
-- 宠物或玩家召唤物加入世界事件。pet 是 Creature 对象。
-- RegisterPlayerEvent(43, function(event, player, pet)
--     print(player:GetName() .. " pet/summon added to world: " .. pet:GetEntry())
-- end)
--
-- 宠物、召唤物或玩家控制的 Creature 击杀 Creature 事件。
-- RegisterPlayerEvent(58, function(event, player, killed)
--     print(player:GetName() .. " pet/summon killed creature " .. killed:GetEntry())
-- end)
--
-- 玩家学习法术事件。学习天赋时可能会和 39 学习天赋事件一起触发。
-- RegisterPlayerEvent(44, function(event, player, spellId)
--     print(player:GetName() .. " learned spell " .. spellId)
-- end)
--
-- FFA PvP 状态变化事件。
-- RegisterPlayerEvent(46, function(event, player, hasFfaPvp)
--     print(player:GetName() .. " FFA PvP=" .. tostring(hasFfaPvp))
-- end)
--
-- RegisterPlayerEvent(47, function(event, player, oldArea, newArea)
--     print(player:GetName() .. " changed area: " .. oldArea .. " -> " .. newArea)
-- end)
--
-- 发起交易前检查。返回 false 可以阻止本次交易。
-- RegisterPlayerEvent(48, function(event, player, target)
--     return true
-- end)
--
-- 发送邮件前检查。receiverGuid/mailboxGuid 是 ObjectGuid。
-- RegisterPlayerEvent(49, function(event, player, receiverGuid, mailboxGuid, subject, body, money, cod, item)
--     print("Mail receiver=" .. receiverGuid:ToString() .. ", mailbox=" .. mailboxGuid:ToString())
--     return true
-- end)
--
-- 任务奖励物品 / 新建物品 / 新物品入包事件。
-- RegisterPlayerEvent(51, function(event, player, item, count)
--     print(player:GetName() .. " received quest reward item " .. item:GetName() .. " x" .. count)
-- end)
--
-- RegisterPlayerEvent(52, function(event, player, item, count)
--     print(player:GetName() .. " created item " .. item:GetEntry() .. " x" .. count)
-- end)
--
-- RegisterPlayerEvent(53, function(event, player, item, count)
--     print(player:GetName() .. " stored new item " .. item:GetEntry() .. " x" .. count)
-- end)

-- 示例：把 12345 替换成真实 NPC entry 后，可测试进战事件。
RegisterCreatureEvent(12345, 1, function(event, creature, target)
    creature:Say("Lua combat hook online", 0)
end)

RegisterCreatureEvent(12345, 9, function(event, creature, attacker, damage)
    -- 返回数字可以改写本次伤害；这里保持原伤害，只做接口示例。
    return damage
end)

RegisterCreatureEvent(12345, 12, function(event, creature, target)
    -- 宠物或守护者的主人攻击目标时触发。
end)

RegisterCreatureEvent(12345, 13, function(event, creature, attacker)
    -- 宠物或守护者的主人被攻击时触发。
end)

RegisterCreatureEvent(12345, 14, function(event, creature, caster, spellId)
    creature:TextEmote("Lua spell hit hook: " .. spellId, caster)
end)

RegisterCreatureEvent(12345, 19, function(event, creature, summon)
    creature:Say("Lua summoned creature " .. summon:GetEntry(), 0)
end)

RegisterCreatureEvent(12345, 20, function(event, creature, summon)
    creature:Say("Lua summon despawned " .. summon:GetEntry(), 0)
end)

-- Spell 动态施法事件：把 133 替换成真实 spell id 后可测试。
-- RegisterSpellEvent(133, 1, function(event, caster, spell)
--     print("Spell prepare: " .. spell:GetEntry() .. ", castTime=" .. spell:GetCastTime())
-- end)
--
-- RegisterSpellEvent(133, 2, function(event, caster, spell, skipCheck)
--     local target = spell:GetTarget()
--     if target then
--         print("Spell cast target guid=" .. target:GetGUID():ToString())
--     end
--     local x, y, z = spell:GetTargetDest()
--     local reagentTotal = 0
--     for _, count in pairs(spell:GetReagentCost()) do
--         reagentTotal = reagentTotal + count
--     end
--     print("Spell state autoRepeat=" .. tostring(spell:IsAutoRepeat()) .. ", duration=" .. spell:GetDuration() .. ", reagents=" .. reagentTotal .. ", dest=" .. tostring(x) .. "," .. tostring(y) .. "," .. tostring(z))
-- end)
--
-- RegisterSpellEvent(133, 3, function(event, caster, spell, bySelf)
--     print("Spell canceled: " .. spell:GetEntry())
-- end)
--
-- 以下动态 Spell 接口会改变当前施法流程，真实脚本里要确认事件阶段和核心状态后再用。
-- spell:SetAutoRepeat(false)
-- spell:Cast(true)
-- spell:Finish(true)
-- spell:Cancel()

RegisterCreatureEvent(12345, 27, function(event, creature, unit)
    -- 视野事件很频繁，真实脚本里尽量只做轻量判断。
end)

-- DummyEffect：需要目标 entry 和法术本身确实走核心 DummyEffect 入口。
-- RegisterCreatureEvent(12345, 30, function(event, caster, spellId, effIndex, creature)
--     creature:TextEmote("Lua DummyEffect: " .. spellId, caster)
-- end)
--
-- RegisterGameObjectEvent(12345, 3, function(event, caster, spellId, effIndex, gameobject)
--     print("GO DummyEffect " .. spellId .. " on " .. gameobject:GetName())
-- end)
--
-- RegisterItemEvent(12345, 1, function(event, caster, spellId, effIndex, item)
--     print("Item DummyEffect " .. spellId .. " on " .. item:GetName())
-- end)

-- 物品使用事件：把 12345 替换成真实 item entry。默认继续释放物品法术；return false 会阻止释放。
-- RegisterItemEvent(12345, 2, function(event, player, item, targets)
--     player:SendBroadcastMessage("Lua item use: " .. item:GetName() .. ", count=" .. item:GetCount())
--     local targetGuid = targets:GetUnitTargetGUID()
--     if not targetGuid:IsEmpty() then
--         player:SendBroadcastMessage("Item use target GUID=" .. targetGuid:ToString())
--     end
--     return true
-- end)
--
-- 物品 Gossip：把 12345 替换成真实 item entry，右键物品会打开菜单。
-- 如果该物品同时有使用法术，在 Hello 里 return false 可以阻止本次物品法术继续释放。
-- RegisterItemGossipEvent(12345, 1, function(event, player, item)
--     player:GossipClearMenu()
--     player:GossipMenuAddItem(0, "查看这个 Lua 物品", 0, 1)
--     player:GossipMenuAddItem(0, "关闭", 0, 2)
--     player:GossipSendMenu(1, item)
--     return false
-- end)
--
-- RegisterItemGossipEvent(12345, 2, function(event, player, item, sender, action, code)
--     if action == 1 then
--         player:SendBroadcastMessage("Item gossip: " .. item:GetName() .. ", guid=" .. item:GetGUID():ToString())
--     end
--     player:GossipComplete()
-- end)
--
-- 限时物品到期前触发；return true 可以阻止这次到期删除。
-- RegisterItemEvent(12345, 4, function(event, player, itemId)
--     player:SendBroadcastMessage("Lua item expire: " .. itemId)
-- end)
--
-- 物品实际删除前触发；return true 可以阻止这次删除。
-- RegisterItemEvent(12345, 5, function(event, player, item)
--     player:SendBroadcastMessage("Lua item remove: " .. item:GetName())
-- end)

-- 生命周期事件会在对象进入/离开世界时触发，真实脚本里建议只做轻量初始化和清理。
-- RegisterCreatureEvent(12345, 36, function(event, creature)
--     creature:Say("Lua creature added to world", 0)
-- end)
--
-- RegisterCreatureEvent(12345, 37, function(event, creature)
--     print("Creature removed: " .. creature:GetEntry())
-- end)
--
-- RegisterGameObjectEvent(12345, 12, function(event, gameobject)
--     print("GameObject added: " .. gameobject:GetName())
-- end)
--
-- RegisterGameObjectEvent(12345, 13, function(event, gameobject)
--     print("GameObject removed: " .. gameobject:GetName())
-- end)
--
-- RegisterGameObjectEvent(12345, 1, function(event, gameobject, diff)
--     -- AIUpdate 可能每次地图更新都会触发，真实脚本里要保持轻量。
-- end)
--
-- RegisterGameObjectEvent(12345, 2, function(event, gameobject)
--     print("GameObject spawned again: " .. gameobject:GetName())
-- end)

local uniqueCreatureBindings = {}
RegisterPlayerEvent(18, function(event, player, message)
    if message == "#luaunique" then
        local creature = player:GetSelectedCreature()
        if not creature then
            player:SendBroadcastMessage("请先选中一个生物，再输入 #luaunique")
            return false
        end

        local key = creature:GetGUID():ToString() .. ":" .. creature:GetInstanceId()
        if not uniqueCreatureBindings[key] then
            RegisterUniqueCreatureEvent(creature, creature:GetInstanceId(), 1, function(uniqueEvent, uniqueCreature, target)
                uniqueCreature:Say("这个 Lua 事件只绑定到我这一个生物实例。", 0)
            end)
            uniqueCreatureBindings[key] = true
        end

        player:SendBroadcastMessage("已绑定唯一生物事件；让这个生物进入战斗即可看到效果。")
        return false
    end
end)

-- 示例：把 12345 替换成真实 NPC entry 后，可测试任务对象和 Gossip 菜单。
RegisterCreatureEvent(12345, 31, function(event, player, creature, quest)
    player:SendBroadcastMessage("Lua quest accepted: " .. quest:GetTitle())
end)

RegisterCreatureGossipEvent(12345, 1, function(event, player, creature)
    player:GossipClearMenu()
    player:GossipMenuAddItem(0, "Lua gossip test", 0, 1)
    player:GossipSendMenu(1, creature)
    return true
end)

RegisterCreatureGossipEvent(12345, 2, function(event, player, creature, sender, action, code)
    if action == 1 then
        player:SendBroadcastMessage("Nearest player: " .. player:GetName())
        player:GossipComplete()
        return true
    end
end)
