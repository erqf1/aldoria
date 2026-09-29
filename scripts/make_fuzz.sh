#!/usr/bin/env bash
# Erzeugt ein Zufallsskript (Stresstest): wilde Eingaben, Raumwechsel, Tod, Menüs.
# Aufruf: bash scripts/make_fuzz.sh <Startzahl> <Schritte> > scripts/fuzz.txt
RANDOM=${1:-1}
STEPS=${2:-300}
ROOMS=(wh01_pforte wh02_gang wh03_sprung wh04_halle wh04b_nische wh05_arena1 wh06_rast1 wh07_schatz wh08_wind wh09_arena2 wh10_rast2 wh11_vorhalle wh12_thron wh13_ende gs01_tor gs02_lavagang gs03_kessel gs04_arena1 gs05_rast1 gs06_schmiedepfad gs07_arena2 gs08_rast2 gs09_vorhalle gs10_kern gs11_ende ht01_fuss ht02_treppe ht03_feder ht04_luecken ht04b_wolkenkammer ht05_arena1 ht06_rast1 ht07_kristalle ht08_windbruecke ht09_arena2 ht10_rast2 ht11_vorhalle ht12_spitze ht13_ende)
MOVES=(forward back left right forward+sprint forward+left forward+right back+left)
PRESS=(ability map jump attack attack attack dodge dash secondary heal interact pause confirm cancel up down ui_left ui_right)
echo "wait 0.4"
echo "new_game"
echo "wait 1.0"
echo "give dash"
echo "give double_jump"
echo "give key 5"
echo "give bosskey"
for ((i = 0; i < STEPS; i++)); do
  r=$((RANDOM % 100))
  if   [ $r -lt 30 ]; then echo "hold ${MOVES[$((RANDOM % ${#MOVES[@]}))]} 0.$((RANDOM % 9 + 1))"
  elif [ $r -lt 62 ]; then echo "press ${PRESS[$((RANDOM % ${#PRESS[@]}))]}"
  elif [ $r -lt 70 ]; then echo "look $((RANDOM % 9 - 4))e-1 $((RANDOM % 5 - 2))e-1"
  elif [ $r -lt 74 ]; then echo "room ${ROOMS[$((RANDOM % ${#ROOMS[@]}))]}"
  elif [ $r -lt 77 ]; then echo "hurt $((RANDOM % 60 + 5))"
  elif [ $r -lt 79 ]; then echo "kill_all"
  elif [ $r -lt 80 ]; then echo "heal"
  elif [ $r -lt 81 ]; then echo "boss_hp 0.$((RANDOM % 9 + 1))"
  elif [ $r -lt 82 ]; then echo "segen_menu"
  elif [ $r -lt 83 ]; then echo "new_game"
  elif [ $r -lt 84 ]; then echo "next_dungeon"
  elif [ $r -lt 85 ]; then if [ $((RANDOM % 2)) -eq 0 ]; then echo "arena"; else echo "bossrush"; fi
  else echo "wait 0.$((RANDOM % 5 + 1))"
  fi
done
echo "status"
echo "quit"
