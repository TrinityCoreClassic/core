#!/usr/bin/python3

import constants
import database as db

#TODO

def Import():
    remove_poi()
    import_templates_vmangos()
    remove_modern()
    
def remove_poi():
    db.tri_world.execute_raw("DELETE FROM quest_poi_points")
    db.tri_world.execute_raw("DELETE FROM quest_poi")
    
def remove_modern():
    db.tri_world.chunk_raw(
        "SELECT ID FROM quest_template WHERE Expansion > 2 AND VerifiedBuild <> 40618 LIMIT %s OFFSET %s",
        500,
        _handle_obsolete_modern_row
    )
    db.tri_world.execute_raw("DELETE FROM quest_details WHERE ID NOT IN (SELECT ID FROM quest_template)")
    db.tri_world.execute_raw("DELETE FROM quest_greeting WHERE ID NOT IN (SELECT ID FROM quest_template)")
    db.tri_world.execute_raw("DELETE FROM quest_visual_effect WHERE ID NOT IN (SELECT ID FROM quest_objectives)")
    
def _handle_obsolete_modern_row(row):
    queries = [
        "DELETE FROM quest_objectives WHERE QuestID = %s",
        "DELETE FROM quest_objectives_locale WHERE QuestId = %s",
        "DELETE FROM quest_offer_reward WHERE ID = %s",
        "DELETE FROM quest_offer_reward_locale WHERE ID = %s",
        "DELETE FROM quest_poi WHERE QuestID = %s",
        "DELETE FROM quest_poi_points WHERE QuestID = %s",
        "DELETE FROM quest_pool_members WHERE questId = %s",
        "DELETE FROM quest_request_items WHERE ID = %s",
        "DELETE FROM quest_request_items_locale WHERE ID = %s",
        "DELETE FROM quest_reward_display_spell WHERE QuestID = %s",
        "DELETE FROM quest_template_addon WHERE ID = %s",
        "DELETE FROM quest_template_locale WHERE ID = %s",
        "DELETE FROM quest_template WHERE ID = %s"
    ]
    
    db.tri_world.execute_many_raw(queries, (row[0],))
    
    return -1
    
def import_templates_vmangos():
    
    rows = db.vm_world.select_chunked(
        db.SelectQuery("quest_template").order_by("entry ASC"),
        250
    )
    
    for row in rows:
        existing_record = db.tri_world.select_one(
                db.SelectQuery("quest_template").where("ID", "=", row['entry'])
            )
        
        _upsert_quest_template(row, existing_record)


