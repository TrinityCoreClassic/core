#!/usr/bin/python3

import constants
import database as db
import mysql.connector
import importer.area
import importer.battleground
import importer.broadcast;
import importer.creature
import importer.event
import importer.gameobject
import importer.graveyard
import importer.gossip
import importer.item
import importer.loot
import importer.misc
import importer.npc
import importer.player
import importer.pool
import importer.quest
import importer.skill
import importer.spell
import importer.trainer
import importer.transport
import importer.waypoint
import importer.cleanup

print("Starting VMangos -> Trinity DB Import...")

print("Opening DB...")
db.OpenAll()

print("Misc...")
importer.misc.Import()

print("Pools (clear)...")
importer.pool.Clear()

print("Events...")
importer.event.Import()

print("Areas...")
importer.area.Import()

print("Battlegrounds...")
importer.battleground.Import()

print("Broadcast Text...")
importer.broadcast.Import()

print("Creatures...")
importer.creature.Import()

print("GameObjects...")
importer.gameobject.Import()

print("gossips...")
importer.gossip.Import()

print("graveyards...")
importer.graveyard.Import()

print("Items...")
importer.item.Import()

print("Loot...")
importer.loot.Import()

print("NPCs...")
importer.npc.Import()

print("Players...")
importer.player.Import()

print("Pool...")
importer.pool.Import()

print("Quests...")
importer.quest.Import()

print("Skills...")
importer.skill.Import()

print("Spells...")
importer.spell.Import()

print("Trainers")
importer.trainer.Import()

print("Transports...")
importer.transport.Import()

print("Waypoints...")
importer.waypoint.Import()

print("Cleanup...")
importer.cleanup.Clean()

print("Closing DB...")
db.CloseAll()

print("Done")
