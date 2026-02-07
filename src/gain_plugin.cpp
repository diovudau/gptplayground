#include "clap/clap.h"
#include "ikigui/ikigui.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

namespace {

constexpr char kPluginId[] = "com.gptplayground.ikigui-gain";
constexpr uint32_t kGainParamId = 0;

const char *kFeatures[] = {
    "audio-effect",
    "stereo",
    nullptr,
};

const clap_plugin_descriptor_t kDescriptor{
    .clap_version = CLAP_VERSION,
    .id = kPluginId,
    .name = "IkiGUI Gain",
    .vendor = "GPT Playground",
    .url = "https://example.invalid",
    .manual_url = "https://example.invalid/manual",
    .support_url = "https://example.invalid/support",
    .version = "0.1.0",
    .description = "Low-latency, RT-safe gain plugin with IkiGUI",
    .features = kFeatures,
};

class GainPlugin {
 public:
  explicit GainPlugin(const clap_host_t *host) : host_(host) {
    clap_plugin_.desc = &kDescriptor;
    clap_plugin_.plugin_data = this;
    clap_plugin_.init = &clap_init;
    clap_plugin_.destroy = &clap_destroy;
    clap_plugin_.activate = &clap_activate;
    clap_plugin_.deactivate = &clap_deactivate;
    clap_plugin_.start_processing = &clap_start_processing;
    clap_plugin_.stop_processing = &clap_stop_processing;
    clap_plugin_.reset = &clap_reset;
    clap_plugin_.process = &clap_process;
    clap_plugin_.get_extension = &clap_get_extension;
    clap_plugin_.on_main_thread = &clap_on_main_thread;
  }

  const clap_plugin_t *plugin() const noexcept { return &clap_plugin_; }

 private:
  static GainPlugin *self(const clap_plugin_t *plugin) noexcept {
    return static_cast<GainPlugin *>(plugin->plugin_data);
  }

  static bool clap_init(const clap_plugin_t *plugin) noexcept { return self(plugin)->init(); }
  static void clap_destroy(const clap_plugin_t *plugin) noexcept { delete self(plugin); }
  static bool clap_activate(const clap_plugin_t *plugin, double sample_rate, uint32_t, uint32_t) noexcept {
    return self(plugin)->activate(sample_rate);
  }
  static void clap_deactivate(const clap_plugin_t *plugin) noexcept { self(plugin)->deactivate(); }
  static bool clap_start_processing(const clap_plugin_t *plugin) noexcept { return self(plugin)->start_processing(); }
  static void clap_stop_processing(const clap_plugin_t *plugin) noexcept { self(plugin)->stop_processing(); }
  static void clap_reset(const clap_plugin_t *plugin) noexcept { self(plugin)->reset(); }
  static int32_t clap_process(const clap_plugin_t *plugin, const clap_process_t *process) noexcept {
    return self(plugin)->process(process);
  }
  static const void *clap_get_extension(const clap_plugin_t *plugin, const char *id) noexcept {
    return self(plugin)->get_extension(id);
  }
  static void clap_on_main_thread(const clap_plugin_t *) noexcept {}

  bool init() noexcept { return host_ != nullptr; }

  bool activate(double sample_rate) noexcept {
    sample_rate_ = sample_rate;
    reset();
    return sample_rate > 1.0;
  }

  void deactivate() noexcept {}
  bool start_processing() noexcept { return true; }
  void stop_processing() noexcept {}

  void reset() noexcept {
    const double db = gain_db_.load(std::memory_order_relaxed);
    current_gain_ = db_to_gain(db);
  }

  int32_t process(const clap_process_t *process) noexcept {
    if (!process || process->audio_inputs_count == 0 || process->audio_outputs_count == 0) {
      return CLAP_PROCESS_CONTINUE;
    }

    apply_param_events(process->in_events);

    const auto &in = process->audio_inputs[0];
    auto &out = process->audio_outputs[0];
    if (!in.data32 || !out.data32) {
      return CLAP_PROCESS_CONTINUE;
    }

    const uint32_t channels = std::min(in.channel_count, out.channel_count);
    if (channels == 0) {
      return CLAP_PROCESS_CONTINUE;
    }

    const double target_gain = db_to_gain(gain_db_.load(std::memory_order_relaxed));
    const double smoothing = std::exp(-1.0 / (0.01 * sample_rate_));

    for (uint32_t i = 0; i < process->frames_count; ++i) {
      current_gain_ = target_gain + smoothing * (current_gain_ - target_gain);
      const float g = static_cast<float>(current_gain_);
      for (uint32_t ch = 0; ch < channels; ++ch) {
        out.data32[ch][i] = in.data32[ch][i] * g;
      }
    }

    return CLAP_PROCESS_CONTINUE;
  }