def _upsert_quest_template(vm_qt, tri_qt = None):
    
    #TODO rewards
    #TODO other tables.
    
    #TODO handle check all fields.
    
    #TODO check how flags can be translated.
    flags = vm_qt['QuestFlags']
    flags_ex = 0
    flags_ex2 = 0
    
    upsert_query = db.UpsertQuery("quest_template").values({
        'QuestType': vm_qt['Method'],
        'QuestLevel': vm_qt['QuestLevel'],
        'MinLevel': vm_qt['MinLevel'],
        'QuestSortID': vm_qt['ZoneOrSort'],
        'QuestInfoID': vm_qt['Type'],
        'SuggestedGroupNum': vm_qt['SuggestedPlayers'],
        'RewardNextQuest': vm_qt['NextQuestInChain'],
        'RewardDisplaySpell1': vm_qt['RewSpell'],
        'RewardSpell': vm_qt['RewSpellCast'],
        'StartItem': vm_qt['SrcItemId'],
        'Flags': flags,
        'FlagsEx': flags_ex,
        'FlagsEx2': flags_ex2,
        'RewardItem1': vm_qt['RewItemId1'],
        'RewardAmount1': vm_qt['RewItemCount1'],
        'RewardItem2': vm_qt['RewItemId2'],
        'RewardAmount2': vm_qt['RewItemCount2'],
        'RewardItem3': vm_qt['RewItemId3'],
        'RewardAmount3': vm_qt['RewItemCount3'],
        'RewardItem4': vm_qt['RewItemId4'],
        'RewardAmount4': vm_qt['RewItemCount4'],
        'ItemDrop1': vm_qt['ReqSourceId1'],
        'ItemDropQuantity1': vm_qt['ReqSourceCount1'],
        'ItemDrop2': vm_qt['ReqSourceId2'],
        'ItemDropQuantity2': vm_qt['ReqSourceCount2'],
        'ItemDrop3': vm_qt['ReqSourceId3'],
        'ItemDropQuantity3': vm_qt['ReqSourceCount3'],
        'ItemDrop4': vm_qt['ReqSourceId4'],
        'ItemDropQuantity4': vm_qt['ReqSourceCount4'],
        'RewardChoiceItemID1': vm_qt['RewChoiceItemId1'],
        'RewardChoiceItemQuantity1': vm_qt['RewChoiceItemCount1'],
        'RewardChoiceItemID2': vm_qt['RewChoiceItemId2'],
        'RewardChoiceItemQuantity2': vm_qt['RewChoiceItemCount2'],
        'RewardChoiceItemID3': vm_qt['RewChoiceItemId3'],
        'RewardChoiceItemQuantity3': vm_qt['RewChoiceItemCount3'],
        'RewardChoiceItemID4': vm_qt['RewChoiceItemId4'],
        'RewardChoiceItemQuantity4': vm_qt['RewChoiceItemCount4'],
        'RewardChoiceItemID5': vm_qt['RewChoiceItemId5'],
        'RewardChoiceItemQuantity5': vm_qt['RewChoiceItemCount5'],
        'RewardChoiceItemID6': vm_qt['RewChoiceItemId6'],
        'RewardChoiceItemQuantity6': vm_qt['RewChoiceItemCount6'],
        'POIContinent': vm_qt['PointMapId'],
        'POIx': vm_qt['PointX'],
        'POIy': vm_qt['PointY'],
        'POIPriority': vm_qt['PointOpt'],
        'RewardFactionID1': vm_qt['RewRepFaction1'],
        'RewardFactionID2': vm_qt['RewRepFaction2'],
        'RewardFactionID3': vm_qt['RewRepFaction3'],
        'RewardFactionID4': vm_qt['RewRepFaction4'],
        'RewardFactionID5': vm_qt['RewRepFaction5'],
        'Expansion': 0,
        'LogTitle': vm_qt['Title'],
        'LogDescription': vm_qt['Objectives'],
        'QuestDescription': vm_qt['Details'],
        'AreaDescription': vm_qt['EndText'],
        'VerifiedBuild': constants.TargetBuild,
    })
    
    for i in range(1, 6): # 1...5
        rew_faction = rep_value_convert(vm_qt['RewRepValue{}'.format(i)])
        upsert_query.values({
            'RewardFactionValue{}'.format(i): rew_faction['value'],
            'RewardFactionOverride{}'.format(i): rew_faction['override']
        })
    
    if tri_qt == None:
        upsert_query.values({
            "ID": vm_qt['entry']
        })
    else:
        upsert_query.where("ID", "=", tri_qt['ID'])
        
    db.tri_world.upsert(upsert_query)
            
    existing_addon = db.tri_world.select_one(
        db.SelectQuery("quest_template_addon").where("ID", "=", vm_qt['entry'])
    )
    
    #TODO decide how to correctly handle column differences
    next_quest_id = vm_qt['NextQuestId']
    if(next_quest_id < 0):
            next_quest_id = 0

    #TODO translate special flags
    special_flags = 0
    addon_upsert_query = db.UpsertQuery("quest_template_addon").values({
        'MaxLevel': vm_qt['MaxLevel'],
        'AllowableClasses': vm_qt['RequiredClasses'],
        'SourceSpellID': vm_qt['SrcSpell'],
        'PrevQuestID': vm_qt['PrevQuestId'],
        'NextQuestID': next_quest_id,
        'ExclusiveGroup': vm_qt['ExclusiveGroup'],
        'BreadcrumbForQuestId': vm_qt['BreadcrumbForQuestId'],
        'RequiredSkillID': vm_qt['RequiredSkill'],
        'RequiredSkillPoints': vm_qt['RequiredSkillValue'],
        'RequiredMinRepFaction': vm_qt['RequiredMinRepFaction'],
        'RequiredMaxRepFaction': vm_qt['RequiredMaxRepFaction'],
        'RequiredMinRepValue': vm_qt['RequiredMinRepValue'],
        'RequiredMaxRepValue': vm_qt['RequiredMaxRepValue'],
        'ProvidedItemCount': vm_qt['SrcItemCount'],
        'SpecialFlags': special_flags
    })
    
    if existing_addon == None:
        addon_upsert_query.values({
            'ID': vm_qt['entry']
        })
    else:
        addon_upsert_query.where("ID", "=", vm_qt['entry'])
        
    db.tri_world.upsert(addon_upsert_query)
        
    _update_quest_objectives(vm_qt['entry'], vm_qt)
    _update_quest_offer_reward(vm_qt['entry'], vm_qt)
    _update_quest_request_items(vm_qt['entry'], vm_qt)
    
 
    
