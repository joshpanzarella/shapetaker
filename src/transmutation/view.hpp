// Minimal read-only interface for Transmutation state used by UI widgets
#pragma once
#include <string>

namespace stx { namespace transmutation {

struct StepInfo {
    int chordIndex;
    int voiceCount;
    int symbolId;
};

// Optional controller surface for UI widgets that program steps
struct TransmutationController {
    virtual ~TransmutationController() {}
    virtual void programStepA(int idx) = 0;
    virtual void programStepB(int idx) = 0;
    virtual void cycleVoiceCountA(int idx) = 0;
    virtual void cycleVoiceCountB(int idx) = 0;
    virtual void setEditCursorA(int idx) = 0;
    virtual void setEditCursorB(int idx) = 0;
    virtual void onSymbolPressed(int symbolId) = 0;
};

struct TransmutationView {
    virtual ~TransmutationView() {}
    virtual float getInternalClockBpm() = 0;
    virtual int getBpmMultiplier() = 0;
    virtual bool isSeqARunning() const = 0;
    virtual bool isSeqBRunning() const = 0;
    virtual int getSeqACurrentStep() const = 0;
    virtual int getSeqALength() const = 0;
    virtual int getSeqBCurrentStep() const = 0;
    virtual int getSeqBLength() const = 0;
    virtual bool isClockAConnected() = 0;
    virtual bool isClockBConnected() = 0;
    virtual int getSeqBMode() = 0; // param value
    virtual bool isEditModeA() const = 0;
    virtual bool isEditModeB() const = 0;

    // Matrix needs
    virtual int getGridSteps() const = 0;
    virtual int getButtonSymbol(int pos) const = 0;         // 0..11 -> symbol id
    virtual int getSymbolToChord(int symbolId) const = 0;   // 0..39 -> chord index
    virtual StepInfo getStepA(int idx) const = 0;
    virtual StepInfo getStepB(int idx) const = 0;
    // Optional: preview
    virtual int getDisplaySymbolId() const = 0;             // -1 rest, -2 tie, -999 none
    virtual std::string getDisplayChordName() const = 0;
    virtual float getSymbolPreviewTimer() const = 0;
    virtual bool getSpookyTvMode() const = 0;
    
    // Symbol button support
    virtual int getSelectedSymbol() const = 0;
    virtual float getButtonPressAnim(int buttonPos) const = 0;
    virtual int getCurrentChordIndex(bool seqA) const = 0;   // Returns the current chord playing or -999
};

}} // namespace stx::transmutation
