/*
 * NAMix Linux VST3 Plugin
 * Copyright (C) 2026 rations
 *
 * Based on NeuralAmpModelerPlugin by Steven Atkinson (MIT Licence).
 * See NOTICE for full attribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */

#pragma once
#include <BinaryData.h>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>

// Exact NAM colour palette
namespace NAMColours {
// NAM_1: Raisin Black — main background
static const juce::Colour BG{0xff1d1a1f};
// NAM_2 / NAM_THEMECOLOR: Azure — active/value colour
static const juce::Colour BLUE{0xff5085e8};
// NAM_3: Cadet Blue Crayola — dimmed / track colour
static const juce::Colour CADET{0xffa2b2bf};
// NAM_THEMEFONTCOLOR: near-white
static const juce::Colour FONT{0xfff2f2f2};
// File row background
static const juce::Colour FILE_BG{0xff1a1720};
} // namespace NAMColours

class NAMixLookAndFeel : public juce::LookAndFeel_V4 {
public:
  NAMixLookAndFeel() {
    auto robotoTypeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::RobotoRegular_ttf, BinaryData::RobotoRegular_ttfSize);
    mRobotoFont = juce::Font(robotoTypeface);

    mKnobBg = juce::ImageCache::getFromMemory(BinaryData::KnobBackground_png,
                                              BinaryData::KnobBackground_pngSize);

