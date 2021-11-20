using namespace rack;

extern Plugin *pluginInstance;

struct BaseDisplayWidget : TransparentWidget {
  NVGcolor backgroundColor = nvgRGB(0x01, 0x01, 0x01);
  NVGcolor lcdColor = nvgRGB(0x12, 0x12, 0x12);

  void drawBackground(const DrawArgs &args) {
    // Background
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0.0, 0.0, box.size.x, box.size.y, 3.0);
    nvgFillColor(args.vg, backgroundColor);
    nvgFill(args.vg);

    // LCD
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 2.0, 2.0, box.size.x - 4.0, box.size.y - 4.0, 1.0);
    nvgFillColor(args.vg, lcdColor);
    nvgFill(args.vg);
  }
};

struct Display32Widget : BaseDisplayWidget {
  float *value = nullptr;
  bool *disabled = nullptr;
  std::shared_ptr<Font> font;

  Display32Widget() {
    font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DSEG/DSEG7ClassicMini-Italic.ttf"));
  };

  void draw(const DrawArgs &args) override {
    drawBackground(args);
    float valueToDraw = std::abs(value ? *value : 120.0f);
    NVGcolor lcdGhostColor = nvgRGB(0x1e, 0x1f, 0x1d);
    NVGcolor lcdTextColor = nvgRGB(0xff, 0xd4, 0x2a);

    // Text (integer part)
    nvgFontSize(args.vg, 11);
    nvgFontFaceId(args.vg, font->handle);
    nvgTextLetterSpacing(args.vg, 1.0);
    nvgTextAlign(args.vg, NVG_ALIGN_RIGHT);

    char integerPartString[10];
    if (valueToDraw >= 1000.0f || (disabled && *disabled)) {
      snprintf(integerPartString, sizeof(integerPartString), "---.");
    } else {
      snprintf(integerPartString, sizeof(integerPartString), "%3.0f.", floor(valueToDraw));
    }

    Vec textPos = Vec(36.0f, 16.0f);

    nvgFillColor(args.vg, lcdGhostColor);
    nvgText(args.vg, textPos.x, textPos.y, "888.", NULL);
    nvgFillColor(args.vg, lcdTextColor);
    nvgText(args.vg, textPos.x, textPos.y, integerPartString, NULL);

    // Text (fractional part)
    nvgFontSize(args.vg, 9);
    nvgTextLetterSpacing(args.vg, 0.0);

    char fractionalPartString[10];
    float remainder = fmod(valueToDraw, 1.0f) * 100.0f;
    float intpart;
    std::modf(remainder, &intpart);
    if (disabled && *disabled) {
      snprintf(fractionalPartString, sizeof(fractionalPartString), "--");
    } else if (valueToDraw >= 1000.0f) {
      snprintf(fractionalPartString, sizeof(fractionalPartString), "--");
    } else if (intpart == 0.0f) {
      snprintf(fractionalPartString, sizeof(fractionalPartString), "00");
    } else {
      snprintf(fractionalPartString, sizeof(fractionalPartString), "%2.0f", intpart);
      if (fractionalPartString[0] == ' ') {
        fractionalPartString[0] = '0';
      }
    }

    textPos = Vec(52.0f, 16.0f);

    nvgFillColor(args.vg, lcdGhostColor);
    nvgText(args.vg, textPos.x, textPos.y, "88", NULL);
    nvgFillColor(args.vg, lcdTextColor);
    nvgText(args.vg, textPos.x, textPos.y, fractionalPartString, NULL);
  }
};

struct DisplayIntpartWidget : BaseDisplayWidget {
  float *value = nullptr;
  std::shared_ptr<Font> font;

  DisplayIntpartWidget() {
    font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DSEG/DSEG7ClassicMini-Italic.ttf"));
  };

  void draw(const DrawArgs &args) override {
    drawBackground(args);
    NVGcolor lcdGhostColor = nvgRGB(0x1e, 0x1f, 0x1d);
    NVGcolor lcdTextColor = nvgRGB(0xff, 0xd4, 0x2a);

    // Text (integer part)
    nvgFontSize(args.vg, 11);
    nvgFontFaceId(args.vg, font->handle);
    nvgTextLetterSpacing(args.vg, 1.0);
    nvgTextAlign(args.vg, NVG_ALIGN_RIGHT);

    char integerPartString[10];
    snprintf(integerPartString, sizeof(integerPartString), "%8.0f", value ? *value : 8.0f);

    Vec textPos = Vec(box.size.x - 5.0f, 16.0f);

    nvgFillColor(args.vg, lcdGhostColor);
    nvgText(args.vg, textPos.x, textPos.y, "88", NULL);
    nvgFillColor(args.vg, lcdTextColor);
    nvgText(args.vg, textPos.x, textPos.y, integerPartString, NULL);
  }
};

