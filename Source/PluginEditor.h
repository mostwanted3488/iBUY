#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

#include "PluginProcessor.h"

// The whole interface is drawn in the coordinate space of the 1102 x 1855 iPod photo,
// then scaled to the window, so every position below is a pixel of that photo.
class IBuyEditor : public juce::AudioProcessorEditor,
                   private juce::Timer
{
public:
    explicit IBuyEditor (IBuyProcessor&);
    ~IBuyEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    struct Item
    {
        Item (juce::String l, juce::String s = {}, juce::String p = {}) : label (std::move (l)), sub (std::move (s)), param (std::move (p)) {}
        juce::String label;   // text shown in the list
        juce::String sub;     // non-empty: opens this submenu
        juce::String param;   // non-empty: toggles this parameter
    };
    struct Level { juce::String key; int idx = 0; };

    enum class Drag { none, ring, centre, row };

    void timerCallback() override { repaint(); }

    // menu model
    std::vector<Item> itemsFor (const juce::String& key) const;
    juce::String titleFor (const juce::String& key) const;
    bool isOn (const juce::String& key, int idx, const Item& it) const;
    juce::String subtitleFor (const juce::String& key, const Item& it, int idx) const;
    void tick (int dir);
    void open();
    bool back();
    void roll (const juce::String& mode);

    // parameters
    bool getBool (const char* id) const;
    int getChoice (const char* id) const;
    void setBool (const char* id, bool v);
    void setChoice (const char* id, int v);
    void beginHold();
    void endHold();

    // drawing
    void rebuildBase();
    void paintScreen (juce::Graphics&);
    void paintViz (juce::Graphics&, juce::Rectangle<float> area, const juce::String& mode, juce::Colour accent, juce::Colour grid);

    IBuyProcessor& proc;
    juce::Image photo, base;
    std::vector<Level> stack;
    Drag drag = Drag::none;
    juce::Point<float> dragStart, lastPos;
    float ringAngle = 0.f, ringAcc = 0.f, ringMoved = 0.f;
    juce::String ringZone;
    bool holding = false;
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IBuyEditor)
};
