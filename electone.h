#ifndef ELECTONE_H
#define ELECTONE_H

#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <algorithm>
#include <RtMidi.h>
#include <mutex> // <--- NECESARIO PARA PROTEGER MEMORIA

// --- CONFIGURACIÓN DE PUERTOS Y CANALES ---
const std::string PORT_MAPLE = "Maple";
const std::string PORT_KORG = "nanoKONTROL";
const std::string PORT_FLUID = "FLUID Synth";

const int CHAN_INPUT_ACOMP = 1;
const int CHAN_OUT_DRUMS   = 9;
const int CHAN_OUT_ACOMP   = 4;

// --- ESTRUCTURAS DE DATOS ---
struct SoundPatch {
    int bank = 0;
    int program = 0;
    bool portamento = false;
    int portamentoTime = 0;
};
extern std::map<int, SoundPatch> generalSoundsDB;
extern std::map<int, SoundPatch> leadSoundsDB;

struct DrumPattern {
    std::map<int, std::vector<int>> tracks;
};
struct DrumStyle {
    int steps = 16;
    std::map<int, DrumPattern> variations;
    std::map<int, DrumPattern> fills;
    std::map<int, int> resolution;
};
struct AcompPattern {
    int program = 0;
    std::string mode = "chord";
    int velocity = 100;
    int steps = 16;
    std::vector<int> pattern;
};
struct AcompStyle {
    std::map<int, AcompPattern> patterns;
};

// --- CLASE SECUENCIADOR ---
class Sequencer {
public:
    Sequencer(RtMidiOut* outPort);

    // Carga (sin mutex porque es al inicio)
    void setDrumDatabase(std::map<int, DrumStyle> db);
    void setAcompDatabase(std::map<int, AcompStyle> db);

    // MÉTODOS PÚBLICOS (TODOS NECESITAN PROTECCIÓN)
    void onClock();
    void onStart();
    void onStop();
    void onNoteInput(int note, bool on);

    void setStyle(int style);
    void setVar(int var);
    void setFill(int fill);
    void setAcompPattern(int pat);
    void changeOctave(int direction);

private:
    std::mutex mtx; // <--- EL SEMÁFORO
    RtMidiOut* midiOut;

    std::map<int, DrumStyle> drumDB;
    std::map<int, AcompStyle> acompDB;

    bool isPlaying = false;
    long tickCounter = 0;
    int stepIndex = 0;

    int currentStyle = 0;
    int currentVar = 0;
    int currentFill = 0;
    int pendingResolution = 0;
    int currentAcompPat = 0;
    int octaveShift = 0;

    std::vector<int> heldNotes;
    std::vector<int> lastDrumNotes;
    std::vector<int> lastAcompNotes;

    // Métodos privados (se llaman desde dentro, no llevan mutex propio)
    void playDrumStep(const DrumStyle& style);
    void playAcompStep(const AcompStyle& style);
    void sendNote(int channel, int note, int velocity);
    void sendProgramChange(int channel, int program);
    void panic();
};

// Funciones de carga
std::map<int, DrumStyle> loadDrumStyles(const std::string& filename);
std::map<int, AcompStyle> loadAcompStyles(const std::string& filename);
void loadInstrumentDB(const std::string& filename);

#endif
