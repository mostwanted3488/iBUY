#include "PluginEditor.h"

#include "BinaryData.h"

namespace
{
constexpr float kImgW = 1102.f, kImgH = 1855.f;

// screen glass, in photo pixels
constexpr float kScrX = 84.f, kScrY = 82.f, kScrW = 929.f, kScrH = 715.f;

// click wheel
const juce::Point<float> kWheelC { 549.f, 1332.f };
constexpr float kWheelR = 337.f, kCentreR = 123.f;

juce::Font bold (float size) { return juce::Font (juce::FontOptions (size, juce::Font::bold)); }
juce::Font semi (float size) { return juce::Font (juce::FontOptions (size, juce::Font::plain)); }

struct Palette
{
    juce::Colour bg, fg, hd1, hd2, hd3, hdLine, hdFg, divider, rbg1, rbg2, sub, li, pick, grid;
};

Palette palette (bool dark)
{
    if (dark)
        return { juce::Colour (0xff0a0c10), juce::Colour (0xfff2f5f9), juce::Colour (0xff4d525a), juce::Colour (0xff2e3239), juce::Colour (0xff23272d),
                 juce::Colour (0xff050607), juce::Colour (0xffeef2f7), juce::Colours::black, juce::Colour (0xff040506), juce::Colour (0xff0a0c10),
                 juce::Colour (0xff8e9aab), juce::Colour (0xffb9c3d1), juce::Colour (0xff5cb8f5), juce::Colour (0x1fffffff) };
    return { juce::Colours::white, juce::Colours::black, juce::Colour (0xfff4f4f4), juce::Colour (0xffc4c8cd), juce::Colour (0xffb2b7bd),
             juce::Colour (0xff6d7580), juce::Colour (0xff111111), juce::Colour (0xff1b1b1b), juce::Colour (0xffe9e9e9), juce::Colours::white,
             juce::Colour (0xff6b7380), juce::Colour (0xff4b5461), juce::Colour (0xff1779c0), juce::Colour (0x1f000000) };
}
} // namespace

IBuyEditor::IBuyEditor (IBuyProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    photo = juce::ImageFileFormat::loadFrom (BinaryData::ipod_jpg, (size_t) BinaryData::ipod_jpgSize);
    stack.push_back ({ "root", 0 });

    setWantsKeyboardFocus (true);
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) kImgW / (double) kImgH);
    setResizeLimits (250, 421, 1100, 1851);
    setSize (440, 741);
    startTimerHz (30);
}

IBuyEditor::~IBuyEditor()
{
    endHold();
    stopTimer();
}

// ------------------------------------------------------------------ parameters
bool IBuyEditor::getBool (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
int IBuyEditor::getChoice (const char* id) const { return (int) std::lround (proc.apvts.getRawParameterValue (id)->load()); }

void IBuyEditor::setBool (const char* id, bool v)
{
    if (auto* prm = proc.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (v ? 1.f : 0.f);
        prm->endChangeGesture();
    }
}

void IBuyEditor::setChoice (const char* id, int v)
{
    if (auto* prm = proc.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) v));
        prm->endChangeGesture();
    }
}

void IBuyEditor::beginHold()
{
    if (holding) return;
    holding = true;
    if (auto* prm = proc.apvts.getParameter ("loopHold"))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (1.f);
    }
}

void IBuyEditor::endHold()
{
    if (! holding) return;
    holding = false;
    if (auto* prm = proc.apvts.getParameter ("loopHold"))
    {
        prm->setValueNotifyingHost (0.f);
        prm->endChangeGesture();
    }
}

