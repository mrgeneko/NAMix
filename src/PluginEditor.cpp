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

#include "PluginEditor.h"

// juce_StandaloneFilterWindow.h needs AudioIODeviceCallback and friends from
// juce_audio_utils, plus a visible declaration of ::createPluginFilter().
#include <juce_audio_utils/juce_audio_utils.h>
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter();
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace {
class NAMixMenuModel : public juce::MenuBarModel {
public:
  juce::StringArray getMenuBarNames() override { return {"File", "Help"}; }

  juce::PopupMenu getMenuForIndex(int index, const juce::String &) override {
    juce::PopupMenu m;
    if (index == 0) {
      m.addItem(1, "Preferences");
      m.addSeparator();
      m.addItem(2, "Quit");
    } else {
      m.addItem(3, "About NAMix");
    }
    return m;
  }

  void menuItemSelected(int id, int) override {
    if (id == 1) {
      if (auto *h = juce::StandalonePluginHolder::getInstance())
        h->showAudioSettingsDialog();
    } else if (id == 2) {
      juce::JUCEApplication::getInstance()->systemRequestedQuit();
    } else if (id == 3) {
      juce::AlertWindow::showMessageBoxAsync(
          juce::MessageBoxIconType::InfoIcon, "About NAMix",
          "NAMix\n\n"
          "Based on NeuralAmpModelerPlugin by Steven Atkinson (MIT licence)\n\n"
          "GPL v3 licence required by JUCE\n\n"
          "https://github.com/rations/namix-linux");
    }
  }
};
} // namespace

// ============================================================
// Layout constants — exact pixel values from NeuralAmpModeler
// PLUG_WIDTH=600, PLUG_HEIGHT=550
// mainArea   = 20px padding → (20,20,560,510)
// contentArea = another 10px → (30,30,540,490)
// titleHeight = 50
// NAM_KNOB_HEIGHT = 120
// knobsPad = 20 (from each side within contentArea)
// knobsExtraSpaceBelowTitle = 25
// fileWidth = 200 (row = 2*200 = 400px, centred in 540)
// fileHeight = 30
// irYOffset = 38
//
// PLUG_HEIGHT is 400 (upstream NAM plugin) + 150, permanently reserving a
// row for parametric-model knobs/switches shown inline below the toggle
// strip -- matching the same change made in the NeuralAmpModelerPlugin
// (iPlug2) fork, in place of a click-to-reveal icon+overlay.
// ============================================================
namespace Layout {
// contentArea origin and size
static constexpr int CL = 30, CT = 30, CW = 540, CH = 490;

// Knobs
static constexpr int KNOB_Y = CT + 50 + 25; // titleHeight + extraSpace = 105
static constexpr int KNOB_H = 120;          // NAM_KNOB_HEIGHT
static constexpr int KNOB_AX = CL + 20;     // knobsPad = 50
static constexpr int KNOB_AW = CW - 40;     // 500

// Toggle strip — below respective knob, reduced 10px from top
static constexpr int TOGGLE_Y = KNOB_Y + KNOB_H + 10; // 235
static constexpr int TOGGLE_H = 50;                   // NAM_SWITCH_HEIGHT

// Parametric-knob row — directly below the toggle strip, same horizontal
// span as the main knob row above, filling the space added by the +150
// window-height increase up to (but not overlapping) the file rows below.
static constexpr int PARAMETRIC_ROW_Y = TOGGLE_Y + TOGGLE_H + 10;

// File rows — centred 400px in contentArea, from bottom
static constexpr int ROW_W = 400;
static constexpr int ROW_X = CL + (CW - ROW_W) / 2;     // 100
static constexpr int ROW_H = 30;                        // fileHeight
static constexpr int MODEL_Y = CT + CH - 2 * ROW_H - 1; // 459
static constexpr int IR_Y = MODEL_Y + 38;               // 497 (irYOffset)

static constexpr int PARAMETRIC_ROW_H = MODEL_Y - PARAMETRIC_ROW_Y - 10;

// Slim icon — centred in the 60px gap between the model row right edge (500)
// and the output meter left edge (560). SLIM_W=40 → 10px margin on each side.
static constexpr int SLIM_W = 40;
static constexpr int SLIM_H = 20;

// Meters — outside contentArea, symmetric on left and right.
// Input: contentArea.GetFromLeft(30).GetHShifted(-20).GetMidVPadded(100).GetVShifted(-25)
// Output:
// contentArea.GetFromRight(30).GetHShifted(20).GetMidVPadded(100).GetVShifted(-25)
static constexpr int METER_INPUT_X = 10;
static constexpr int METER_X = 560;

// 510 was too left (6px gap), 530 was too right (30px gap). 520 splits the difference:
// 20px from model row right edge (x=500), 40px from plugin right edge.
static constexpr int SLIM_X = ROW_X + ROW_W + 20; // 520
static constexpr int METER_Y = 75;
static constexpr int METER_W = 30;
static constexpr int METER_H = 200;

// Settings button — mainArea corner 50×50 centred on 20×20
static constexpr int GEAR_X = 545;
static constexpr int GEAR_Y = 35;
static constexpr int GEAR_S = 22;
} // namespace Layout

