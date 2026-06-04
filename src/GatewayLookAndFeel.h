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

class GatewayLookAndFeel : public juce::LookAndFeel_V4
{
public:
  GatewayLookAndFeel()
  {
    setColour(juce::Slider::textBoxTextColourId,       juce::Colours::white);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId,               juce::Colours::white);
    setColour(juce::TextButton::textColourOffId,       juce::Colours::white);
    setColour(juce::TextButton::textColourOnId,        juce::Colours::white);
  }

  void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                        float sliderPos, float startAngle, float endAngle,
                        juce::Slider&) override
  {
    const float cx       = x + width  * 0.5f;
    const float cy       = y + height * 0.5f;
    const float outerR   = std::min(width, height) * 0.5f - 3.0f;
    const float innerR   = outerR - 6.0f;
    const float arcW     = 3.0f;
    const float curAngle = startAngle + sliderPos * (endAngle - startAngle);

    // Grey track arc
    {
      juce::Path track;
      track.addCentredArc(cx, cy, outerR, outerR, 0.0f, startAngle, endAngle, true);
      g.setColour(juce::Colour(0xff333344));
      g.strokePath(track, juce::PathStrokeType(arcW, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }

    // Blue value arc
    if (sliderPos > 0.0f)
    {
      juce::Path value;
      value.addCentredArc(cx, cy, outerR, outerR, 0.0f, startAngle, curAngle, true);
      g.setColour(juce::Colour(0xff4a90d9));
      g.strokePath(value, juce::PathStrokeType(arcW, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }

    // Dark knob body
    g.setColour(juce::Colour(0xff111111));
    g.fillEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);

    // Subtle rim
    g.setColour(juce::Colour(0xff2a2a3a));
    g.drawEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f, 1.0f);

    // Blue indicator dot
    const float dotR   = 3.0f;
    const float dotDst = innerR - dotR - 2.5f;
    const float dotX   = cx + dotDst * std::sin(curAngle);
    const float dotY   = cy - dotDst * std::cos(curAngle);
    g.setColour(juce::Colour(0xff4a90d9));
    g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
  }

  void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                        bool, bool) override
  {
    const bool on   = button.getToggleState();
    const auto b    = button.getLocalBounds().toFloat();
    constexpr float pw = 36.0f, ph = 20.0f;
    const float px  = (b.getWidth() - pw) * 0.5f;
    const float py  = 1.0f;

    // Track
    g.setColour(on ? juce::Colour(0xff4a90d9) : juce::Colour(0xff444444));
    g.fillRoundedRectangle(px, py, pw, ph, ph * 0.5f);

    // Thumb
    constexpr float tr = ph - 4.0f;
    const float tx = on ? px + pw - tr - 2.0f : px + 2.0f;
    g.setColour(juce::Colours::white);
    g.fillEllipse(tx, py + 2.0f, tr, tr);

    // Label below pill
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(11.5f));
    g.drawText(button.getButtonText(),
               juce::Rectangle<float>(0.0f, py + ph + 1.0f, b.getWidth(), 14.0f),
               juce::Justification::centred, false);
  }

  void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                            const juce::Colour&, bool highlighted, bool down) override
  {
    // Transparent for icon buttons (no text) — only show faint press state
    if (button.getButtonText().isEmpty())
    {
      if (highlighted || down)
      {
        g.setColour(juce::Colours::white.withAlpha(down ? 0.12f : 0.06f));
        g.fillRoundedRectangle(button.getLocalBounds().toFloat(), 3.0f);
      }
      return;
    }
    // Flat style for small text buttons
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
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.setFont(juce::Font(13.0f));
    g.drawText(button.getButtonText(), button.getLocalBounds(),
               juce::Justification::centred, false);
  }
};
