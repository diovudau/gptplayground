#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLAP_VERSION_MAJOR 1
#define CLAP_VERSION_MINOR 2
#define CLAP_VERSION_REVISION 0

typedef struct clap_version {
  uint32_t major;
  uint32_t minor;
  uint32_t revision;
} clap_version_t;

static const clap_version_t CLAP_VERSION = {CLAP_VERSION_MAJOR, CLAP_VERSION_MINOR, CLAP_VERSION_REVISION};

#define CLAP_PLUGIN_FACTORY_ID "clap.plugin-factory"
#define CLAP_EXT_AUDIO_PORTS "clap.audio-ports"
#define CLAP_EXT_PARAMS "clap.params"
#define CLAP_EXT_GUI "clap.gui"

#define CLAP_PORT_STEREO "stereo"

#define CLAP_PARAM_IS_AUTOMATABLE (1u << 0)

#define CLAP_EVENT_PARAM_VALUE 5

#define CLAP_PROCESS_CONTINUE 0

typedef struct clap_host clap_host_t;
typedef struct clap_plugin clap_plugin_t;
typedef struct clap_event_transport clap_event_transport_t;

typedef struct clap_audio_buffer {
  float **data32;
  double **data64;
  uint32_t channel_count;
  uint32_t latency;
  uint64_t constant_mask;
} clap_audio_buffer_t;

typedef struct clap_input_events {
  uint32_t (*size)(const struct clap_input_events *list);
  const struct clap_event_header *(*get)(const struct clap_input_events *list, uint32_t index);
} clap_input_events_t;

typedef struct clap_output_events {
  bool (*try_push)(const struct clap_output_events *list, const struct clap_event_header *event);
} clap_output_events_t;

typedef struct clap_process {
  int64_t steady_time;
  uint32_t frames_count;
  const clap_event_transport_t *transport;
  const clap_audio_buffer_t *audio_inputs;
  clap_audio_buffer_t *audio_outputs;
  uint32_t audio_inputs_count;
  uint32_t audio_outputs_count;
  const clap_input_events_t *in_events;
  const clap_output_events_t *out_events;
} clap_process_t;

typedef struct clap_plugin_descriptor {
  clap_version_t clap_version;
  const char *id;
  const char *name;
  const char *vendor;
  const char *url;
  const char *manual_url;
  const char *support_url;
  const char *version;
  const char *description;
  const char *const *features;
} clap_plugin_descriptor_t;

typedef struct clap_plugin {
  const clap_plugin_descriptor_t *desc;
  void *plugin_data;
  bool (*init)(const clap_plugin_t *plugin);
  void (*destroy)(const clap_plugin_t *plugin);
  bool (*activate)(const clap_plugin_t *plugin, double sample_rate, uint32_t min_frames_count, uint32_t max_frames_count);
  void (*deactivate)(const clap_plugin_t *plugin);
  bool (*start_processing)(const clap_plugin_t *plugin);
  void (*stop_processing)(const clap_plugin_t *plugin);
  void (*reset)(const clap_plugin_t *plugin);
  int32_t (*process)(const clap_plugin_t *plugin, const clap_process_t *process);
  const void *(*get_extension)(const clap_plugin_t *plugin, const char *id);
  void (*on_main_thread)(const clap_plugin_t *plugin);
} clap_plugin_t;

typedef struct clap_plugin_factory {
  uint32_t (*get_plugin_count)(const struct clap_plugin_factory *factory);
  const clap_plugin_descriptor_t *(*get_plugin_descriptor)(const struct clap_plugin_factory *factory, uint32_t index);
  const clap_plugin_t *(*create_plugin)(const struct clap_plugin_factory *factory, const clap_host_t *host, const char *plugin_id);
} clap_plugin_factory_t;

typedef struct clap_plugin_entry {
  clap_version_t clap_version;
  bool (*init)(const char *plugin_path);
  void (*deinit)(void);
  const void *(*get_factory)(const char *factory_id);
} clap_plugin_entry_t;