def _update_quest_objectives(quest_id, vm_qt):
    
    existing_objectives = db.tri_world.select_all(
        db.SelectQuery("quest_objectives").where('QuestID', '=', quest_id).order_by(["`Order` ASC", '`ID` ASC'])
    )
    
    #TODO check all fields
    #TODO handle spellCast (trinity doesnt seem to use this, but the GO for it instead), possibly easier to manually add.
    #TODO ensure all objective types are covered.
    
    #order is GO/Creature, Item, MinRep, Money
    #storage index appears to the same as order.
    #type for and above (money/currency and rep) appear to be -1 storage index.
    
    intermediate_rows = [] #type, id, amount
    
    descriptions = [
        vm_qt['ObjectiveText1'],
        vm_qt['ObjectiveText2'],
        vm_qt['ObjectiveText3'],
        vm_qt['ObjectiveText4']
    ]
    
    #creatures / GO
    for i in range(1, 4 + 1):
        count_name = 'ReqCreatureOrGOCount' + str(i)
        id_name = 'ReqCreatureOrGOId' + str(i)
        if vm_qt[count_name] > 0:
            row_type = 0 if vm_qt[id_name] > 0 else 2
            intermediate_rows.append([row_type, vm_qt[id_name], vm_qt[count_name]])
    
    #items
    for i in range(1, 4 + 1):
        count_name = 'ReqItemCount' + str(i)
        id_name = 'ReqItemId' + str(i)
        if vm_qt[count_name] > 0:
            intermediate_rows.append([1, vm_qt[id_name], vm_qt[count_name]])
    
    #rep
    if vm_qt['RepObjectiveValue'] > 0:
        intermediate_rows.append([6, vm_qt['RepObjectiveFaction'], vm_qt['RepObjectiveValue']])
            
    #money
    if vm_qt['RewOrReqMoney'] < 0:
        intermediate_rows.append([8, 0, vm_qt['RewOrReqMoney']])
            
    added_ids = []
            
    order_index = 0
    storage_index = 0
    for intermediate in intermediate_rows:
        existing_id = 0
        
        if len(existing_objectives) > order_index:
            existing_id = existing_objectives[order_index]['ID']
            
            
        effective_storage_index = -1
        description = ""
        if intermediate[0] < 4:
            effective_storage_index = storage_index
            if len(descriptions) > storage_index:
                description = descriptions[storage_index]
            storage_index += 1
        
        #TODO confirm descriptions match objectives. 
        #TODO handle flags.
        objective_upsert = db.UpsertQuery("quest_objectives").values({
            'QuestID': quest_id,
            'type': intermediate[0],
            '`Order`': order_index,
            'StorageIndex': effective_storage_index,
            'ObjectID': intermediate[1],
            'Amount': intermediate[2],
            'Flags': 0,
            'Flags2': 0,
            'ProgressBarWeight': 0,
            'Description': description,
            'VerifiedBuild': constants.TargetBuild
        })
        
        added_id = existing_id
        
        if existing_id > 0:
            objective_upsert.where('ID', '=', existing_id)
        else:
            last_id = db.tri_world.get_row_raw("SELECT MAX(ID) FROM quest_objectives")
            added_id = last_id[0] + 1
            objective_upsert.values({
                'ID': last_id[0] + 1
            })
            
        db.tri_world.upsert(objective_upsert)
        added_ids.append(added_id)
        
        order_index += 1
    
    del_cond = db.GroupCondition('AND').condition('QuestID', '=', quest_id)
    
    if len(added_ids) > 0:
        del_cond.condition('ID', 'NOT IN', added_ids)
    
    db.tri_world.delete(
        db.DeleteQuery("quest_objectives").where(del_cond)
    )  
 