struct IntDisplayWidget : BaseDisplayWidget {
  int *value = nullptr;
  int *polarity = nullptr;
  bool *isPoly = nullptr;
  bool *blinking = nullptr;
  int blinkingPhase = 0;
  std::string textGhost = "88";
  std::shared_ptr<Font> font;
  NVGcolor lcdGhostColor = nvgRGB(0x1e, 0x1f, 0x1d);
  NVGcolor lcdTextColor = nvgRGB(0xff, 0xd4, 0x2a);
  NVGcolor lcdTextColorBlink = nvgRGB(0x8a, 0x72, 0x17);
  NVGcolor negColor = nvgRGB(0xe7, 0x34, 0x2d);
  NVGcolor negColorBlink = nvgRGB(0x8a, 0x1f, 0x1b);
  NVGcolor posColor = nvgRGB(0x9c, 0xd7, 0x43);
  NVGcolor posColorBlink = nvgRGB(0x51, 0x70, 0x23);
  NVGcolor polyColor = nvgRGB(0x76, 0xdc, 0xfa);
  NVGcolor polyColorBlink = nvgRGB(0x43, 0x7e, 0x8f);

  IntDisplayWidget() {
    font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DSEG/DSEG7ClassicMini-Italic.ttf"));
  };

  void draw(const DrawArgs &args) override {
    drawBackground(args);

    nvgFontSize(args.vg, 11);
    nvgFontFaceId(args.vg, font->handle);
    nvgTextLetterSpacing(args.vg, 1.0);
    nvgTextAlign(args.vg, NVG_ALIGN_RIGHT);

    char integerString[10];
    snprintf(integerString, sizeof(integerString), "%d", value ? *value : 1);

    Vec textPos = Vec(box.size.x - 5.0f, 16.0f);

    bool isBlinking = blinking && *blinking;
    if (isBlinking) {
      blinkingPhase = (blinkingPhase + 1) % 24;
    }
    bool blink = isBlinking && blinkingPhase < 12;

    nvgFillColor(args.vg, lcdGhostColor);
    nvgText(args.vg, textPos.x, textPos.y, textGhost.c_str(), NULL);
    if (isPoly && *isPoly) {
      nvgFillColor(args.vg, blink ? polyColorBlink : polyColor);
    } else {
      if (polarity) {
        if (*polarity == 0) {
          nvgFillColor(args.vg, blink ? lcdTextColorBlink : lcdTextColor);
        } else {
          nvgFillColor(args.vg, *polarity > 0 ? (blink ? posColorBlink : posColor) : (blink ? negColorBlink : negColor));
        }
      } else {
        nvgFillColor(args.vg, blink ? lcdTextColorBlink : lcdTextColor);
      }
    }
    nvgText(args.vg, textPos.x, textPos.y, integerString, NULL);
  }
};

struct RatioDisplayWidget : BaseDisplayWidget {
  float *from = nullptr;
  float *to = nullptr;
  std::shared_ptr<Font> font;

  RatioDisplayWidget() {
    font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DSEG/DSEG7ClassicMini-Italic.ttf"));
  };

  void draw(const DrawArgs &args) override {
    drawBackground(args);
    NVGcolor lcdGhostColor = nvgRGB(0x1e, 0x1f, 0x1d);
    NVGcolor lcdTextColor = nvgRGB(0xff, 0xd4, 0x2a);

    nvgFontSize(args.vg, 11);
    nvgFontFaceId(args.vg, font->handle);
    nvgTextLetterSpacing(args.vg, 1.0);

    // Text (from)
    nvgTextAlign(args.vg, NVG_ALIGN_RIGHT);

    char fromString[10];
    snprintf(fromString, sizeof(fromString), "%2.0f", from ? *from : 1.0);

    Vec textPos = Vec(box.size.x / 2.0f - 3.0f, 16.0f);

    nvgFillColor(args.vg, lcdGhostColor);
    nvgText(args.vg, textPos.x, textPos.y, "88", NULL);
    nvgFillColor(args.vg, lcdTextColor);
    nvgText(args.vg, textPos.x, textPos.y, fromString, NULL);

    // Text (from)
    nvgTextAlign(args.vg, NVG_ALIGN_LEFT);

    char toString[10];
    snprintf(toString, sizeof(toString), "%2.0f", to ? *to : 1.0);
    if (toString[0] == ' ') {
      toString[0] = toString[1];
      toString[1] = ' ';
    }

    textPos = Vec(box.size.x / 2.0f + 2.0f, 16.0f);

    nvgFillColor(args.vg, lcdGhostColor);
    nvgText(args.vg, textPos.x, textPos.y, "88", NULL);
    nvgFillColor(args.vg, lcdTextColor);
    nvgText(args.vg, textPos.x, textPos.y, toString, NULL);

    // Text (:)
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER);

    textPos = Vec(box.size.x / 2.0f, 16.0f);

    nvgFillColor(args.vg, lcdTextColor);
    nvgText(args.vg, textPos.x, textPos.y, ":", NULL);
  }
};
