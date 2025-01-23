#!/usr/bin/python3

import constants
import database as db

classes_str = ','.join(map(str, constants.ClassIds))
races_str = ','.join(map(str, constants.RaceIds))

def Import():
    cleanClassExpansionRequirements()
    cleanRaceUnlockRequirements()
    cleanPlayerCreateInfo()
    handleXpForLevel()
    handleExplorationXP()
    update_player_create_info()
    update_class_level_stats()


def cleanClassExpansionRequirements():
    query = "DELETE FROM class_expansion_requirement WHERE ClassID NOT IN ({0}) OR RaceID NOT IN ({1})".format(classes_str, races_str)
    db.tri_world.execute_raw(query)
    
def cleanRaceUnlockRequirements():
    query = "DELETE FROM race_unlock_requirement WHERE raceID NOT IN ({0})".format(races_str)
    db.tri_world.execute_raw(query)
    
def cleanPlayerCreateInfo():
    query = "DELETE FROM playercreateinfo WHERE class NOT IN ({0}) OR race NOT IN ({1})".format(classes_str, races_str)
    db.tri_world.execute_raw(query)
    query2 = "DELETE FROM playercreateinfo_action WHERE class NOT IN ({0}) OR race NOT IN ({1})".format(classes_str, races_str)
    db.tri_world.execute_raw(query2)
    
def handleXpForLevel():
    rows = db.vm_world.get_rows_raw("SELECT * FROM player_xp_for_level")
    db.tri_world.execute_raw("DELETE FROM player_xp_for_level")
    
    for row in rows:
        db.tri_world.execute_raw(
            "INSERT INTO player_xp_for_level (Level, Experience) VALUES (%s, %s)",
            (row[0], row[1],)
        )
        
def handleExplorationXP():
    rows = db.vm_world.get_rows_raw("SELECT * FROM exploration_basexp")
    db.tri_world.execute_raw("DELETE FROM exploration_basexp")
    
    for row in rows:
        db.tri_world.execute_raw(
            "INSERT INTO exploration_basexp (level, basexp) VALUES (%s, %s)",
            (row[0], row[1],)
        )
    
def update_player_create_info():
    vm_rows = db.vm_world.get_rows_raw("SELECT map, position_x, position_y, position_z, orientation, race, class FROM playercreateinfo")
    
    for vm_row in vm_rows:
        update_query = ("UPDATE playercreateinfo SET "
                        "map = %s, position_x = %s, position_y = %s, position_z = %s, orientation = %s "
                        "WHERE race = %s AND class = %s")
        db.tri_world.execute_raw(update_query, vm_row)
    
    race_class_combos =  db.vm_world.select_all(
        db.SelectQuery("playercreateinfo_action").select('race, class').group_by(['race', 'class'])
    )
    
    for row in race_class_combos:
        db.tri_world.delete(
            db.DeleteQuery('playercreateinfo_action').where(
                db.GroupCondition('AND').condition('race', '=', row['race']).condition('class', '=', row['class'])
            )
        )
    
    vm_rows = db.vm_world.select_all(db.SelectQuery("playercreateinfo_action"))
    
    for row in vm_rows:
        db.tri_world.upsert(
            db.UpsertQuery('playercreateinfo_action').values({
                'race': row['race'],
                'class': row['class'],
                'button': row['button'],
                'action': row['action'],
                'type': row['type']
            })
        )
        
    db.tri_world.delete(
        db.DeleteQuery('playercreateinfo_cast_spell')
    )

        
def update_class_level_stats():
    
    vm_rows = db.vm_world.select_all(db.SelectQuery("player_levelstats"))
    
    for row in vm_rows:
        cond = db.GroupCondition("AND").condition("race", "=", row['race']).condition("class", "=", row['class']).condition("level", "=", row['level'])
        
        existing = db.tri_world.select_one(
            db.SelectQuery('player_classlevelstats')
                .where(cond)
        )
        
        upsert = db.UpsertQuery("player_classlevelstats").values({
            'str': row['str'],
            'agi': row['agi'],
            'sta': row['sta'],
            'inte': row['inte'],
            'spi': row['spi'],
            'VerifiedBuild': constants.TargetBuild
        })
        
        if existing:
            upsert.where(cond)
        else:
            upsert.values({
                'race': row['race'],
                'class': row['class'],
                'level': row['level'],
                'basehp': 0,
                'basemana': 0
            })

        db.tri_world.upsert(upsert)
    
    vm_rows = db.vm_world.get_rows_raw("SELECT basehp, basemana, class, level FROM player_classlevelstats")
    
    for vm_row in vm_rows:
        update_query = ("UPDATE player_classlevelstats SET "
                        "basehp = %s, basemana = %s "
                        "WHERE class = %s AND level = %s")
        
        db.tri_world.execute_raw(update_query, vm_row)
        