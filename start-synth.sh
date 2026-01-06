#!/bin/bash

echo "--- INICIANDO SISTEMA ELECTONE ---"

# 1. Limpieza
echo "[1/4] Matando procesos viejos..."
killall -9 fluidsynth electone_core 2>/dev/null
sleep 2

# 2. Iniciar Audio
echo "[2/4] Iniciando FluidSynth..."
# Usá la ruta absoluta a tu SF2
fluidsynth -is -a alsa -m alsa_seq -o audio.alsa.device=plughw:3,0 -g 1.5 -c 2 /home/pcvalen/sf2/GeneralUser-GS.sf2 &

# DALE TIEMPO: FluidSynth necesita unos segundos para crear los puertos ALSA
echo "      Esperando carga de SoundFont (8 seg)..."
sleep 8

# 3. Scripts Python (Los iniciamos ANTES de conectar para que creen sus puertos)
echo "[3/4] Iniciando Scripts..."
./electone_core &

sleep 3

# 4. Conectar Cables (AHORA SÍ)
echo "[4/4] Conectando MIDI..."
# Conectamos Maple (STM32) a FluidSynth (Audio)
aconnect 'Maple' 'FLUID Synth' 2>/dev/null
# Conectamos Korg a FluidSynth (si querés control directo, aunque el router ya lo hace)
# aconnect 'nanoKONTROL' 'FLUID Synth' 2>/dev/null

echo ">>> SISTEMA LISTO. ¡A TOCAR! <<<"
wait