// Helper: fractional grid cell across the knob area
static juce::Rectangle<int> knobCell(int i) {
  const float x0 = Layout::KNOB_AX + (float)i * Layout::KNOB_AW / 6.0f;
  const float x1 = Layout::KNOB_AX + (float)(i + 1) * Layout::KNOB_AW / 6.0f;
  return {juce::roundToInt(x0), Layout::KNOB_Y,
          juce::roundToInt(x1) - juce::roundToInt(x0), Layout::KNOB_H};
}

// ============================================================

NAMixAudioProcessorEditor::NAMixAudioProcessorEditor(NAMixAudioProcessor &p)
    : AudioProcessorEditor(&p), mProcessor(p), mSettingsPanel(p.apvts),
      mSlimOverlay(p.apvts, mLookAndFeel), mParametricRow(p, mLookAndFeel) {
  // Load embedded fonts (binary data from resources/fonts/)
  {
    auto michromaTypeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::MichromaRegular_ttf, BinaryData::MichromaRegular_ttfSize);
    mMichromaFont = juce::Font(michromaTypeface).withHeight(30.0f);

    auto robotoTypeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::RobotoRegular_ttf, BinaryData::RobotoRegular_ttfSize);
    mRobotoFont = juce::Font(robotoTypeface).withHeight(17.0f);
  }

  setLookAndFeel(&mLookAndFeel);
  setSize(600, 550);

  mBgImage = juce::ImageCache::getFromMemory(BinaryData::Background_jpg,
                                             BinaryData::Background_jpgSize);
  mLinesImage =
      juce::ImageCache::getFromMemory(BinaryData::Lines_png, BinaryData::Lines_pngSize);

  // In standalone mode: enable native OS title bar and attach a File/Help
  // menu bar just below it (DocumentWindow::setMenuBar positions it
  // automatically between the title bar and content).
  juce::MessageManager::callAsync([this] {
    if (auto *dw = findParentComponentOfClass<juce::DocumentWindow>()) {
      dw->setUsingNativeTitleBar(true);
      mMenuBarModel = std::make_unique<NAMixMenuModel>();
      dw->setMenuBar(mMenuBarModel.get());
    }
  });

  // --- Knobs (labels use Roboto-Regular 17px = DEFAULT_TEXT_SIZE+3) ---
  setupKnob(mInputGainSlider, mInputGainLabel, "Input");
  setupKnob(mNoiseGateSlider, mNoiseGateLabel, "Threshold");
  setupKnob(mBassSlider, mBassLabel, "Bass");
  setupKnob(mMiddleSlider, mMiddleLabel, "Mid");
  setupKnob(mTrebleSlider, mTrebleLabel, "Treble");
  setupKnob(mOutputGainSlider, mOutputGainLabel, "Output");

  // --- APVTS attachments ---
  auto &apvts = mProcessor.apvts;
  mInputGainAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          apvts, "inputGain", mInputGainSlider);
  mOutputGainAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          apvts, "outputGain", mOutputGainSlider);
  mNoiseGateAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          apvts, "noiseGateThreshold", mNoiseGateSlider);
  mBassAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          apvts, "bass", mBassSlider);
  mMiddleAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          apvts, "middle", mMiddleSlider);
  mTrebleAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          apvts, "treble", mTrebleSlider);
  mToneStackAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          apvts, "toneStackOn", mToneStackButton);
  mNoiseGateButtonAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          apvts, "noiseGateOn", mNoiseGateButton);

  addAndMakeVisible(mToneStackButton);
  addAndMakeVisible(mNoiseGateButton);

  // --- Slim icon button (NAMSquareButtonControl equivalent) ---
  // Loads SlimmableIcon.svg from binary data and displays it as a clickable
  // image. Hidden until a SlimmableModel is loaded. Clicking opens mSlimOverlay.
  {
    auto xml = juce::XmlDocument::parse(
        juce::String(BinaryData::SlimmableIcon_svg, BinaryData::SlimmableIcon_svgSize));
    if (xml != nullptr) {
      auto drawable = juce::Drawable::createFromSVG(*xml);
      mSlimButton.setImages(drawable.get());
    }
  }
  mSlimButton.setTooltip("Adjust model size (Slim). Click to open the dial.");
  mSlimButton.setVisible(false);
  mSlimButton.onClick = [this] {
    mSlimOverlay.setVisible(true);
    mSlimOverlay.toFront(false);
  };
  addAndMakeVisible(mSlimButton);

  // --- Slim overlay (dark backdrop + centred knob, initially hidden) ---
  // Covers the full editor. Added last so it renders on top of everything.
  mSlimOverlay.setVisible(false);
  addChildComponent(mSlimOverlay);

  // --- Parametric knob row ---
  // Always part of the layout; refresh() shows/hides individual knobs
  // depending on whether the loaded model is parametric and how many
  // knobs it declares. No button/overlay — matches the inline row in the
  // NeuralAmpModelerPlugin (iPlug2) fork.
  addAndMakeVisible(mParametricRow);

  // --- Gear button (transparent, overlays drawn gear icon) ---
  mGearButton.setButtonText("");
  mGearButton.setTooltip("Settings");
  mGearButton.onClick = [this] {
    const bool show = !mSettingsPanel.isVisible();
    mSettingsPanel.setVisible(show);
    if (show)
      mSettingsPanel.toFront(false); // ensure it covers all other children
  };
  addAndMakeVisible(mGearButton);

  // Settings panel wired here but added as child LAST (after all other
  // components) so it sits at the top of the z-order when visible.
  mSettingsPanel.onClose = [this] { mSettingsPanel.setVisible(false); };

  // --- Model file row ---
  mModelRow.onOpenChooser = [this] { chooseModelFile(); };
  mModelRow.onLoad = [this](juce::File f) {
    if (mProcessor.loadModel(f)) {
      mModelRow.setLoadedFile(f, "nam");
      mSettingsPanel.setModelSampleRate(mProcessor.getSampleRate());
      mSettingsPanel.setOutputModeSupport(mProcessor.getModelHasLoudness(),
                                          mProcessor.getModelHasOutputLevel());
      mSlimButton.setVisible(mProcessor.getModelIsSlimmable());
      mParametricRow.refresh();
    }
  };
  mModelRow.onClear = [this] {
    mProcessor.clearModel();
    mModelRow.clearFile();
    mSettingsPanel.clearModelInfo();
    mSettingsPanel.clearOutputModeSupport();
    mSlimButton.setVisible(false);
    mSlimOverlay.setVisible(false);
    mParametricRow.refresh();
  };
  {
    const juce::String mp = mProcessor.getModelPath();
    if (mp.isNotEmpty()) {
      mModelRow.setLoadedFile(juce::File(mp), "nam");
      mSettingsPanel.setModelSampleRate(mProcessor.getSampleRate());
      mSlimButton.setVisible(mProcessor.getModelIsSlimmable());
      mParametricRow.refresh();
    }
  }
  addAndMakeVisible(mModelRow);

  // --- IR file row ---
  mIRRow.onOpenChooser = [this] { chooseIRFile(); };
  mIRRow.onLoad = [this](juce::File f) {
    if (mProcessor.loadIR(f))
      mIRRow.setLoadedFile(f, "wav");
  };
  mIRRow.onClear = [this] {
    mProcessor.clearIR();
    mIRRow.clearFile();
  };
  {
    const juce::String ip = mProcessor.getIRPath();
    if (ip.isNotEmpty())
      mIRRow.setLoadedFile(juce::File(ip), "wav");
  }
  addAndMakeVisible(mIRRow);

  addAndMakeVisible(mInputLevelMeter);
  addAndMakeVisible(mLevelMeter);

  // Add settings panel last so it is at the top of the z-order and covers
  // every other child when shown.
  addChildComponent(mSettingsPanel);

  startTimerHz(30);
}