// ------------------------------------------------------------------ menu model
std::vector<IBuyEditor::Item> IBuyEditor::itemsFor (const juce::String& key) const
{
    if (key == "root")    return { { "EQ Presets", "eq" }, { "Effects", "effects" }, { "Random", "random" }, { "Now Playing", "playing" }, { "Themes", "themes" } };
    if (key == "eq")      return { { "Flat" }, { "Bass Booster" }, { "Treble Booster" }, { "Lounge" }, { "Small Speakers" }, { "Spoken Word" } };
    if (key == "effects") return { { "Low Battery", "", "lowBattery" }, { "Looper", "looper" }, { "Next Room", "", "nextRoom" }, { "Earbuds", "", "earbuds" }, { "Basics", "basics" } };
    if (key == "looper")  return { { "1/2" }, { "1/4" }, { "1/8" }, { "1/16" }, { "1/32" }, { "1/64" } };
    if (key == "basics")  return { { "Reverb", "", "reverb" }, { "Delay", "", "delay" }, { "Chorus", "", "chorus" }, { "Filter", "", "filter" }, { "Bitcrush", "", "bitcrush" } };
    if (key == "random")  return { { "All" }, { "EQ Preset" }, { "Effects" }, { "Reset" } };
    if (key == "playing") return { { "Levels" }, { "Spectrum" }, { "Preview" } };
    if (key == "themes")  return { { "Light" }, { "Dark" } };
    return {};
}

juce::String IBuyEditor::titleFor (const juce::String& key) const
{
    if (key == "root") return "iBuy";
    if (key == "eq") return "EQ Presets";
    if (key == "effects") return "Effects";
    if (key == "looper") return "Looper";
    if (key == "basics") return "Basics";
    if (key == "random") return "Random";
    if (key == "playing") return "Now Playing";
    if (key == "themes") return "Themes";
    return key;
}

bool IBuyEditor::isOn (const juce::String& key, int idx, const Item& it) const
{
    if (key == "effects" || key == "basics") return it.param.isNotEmpty() && getBool (it.param.toRawUTF8());
    if (key == "eq") return getChoice ("eq") == idx;
    if (key == "looper") return getChoice ("loopDiv") == idx;
    if (key == "themes") return proc.theme.load() == idx;
    return false;
}

juce::String IBuyEditor::subtitleFor (const juce::String& key, const Item& it, int idx) const
{
    if (it.sub.isNotEmpty())
    {
        if (it.sub == "eq") return "6 presets";
        if (it.sub == "effects") return "4 effects + basics";
        if (it.sub == "basics") return "5 effects";
        if (it.sub == "themes") return "2 themes";
        if (it.sub == "random") return "4 options";
        if (it.sub == "playing") return "Live input";
        if (it.sub == "looper") return itemsFor ("looper")[(size_t) getChoice ("loopDiv")].label + " - hold to loop";
        return {};
    }
    if (key == "effects" || key == "basics")
    {
        if (it.label == "Low Battery") return isOn (key, idx, it) ? "On. Press to power back up" : "Press to drain the battery";
        return isOn (key, idx, it) ? "On. Press to switch off" : "Off. Press to switch on";
    }
    if (key == "looper")
        return (holding && getChoice ("loopDiv") == idx) ? "Looping " + it.label + " at " + juce::String (juce::roundToInt (proc.hostBpm.load())) + " BPM"
                                                         : "Hold the center button to loop " + it.label;
    if (key == "random") return it.label == "Reset" ? "Back to Flat, all effects off" : "Press to roll the dice";
    return isOn (key, idx, it) ? "Selected" : "Press the center button to select";
}

void IBuyEditor::tick (int dir)
{
    auto& lvl = stack.back();
    const int n = (int) itemsFor (lvl.key).size();
    lvl.idx = juce::jlimit (0, juce::jmax (0, n - 1), lvl.idx + dir);
}

bool IBuyEditor::back()
{
    if (stack.size() <= 1) return false;
    stack.pop_back();
    return true;
}

void IBuyEditor::roll (const juce::String& mode)
{
    static const char* fxIds[] = { "nextRoom", "earbuds", "reverb", "delay", "chorus", "filter", "bitcrush" };

    if (mode == "Reset")
    {
        setChoice ("eq", 0);
        setChoice ("loopDiv", 2);
        setBool ("lowBattery", false);
        for (auto* id : fxIds) setBool (id, false);
        return;
    }
    if (mode != "Effects") setChoice ("eq", rng.nextInt (6));
    if (mode != "EQ Preset")
        for (auto* id : fxIds) setBool (id, rng.nextFloat() < 0.3f);
}