  void apply_param_events(const clap_input_events_t *events) noexcept {
    if (!events || !events->size || !events->get) {
      return;
    }

    const uint32_t n = events->size(events);
    for (uint32_t i = 0; i < n; ++i) {
      const clap_event_header_t *event = events->get(events, i);
      if (!event || event->type != CLAP_EVENT_PARAM_VALUE || event->size < sizeof(clap_event_param_value_t)) {
        continue;
      }
      const auto *param = reinterpret_cast<const clap_event_param_value_t *>(event);
      if (param->param_id == static_cast<int32_t>(kGainParamId)) {
        const double clamped = std::clamp(param->value, -60.0, 12.0);
        gain_db_.store(clamped, std::memory_order_relaxed);
        view_model_.set_gain_db(clamped);
      }
    }
  }

  const void *get_extension(const char *id) noexcept {
    if (!id) {
      return nullptr;
    }
    if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) {
      return &audio_ports_;
    }
    if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) {
      return &params_;
    }
    if (std::strcmp(id, CLAP_EXT_GUI) == 0) {
      return &gui_;
    }
    return nullptr;
  }

  static uint32_t audio_ports_count(const clap_plugin_t *, bool) noexcept { return 1; }

  static bool audio_ports_get(const clap_plugin_t *, uint32_t index, bool is_input, clap_audio_port_info_t *info) noexcept {
    if (!info || index != 0) {
      return false;
    }
    std::memset(info, 0, sizeof(*info));
    info->id = is_input ? 0 : 1;
    std::snprintf(info->name, sizeof(info->name), "%s", is_input ? "Input" : "Output");
    info->channel_count = 2;
    info->port_type = CLAP_PORT_STEREO;
    info->in_place_pair = 0;
    return true;
  }

  static uint32_t params_count(const clap_plugin_t *) noexcept { return 1; }

  static bool params_get_info(const clap_plugin_t *, uint32_t index, clap_param_info_t *info) noexcept {
    if (!info || index != 0) {
      return false;
    }
    std::memset(info, 0, sizeof(*info));
    info->id = kGainParamId;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE;
    std::snprintf(info->name, sizeof(info->name), "Gain");
    info->min_value = -60.0;
    info->max_value = 12.0;
    info->default_value = 0.0;
    return true;
  }

  static bool params_get_value(const clap_plugin_t *plugin, uint32_t param_id, double *value) noexcept {
    if (!value || param_id != kGainParamId) {
      return false;
    }
    *value = self(plugin)->gain_db_.load(std::memory_order_relaxed);
    return true;
  }

  static bool params_value_to_text(const clap_plugin_t *, uint32_t param_id, double value, char *display, uint32_t size) noexcept {
    if (!display || size == 0 || param_id != kGainParamId) {
      return false;
    }
    std::snprintf(display, size, "%.2f dB", value);
    return true;
  }

  static bool params_text_to_value(const clap_plugin_t *, uint32_t param_id, const char *display, double *value) noexcept {
    if (!display || !value || param_id != kGainParamId) {
      return false;
    }
    *value = std::clamp(std::strtod(display, nullptr), -60.0, 12.0);
    return true;
  }

  static void params_flush(const clap_plugin_t *plugin, const clap_input_events_t *in, const clap_output_events_t *) noexcept {
    self(plugin)->apply_param_events(in);
  }

  static bool gui_is_api_supported(const clap_plugin_t *, const char *api, bool is_floating) noexcept {
    return is_floating && api && std::strcmp(api, "x11") == 0;
  }

  static bool gui_create(const clap_plugin_t *plugin, const char *api, bool is_floating) noexcept {
    auto *p = self(plugin);
    if (!gui_is_api_supported(plugin, api, is_floating)) {
      return false;
    }
    return p->gui_context_.create();
  }

  static void gui_destroy(const clap_plugin_t *plugin) noexcept { self(plugin)->gui_context_.destroy(); }
  static bool gui_set_scale(const clap_plugin_t *, double) noexcept { return true; }

  static bool gui_get_size(const clap_plugin_t *, uint32_t *width, uint32_t *height) noexcept {
    if (!width || !height) {
      return false;
    }
    *width = 400;
    *height = 120;
    return true;
  }

  static bool gui_can_resize(const clap_plugin_t *) noexcept { return false; }
  static bool gui_get_resize_hints(const clap_plugin_t *, void *) noexcept { return false; }
  static bool gui_adjust_size(const clap_plugin_t *, uint32_t *width, uint32_t *height) noexcept {
    return gui_get_size(nullptr, width, height);
  }
  static bool gui_set_size(const clap_plugin_t *, uint32_t, uint32_t) noexcept { return true; }
  static bool gui_set_parent(const clap_plugin_t *, const clap_window_t *) noexcept { return true; }
  static bool gui_set_transient(const clap_plugin_t *, const clap_window_t *) noexcept { return true; }
  static void gui_suggest_title(const clap_plugin_t *, const char *) noexcept {}
  static bool gui_show(const clap_plugin_t *plugin) noexcept { return self(plugin)->gui_context_.show(); }
  static bool gui_hide(const clap_plugin_t *plugin) noexcept { return self(plugin)->gui_context_.hide(); }

  const clap_host_t *host_{};
  clap_plugin_t clap_plugin_{};
  std::atomic<double> gain_db_{0.0};
  double sample_rate_{48000.0};
  double current_gain_{1.0};
  ikigui::GainViewModel view_model_{};
  ikigui::GuiContext gui_context_{};

  const clap_plugin_audio_ports_t audio_ports_{
      .count = &audio_ports_count,
      .get = &audio_ports_get,
  };

  const clap_plugin_params_t params_{
      .count = &params_count,
      .get_info = &params_get_info,
      .get_value = &params_get_value,
      .value_to_text = &params_value_to_text,
      .text_to_value = &params_text_to_value,
      .flush = &params_flush,
  };

  const clap_plugin_gui_t gui_{
      .is_api_supported = &gui_is_api_supported,
      .create = &gui_create,
      .destroy = &gui_destroy,
      .set_scale = &gui_set_scale,
      .get_size = &gui_get_size,
      .can_resize = &gui_can_resize,
      .get_resize_hints = &gui_get_resize_hints,
      .adjust_size = &gui_adjust_size,
      .set_size = &gui_set_size,
      .set_parent = &gui_set_parent,
      .set_transient = &gui_set_transient,
      .suggest_title = &gui_suggest_title,
      .show = &gui_show,
      .hide = &gui_hide,
  };

  static double db_to_gain(double db) noexcept { return std::pow(10.0, db / 20.0); }
};

