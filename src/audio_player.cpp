// ============================================================================
// audio_player.cpp
//  WAV/MP3 playback, pause, next, volume
// ============================================================================

#include <SD_MMC.h>
#include "driver/i2s.h"
#include "../_official_3p5_demo/Arduino/libraries/es8311/es8311.h"
#include "../_official_3p5_demo/Arduino/libraries/es8311/es8311.cpp"
#include "audio_player.h"

#define I2S_MCLK 12
#define I2S_BCLK 13
#define I2S_LRCK 15
#define I2S_SDOUT 16

#define AUDIO_MCLK_MULTIPLE 256
#define AUDIO_VOLUME 70

struct WavInfo
{
    // Stuff we pull out of the WAV header before audio starts.
    // The actual sound bytes live later in the file, after the header chunks.
    uint32_t sample_rate = 0;
    uint32_t data_offset = 0;
    uint32_t data_size = 0;
    uint16_t channels = 0;
    uint16_t bits_per_sample = 0;
};

static File wav_file;
static WavInfo current_wav;
static bool audio_playing = false;
static uint32_t bytes_played = 0;
static const char *last_error = "";

static uint16_t read_u16(File &file)
{
    // WAV files store numbers little-endian, meaning the smallest byte comes first.
    // So two bytes like [0x44, 0xAC] become 0xAC44.
    uint8_t b[2];
    if (file.read(b, sizeof(b)) != sizeof(b))
    {
        return 0;
    }
    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

static uint32_t read_u32(File &file)
{
    // Same little-endian idea as read_u16(), but for four-byte numbers.
    // WAV chunk sizes and sample rates use this a lot.
    uint8_t b[4];
    if (file.read(b, sizeof(b)) != sizeof(b))
    {
        return 0;
    }
    return (uint32_t)b[0] |
           ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) |
           ((uint32_t)b[3] << 24);
}

static bool read_fourcc(File &file, char out[5])
{
    // FourCC means "four character code".
    // WAV chunks are tagged with names like "RIFF", "WAVE", "fmt ", and "data".
    if (file.read((uint8_t *)out, 4) != 4)
    {
        return false;
    }
    out[4] = '\0';
    return true;
}

static bool fourcc_equals(const char *a, const char *b)
{
    // Only compare the first 4 chars because FourCC tags are exactly 4 bytes.
    // "fmt " has a space at the end on purpose.
    return strncmp(a, b, 4) == 0;
}

static bool parse_wav_header(File &file, WavInfo &info)
{
    // A normal WAV starts with RIFF, then a file size, then WAVE.
    // If this fails, it probably is not a WAV at all.
    char id[5];
    if (!read_fourcc(file, id) || !fourcc_equals(id, "RIFF"))
    {
        return false;
    }

    read_u32(file);
    if (!read_fourcc(file, id) || !fourcc_equals(id, "WAVE"))
    {
        return false;
    }

    bool found_fmt = false;
    bool found_data = false;
    uint16_t audio_format = 0;

    // WAV files are made of chunks. We keep hopping chunk to chunk until we find:
    // "fmt "  = what kind of audio this is
    // "data" = where the actual speaker samples begin
    while (file.available())
    {
        if (!read_fourcc(file, id))
        {
            break;
        }

        uint32_t chunk_size = read_u32(file);
        uint32_t next_chunk = file.position() + chunk_size + (chunk_size & 1);

        if (fourcc_equals(id, "fmt "))
        {
            // PCM format is the simple raw-audio kind we can stream directly.
            // Compressed WAV exists too, but that needs a decoder, so we reject it below.
            audio_format = read_u16(file);
            info.channels = read_u16(file);
            info.sample_rate = read_u32(file);
            read_u32(file);
            read_u16(file);
            info.bits_per_sample = read_u16(file);
            found_fmt = true;
        }
        else if (fourcc_equals(id, "data"))
        {
            // This is the treasure chest: raw PCM sample bytes.
            // Save where it starts and how many bytes it has, then playback can begin.
            info.data_offset = file.position();
            info.data_size = chunk_size;
            found_data = true;
            break;
        }

        file.seek(next_chunk);
    }

    // Keep the first version intentionally simple:
    // format 1 = uncompressed PCM, 16-bit samples, mono or stereo, reasonable sample rate.
    return found_fmt &&
           found_data &&
           audio_format == 1 &&
           info.channels >= 1 &&
           info.channels <= 2 &&
           info.bits_per_sample == 16 &&
           info.sample_rate >= 8000 &&
           info.sample_rate <= 96000;
}