NAMixAudioProcessorEditor::~NAMixAudioProcessorEditor() {
  if (mMenuBarModel) {
    if (auto *dw = findParentComponentOfClass<juce::DocumentWindow>())
      dw->setMenuBar(nullptr);
  }
  stopTimer();
  setLookAndFeel(nullptr);
}

void NAMixAudioProcessorEditor::timerCallback() {
  mInputLevelMeter.setLevel(mProcessor.getInputLevel());
  mLevelMeter.setLevel(mProcessor.getOutputLevel());
  mSettingsPanel.setInputCalibrationEnabled(mProcessor.getModelHasInputLevel());
  mSettingsPanel.refreshCalibrationDisplay();
}

void NAMixAudioProcessorEditor::paint(juce::Graphics &g) {
  // Plugin background — use pre-rendered Background.jpg for depth/texture
  if (mBgImage.isValid())
    g.drawImage(mBgImage, 0, 0, getWidth(), getHeight(), 0, 0, mBgImage.getWidth(),
                mBgImage.getHeight(), false);
  else
    g.fillAll(juce::Colour(0xff1d1a1f));

  // Lines texture overlay at low opacity
  if (mLinesImage.isValid()) {
    g.setOpacity(0.35f);
    g.drawImage(mLinesImage, 0, 0, getWidth(), getHeight(), 0, 0, mLinesImage.getWidth(),
                mLinesImage.getHeight(), false);
    g.setOpacity(1.0f);
  }

  // Title — Michroma-Regular 30px, white, centred in contentArea.GetFromTop(50)
  g.setColour(juce::Colour(0xfff2f2f2));
  g.setFont(mMichromaFont.withHeight(30.0f));
  g.drawText("Anti-Static NAMix", juce::Rectangle<int>(Layout::CL, Layout::CT, Layout::CW, 50),
             juce::Justification::centred);

  // Gear icon — drawn in the settingsButtonArea corner (centred in 22×22)
  {
    const float gx = Layout::GEAR_X + Layout::GEAR_S * 0.5f;
    const float gy = Layout::GEAR_Y + Layout::GEAR_S * 0.5f;
    const float outer = 8.0f;
    const float inner = 5.0f;
    const float holeR = 2.5f;
    const int teeth = 8;

    g.setColour(juce::Colour(0xffa2b2bf).withAlpha(0.7f)); // Cadet Blue dimmed

    juce::Path gear;
    for (int i = 0; i < teeth; ++i) {
      const float a0 = juce::MathConstants<float>::twoPi * i / teeth;
      const float a1 = juce::MathConstants<float>::twoPi * (i + 0.3f) / teeth;
      const float a2 = juce::MathConstants<float>::twoPi * (i + 0.7f) / teeth;
      const float a3 = juce::MathConstants<float>::twoPi * (i + 1.0f) / teeth;
      auto pt = [&](float r, float a) -> juce::Point<float> {
        return {gx + r * std::sin(a), gy - r * std::cos(a)};
      };
      if (i == 0)
        gear.startNewSubPath(pt(inner, a0));
      else
        gear.lineTo(pt(inner, a0));
      gear.lineTo(pt(outer, a1));
      gear.lineTo(pt(outer, a2));
      gear.lineTo(pt(inner, a3));
    }
    gear.closeSubPath();
    g.fillPath(gear);
    g.setColour(juce::Colour(0xff1d1a1f));
    g.fillEllipse(gx - holeR, gy - holeR, holeR * 2.0f, holeR * 2.0f);
  }

  // Subtle separator between toggles and file rows
  g.setColour(juce::Colours::white.withAlpha(0.07f));
  g.drawHorizontalLine(Layout::MODEL_Y - 10, (float)Layout::ROW_X,
                       (float)(Layout::ROW_X + Layout::ROW_W + Layout::SLIM_W + 10));
  // Note: the slim button may extend past ROW_W; the line covers that area.
}

