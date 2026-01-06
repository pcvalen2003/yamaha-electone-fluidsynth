#include "electone.h"
#include <thread>
#include <chrono>

// Globales
Sequencer* seq = nullptr;
RtMidiOut* midiOut = nullptr;

// Envía mensajes cortos (3 bytes)
void sendMidi(int status, int d1, int d2) {
    std::vector<unsigned char> msg = {(unsigned char)status, (unsigned char)d1, (unsigned char)d2};
    midiOut->sendMessage(&msg);
}

// Función auxiliar para aplicar un Patch completo
void applyPatch(int channel, const SoundPatch& patch) {
    // 1. Bank Select (CC 0)
    sendMidi(0xB0 + channel, 0, patch.bank);

    // 2. Program Change
    std::vector<unsigned char> pcMsg = {(unsigned char)(0xC0 + channel), (unsigned char)patch.program};
    midiOut->sendMessage(&pcMsg);

    // 3. Portamento (Solo si es lead o se especifica)
    // CC 65 (Portamento Switch)
    sendMidi(0xB0 + channel, 65, patch.portamento ? 127 : 0);

    // CC 5 (Portamento Time)
    if (patch.portamento) {
        sendMidi(0xB0 + channel, 5, patch.portamentoTime);
    }
}

// Función para enviar SysEx
void sendSysEx(const std::vector<unsigned char>& data) {
    midiOut->sendMessage(&data);
}

// --- CALLBACK STM32 (Maple) ---
void mapleCallback(double deltatime, std::vector<unsigned char> *message, void *userData) {
    if (message->empty()) return;
    unsigned char status = message->at(0);

    // CLOCK
    if (status == 0xF8) {
        midiOut->sendMessage(message);
        seq->onClock();
        return;
    }
    if (status == 0xFA || status == 0xFB) { seq->onStart(); midiOut->sendMessage(message); return; }
    if (status == 0xFC) { seq->onStop(); midiOut->sendMessage(message); return; }

    // NOTAS INPUT
    if ((status & 0x0F) == CHAN_INPUT_ACOMP) {
        int type = status & 0xF0;
        int note = message->at(1);
        if (type == 0x90 && message->at(2) > 0) seq->onNoteInput(note, true);
        else if (type == 0x80 || (type == 0x90 && message->at(2) == 0)) seq->onNoteInput(note, false);
    }

    // CONTROL CHANGE
    if ((status & 0xF0) == 0xB0) {
        int cc = message->at(1);
        int val = message->at(2);

        // --- SELECCIÓN DE SONIDOS (Usando YAML) ---

        // CC 51 -> Lower (Canal 1)
        if (cc == 51) {
            if (generalSoundsDB.count(val)) applyPatch(1, generalSoundsDB[val]);
        }
        // CC 52 -> Upper (Canal 0)
        else if (cc == 52) {
            if (generalSoundsDB.count(val)) applyPatch(0, generalSoundsDB[val]);
        }
        // CC 54 -> Lead (Canal 3)
        else if (cc == 54) {
            if (leadSoundsDB.count(val)) applyPatch(3, leadSoundsDB[val]);
        }

        // Control Secuenciador
        else if (cc == 55) { seq->setStyle(val); }
        else if (cc == 56) { seq->setVar(val); }
        else if (cc == 57) { if(val > 0) seq->setFill(val); }
        else if (cc == 58) { seq->setAcompPattern(val); }

        // Vibrato Lead
        else if (cc == 17) { sendMidi(0xB0 + 3, 1, val); }
    }
}

// --- CALLBACK KORG ---
void korgCallback(double deltatime, std::vector<unsigned char> *message, void *userData) {
    if (message->size() < 3) return;
    if ((message->at(0) & 0xF0) != 0xB0) return;

    int cc = message->at(1);
    int val = message->at(2);

    if (cc == 44 && val == 127) {
        std::cout << "SHUTDOWN..." << std::endl;
        system("sudo shutdown -h now");
    }
    // Volúmenes
    else if (cc == 12) { sendMidi(0xB0 + 9, 7, val); } // Drums
    else if (cc == 3)  { sendMidi(0xB0 + 0, 7, val); } // Upper
    else if (cc == 2)  { sendMidi(0xB0 + 1, 7, val); } // Lower
    else if (cc == 4)  { sendMidi(0xB0 + 3, 7, val); } // Lead
    else if (cc == 6)  { sendMidi(0xB0 + 4, 7, val); } // Acomp (Ch 4 index 5)
    else if (cc == 27 && val == 127)  { seq->changeOctave(1); }
    else if (cc == 37 && val == 127)  { seq->changeOctave(-1); }

    else if (cc == 13) {
        std::vector<unsigned char> sysex = {0xF0, 0x7F, 0x7F, 0x04, 0x01, 0x00, (unsigned char)val, 0xF7};
        sendSysEx(sysex);
    }
    else if (cc == 17) { sendMidi(0xB0 + 3, 1, val); }
}

int main() {
    RtMidiIn *mapleIn = 0;
    RtMidiIn *korgIn = 0;

    try {
        midiOut = new RtMidiOut();
        int nPorts = midiOut->getPortCount();
        bool found = false;
        for (int i=0; i<nPorts; i++) {
            if (midiOut->getPortName(i).find(PORT_FLUID) != std::string::npos) {
                midiOut->openPort(i); found = true;
                std::cout << "Output: " << midiOut->getPortName(i) << std::endl;
                break;
            }
        }
        if (!found) midiOut->openVirtualPort("Electone Out");

        // CARGA DE DATOS
        loadInstrumentDB("sounds.yaml"); // <--- CARGAMOS SONIDOS
        seq = new Sequencer(midiOut);
        seq->setDrumDatabase(loadDrumStyles("ritmos.yaml"));
        seq->setAcompDatabase(loadAcompStyles("chords.yaml"));

        mapleIn = new RtMidiIn();
        korgIn = new RtMidiIn();

        nPorts = mapleIn->getPortCount();
        for (int i=0; i<nPorts; i++) {
            if (mapleIn->getPortName(i).find(PORT_MAPLE) != std::string::npos) {
                mapleIn->openPort(i);
                mapleIn->setCallback(&mapleCallback);
                mapleIn->ignoreTypes(false, false, false);
                std::cout << "Input: " << mapleIn->getPortName(i) << std::endl;
                break;
            }
        }

        nPorts = korgIn->getPortCount();
        for (int i=0; i<nPorts; i++) {
            if (korgIn->getPortName(i).find(PORT_KORG) != std::string::npos) {
                korgIn->openPort(i);
                korgIn->setCallback(&korgCallback);
                std::cout << "Input: " << korgIn->getPortName(i) << std::endl;
                break;
            }
        }

        std::cout << ">>> ELECTONE C++ ENGINE RUNNING <<<" << std::endl;

        while(true) std::this_thread::sleep_for(std::chrono::seconds(1));

    } catch (RtMidiError &error) {
        error.printMessage();
    }

    delete mapleIn;
    delete korgIn;
    delete seq;
    delete midiOut;
    return 0;
}