void IBuyEditor::open()
{
    auto& lvl = stack.back();
    const auto items = itemsFor (lvl.key);
    if (lvl.idx < 0 || lvl.idx >= (int) items.size()) return;
    const auto& it = items[(size_t) lvl.idx];

    if (it.sub.isNotEmpty())
    {
        stack.push_back ({ it.sub, it.sub == "looper" ? getChoice ("loopDiv") : 0 });
        return;
    }
    if (lvl.key == "themes") proc.theme.store (lvl.idx);
    else if (lvl.key == "eq") setChoice ("eq", lvl.idx);
    else if (lvl.key == "looper") setChoice ("loopDiv", lvl.idx);
    else if (lvl.key == "effects" || lvl.key == "basics")
    {
        if (it.param.isNotEmpty()) setBool (it.param.toRawUTF8(), ! getBool (it.param.toRawUTF8()));
    }
    else if (lvl.key == "random") roll (it.label);
}

// ------------------------------------------------------------------ drawing
void IBuyEditor::resized()
{
    rebuildBase();
}

void IBuyEditor::rebuildBase()
{
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0 || ! photo.isValid()) return;

    base = juce::Image (juce::Image::ARGB, w, h, true);
    juce::Graphics g (base);
    const float s = (float) w / kImgW;
    juce::Path clip;
    clip.addRoundedRectangle (0.f, 0.f, (float) w, (float) h, 115.f * s);
    g.reduceClipRegion (clip, {});
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (photo, 0, 0, w, h, 0, 0, photo.getWidth(), photo.getHeight());
}

void IBuyEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff060a10));
    if (base.isValid()) g.drawImageAt (base, 0, 0);

    const float s = (float) getWidth() / kImgW;
    g.addTransform (juce::AffineTransform::scale (s));
    paintScreen (g);
}

