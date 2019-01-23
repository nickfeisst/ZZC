#include "ZZC.hpp"
#include "shared.hpp"
#include "widgets.hpp"

float fn3Sin(float phase) {
  return (sinf(phase * M_PI * 2.0f - M_PI / 2.0f) + 1.0f) / 2.0f;
}

float fn3Sqr(float value) {
  return value < 0.5f ? 1.0f : 0.0f;
}

float fn3Tri(float value) {
  return value < 0.5f ? value * 2.0f : (1.0f - value) * 2.0f;
}

float applyShift(float phase, float shift) {
  if (shift < 0.0f) {
    shift = 1.0f - fmodf(fabsf(shift), 1.0f);
  }
  return fmodf(phase + shift, 1.0f);
}

float applyPW(float phase, float pw) {
  return phase > pw ? (phase - pw) / (1.0f - pw) / 2.0f + 0.5f : phase / pw / 2.0f;
}

struct FN3DisplayWidget : BaseDisplayWidget {
  float *wave;
  float *pw;
  float *shift;

  void draw(NVGcontext *vg) override {
    drawBackground(vg);

    NVGcolor graphColor = nvgRGB(0xff, 0xd4, 0x2a);

		nvgBeginPath(vg);
    float firstCoord = true;
    for (float i = 0.0f; i <= 1.0f; i = i + 0.01f) {
      float x, y, value, phase;
      value = 0.0f;
      x = 2.0f + (box.size.x - 4.0f) * i;
      phase = applyPW(applyShift(i, *shift), *pw);
      if (*wave == 0.0f) {
        value = fn3Sin(phase);
      } else if (*wave == 1.0f) {
        value = fn3Tri(phase);
      } else if (*wave == 2.0f) {
        value = fn3Sqr(phase);
      }
      y = (1.0f - value) * (box.size.y / 4.0f) + (0.375f * box.size.y);

      if (firstCoord) {
        nvgMoveTo(vg, x, y);
        firstCoord = false;
        continue;
      }
      nvgLineTo(vg, x, y);
    }

		nvgStrokeColor(vg, graphColor);
		nvgLineCap(vg, NVG_ROUND);
		nvgMiterLimit(vg, 2.0f);
		nvgStrokeWidth(vg, 1.0f);
		nvgStroke(vg);

  }
};


struct FN3 : Module {
  enum ParamIds {
    PW_PARAM,
    WAVE_PARAM,
    OFFSET_PARAM,
    SHIFT_PARAM,
    NUM_PARAMS
  };
  enum InputIds {
    PW_INPUT,
    SHIFT_INPUT,
    PHASE_INPUT,
    NUM_INPUTS
  };
  enum OutputIds {
    WAVE_OUTPUT,
    NUM_OUTPUTS
  };
  enum LightIds {
    NUM_LIGHTS
  };

  float phase;
  float pw;
  float shift;
  float wave;

  FN3() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
  }
  void step() override;
};


void FN3::step() {
  pw = clamp(params[PW_PARAM].value + (inputs[PW_INPUT].active ? inputs[PW_INPUT].value / 10.0f : 0.0f), 0.0f, 1.0f);
  shift = params[SHIFT_PARAM].value + (inputs[SHIFT_INPUT].active ? inputs[SHIFT_INPUT].value / -5.0f : 0.0f);
  phase = applyPW(
    applyShift((inputs[PHASE_INPUT].active ? inputs[PHASE_INPUT].value / 10.0f : 0.0f), shift),
    pw
  );
  wave = params[WAVE_PARAM].value;

  if (wave == 0.0f) {
    outputs[WAVE_OUTPUT].value = fn3Sin(phase) * 10.0f;
  } else if (wave == 1.0f) {
    outputs[WAVE_OUTPUT].value = fn3Tri(phase) * 10.0f;
  } else {
    outputs[WAVE_OUTPUT].value = fn3Sqr(phase) * 10.0f;
  }
}


struct FN3Widget : ModuleWidget {
  FN3Widget(FN3 *module) : ModuleWidget(module) {
    setPanel(SVG::load(assetPlugin(plugin, "res/FN-3.svg")));

		addParam(ParamWidget::create<ZZC_ToothKnob>(Vec(10, 60), module, FN3::PW_PARAM, 0.0f, 1.0f, 0.5f));
    addInput(Port::create<ZZC_PJ301MPort>(Vec(10, 93), Port::INPUT, module, FN3::PW_INPUT));

    FN3DisplayWidget *display = new FN3DisplayWidget();
    display->box.pos = Vec(8.0f, 127.0f);
    display->box.size = Vec(29.0f, 49.0f);
    display->wave = &module->wave;
    display->pw = &module->pw;
    display->shift = &module->shift;
    addChild(display);
		addParam(ParamWidget::create<ZZC_FN3_Wave>(Vec(8, 127), module, FN3::WAVE_PARAM, 0.0f, 2.0f, 0.0f));
		addParam(ParamWidget::create<ZZC_FN3_UniBi>(Vec(8, 153), module, FN3::OFFSET_PARAM, 0.0f, 1.0f, 0.0f));

    addInput(Port::create<ZZC_PJ301MPort>(Vec(10, 196), Port::INPUT, module, FN3::SHIFT_INPUT));
		addParam(ParamWidget::create<ZZC_ToothKnob>(Vec(10, 229), module, FN3::SHIFT_PARAM, 1.0f, -1.0f, 0.0f));

    addInput(Port::create<ZZC_PJ301MPort>(Vec(10, 275), Port::INPUT, module, FN3::PHASE_INPUT));
    addOutput(Port::create<ZZC_PJ301MIPort>(Vec(10, 319), Port::OUTPUT, module, FN3::WAVE_OUTPUT));

		addChild(Widget::create<ScrewBlack>(Vec(box.size.x / 2 - RACK_GRID_WIDTH / 2, 0)));
		addChild(Widget::create<ScrewBlack>(Vec(box.size.x / 2 - RACK_GRID_WIDTH / 2, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
  }
};


Model *modelFN3 = Model::create<FN3, FN3Widget>("ZZC", "FN-3", "FN-3 Function Generator", FUNCTION_GENERATOR_TAG);