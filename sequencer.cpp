#include "electone.h"

Sequencer::Sequencer(RtMidiOut* outPort) : midiOut(outPort) {}

void Sequencer::setDrumDatabase(std::map<int, DrumStyle> db) { drumDB = db; }
void Sequencer::setAcompDatabase(std::map<int, AcompStyle> db) { acompDB = db; }

// --- ENTRADA DE NOTAS ---
void Sequencer::onNoteInput(int note, bool on) {
    std::lock_guard<std::mutex> lock(mtx); // BLOQUEO
    if (on) {
        if (std::find(heldNotes.begin(), heldNotes.end(), note) == heldNotes.end()) {
            heldNotes.push_back(note);
            std::sort(heldNotes.begin(), heldNotes.end());
        }
    } else {
        auto it = std::remove(heldNotes.begin(), heldNotes.end(), note);
        heldNotes.erase(it, heldNotes.end());
    }
}

// --- CLOCK ---
void Sequencer::onClock() {
    std::lock_guard<std::mutex> lock(mtx); // BLOQUEO CRÍTICO
    if (!isPlaying) return;

    if (tickCounter % 6 == 0) {
        // Apagar notas
        for (int n : lastDrumNotes) sendNote(CHAN_OUT_DRUMS, n, 0);
        lastDrumNotes.clear();
        for (int n : lastAcompNotes) sendNote(CHAN_OUT_ACOMP, n, 0);
        lastAcompNotes.clear();

        // Lógica
        if (drumDB.count(currentStyle)) {
            DrumStyle& ds = drumDB[currentStyle];

            if (tickCounter > 24000) tickCounter = 0;
            stepIndex = (tickCounter / 6) % ds.steps;

            playDrumStep(ds);

            if (acompDB.count(currentStyle)) {
                playAcompStep(acompDB[currentStyle]);
            }
        }
    }
    tickCounter++;
}

// --- LOGICA INTERNA (Privada, sin lock para evitar deadlock) ---
void Sequencer::playDrumStep(const DrumStyle& ds) {
    bool notesPlayed = false;
    if (stepIndex == 0 && pendingResolution > 0) {
        if (ds.resolution.size() > 0) {
            for (auto const& [note, vel] : ds.resolution) {
                sendNote(CHAN_OUT_DRUMS, note, vel);
                lastDrumNotes.push_back(note);
            }
            notesPlayed = true;
        }
        pendingResolution = 0;
    }
    if (!notesPlayed) {
        const DrumPattern* target = nullptr;
        if (currentFill > 0) {
            if (ds.fills.count(currentFill)) {
                target = &ds.fills.at(currentFill);
                if (stepIndex == ds.steps - 1) {
                    pendingResolution = currentFill;
                    currentFill = 0;
                }
            } else { if (ds.variations.count(currentVar)) target = &ds.variations.at(currentVar); }
        } else { if (ds.variations.count(currentVar)) target = &ds.variations.at(currentVar); }

        if (target) {
            for (auto const& [note, steps] : target->tracks) {
                if (stepIndex < (int)steps.size() && steps[stepIndex] > 0) {
                    int vel = (steps[stepIndex] == 1) ? 100 : steps[stepIndex];
                    sendNote(CHAN_OUT_DRUMS, note, vel);
                    lastDrumNotes.push_back(note);
                }
            }
        }
    }
}

