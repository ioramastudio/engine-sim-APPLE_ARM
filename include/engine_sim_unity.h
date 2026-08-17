#ifndef ATG_ENGINE_SIM_UNITY_H
#define ATG_ENGINE_SIM_UNITY_H

#include <stdint.h>

#if defined(_WIN32)
#define ES_UNITY_API __declspec(dllexport)
#else
#define ES_UNITY_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void *es_engine_handle;

ES_UNITY_API es_engine_handle es_engine_create(void);
ES_UNITY_API void es_engine_destroy(es_engine_handle handle);

/* asset_root is the directory containing the es/, assets/, and sound-library data. */
ES_UNITY_API int es_engine_load(es_engine_handle handle, const char *script_path,
    const char *asset_root, int audio_sample_rate);
ES_UNITY_API const char *es_engine_get_last_error(es_engine_handle handle);

ES_UNITY_API void es_engine_set_throttle(es_engine_handle handle, float throttle);
ES_UNITY_API void es_engine_set_ignition(es_engine_handle handle, int enabled);
ES_UNITY_API void es_engine_set_starter(es_engine_handle handle, int enabled);
ES_UNITY_API void es_engine_set_clutch(es_engine_handle handle, float pressure);
ES_UNITY_API void es_engine_set_gear(es_engine_handle handle, int gear);
/* Pin engine-sim crank speed to an authoritative gameplay RPM. */
ES_UNITY_API void es_engine_set_rpm(es_engine_handle handle, float rpm);

/* Call from Unity's Update. dt is seconds and is clamped to a safe range. */
ES_UNITY_API int es_engine_update(es_engine_handle handle, double dt);

/* Call from OnAudioFilterRead. Mono native audio is duplicated to every channel. */
ES_UNITY_API int es_engine_render_audio(es_engine_handle handle, float *interleaved,
    int frames, int channels);

ES_UNITY_API float es_engine_get_rpm(es_engine_handle handle);
ES_UNITY_API float es_engine_get_torque(es_engine_handle handle);
ES_UNITY_API float es_engine_get_power(es_engine_handle handle);

#ifdef __cplusplus
}
#endif

#endif
