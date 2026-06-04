/*
 * Gateway Linux VST3 Plugin
 * Copyright (C) 2026 rations
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

// Exact NAM colour palette
namespace NAMColours
{
  // NAM_1: Raisin Black — main background
  static const juce::Colour BG       { 0xff1d1a1f };
  // NAM_2 / NAM_THEMECOLOR: Azure — active/value colour
  static const juce::Colour BLUE     { 0xff5085e8 };
  // NAM_3: Cadet Blue Crayola — dimmed / track colour
  static const juce::Colour CADET    { 0xffa2b2bf };
  // NAM_THEMEFONTCOLOR: near-white
  static const juce::Colour FONT     { 0xfff2f2f2 };
  // File row background
  static const juce::Colour FILE_BG  { 0xff1a1720 };
}

class GatewayLookAndFeel : public juce::LookAndFeel_V4
{
public:
  GatewayLookAndFeel()
  {
    // Slider text colours — use NAM font colour on transparent background
    setColour(juce::Slider::textBoxTextColourId,
              NAMColours::FONT.withAlpha(0.85f));
    setColour(juce::Slider::textBoxBackgroundColourId,
              juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId,
              juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId,           NAMColours::FONT);
    setColour(juce::TextButton::textColourOffId,   NAMColours::FONT);
    setColour(juce::TextButton::textColourOnId,    NAMColours::FONT);
  }

  // -----------------------------------------------------------------------
  // Rotary knob — dark body, Cadet Blue track arc, Azure value arc + dot
  // -----------------------------------------------------------------------
  void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                        float sliderPos, float startAngle, float endAngle,
                        juce::Slider&) override
  {
    const float cx       = x + width  * 0.5f;
    const float cy       = y + height * 0.5f;
    const float outerR   = std::min(width, height) * 0.5f - 4.0f;
    const float innerR   = outerR * 0.73f; // matches NAM's widget radius factor
    const float arcW     = 3.5f;
    const float curAngle = startAngle + sliderPos * (endAngle - startAngle);

    // Shadow ring (NAM draws a shadow behind the knob body)
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillEllipse(cx - innerR + 1.5f, cy - innerR + 1.5f,
                  innerR * 2.0f, innerR * 2.0f);

    // Grey (Cadet Blue) track arc
    {
      juce::Path track;
      track.addCentredArc(cx, cy, outerR, outerR, 0.0f, startAngle, endAngle, true);
      g.setColour(NAMColours::CADET.withAlpha(0.4f));
      g.strokePath(track, juce::PathStrokeType(arcW, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }

    // Azure value arc
    if (sliderPos > 0.0f)
    {
      juce::Path value;
      value.addCentredArc(cx, cy, outerR, outerR, 0.0f, startAngle, curAngle, true);
      g.setColour(NAMColours::BLUE);
      g.strokePath(value, juce::PathStrokeType(arcW, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }

    // Dark knob body (NAM_1-ish, slightly lighter than bg)
    g.setColour(juce::Colour(0xff211e25));
    g.fillEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);

    // Subtle rim highlight (NAM uses emboss)
    g.setColour(juce::Colour(0xff3a3542));
    g.drawEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f, 0.8f);

    // Azure indicator dot (NAM: 3px filled circle at pointer tip)
    const float dotR   = 3.0f;
    const float dotDst = innerR * 0.55f; // mInnerPointerFrac = 0.55
    const float dotX   = cx + dotDst * std::sin(curAngle);
    const float dotY   = cy - dotDst * std::cos(curAngle);

    // Drop shadow for dot
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillEllipse(dotX - dotR + 0.5f, dotY - dotR + 0.5f,
                  dotR * 2.0f, dotR * 2.0f);
    g.setColour(NAMColours::BLUE);
    g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
  }

  // -----------------------------------------------------------------------
  // Pill-style toggle — Azure when on, dimmed when off, label below
  // Matches NAMSwitchControl style (roundness ≈ 7px)
  // -----------------------------------------------------------------------
  void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                        bool, bool) override
  {
    const bool on = button.getToggleState();
    const auto b  = button.getLocalBounds().toFloat();

    // Pill dimensions — same proportions as NAMSwitchControl
    constexpr float pw = 40.0f, ph = 22.0f;
    const float px = (b.getWidth()  - pw) * 0.5f;
    const float py = (b.getHeight() - ph - 16.0f) * 0.5f; // leave room for label

    // Track background — Azure (on) / dark (off)
    g.setColour(on ? NAMColours::BLUE : NAMColours::BLUE.withAlpha(0.15f));
    g.fillRoundedRectangle(px, py, pw, ph, 7.0f); // cR = 7 from NAM

    // Frame stroke
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawRoundedRectangle(px, py, pw, ph, 7.0f, 0.5f);

    // Thumb (white pill handle)
    constexpr float tw = ph - 6.0f, th = ph - 6.0f;
    const float tx = on ? px + pw - tw - 3.0f : px + 3.0f;
    g.setColour(juce::Colours::white);
    g.fillRoundedRectangle(tx, py + 3.0f, tw, th, 7.0f);

    // Label (below pill) — NAM_THEMEFONTCOLOR at full opacity
    g.setColour(NAMColours::FONT.withAlpha(on ? 1.0f : 0.55f));
    g.setFont(juce::Font("Roboto-Regular", 12.0f, juce::Font::plain));
    g.drawText(button.getButtonText(),
               juce::Rectangle<float>(0.0f, py + ph + 3.0f, b.getWidth(), 14.0f),
               juce::Justification::centred, false);
  }

  // Transparent background for icon/empty buttons; faint press state only
  void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                            const juce::Colour&, bool highlighted, bool down) override
  {
    if (button.getButtonText().isEmpty())
    {
      if (highlighted || down)
      {
        g.setColour(juce::Colours::white.withAlpha(down ? 0.12f : 0.06f));
        g.fillRoundedRectangle(button.getLocalBounds().toFloat(), 3.0f);
      }
      return;
    }
    if (highlighted || down)
    {
      g.setColour(juce::Colours::white.withAlpha(down ? 0.15f : 0.07f));
      g.fillRoundedRectangle(button.getLocalBounds().toFloat(), 3.0f);
    }
  }

  void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                      bool, bool) override
  {
    if (button.getButtonText().isEmpty()) return;
    g.setColour(NAMColours::FONT.withAlpha(0.8f));
    g.setFont(juce::Font("Roboto-Regular", 13.0f, juce::Font::plain));
    g.drawText(button.getButtonText(), button.getLocalBounds(),
               juce::Justification::centred, false);
  }

  // -----------------------------------------------------------------------
  // Horizontal slider (Slim) — thin track, Azure fill + thumb
  // -----------------------------------------------------------------------
  void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                        float sliderPos, float, float,
                        juce::Slider::SliderStyle style, juce::Slider&) override
  {
    if (style != juce::Slider::LinearHorizontal)
    {
      g.setColour(NAMColours::BG);
      g.fillRect(x, y, width, height);
      return;
    }

    const float trackY = y + height * 0.5f;
    constexpr float trackH = 3.0f;

    // Track background (Cadet Blue dimmed)
    g.setColour(NAMColours::CADET.withAlpha(0.25f));
    g.fillRoundedRectangle((float)x, trackY - trackH * 0.5f,
                            (float)width, trackH, trackH * 0.5f);

    // Azure filled portion
    const float filled = sliderPos - x;
    if (filled > 0.0f)
    {
      g.setColour(NAMColours::BLUE);
      g.fillRoundedRectangle((float)x, trackY - trackH * 0.5f,
                              filled, trackH, trackH * 0.5f);
    }

    // Thumb
    constexpr float thumbR = 5.5f;
    g.setColour(juce::Colours::black.withAlpha(0.4f)); // shadow
    g.fillEllipse(sliderPos - thumbR + 0.5f, trackY - thumbR + 0.5f,
                  thumbR * 2.0f, thumbR * 2.0f);
    g.setColour(NAMColours::BLUE);
    g.fillEllipse(sliderPos - thumbR, trackY - thumbR,
                  thumbR * 2.0f, thumbR * 2.0f);
  }
};