void Sequencer::playAcompStep(const AcompStyle& as) {
    if (as.patterns.count(currentAcompPat) == 0) return;
    if (heldNotes.empty()) return;

    const AcompPattern& ap = as.patterns.at(currentAcompPat);
    int currentStep = (tickCounter / 6) % ap.steps;
    if (currentStep >= (int)ap.pattern.size()) return;

    int val = ap.pattern[currentStep];
    if (val == 0) return;

    int shiftAmount = octaveShift * 12;

    if (ap.mode == "chord") {
        for (int note : heldNotes) {
            int finalNote = note + shiftAmount;
            if (finalNote < 0) finalNote = 0; if (finalNote > 127) finalNote = 127;
            sendNote(CHAN_OUT_ACOMP, finalNote, ap.velocity);
            lastAcompNotes.push_back(finalNote);
        }
    }
    else {
        int idxRequest = val - 1;
        int numNotes = heldNotes.size();
        int noteToPlay = -1;
        if (ap.mode == "arp-once") { noteToPlay = heldNotes[idxRequest % numNotes]; }
        else if (ap.mode == "arp-loop") {
            if (numNotes == 1) noteToPlay = heldNotes[0];
            else {
                int cycleLen = (numNotes * 2) - 2;
                int posInCycle = idxRequest % cycleLen;
                noteToPlay = (posInCycle < numNotes) ? heldNotes[posInCycle] : heldNotes[cycleLen - posInCycle];
            }
        }
        if (noteToPlay != -1) {
            int finalNote = noteToPlay + shiftAmount;
            if (finalNote < 0) finalNote = 0; if (finalNote > 127) finalNote = 127;
            sendNote(CHAN_OUT_ACOMP, finalNote, ap.velocity);
            lastAcompNotes.push_back(finalNote);
        }
    }
}

// --- CONTROLES Y TRANSPORTE (CON LOCK) ---

void Sequencer::onStart() {
    std::lock_guard<std::mutex> lock(mtx); // BLOQUEO
    isPlaying = true; tickCounter = 0; stepIndex = 0; pendingResolution = 0;
    // Panic soft
    sendProgramChange(CHAN_OUT_DRUMS, 123);
    sendProgramChange(CHAN_OUT_ACOMP, 123);
}

void Sequencer::onStop() {
    std::lock_guard<std::mutex> lock(mtx); // BLOQUEO
    isPlaying = false;
    panic();
}

void Sequencer::setStyle(int style) {
    std::lock_guard<std::mutex> lock(mtx); // BLOQUEO
    currentStyle = style;
    if (acompDB.count(currentStyle) && acompDB[currentStyle].patterns.count(currentAcompPat)) {
        sendProgramChange(CHAN_OUT_ACOMP, acompDB[currentStyle].patterns[currentAcompPat].program);
    }
}

void Sequencer::setVar(int var) { std::lock_guard<std::mutex> lock(mtx); currentVar = var; }
void Sequencer::setFill(int fill) { std::lock_guard<std::mutex> lock(mtx); currentFill = fill; }

void Sequencer::setAcompPattern(int pat) {
    std::lock_guard<std::mutex> lock(mtx); // BLOQUEO
    currentAcompPat = pat;
    if (acompDB.count(currentStyle) && acompDB[currentStyle].patterns.count(currentAcompPat)) {
        sendProgramChange(CHAN_OUT_ACOMP, acompDB[currentStyle].patterns[currentAcompPat].program);
    }
}

void Sequencer::changeOctave(int direction) {
    std::lock_guard<std::mutex> lock(mtx); // BLOQUEO
    octaveShift += direction;
    if (octaveShift > 3) octaveShift = 3;
    if (octaveShift < -3) octaveShift = -3;
}


// --- HELPERS (Privados) ---
void Sequencer::sendNote(int channel, int note, int velocity) {
    std::vector<unsigned char> message;
    message.push_back((velocity > 0 ? 0x90 : 0x80) + channel);
    message.push_back(note);
    message.push_back(velocity);
    midiOut->sendMessage(&message);
}

void Sequencer::sendProgramChange(int channel, int program) {
    std::vector<unsigned char> message = { (unsigned char)(0xC0 + channel), (unsigned char)program };
    midiOut->sendMessage(&message);
}

void Sequencer::panic() {
    for (int n : lastDrumNotes) sendNote(CHAN_OUT_DRUMS, n, 0);
    for (int n : lastAcompNotes) sendNote(CHAN_OUT_ACOMP, n, 0);
    lastDrumNotes.clear();
    lastAcompNotes.clear();
}