void IBuyEditor::paintScreen (juce::Graphics& g)
{
    const auto pal = palette (proc.theme.load() == 1);
    const juce::Rectangle<float> screen (kScrX, kScrY, kScrW, kScrH);
    const float leftW = kScrW * 0.54f, hdH = kScrH * 0.114f, rowH = kScrH * 0.122f;

    juce::Graphics::ScopedSaveState save (g);
    juce::Path clip;
    clip.addRoundedRectangle (screen, 9.f);
    g.reduceClipRegion (clip, {});

    // right pane
    const juce::Rectangle<float> rightPane (kScrX + leftW, kScrY, kScrW - leftW, kScrH);
    g.setGradientFill (juce::ColourGradient (pal.rbg1, rightPane.getX(), 0.f, pal.rbg2, rightPane.getX() + 56.f, 0.f, false));
    g.fillRect (rightPane);

    // left pane
    const juce::Rectangle<float> leftPane (kScrX, kScrY, leftW, kScrH);
    g.setColour (pal.bg);
    g.fillRect (leftPane);

    // header
    {
        juce::ColourGradient grad (pal.hd1, 0.f, kScrY, pal.hd3, 0.f, kScrY + hdH, false);
        grad.addColour (0.55, pal.hd2);
        g.setGradientFill (grad);
        g.fillRect (kScrX, kScrY, leftW, hdH);
        g.setColour (pal.hdLine);
        g.fillRect (kScrX, kScrY + hdH - 2.5f, leftW, 2.5f);

        const auto& lvl = stack.back();
        g.setColour (pal.hdFg);
        g.setFont (bold (kScrH * 0.052f));
        g.drawText (titleFor (lvl.key), juce::Rectangle<float> (kScrX + 20.f, kScrY, leftW - 190.f, hdH - 2.f), juce::Justification::centredLeft, true);

        // play / pause icon
        const float cy = kScrY + (hdH - 2.f) * 0.5f;
        const float ix = kScrX + leftW - 150.f;
        if (getBool ("power"))
        {
            juce::Path tri;
            tri.addTriangle (ix, cy - 14.f, ix, cy + 14.f, ix + 22.f, cy);
            g.setColour (juce::Colour (0xff2a8ee2));
            g.fillPath (tri);
        }
        else
        {
            g.setColour (pal.hdFg.withAlpha (0.7f));
            g.fillRect (ix, cy - 13.f, 6.f, 26.f);
            g.fillRect (ix + 12.f, cy - 13.f, 6.f, 26.f);
        }

        // battery
        const float level = juce::jlimit (0.f, 1.f, proc.engine.getBattery());
        const juce::Rectangle<float> bat (kScrX + leftW - 82.f, cy - 10.f, 54.f, 20.f);
        g.setColour (pal.hdFg.withAlpha (0.9f));
        g.drawRoundedRectangle (bat, 3.f, 2.2f);
        const auto fill = bat.reduced (3.f);
        const auto filled = fill.withWidth (fill.getWidth() * level);
        const bool low = level < 0.3f;
        g.setGradientFill (juce::ColourGradient (low ? juce::Colour (0xffff8f80) : juce::Colour (0xff7fe07a), 0.f, filled.getY(),
                                                  low ? juce::Colour (0xffd33a2a) : juce::Colour (0xff1f9a2a), 0.f, filled.getBottom(), false));
        g.fillRoundedRectangle (filled, 1.5f);
    }

    // rows
    const auto& lvl = stack.back();
    const auto items = itemsFor (lvl.key);
    for (int i = 0; i < (int) items.size(); ++i)
    {
        const auto& it = items[(size_t) i];
        const juce::Rectangle<float> row (kScrX, kScrY + hdH + (float) i * rowH, leftW, rowH);
        const bool sel = i == lvl.idx;

        if (sel)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff55b3ec), 0.f, row.getY(), juce::Colour (0xff1779c0), 0.f, row.getBottom(), false));
            g.fillRect (row);
        }
        const auto txt = sel ? juce::Colours::white : pal.fg;
        g.setColour (txt);
        g.setFont (bold (kScrH * 0.054f));
        g.drawText (it.label, row.withTrimmedLeft (37.f).withTrimmedRight (110.f), juce::Justification::centredLeft, true);

        const float cy = row.getCentreY(), rx = row.getRight() - 24.f;
        if (it.sub.isNotEmpty())
        {
            juce::Path chev;
            chev.startNewSubPath (rx - 11.f, cy - 15.f); chev.lineTo (rx, cy); chev.lineTo (rx - 11.f, cy + 15.f);
            g.strokePath (chev, juce::PathStrokeType (5.f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
            if (it.sub == "looper")
            {
                g.setFont (bold (kScrH * 0.044f));
                g.drawText (itemsFor ("looper")[(size_t) getChoice ("loopDiv")].label, juce::Rectangle<float> (rx - 120.f, row.getY(), 96.f, rowH), juce::Justification::centredRight, false);
            }
        }
        else if (lvl.key != "playing" && lvl.key != "random" && isOn (lvl.key, i, it))
        {
            juce::Path tick;
            tick.startNewSubPath (rx - 26.f, cy + 1.f); tick.lineTo (rx - 15.f, cy + 13.f); tick.lineTo (rx + 3.f, cy - 14.f);
            g.strokePath (tick, juce::PathStrokeType (5.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // right pane content
    if (lvl.idx >= 0 && lvl.idx < (int) items.size())
    {
        const auto& it = items[(size_t) lvl.idx];
        const float px = rightPane.getX() + 28.f;
        const float pw = rightPane.getWidth() - 50.f;
        float y = kScrY + 26.f;

        g.setColour (pal.fg);
        g.setFont (bold (kScrH * 0.066f));
        g.drawText (it.label, juce::Rectangle<float> (px, y, pw, 52.f), juce::Justification::centredLeft, true);
        y += 56.f;

        g.setColour (pal.sub);
        g.setFont (bold (kScrH * 0.040f));
        g.drawText (subtitleFor (lvl.key, it, lvl.idx), juce::Rectangle<float> (px, y, pw, 34.f), juce::Justification::centredLeft, true);
        y += 46.f;

        const bool showViz = lvl.key == "playing" || it.sub == "playing";
        if (showViz)
        {
            const juce::Rectangle<float> area (px, y, pw, kScrY + kScrH - y - 26.f);
            paintViz (g, area, lvl.key == "playing" ? it.label : juce::String ("Spectrum"), pal.pick, pal.grid);
        }
        else if (it.sub.isNotEmpty())
        {
            const auto kids = itemsFor (it.sub);
            for (int k = 0; k < (int) kids.size(); ++k)
            {
                const bool on = it.sub != "random" && it.sub != "playing" && isOn (it.sub, k, kids[(size_t) k]);
                g.setColour (on ? pal.pick : pal.li);
                g.setFont (bold (kScrH * 0.048f));
                g.drawText (kids[(size_t) k].label, juce::Rectangle<float> (px, y, pw, 38.f), juce::Justification::centredLeft, true);
                y += 43.f;
            }
        }
    }

    // glass
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.14f), kScrX, kScrY,
                                              juce::Colours::white.withAlpha (0.f), kScrX + kScrW * 0.45f, kScrY + kScrH * 0.55f, false));
    g.fillRect (screen);
}

void IBuyEditor::paintViz (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& mode, juce::Colour accent, juce::Colour grid)
{
    if (area.getHeight() < 20.f) return;

    if (mode == "Spectrum")
    {
        const int bars = ibuy::Engine::kBars;
        const float gap = area.getWidth() * 0.018f, bw = (area.getWidth() - gap * (float) (bars - 1)) / (float) bars;
        g.setColour (accent);
        for (int i = 0; i < bars; ++i)
        {
            const float bh = juce::jmax (area.getHeight() * 0.03f, proc.engine.getBar (i) * area.getHeight());
            g.fillRect (area.getX() + (float) i * (bw + gap), area.getBottom() - bh, bw, bh);
        }
    }
    else if (mode == "Levels")
    {
        const float lv[2] = { proc.engine.getLevelL(), proc.engine.getLevelR() };
        const float bh = area.getHeight() * 0.28f;
        for (int i = 0; i < 2; ++i)
        {
            const float db = lv[i] > 0.f ? 20.f * std::log10 (lv[i]) : -80.f;
            const float f = juce::jlimit (0.f, 1.f, (db + 48.f) / 48.f);
            const float y = area.getY() + area.getHeight() * 0.12f + (float) i * (bh + area.getHeight() * 0.16f);
            g.setColour (grid);
            g.fillRect (area.getX(), y, area.getWidth(), bh);
            juce::ColourGradient grad (juce::Colour (0xff2fcf5a), area.getX(), 0.f, juce::Colour (0xffee4a3a), area.getRight(), 0.f, false);
            grad.addColour (0.7, juce::Colour (0xffe8d23a));
            g.setGradientFill (grad);
            g.fillRect (area.getX(), y, area.getWidth() * f, bh);
        }
    }
    else
    {
        float buf[ibuy::Engine::kScope];
        proc.engine.copyScope (buf, ibuy::Engine::kScope);
        g.setColour (grid);
        g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());
        juce::Path wave;
        for (int i = 0; i < ibuy::Engine::kScope; ++i)
        {
            const float x = area.getX() + (float) i / (float) (ibuy::Engine::kScope - 1) * area.getWidth();
            const float y = area.getCentreY() - juce::jlimit (-1.f, 1.f, buf[i]) * area.getHeight() * 0.45f;
            if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
        }
        g.setColour (accent);
        g.strokePath (wave, juce::PathStrokeType (3.5f));
    }
}

