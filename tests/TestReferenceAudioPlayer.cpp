// ═══════════════════════════════════════════════════════════════════════════
//  TestReferenceAudioPlayer.cpp — Unit test para ReferenceAudioPlayer
//  Verifica carga estéreo, mono, y reproducción correcta.
//
//  Compilado via CMake (ver CMakeLists.txt):
//    cmake --build build --config Release --target TestReferenceAudioPlayer
//    ./build/tests/Release/TestReferenceAudioPlayer.exe
//
//  Dependencias: PluginProcessor.h (ReferenceAudioPlayer)
//                juce_audio_basics, juce_audio_formats
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstdint>

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

// Incluimos ReferenceAudioPlayer.h directamente (header autónomo).
// NO incluimos PluginProcessor.h — evitamos sus dependencias pesadas
// (SharedData, SlotRegistry, PhaseManager, etc.) que requieren linkear
// todo el plugin.
#include "MixCoach/core/ReferenceAudioPlayer.h"

static int gTestsPassed = 0;
static int gTestsFailed = 0;

#define TEST(name, expr) do {                                                  \
    if (!(expr)) {                                                             \
        std::fprintf(stderr, "  \xe2\x9d\x8c FAIL: %s (%s:%d)\n",              \
                     name, __FILE__, __LINE__);                                \
        std::fflush(stderr);                                                   \
        gTestsFailed++;                                                        \
    } else {                                                                   \
        std::printf("  \xe2\x9c\x85 PASS: %s\n", name);                        \
        std::fflush(stdout);                                                   \
        gTestsPassed++;                                                        \
    }                                                                          \
} while(0)

#define TEST_NEAR(name, a, b, eps) TEST(name, std::fabs((a) - (b)) < (eps))

// ─── Helper: crear archivo WAV temporal ──────────────────────────────────
// Crea un WAV con las caracteristicas especificadas y retorna la ruta.
#pragma warning(push)
#pragma warning(disable : 4996) // 'createWriterFor' deprecated in JUCE 8
static juce::File createTestWav(const juce::String& name,
                                 int numChannels,
                                 float freqHzL,
                                 float freqHzR,
                                 float amplitudeL,
                                 float amplitudeR,
                                 int numSamples = 44100,
                                 double sampleRate = 44100.0)
{
    juce::WavAudioFormat wavFormat;

    // Limpiar archivo temporal previo si existe
    auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getChildFile(name);
    if (tempFile.existsAsFile())
        tempFile.deleteFile();

    auto* writer = wavFormat.createWriterFor(
        std::make_unique<juce::FileOutputStream>(tempFile).release(),
        sampleRate, numChannels, 16, {}, 0);

    if (writer == nullptr)
        return {};

    juce::AudioBuffer<float> buffer(numChannels, numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        float freq = (ch == 0) ? freqHzL : freqHzR;
        float amp  = (ch == 0) ? amplitudeL : amplitudeR;
        for (int i = 0; i < numSamples; ++i)
        {
            float t = (float)i / (float)sampleRate;
            data[i] = std::sin(2.0f * juce::MathConstants<float>::pi * freq * t) * amp;
        }
    }

    writer->writeFromAudioSampleBuffer(buffer, 0, numSamples);
    delete writer;

    return tempFile;
}
#pragma warning(pop) // Restore C4996 warning