static bool init_audio_codec(uint32_t sample_rate)
{
    // ES8311 is the little audio codec chip that turns I2S digital audio into speaker/headphone sound.
    // We set it to the same sample rate as the WAV so the song does not play too fast or too slow.
    es8311_handle_t es_handle = es8311_create(I2C_NUM_0, ES8311_ADDRRES_0);
    if (!es_handle)
    {
        return false;
    }

    const es8311_clock_config_t es_clk = {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency = (int)(sample_rate * AUDIO_MCLK_MULTIPLE),
        .sample_frequency = (int)sample_rate};

    if (es8311_init(es_handle, &es_clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK)
    {
        return false;
    }

    es8311_voice_volume_set(es_handle, AUDIO_VOLUME, NULL);
    es8311_microphone_config(es_handle, false);

    // I2S is the audio "conveyor belt" from the ESP32 to the ES8311.
    // DMA buffers let hardware keep pushing sound while our code reads the next SD card chunk.
    const i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = sample_rate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 512,
        .use_apll = true,
        .tx_desc_auto_clear = true,
        .fixed_mclk = (int)(sample_rate * AUDIO_MCLK_MULTIPLE),
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        .bits_per_chan = I2S_BITS_PER_CHAN_16BIT};

    // These pins are the physical audio wires on the Waveshare board.
    // MCLK/BCLK/LRCK are clocks, SDOUT is the actual audio data.
    const i2s_pin_config_t pin_config = {
        .mck_io_num = I2S_MCLK,
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_LRCK,
        .data_out_num = I2S_SDOUT,
        .data_in_num = I2S_PIN_NO_CHANGE};

    if (i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL) != ESP_OK)
    {
        return false;
    }

    if (i2s_set_pin(I2S_NUM_0, &pin_config) != ESP_OK)
    {
        return false;
    }

    i2s_zero_dma_buffer(I2S_NUM_0);
    return true;
}

bool audio_player_start_wav(const String &path)
{
    // Open the chosen file, read its WAV header, then configure audio hardware to match it.
    wav_file = SD_MMC.open(path, "r");
    if (!wav_file || !parse_wav_header(wav_file, current_wav))
    {
        last_error = "Use 16-bit PCM WAV";
        return false;
    }

    if (!init_audio_codec(current_wav.sample_rate))
    {
        last_error = "Audio init failed";
        return false;
    }

    // Jump past the WAV header/chunks so the next read starts on real audio sample bytes.
    wav_file.seek(current_wav.data_offset);
    bytes_played = 0;
    audio_playing = true;
    last_error = "";
    return true;
}

void audio_player_loop(void)
{
    // Small buffers are fine here because loop() runs over and over.
    // file_buffer gets raw bytes from the SD card.
    // stereo_buffer is only used when a mono WAV needs to be duplicated into left+right.
    static uint8_t file_buffer[2048];
    static int16_t stereo_buffer[2048];

    if (!audio_playing)
    {
        return;
    }

    uint32_t remaining = current_wav.data_size - bytes_played;
    if (remaining == 0 || !wav_file.available())
    {
        // No more sound bytes. Stop cleanly and clear the DMA buffer so no old audio hangs around.
        audio_playing = false;
        i2s_zero_dma_buffer(I2S_NUM_0);
        last_error = "Finished";
        return;
    }

    size_t to_read = min((uint32_t)sizeof(file_buffer), remaining);
    size_t bytes_read = wav_file.read(file_buffer, to_read);
    if (bytes_read == 0)
    {
        audio_playing = false;
        return;
    }

    bytes_played += bytes_read;

    // Stereo WAV data is already left/right/left/right, so we can send it as-is.
    const void *write_buffer = file_buffer;
    size_t write_bytes = bytes_read;

    if (current_wav.channels == 1)
    {
        // Mono WAV is one sample at a time.
        // The I2S setup expects stereo, so duplicate each mono sample into left and right.
        size_t sample_count = bytes_read / sizeof(int16_t);
        int16_t *mono_samples = (int16_t *)file_buffer;
        for (int i = (int)sample_count - 1; i >= 0; --i)
        {
            stereo_buffer[i * 2] = mono_samples[i];
            stereo_buffer[i * 2 + 1] = mono_samples[i];
        }
        write_buffer = stereo_buffer;
        write_bytes = sample_count * sizeof(int16_t) * 2;
    }

    // This is the actual "make noise" call: hand the sample bytes to I2S and wait until accepted.
    size_t bytes_written = 0;
    i2s_write(I2S_NUM_0, write_buffer, write_bytes, &bytes_written, portMAX_DELAY);
}

const char *audio_player_last_error(void)
{
    // Tiny status pipe back to main/display_ui.
    // Empty string means no problem right now.
    return last_error;
}