typedef struct clap_host {
  clap_version_t clap_version;
  const char *name;
  const char *vendor;
  const char *url;
  const char *version;
  void *host_data;
  const void *(*get_extension)(const clap_host_t *host, const char *extension_id);
  void (*request_restart)(const clap_host_t *host);
  void (*request_process)(const clap_host_t *host);
  void (*request_callback)(const clap_host_t *host);
} clap_host_t;

typedef struct clap_event_header {
  uint32_t size;
  uint16_t time;
  uint16_t space_id;
  uint16_t type;
  uint16_t flags;
} clap_event_header_t;

typedef struct clap_event_param_value {
  clap_event_header_t header;
  int32_t param_id;
  int32_t cookie;
  int32_t note_id;
  int16_t port_index;
  int16_t channel;
  int16_t key;
  int16_t reserved;
  double value;
} clap_event_param_value_t;

typedef struct clap_param_info {
  uint32_t id;
  uint32_t flags;
  void *cookie;
  char name[256];
  char module[1024];
  double min_value;
  double max_value;
  double default_value;
} clap_param_info_t;

typedef struct clap_audio_port_info {
  uint32_t id;
  char name[256];
  uint32_t flags;
  uint32_t channel_count;
  const char *port_type;
  uint32_t in_place_pair;
} clap_audio_port_info_t;

typedef struct clap_plugin_audio_ports {
  uint32_t (*count)(const clap_plugin_t *plugin, bool is_input);
  bool (*get)(const clap_plugin_t *plugin, uint32_t index, bool is_input, clap_audio_port_info_t *info);
} clap_plugin_audio_ports_t;

typedef struct clap_plugin_params {
  uint32_t (*count)(const clap_plugin_t *plugin);
  bool (*get_info)(const clap_plugin_t *plugin, uint32_t index, clap_param_info_t *param_info);
  bool (*get_value)(const clap_plugin_t *plugin, uint32_t param_id, double *value);
  bool (*value_to_text)(const clap_plugin_t *plugin, uint32_t param_id, double value, char *display, uint32_t size);
  bool (*text_to_value)(const clap_plugin_t *plugin, uint32_t param_id, const char *display, double *value);
  void (*flush)(const clap_plugin_t *plugin, const clap_input_events_t *in, const clap_output_events_t *out);
} clap_plugin_params_t;

typedef struct clap_window {
  const char *api;
  void *ptr;
} clap_window_t;

typedef struct clap_plugin_gui {
  bool (*is_api_supported)(const clap_plugin_t *plugin, const char *api, bool is_floating);
  bool (*create)(const clap_plugin_t *plugin, const char *api, bool is_floating);
  void (*destroy)(const clap_plugin_t *plugin);
  bool (*set_scale)(const clap_plugin_t *plugin, double scale);
  bool (*get_size)(const clap_plugin_t *plugin, uint32_t *width, uint32_t *height);
  bool (*can_resize)(const clap_plugin_t *plugin);
  bool (*get_resize_hints)(const clap_plugin_t *plugin, void *hints);
  bool (*adjust_size)(const clap_plugin_t *plugin, uint32_t *width, uint32_t *height);
  bool (*set_size)(const clap_plugin_t *plugin, uint32_t width, uint32_t height);
  bool (*set_parent)(const clap_plugin_t *plugin, const clap_window_t *window);
  bool (*set_transient)(const clap_plugin_t *plugin, const clap_window_t *window);
  void (*suggest_title)(const clap_plugin_t *plugin, const char *title);
  bool (*show)(const clap_plugin_t *plugin);
  bool (*hide)(const clap_plugin_t *plugin);
} clap_plugin_gui_t;

#define CLAP_EXPORT __attribute__((visibility("default")))

#ifdef __cplusplus
}
#endif
