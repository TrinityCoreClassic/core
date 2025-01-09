#!/usr/bin/python3

import constants
import database as db

def Import():
    handle_spell_target_positions()
    handle_spell_groups()
    handle_spell_threat()
    handle_spell_ranks()

def handle_spell_target_positions():
    vm_rows = db.vm_world.get_rows_raw("SELECT id, target_map, target_position_x, target_position_y, target_position_z, target_orientation FROM spell_target_position WHERE build_max = 5875")
    
    for vm_row in vm_rows:
        existing = db.tri_world.get_row_raw("SELECT ID FROM spell_target_position WHERE ID = %s", (vm_row[0],))
        
        #TODO confirm EffectIndex usage.
        #TODO orientaion.
        
        if(existing == None):
            db.tri_world.execute_raw((
                "INSERT INTO spell_target_position ("
                "ID, EffectIndex, MapID, PositionX, PositionY, PositionZ, VerifiedBuild"
                ") VALUES ("
                "%s, 0, %s, %s, %s, %s, 40618"
                ")"
            ), (
                vm_row[0],
                vm_row[1],
                vm_row[2],
                vm_row[3],
                vm_row[4],
            ))
        else:
            db.tri_world.execute_raw((
                "UPDATE spell_target_position SET "
                "MapID = %s, PositionX = %s, PositionY = %s, PositionZ = %s, VerifiedBuild = 40618 "
                "WHERE ID = %s"
            ), (
                vm_row[1],
                vm_row[2],
                vm_row[3],
                vm_row[4],
                vm_row[0],
            ))
            
    #TODO delete excess.
    
    
def handle_spell_groups():
    
    vm_rows = db.vm_world.select_chunked(
        db.SelectQuery("spell_group").where("build_max", "=", 5875).order_by("group_id ASC"), 
        500
    )
    
    for vm_row in vm_rows:
        existing = db.tri_world.select_one(
            db.SelectQuery("spell_group").where(
                db.GroupCondition("AND").condition(
                    'id', "=", vm_row['group_id']
                ).condition(
                    'spell_id', '=', vm_row['spell_id']
                )
            )
        )
        
        if existing == None:
            upsert = db.UpsertQuery('spell_group').values({
                'id': vm_row['group_id'],
                'spell_id': vm_row['spell_id']
            })

            db.tri_world.upsert(upsert)
            
    vm_stack_rows = db.vm_world.select_chunked(
        db.SelectQuery("spell_group_stack_rules").order_by("group_id ASC"), 
        500
    )
    
    for vm_stack in vm_stack_rows:
        existing = db.tri_world.select_one(
            db.SelectQuery("spell_group_stack_rules").where('group_id', '=', vm_stack['group_id'])
        )
        
        upsert = db.UpsertQuery('spell_group_stack_rules').values({
            'stack_rule': vm_stack['stack_rule']
        })
        
        if existing == None:
            upsert.values({'group_id': vm_stack['group_id']})
        else:
             upsert.where("group_id", "=", vm_stack['group_id'])
             
        db.tri_world.upsert(upsert)
             
def handle_spell_threat():
    vm_rows = db.vm_world.select_chunked(
        db.SelectQuery("spell_threat").where("build_max", "=", 5875).order_by("entry ASC"), 
        500
    )
    
    for vm_row in vm_rows:
        
        existing = db.tri_world.select_one(
            db.SelectQuery("spell_threat").where("entry", "=", vm_row['entry'])
        )
    
        upsert = db.UpsertQuery("spell_threat").values({
            'flatMod': vm_row['Threat'],
            'pctMod': vm_row['multiplier'],
            'apPctMod': vm_row['ap_bonus']
        })
        
        if existing == None:
            upsert.values({
                'entry': vm_row['entry']
            })
        else:
            upsert.where("entry", "=", vm_row['entry'])
            
        db.tri_world.upsert(upsert)
        
        
def handle_spell_ranks():
    
    vm_first_spells = db.vm_world.select_chunked(
        db.SelectQuery("spell_chain").select("first_spell").where("build_max", "=", 5875).group_by("first_spell").order_by("first_spell ASC"),
        500
    )
        
    for vm_first_spell in vm_first_spells:
        vm_spells = db.vm_world.select_all(
            db.SelectQuery("spell_chain").where(
                db.GroupCondition("AND").condition("build_max", "=", 5875).condition("first_spell", "=", vm_first_spell["first_spell"])
            ).order_by("spell_id ASC")
        )
        
        for vm_spell in vm_spells:
            # avoid hassle with duplicate keys, always delete.
            db.tri_world.delete(
                db.DeleteQuery("spell_ranks").where("spell_id", "=", vm_spell['spell_id'])
            )
            
            existing_cond = db.GroupCondition("AND").condition("first_spell_id", "=", vm_spell['first_spell']).condition("rank", "=", vm_spell["rank"])
            
            existing = db.tri_world.select_one(
                db.SelectQuery("spell_ranks").where(existing_cond)
            )
            
            if existing:
                db.tri_world.upsert(
                    db.UpsertQuery("spell_ranks").values({
                        'spell_id': vm_spell['spell_id']
                    }).where(existing_cond)
                )
            else:
                db.tri_world.upsert(
                    db.UpsertQuery("spell_ranks").values({
                        'first_spell_id': vm_spell['first_spell'],
                        'rank': vm_spell['rank'],
                        'spell_id': vm_spell['spell_id']
                    })
                )
                
    
        
    
        
        
        