// ─── Helper: limpiar archivo temporal ───────────────────────────────────
static void cleanupFile(const juce::File& f)
{
    if (f.existsAsFile())
        f.deleteFile();
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 1: Carga de archivo estéreo — verificar que L y R tienen datos
//          DIFERENTES (frecuencias distintas por canal)
// ═══════════════════════════════════════════════════════════════════════════
static void test_stereo_file_load()
{
    std::printf("\n── Test 1: Stereo File Load ──\n");
    std::fflush(stdout);

    // Crear WAV estéreo: L=440Hz, R=880Hz con mismas amplitudes
    auto wavFile = createTestWav("test_ref_stereo.wav", 2,
                                  440.0f, 880.0f,  // L=440Hz, R=880Hz
                                  0.5f, 0.5f,       // misma amplitud
                                  44100, 44100.0);
    TEST("Stereo WAV creado", wavFile.existsAsFile());
    if (!wavFile.existsAsFile()) return;

    mixcoach::ReferenceAudioPlayer player;
    player.prepare(44100.0, 512);

    // Cargar y reproducir
    player.playFile(wavFile.getFullPathName());
    TEST("Stereo: isPlaying after playFile()", player.isPlaying());

    // Leer 1024 samples en buffer estéreo
    juce::AudioBuffer<float> output(2, 1024);
    output.clear();
    player.mixIntoBuffer(output, 1.0f);

    auto* left  = output.getReadPointer(0);
    auto* right = output.getReadPointer(1);

    // Verificar que L y R tienen datos
    bool leftHasSignal = false, rightHasSignal = false;
    for (int i = 0; i < 1024; ++i)
    {
        if (std::abs(left[i]) > 0.001f)  leftHasSignal = true;
        if (std::abs(right[i]) > 0.001f) rightHasSignal = true;
    }
    TEST("Stereo: L channel has signal", leftHasSignal);
    TEST("Stereo: R channel has signal", rightHasSignal);

    // ═══ VERIFICACIÓN CLAVE: L y R deben ser DIFERENTES ═══════════════
    // 440Hz y 880Hz producen formas de onda diferentes.
    bool channelsDiffer = false;
    for (int i = 0; i < 1024; ++i)
    {
        if (std::abs(left[i] - right[i]) > 0.01f)
        {
            channelsDiffer = true;
            break;
        }
    }
    TEST("Stereo: L and R channels have DIFFERENT data (fixed bug)", channelsDiffer);

    // Verificar posición de reproducción
    TEST("Stereo: position > 0 after mixIntoBuffer",
         player.getPosition() > 0.0);
    TEST("Stereo: length > 0",
         player.getLength() > 0.0);

    // Verificar que avanza: mixIntoBuffer otra vez, posición debe crecer
    double pos1 = player.getPosition();
    player.mixIntoBuffer(output, 1.0f);
    double pos2 = player.getPosition();
    TEST("Stereo: position advances after mixing", pos2 > pos1);

    player.stop();
    cleanupFile(wavFile);
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 2: Carga de archivo mono — verificar que L y R tienen datos
//          IGUALES (mono se copia a ambos canales)
// ═══════════════════════════════════════════════════════════════════════════
static void test_mono_file_load()
{
    std::printf("\n── Test 2: Mono File Load ──\n");
    std::fflush(stdout);

    // Crear WAV mono: 440Hz
    auto wavFile = createTestWav("test_ref_mono.wav", 1,
                                  440.0f, 440.0f,  // mismo tono (mono = 1 canal)
                                  0.5f, 0.5f,
                                  44100, 44100.0);
    TEST("Mono WAV creado", wavFile.existsAsFile());
    if (!wavFile.existsAsFile()) return;

    mixcoach::ReferenceAudioPlayer player;
    player.prepare(44100.0, 512);

    player.playFile(wavFile.getFullPathName());
    TEST("Mono: isPlaying after playFile()", player.isPlaying());

    // Leer 1024 samples
    juce::AudioBuffer<float> output(2, 1024);
    output.clear();
    player.mixIntoBuffer(output, 1.0f);

    auto* left  = output.getReadPointer(0);
    auto* right = output.getReadPointer(1);

    // Verificar que ambos canales tienen señal
    bool leftHasSignal = false, rightHasSignal = false;
    for (int i = 0; i < 1024; ++i)
    {
        if (std::abs(left[i]) > 0.001f)  leftHasSignal = true;
        if (std::abs(right[i]) > 0.001f) rightHasSignal = true;
    }
    TEST("Mono: L channel has signal", leftHasSignal);
    TEST("Mono: R channel has signal", rightHasSignal);

    // ═══ VERIFICACIÓN CLAVE: L y R deben ser IDÉNTICOS ════════════════
    // (mono → copy L to R)
    bool channelsMatch = true;
    for (int i = 0; i < 1024; ++i)
    {
        if (std::abs(left[i] - right[i]) > 0.0001f)
        {
            channelsMatch = false;
            break;
        }
    }
    TEST("Mono: L and R channels are IDENTICAL (mono copy)", channelsMatch);

    player.stop();
    cleanupFile(wavFile);
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 3: Play/Pause/Stop lifecycle
// ═══════════════════════════════════════════════════════════════════════════
static void test_playback_lifecycle()
{
    std::printf("\n── Test 3: Playback Lifecycle ──\n");
    std::fflush(stdout);

    auto wavFile = createTestWav("test_ref_lifecycle.wav", 2,
                                  220.0f, 220.0f,
                                  0.3f, 0.3f,
                                  44100 * 2, // 2 segundos
                                  44100.0);
    TEST("Lifecycle WAV creado", wavFile.existsAsFile());
    if (!wavFile.existsAsFile()) return;

    mixcoach::ReferenceAudioPlayer player;
    player.prepare(44100.0, 512);

    // --- Play ---
    player.playFile(wavFile.getFullPathName());
    TEST("Lifecycle: isPlaying true after play", player.isPlaying());

    // --- Pause ---
    player.pause();
    TEST("Lifecycle: isPlaying false after pause", !player.isPlaying());

    // --- Play again (reanudar) ---
    player.playFile(wavFile.getFullPathName());
    TEST("Lifecycle: isPlaying true after re-play", player.isPlaying());

    // --- Stop ---
    player.stop();
    TEST("Lifecycle: isPlaying false after stop", !player.isPlaying());
    TEST("Lifecycle: position 0 after stop",
         player.getPosition() < 0.001);

    player.stop();
    cleanupFile(wavFile);
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 4: Seek / SetPosition
// ═══════════════════════════════════════════════════════════════════════════
static void test_seek_position()
{
    std::printf("\n── Test 4: Seek / SetPosition ──\n");
    std::fflush(stdout);

    auto wavFile = createTestWav("test_ref_seek.wav", 2,
                                  440.0f, 440.0f,
                                  0.5f, 0.5f,
                                  44100 * 3, // 3 segundos
                                  44100.0);
    TEST("Seek WAV creado", wavFile.existsAsFile());
    if (!wavFile.existsAsFile()) return;

    mixcoach::ReferenceAudioPlayer player;
    player.prepare(44100.0, 512);

    player.playFile(wavFile.getFullPathName());
    TEST("Seek: length ~3s",
         std::fabs(player.getLength() - 3.0) < 0.1);

    // Seek al segundo 1.0
    player.setPosition(1.0);
    double pos = player.getPosition();
    TEST_NEAR("Seek: position ~1.0s after setPosition(1.0)",
              pos, 1.0, 0.05);

    // Seek al inicio
    player.setPosition(0.0);
    pos = player.getPosition();
    TEST_NEAR("Seek: position ~0.0s after setPosition(0.0)",
              pos, 0.0, 0.05);

    player.stop();
    cleanupFile(wavFile);
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 5: Volumen (gain)
// ═══════════════════════════════════════════════════════════════════════════
static void test_volume_control()
{
    std::printf("\n── Test 5: Volume Control ──\n");
    std::fflush(stdout);

    auto wavFile = createTestWav("test_ref_vol.wav", 2,
                                  440.0f, 440.0f,
                                  0.5f, 0.5f,
                                  44100, 44100.0);
    TEST("Volume WAV creado", wavFile.existsAsFile());
    if (!wavFile.existsAsFile()) return;

    mixcoach::ReferenceAudioPlayer player;
    player.prepare(44100.0, 512);

    player.playFile(wavFile.getFullPathName());

    // Mix con volumen 1.0
    juce::AudioBuffer<float> out1(2, 512);
    out1.clear();
    player.mixIntoBuffer(out1, 1.0f);

    // Reset y mix con volumen 0.5
    // Nota: playFile() arranca desde el inicio. Pero como mixIntoBuffer
    // avanza readIndex_, necesitamos un nuevo playFile para resetear.
    player.stop();
    player.playFile(wavFile.getFullPathName());
    juce::AudioBuffer<float> out2(2, 512);
    out2.clear();
    player.mixIntoBuffer(out2, 0.5f);

    // Comparar amplitudes: out1 debe ser ~2x out2
    double sum1 = 0.0, sum2 = 0.0;
    for (int i = 0; i < 512; ++i)
    {
        sum1 += std::abs(out1.getSample(0, i));
        sum2 += std::abs(out2.getSample(0, i));
    }
    double ratio = (sum2 > 0.0001) ? (sum1 / sum2) : 0.0;
    std::printf("  Volume ratio (1.0/0.5): %.2f (expected ~2.0)\n", ratio);
    TEST_NEAR("Volume: gain=1.0 is ~2x gain=0.5", ratio, 2.0, 0.3);

    player.stop();
    cleanupFile(wavFile);
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 6: Carga de archivo inexistente (no debe crashear)
// ═══════════════════════════════════════════════════════════════════════════
static void test_file_not_found()
{
    std::printf("\n── Test 6: File Not Found ──\n");
    std::fflush(stdout);

    mixcoach::ReferenceAudioPlayer player;
    player.prepare(44100.0, 512);

    // playFile con archivo inexistente: no debe crashear ni lanzar excepción
    player.playFile("C:/ruta_inexistente/archivo_que_no_existe.wav");

    // No deberia estar playing
    TEST("Not found: isPlaying false", !player.isPlaying());

    // mixIntoBuffer con buffer vacio: no debe crashear
    juce::AudioBuffer<float> output(2, 256);
    output.clear();
    player.mixIntoBuffer(output, 1.0f); // No debe crashear

    TEST("Not found: mixIntoBuffer completed safely", true);
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 7: Varios archivos consecutivos (prevenir memory leaks)
// ═══════════════════════════════════════════════════════════════════════════
static void test_multiple_loads()
{
    std::printf("\n── Test 7: Multiple Consecutive Loads ──\n");
    std::fflush(stdout);

    // Crear varios archivos de prueba
    auto wav1 = createTestWav("test_ref_mult1.wav", 2, 220.0f, 220.0f, 0.3f, 0.3f, 44100, 44100.0);
    auto wav2 = createTestWav("test_ref_mult2.wav", 2, 440.0f, 440.0f, 0.5f, 0.5f, 44100, 44100.0);
    auto wav3 = createTestWav("test_ref_mult3.wav", 1, 880.0f, 880.0f, 0.4f, 0.4f, 44100, 44100.0);

    TEST("Multi: archivos creados", wav1.existsAsFile() && wav2.existsAsFile() && wav3.existsAsFile());
    if (!wav1.existsAsFile() || !wav2.existsAsFile() || !wav3.existsAsFile())
    {
        cleanupFile(wav1);
        cleanupFile(wav2);
        cleanupFile(wav3);
        return;
    }

    mixcoach::ReferenceAudioPlayer player;
    player.prepare(44100.0, 512);

    // Cargar y reproducir 3 archivos consecutivamente
    for (int iter = 0; iter < 3; ++iter)
    {
        player.playFile(wav1.getFullPathName());
        TEST("Multi: isPlaying after load1", player.isPlaying());

        juce::AudioBuffer<float> buf(2, 256);
        buf.clear();
        player.mixIntoBuffer(buf, 1.0f);

        player.playFile(wav2.getFullPathName());
        TEST("Multi: isPlaying after load2", player.isPlaying());

        buf.clear();
        player.mixIntoBuffer(buf, 1.0f);

        player.playFile(wav3.getFullPathName());
        TEST("Multi: isPlaying after load3 (mono)", player.isPlaying());

        buf.clear();
        player.mixIntoBuffer(buf, 1.0f);
    }

    TEST("Multi: loaded 3 files 3 times without crash", true);

    player.stop();
    cleanupFile(wav1);
    cleanupFile(wav2);
    cleanupFile(wav3);
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 8: Progress reporting
// ═══════════════════════════════════════════════════════════════════════════
static void test_progress()
{
    std::printf("\n── Test 8: Progress ──\n");
    std::fflush(stdout);

    // Archivo corto de 0.5s
    auto wavFile = createTestWav("test_ref_progress.wav", 2,
                                  440.0f, 440.0f,
                                  0.5f, 0.5f,
                                  22050, // 0.5s a 44100
                                  44100.0);
    TEST("Progress WAV creado", wavFile.existsAsFile());
    if (!wavFile.existsAsFile()) return;

    mixcoach::ReferenceAudioPlayer player;
    player.prepare(44100.0, 512);

    player.playFile(wavFile.getFullPathName());
    double len = player.getLength();
    double progress = player.getProgress();
    TEST_NEAR("Progress: initial progress ~0", progress, 0.0, 0.01);
    TEST_NEAR("Progress: length ~0.5s", len, 0.5, 0.05);

    // Mix algunas muestras
    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    for (int i = 0; i < 20; ++i)
        player.mixIntoBuffer(buf, 1.0f);

    progress = player.getProgress();
    TEST("Progress: progress > 0 after mixing", progress > 0.0);
    TEST("Progress: progress < 1.0 (not finished)", progress < 1.0);

    player.stop();
    cleanupFile(wavFile);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════
int main()
{
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  ReferenceAudioPlayer Unit Tests\n");
    std::printf("  Stereo/Mono/Playback verification\n");
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n\n");
    std::fflush(stdout);

    test_stereo_file_load();
    test_mono_file_load();
    test_playback_lifecycle();
    test_seek_position();
    test_volume_control();
    test_file_not_found();
    test_multiple_loads();
    test_progress();

    std::printf("\n\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::fflush(stdout);

    return gTestsFailed > 0 ? 1 : 0;
}