void NAMixAudioProcessorEditor::resized() {
  // Input meter — outside contentArea on the left (matches inputMeterArea)
  mInputLevelMeter.setBounds(Layout::METER_INPUT_X, Layout::METER_Y, Layout::METER_W,
                             Layout::METER_H);
  // Output meter — outside contentArea on the right (matches outputMeterArea)
  mLevelMeter.setBounds(Layout::METER_X, Layout::METER_Y, Layout::METER_W,
                        Layout::METER_H);

  // Gear button overlays the drawn gear icon
  mGearButton.setBounds(Layout::GEAR_X, Layout::GEAR_Y, Layout::GEAR_S, Layout::GEAR_S);

  // Settings panel: full-editor overlay
  mSettingsPanel.setBounds(getLocalBounds());

  // Knobs — 6 equal cells across knobsArea (500px), labels at top (17px)
  constexpr int labelH = 17; // Roboto-Regular at DEFAULT_TEXT_SIZE+3
  for (int i = 0; i < 6; ++i) {
    auto cell = knobCell(i);
    juce::Label *lbl = nullptr;
    juce::Slider *sld = nullptr;
    switch (i) {
    case 0:
      lbl = &mInputGainLabel;
      sld = &mInputGainSlider;
      break;
    case 1:
      lbl = &mNoiseGateLabel;
      sld = &mNoiseGateSlider;
      break;
    case 2:
      lbl = &mBassLabel;
      sld = &mBassSlider;
      break;
    case 3:
      lbl = &mMiddleLabel;
      sld = &mMiddleSlider;
      break;
    case 4:
      lbl = &mTrebleLabel;
      sld = &mTrebleSlider;
      break;
    case 5:
      lbl = &mOutputGainLabel;
      sld = &mOutputGainSlider;
      break;
    }
    lbl->setBounds(cell.removeFromTop(labelH));
    sld->setBounds(cell);
  }

  // Toggle strip — centred under NoiseGate (knob 1) and Mid (knob 3).
  {
    auto ngCell = knobCell(1);
    auto midCell = knobCell(3);
    mNoiseGateButton.setBounds(
        ngCell.withY(Layout::TOGGLE_Y).withHeight(Layout::TOGGLE_H));
    mToneStackButton.setBounds(
        midCell.withY(Layout::TOGGLE_Y).withHeight(Layout::TOGGLE_H));
  }

  // Parametric-knob row — directly below the toggle strip, same horizontal
  // span as the main knob row.
  mParametricRow.setBounds(Layout::KNOB_AX, Layout::PARAMETRIC_ROW_Y, Layout::KNOB_AW,
                           Layout::PARAMETRIC_ROW_H);

  // Model row — 400px centred; slim icon button in the 56px gap to the right.
  // Icon is 56×28px, vertically centred on the row (matching original slimIconArea).
  mModelRow.setBounds(Layout::ROW_X, Layout::MODEL_Y, Layout::ROW_W, Layout::ROW_H);
  mSlimButton.setBounds(Layout::SLIM_X,
                        Layout::MODEL_Y + (Layout::ROW_H - Layout::SLIM_H) / 2,
                        Layout::SLIM_W, Layout::SLIM_H);
  mSlimOverlay.setBounds(getLocalBounds());

  // IR row — same width and x as model row
  mIRRow.setBounds(Layout::ROW_X, Layout::IR_Y, Layout::ROW_W, Layout::ROW_H);
}

