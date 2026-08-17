using System;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;
using VehiclePhysics;

[RequireComponent(typeof(AudioSource))]
public sealed class EngineSimNative : MonoBehaviour
{
    private const string Library = "engine_sim_unity";

    [DllImport(Library)] private static extern IntPtr es_engine_create();
    [DllImport(Library)] private static extern void es_engine_destroy(IntPtr handle);
    [DllImport(Library)] private static extern int es_engine_load(
        IntPtr handle, string scriptPath, string assetRoot, int audioSampleRate);
    [DllImport(Library)] private static extern IntPtr es_engine_get_last_error(IntPtr handle);
    [DllImport(Library)] private static extern void es_engine_set_throttle(IntPtr handle, float value);
    [DllImport(Library)] private static extern void es_engine_set_ignition(IntPtr handle, int enabled);
    [DllImport(Library)] private static extern void es_engine_set_starter(IntPtr handle, int enabled);
    [DllImport(Library)] private static extern void es_engine_set_clutch(IntPtr handle, float pressure);
    [DllImport(Library)] private static extern void es_engine_set_gear(IntPtr handle, int gear);
    [DllImport(Library)] private static extern void es_engine_set_rpm(IntPtr handle, float rpm);
    [DllImport(Library)] private static extern int es_engine_update(IntPtr handle, double dt);
    [DllImport(Library)] private static extern int es_engine_render_audio(
        IntPtr handle, [Out] float[] samples, int frames, int channels);
    [DllImport(Library)] private static extern float es_engine_get_rpm(IntPtr handle);

    [Range(0, 1)] public float throttle;
    public bool ignition = true;
    public bool starter;
    public bool autoStart = true;
    [Min(0)] public float autoStartDuration = 1.25f;
    [Range(0, 1)] public float clutchPressure;
    public int gear;
    [Header("Vehicle Physics Pro")]
    public VehicleBase vehicle;
    public bool synchronizeVppRpm = true;
    public bool synchronizeVppThrottle = true;
    public string mainScript = "assets/main.mr";

    public float Rpm => handle == IntPtr.Zero ? 0 : es_engine_get_rpm(handle);

    private IntPtr handle;
    private AudioSource audioSource;
    private AudioClip carrierClip;
    private float autoStartRemaining;

    private void Awake()
    {
        handle = es_engine_create();
        string root = Application.streamingAssetsPath;
        string script = Path.Combine(root, mainScript);
        if (handle == IntPtr.Zero || es_engine_load(
                handle, script, root, AudioSettings.outputSampleRate) == 0)
        {
            string error = handle == IntPtr.Zero
                ? "Could not create native engine"
                : Marshal.PtrToStringAnsi(es_engine_get_last_error(handle));
            Debug.LogError("Engine Sim: " + error, this);
            enabled = false;
            return;
        }

        // OnAudioFilterRead is only called continuously while the AudioSource
        // is playing. Use a silent looping clip as the carrier that the native
        // engine output replaces in OnAudioFilterRead.
        audioSource = GetComponent<AudioSource>();
        int sampleRate = AudioSettings.outputSampleRate;
        carrierClip = AudioClip.Create("Engine Sim Native Carrier", sampleRate,
            1, sampleRate, false);
        audioSource.clip = carrierClip;
        audioSource.loop = true;
        audioSource.playOnAwake = false;
        audioSource.Play();
        autoStartRemaining = autoStart ? autoStartDuration : 0;
        if (vehicle == null) vehicle = FindFirstObjectByType<VehicleBase>();
    }

    private void Update()
    {
        if (handle == IntPtr.Zero) return;
        float nativeThrottle = throttle;
        if (vehicle != null && vehicle.isActiveAndEnabled)
        {
            if (synchronizeVppThrottle)
                nativeThrottle = Mathf.Clamp01(
                    vehicle.data.Get(Channel.Input, InputData.Throttle) / 10000.0f);
            if (synchronizeVppRpm)
            {
                float vppRpm = Mathf.Max(0,
                    vehicle.data.Get(Channel.Vehicle, VehicleData.EngineRpm) / 1000.0f);
                es_engine_set_rpm(handle, vppRpm);
            }
        }
        es_engine_set_throttle(handle, nativeThrottle);
        es_engine_set_ignition(handle, ignition ? 1 : 0);
        bool starterActive = starter || autoStartRemaining > 0;
        es_engine_set_starter(handle, starterActive ? 1 : 0);
        es_engine_set_clutch(handle, clutchPressure);
        es_engine_set_gear(handle, gear);
        es_engine_update(handle, Time.deltaTime);
        autoStartRemaining = Mathf.Max(0, autoStartRemaining - Time.unscaledDeltaTime);
    }

    private void OnAudioFilterRead(float[] data, int channels)
    {
        if (handle != IntPtr.Zero)
            es_engine_render_audio(handle, data, data.Length / channels, channels);
        else
            Array.Clear(data, 0, data.Length);
    }

    private void OnDestroy()
    {
        if (handle == IntPtr.Zero) return;
        if (audioSource != null) audioSource.Stop();
        IntPtr oldHandle = handle;
        handle = IntPtr.Zero;
        es_engine_destroy(oldHandle);
        if (carrierClip != null) Destroy(carrierClip);
    }
}