def _update_quest_offer_reward(quest_id, vm_qt):
    existing_record = db.tri_world.select_one(
                db.SelectQuery("quest_offer_reward").where("ID", "=", quest_id)
            )
    
    upsert = db.UpsertQuery("quest_offer_reward").values({
        'Emote1': vm_qt['OfferRewardEmote1'],
        'Emote2': vm_qt['OfferRewardEmote2'],
        'Emote3': vm_qt['OfferRewardEmote3'],
        'Emote4': vm_qt['OfferRewardEmote4'],
        'EmoteDelay1': vm_qt['OfferRewardEmoteDelay1'],
        'EmoteDelay2': vm_qt['OfferRewardEmoteDelay2'],
        'EmoteDelay3': vm_qt['OfferRewardEmoteDelay3'],
        'EmoteDelay4': vm_qt['OfferRewardEmoteDelay4'],
        'RewardText': vm_qt['OfferRewardText'],
        'VerifiedBuild': constants.TargetBuild
    })
    
    if existing_record == None:
        upsert.values({
            'ID': quest_id
        })
    else:
        upsert.where("ID", "=", quest_id)
        
    db.tri_world.upsert(upsert)

def _update_quest_request_items(quest_id, vm_qt):
    existing_record = db.tri_world.select_one(
                db.SelectQuery("quest_request_items").where("ID", "=", quest_id)
            )
    
    upsert = db.UpsertQuery("quest_request_items").values({
        'EmoteOnComplete': vm_qt['CompleteEmote'],
        'EmoteOnIncomplete': vm_qt['IncompleteEmote'],
        'EmoteOnCompleteDelay': 0,
        'EmoteOnIncompleteDelay': 0,
        'CompletionText': vm_qt['RequestItemsText'],
        'VerifiedBuild': constants.TargetBuild
    })
     
    if existing_record == None:
        upsert.values({
            'ID': quest_id
        })
    else:
        upsert.where("ID", "=", quest_id)
        
    db.tri_world.upsert(upsert)
        
# convert vmangos value to TC index.
def rep_value_convert(value):
    # see questfactionreward.db2
    conv = {
        -9: -5,
        -8: -1,
        -7: -1000,
        -6: -750,
        -5: -500,
        -4: -375,
        -3: -250,
        -2: -125,
        -1: -50,
        0: 0,
        1: 10,
        2: 25,
        3: 50, 
        4: 75,
        5: 100,
        6: 150,
        7: 200,
        8: 1,
        9: 5
    }
    
    result = {
        'value': 0,
        'override': 0
    }
    
    for i, (key, val) in enumerate(conv.items()):
        if val == value:
            result['value'] = key
            break
    
    if result['value'] == 0:
        result['override'] = value * 100

    return result