uint32_t factory_get_plugin_count(const clap_plugin_factory_t *) noexcept { return 1; }

const clap_plugin_descriptor_t *factory_get_plugin_descriptor(const clap_plugin_factory_t *, uint32_t index) noexcept {
  return (index == 0) ? &kDescriptor : nullptr;
}

const clap_plugin_t *factory_create_plugin(const clap_plugin_factory_t *, const clap_host_t *host, const char *plugin_id) noexcept {
  if (!plugin_id || std::strcmp(plugin_id, kPluginId) != 0) {
    return nullptr;
  }

  auto *plugin = new (std::nothrow) GainPlugin(host);
  if (!plugin) {
    return nullptr;
  }

  return plugin->plugin();
}

const clap_plugin_factory_t kFactory{
    .get_plugin_count = &factory_get_plugin_count,
    .get_plugin_descriptor = &factory_get_plugin_descriptor,
    .create_plugin = &factory_create_plugin,
};

bool entry_init(const char *) noexcept { return true; }
void entry_deinit() noexcept {}

const void *entry_get_factory(const char *factory_id) noexcept {
  if (factory_id && std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {
    return &kFactory;
  }
  return nullptr;
}

}  // namespace

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry{
    .clap_version = CLAP_VERSION,
    .init = &entry_init,
    .deinit = &entry_deinit,
    .get_factory = &entry_get_factory,
};