// ------------------------------------------------------------------ mouse + keyboard
void IBuyEditor::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const float s = (float) getWidth() / kImgW;
    const auto p = e.position / s;
    dragStart = lastPos = p;
    drag = Drag::none;

    const auto d = p - kWheelC;
    const float dist = std::hypot (d.x, d.y);

    if (dist <= kCentreR)
    {
        drag = Drag::centre;
        if (stack.back().key == "looper")
        {
            setChoice ("loopDiv", stack.back().idx);
            beginHold();
        }
    }
    else if (dist <= kWheelR)
    {
        drag = Drag::ring;
        ringAngle = juce::radiansToDegrees (std::atan2 (d.y, d.x));
        ringAcc = 0.f;
        ringMoved = 0.f;
        if (ringAngle > -45.f && ringAngle <= 45.f) ringZone = "next";
        else if (ringAngle > 45.f && ringAngle <= 135.f) ringZone = "play";
        else if (ringAngle > -135.f && ringAngle <= -45.f) ringZone = "menu";
        else ringZone = "prev";
    }
    else
    {
        const float leftW = kScrW * 0.54f, hdH = kScrH * 0.114f, rowH = kScrH * 0.122f;
        const juce::Rectangle<float> list (kScrX, kScrY + hdH, leftW, kScrH - hdH);
        if (list.contains (p))
        {
            const int row = (int) ((p.y - list.getY()) / rowH);
            if (row >= 0 && row < (int) itemsFor (stack.back().key).size())
            {
                drag = Drag::row;
                auto& lvl = stack.back();
                if (lvl.key == "looper")
                {
                    lvl.idx = row;
                    setChoice ("loopDiv", row);
                    beginHold();
                }
                else if (row == lvl.idx) open();
                else lvl.idx = row;
            }
        }
    }
}

void IBuyEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (drag != Drag::ring) return;
    const float s = (float) getWidth() / kImgW;
    const auto d = e.position / s - kWheelC;
    const float a = juce::radiansToDegrees (std::atan2 (d.y, d.x));
    float delta = a - ringAngle;
    if (delta > 180.f) delta -= 360.f;
    if (delta < -180.f) delta += 360.f;
    ringAngle = a;
    ringAcc += delta;
    ringMoved += std::abs (delta);
    constexpr float step = 24.f;
    while (std::abs (ringAcc) >= step)
    {
        const int dir = ringAcc > 0.f ? 1 : -1;
        tick (dir);
        ringAcc -= (float) dir * step;
    }
    lastPos = e.position / s;
}

void IBuyEditor::mouseUp (const juce::MouseEvent& e)
{
    const float s = (float) getWidth() / kImgW;
    const auto p = e.position / s;
    const auto mode = drag;
    drag = Drag::none;

    if (mode == Drag::centre)
    {
        if (stack.back().key == "looper") endHold();
        else open();
    }
    else if (mode == Drag::row)
    {
        endHold();
    }
    else if (mode == Drag::ring)
    {
        const bool tap = ringMoved < 8.f && std::hypot (p.x - dragStart.x, p.y - dragStart.y) < 10.f;
        if (tap)
        {
            if (ringZone == "menu") back();
            else if (ringZone == "next") tick (1);
            else if (ringZone == "prev") tick (-1);
            else setBool ("power", ! getBool ("power"));
        }
    }
}

void IBuyEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float s = (float) getWidth() / kImgW;
    const auto d = e.position / s - kWheelC;
    if (std::hypot (d.x, d.y) > kWheelR || std::abs (w.deltaY) < 0.0001f) return;
    tick (w.deltaY < 0.f ? 1 : -1);
}

bool IBuyEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::downKey) { tick (1); return true; }
    if (k == juce::KeyPress::upKey) { tick (-1); return true; }
    if (k == juce::KeyPress::returnKey || k == juce::KeyPress::rightKey) { open(); return true; }
    if (k == juce::KeyPress::backspaceKey || k == juce::KeyPress::leftKey || k == juce::KeyPress::escapeKey) { back(); return true; }
    return false;
}