void NAMixAudioProcessorEditor::setupKnob(juce::Slider &slider, juce::Label &label,
                                            const juce::String &name) {
  label.setText(name, juce::dontSendNotification);
  label.setJustificationType(juce::Justification::centred);
  label.setFont(mRobotoFont.withHeight(14.0f));
  label.setColour(juce::Label::textColourId, juce::Colour(0xfff2f2f2));
  addAndMakeVisible(label);

  slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 17);
  addAndMakeVisible(slider);
}

void NAMixAudioProcessorEditor::chooseModelFile() {
  mFileChooser = std::make_unique<juce::FileChooser>(
      "Load NAM Model", juce::File::getSpecialLocation(juce::File::userHomeDirectory),
      "*.nam");

  mFileChooser->launchAsync(
      juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
      [this](const juce::FileChooser &fc) {
        auto result = fc.getResult();
        if (result.existsAsFile() && mProcessor.loadModel(result)) {
          mModelRow.setLoadedFile(result, "nam");
          mSettingsPanel.setModelSampleRate(mProcessor.getSampleRate());
          mSettingsPanel.setOutputModeSupport(mProcessor.getModelHasLoudness(),
                                              mProcessor.getModelHasOutputLevel());
          mSlimButton.setVisible(mProcessor.getModelIsSlimmable());
          mParametricRow.refresh();
        }
      });
}

void NAMixAudioProcessorEditor::chooseIRFile() {
  mFileChooser = std::make_unique<juce::FileChooser>(
      "Load Impulse Response",
      juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.wav");

  mFileChooser->launchAsync(juce::FileBrowserComponent::openMode |
                                juce::FileBrowserComponent::canSelectFiles,
                            [this](const juce::FileChooser &fc) {
                              auto result = fc.getResult();
                              if (result.existsAsFile() && mProcessor.loadIR(result))
                                mIRRow.setLoadedFile(result, "wav");
                            });
}