    setColour(juce::Slider::textBoxTextColourId, NAMColours::FONT.withAlpha(0.85f));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, NAMColours::FONT);
    setColour(juce::TextButton::textColourOffId, NAMColours::FONT);
    setColour(juce::TextButton::textColourOnId, NAMColours::FONT);
  }

  // -----------------------------------------------------------------------
  // Rotary knob — matches NAMKnobControl draw order:
  //
  // KnobBackground.png is a 92x92 RGBA donut/ring shape:
  //   centre 0–60% radius: alpha=0 (transparent hole)
  //   ring  65–100% radius: semi-transparent (~42% alpha)
  //
  // Draw order (mirrors original):
  //   1. Drop shadow behind the ring bitmap
  //   2. Azure value arc at innerR (73%) — only filled portion, no gray track
  //   3. Ring bitmap at outerR — semi-transparent pixels blend the arc at the rim
  //   4. Gradient dot in the transparent centre
  // -----------------------------------------------------------------------
  void drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height,
                        float sliderPos, float startAngle, float endAngle,
                        juce::Slider &) override {
    const float cx = x + width * 0.5f;
    const float cy = y + height * 0.5f;
    const float outerR = std::min(width, height) * 0.5f - 2.0f;
    const float innerR = outerR * 0.73f; // arc radius — matches widgetRadius in original
    const float arcW = 3.5f;
    const float curAngle = startAngle + sliderPos * (endAngle - startAngle);

    // Drop shadow — offset dark ellipse at outerR gives the raised-knob look
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillEllipse(cx - outerR + 2.0f, cy - outerR + 2.0f, outerR * 2.0f, outerR * 2.0f);

    // Azure value arc at innerR — no gray background track (matches original)
    if (sliderPos > 0.0f) {
      juce::Path value;
      value.addCentredArc(cx, cy, innerR, innerR, 0.0f, startAngle, curAngle, true);
      g.setColour(NAMColours::BLUE);
      g.strokePath(value, juce::PathStrokeType(arcW, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }

    // Ring bitmap at full outerR — semi-transparent ring blends over the arc
    if (mKnobBg.isValid()) {
      g.drawImage(mKnobBg, (int)(cx - outerR), (int)(cy - outerR), (int)(outerR * 2.0f),
                  (int)(outerR * 2.0f), 0, 0, mKnobBg.getWidth(), mKnobBg.getHeight(),
                  false);
    } else {
      g.setColour(juce::Colour(0xff211e25));
      g.fillEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);
      g.setColour(juce::Colour(0xff3a3542));
      g.drawEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f, 0.8f);
    }

    // Indicator dot in the transparent centre (< 60% radius = transparent hole)
    // dotDst = innerR * 0.55 = outerR * 0.73 * 0.55 ≈ outerR * 0.40 → inside transparent
    // zone
    const float dotR = 3.0f;
    const float dotDst = innerR * 0.55f;
    const float dotX = cx + dotDst * std::sin(curAngle);
    const float dotY = cy - dotDst * std::cos(curAngle);

    juce::ColourGradient dotGrad(NAMColours::BLUE, dotX, dotY,
                                 juce::Colours::transparentBlack, dotX + dotR * 1.33f,
                                 dotY, true);
    dotGrad.addColour(0.8, NAMColours::BLUE);
    g.setGradientFill(dotGrad);
    g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);

    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f, 0.8f);
  }

  // -----------------------------------------------------------------------
  // Pill-style toggle — Azure when on, dimmed when off.
  // Matches NAMSwitchControl style (roundness ≈ 7px).
  //
  // Two modes based on button width:
  //  • Narrow (≤80px) — used on main plugin: pill centred, label below.
  //  • Wide   (>80px) — used in settings radio group: pill on left,
  //    label to the right, both vertically centred.
  // -----------------------------------------------------------------------
  void drawToggleButton(juce::Graphics &g, juce::ToggleButton &button, bool,
                        bool) override {
    const bool on = button.getToggleState();
    const auto b = button.getLocalBounds().toFloat();

    // Radio group members (output mode) — circle + label, matching the
    // original NAM IVRadioButtonControl with EVShape::Ellipse.
    if (button.getRadioGroupId() > 0) {
      // Circle sized to match original NAM IVRadioButtonControl (EVShape::Ellipse).
      // Keep it small enough that "Calibrated [Not supported by model]" fits.
      const float cx = 8.0f;
      const float cy = b.getCentreY();
      const float outerR = 5.0f;
      const float innerR = 2.5f;
      const auto ringCol = on ? NAMColours::BLUE : NAMColours::CADET.withAlpha(0.6f);
      g.setColour(ringCol);
      g.drawEllipse(cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f, 1.5f);
      if (on) {
        g.setColour(NAMColours::BLUE);
        g.fillEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);
      }
      const float textX = cx + outerR + 5.0f;
      g.setColour(NAMColours::FONT.withAlpha(on ? 1.0f : 0.65f));
      g.setFont(mRobotoFont.withHeight(13.0f));
      g.drawText(
          button.getButtonText(),
          juce::Rectangle<float>(textX, 0.0f, b.getWidth() - textX - 2.0f, b.getHeight()),
          juce::Justification::centredLeft, true);
      return;
    }

    const bool wide = (b.getWidth() > 100.0f);

    constexpr float pw = 40.0f, ph = 22.0f;

    // Pill position
    const float px = wide ? 4.0f : (b.getWidth() - pw) * 0.5f;
    const float py =
        wide ? (b.getHeight() - ph) * 0.5f : (b.getHeight() - ph - 16.0f) * 0.5f;

    // Track background — Azure (on) / dimmed Azure (off)
    g.setColour(on ? NAMColours::BLUE : NAMColours::BLUE.withAlpha(0.15f));
    g.fillRoundedRectangle(px, py, pw, ph, ph * 0.5f);

    // Frame stroke
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawRoundedRectangle(px, py, pw, ph, ph * 0.5f, 0.5f);

    // Thumb (white rounded handle, slides left/right)
    constexpr float tw = ph - 6.0f, th = ph - 6.0f;
    const float tx = on ? px + pw - tw - 3.0f : px + 3.0f;
    g.setColour(juce::Colours::white);
    g.fillRoundedRectangle(tx, py + 3.0f, tw, th, ph * 0.5f);

    // Label text
    if (button.getButtonText().isNotEmpty()) {
      g.setColour(NAMColours::FONT.withAlpha(on ? 1.0f : 0.55f));
      if (wide) {
        // Right of pill, vertically centred — settings radio group style
        g.setFont(mRobotoFont.withHeight(14.0f));
        g.drawText(button.getButtonText(),
                   juce::Rectangle<float>(px + pw + 8.0f, 0.0f,
                                          b.getWidth() - px - pw - 12.0f, b.getHeight()),
                   juce::Justification::centredLeft, false);
      } else {
        // Below pill, horizontally centred — main plugin switch style
        g.setFont(mRobotoFont.withHeight(12.0f));
        g.drawText(button.getButtonText(),
                   juce::Rectangle<float>(0.0f, py + ph + 3.0f, b.getWidth(), 14.0f),
                   juce::Justification::centred, false);
      }
    }
  }

  // Transparent background for icon/empty buttons; faint press state only
  void drawButtonBackground(juce::Graphics &g, juce::Button &button, const juce::Colour &,
                            bool highlighted, bool down) override {
    if (button.getButtonText().isEmpty()) {
      if (highlighted || down) {
        g.setColour(juce::Colours::white.withAlpha(down ? 0.12f : 0.06f));
        g.fillRoundedRectangle(button.getLocalBounds().toFloat(), 3.0f);
      }
      return;
    }
    if (highlighted || down) {
      g.setColour(juce::Colours::white.withAlpha(down ? 0.15f : 0.07f));
      g.fillRoundedRectangle(button.getLocalBounds().toFloat(), 3.0f);
    }
  }

  void drawButtonText(juce::Graphics &g, juce::TextButton &button, bool, bool) override {
    if (button.getButtonText().isEmpty())
      return;
    g.setColour(NAMColours::FONT.withAlpha(0.8f));
    g.setFont(mRobotoFont.withHeight(13.0f));
    g.drawText(button.getButtonText(), button.getLocalBounds(),
               juce::Justification::centred, false);
  }

  // -----------------------------------------------------------------------
  // Label — slider text boxes get a dark rounded box; all others use default.
  // The input calibration level uses LinearHorizontal with a full-width text
  // box, matching the original NAM InputLevelControl (IEditableTextControl).
  // -----------------------------------------------------------------------
  void drawLabel(juce::Graphics &g, juce::Label &label) override {
    auto *parentSlider = dynamic_cast<juce::Slider *>(label.getParentComponent());
    if (parentSlider != nullptr &&
        parentSlider->getSliderStyle() == juce::Slider::LinearHorizontal) {
      g.setColour(juce::Colour(0xff241f28));
      g.fillRoundedRectangle(label.getLocalBounds().toFloat().reduced(1.0f), 3.0f);
      g.setColour(NAMColours::FONT);
      g.setFont(mRobotoFont.withHeight(13.0f));
      g.drawText(label.getText(), label.getLocalBounds(), juce::Justification::centred,
                 true);
    } else {
      juce::LookAndFeel_V4::drawLabel(g, label);
    }
  }

  // -----------------------------------------------------------------------
  // Horizontal slider (Slim) — thin track, Azure fill + thumb
  // -----------------------------------------------------------------------
  void drawLinearSlider(juce::Graphics &g, int x, int y, int width, int height,
                        float sliderPos, float, float, juce::Slider::SliderStyle style,
                        juce::Slider &) override {
    if (style != juce::Slider::LinearHorizontal) {
      g.setColour(NAMColours::BG);
      g.fillRect(x, y, width, height);
      return;
    }

    const float trackY = y + height * 0.5f;
    constexpr float trackH = 3.0f;

    // Track background (Cadet Blue dimmed)
    g.setColour(NAMColours::CADET.withAlpha(0.25f));
    g.fillRoundedRectangle((float)x, trackY - trackH * 0.5f, (float)width, trackH,
                           trackH * 0.5f);

    // Azure filled portion
    const float filled = sliderPos - x;
    if (filled > 0.0f) {
      g.setColour(NAMColours::BLUE);
      g.fillRoundedRectangle((float)x, trackY - trackH * 0.5f, filled, trackH,
                             trackH * 0.5f);
    }

    // Thumb
    constexpr float thumbR = 5.5f;
    g.setColour(juce::Colours::black.withAlpha(0.4f)); // shadow
    g.fillEllipse(sliderPos - thumbR + 0.5f, trackY - thumbR + 0.5f, thumbR * 2.0f,
                  thumbR * 2.0f);
    g.setColour(NAMColours::BLUE);
    g.fillEllipse(sliderPos - thumbR, trackY - thumbR, thumbR * 2.0f, thumbR * 2.0f);
  }

private:
  juce::Font mRobotoFont;
  juce::Image mKnobBg;
};
