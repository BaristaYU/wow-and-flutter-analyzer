#ifndef WFGUI_WAVE_CAPTURE_FORMAT_H
#define WFGUI_WAVE_CAPTURE_FORMAT_H

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace WfguiWave
{
typedef std::int32_t Sample;
static_assert(sizeof(Sample) == 4, "Raw capture samples must be exactly 32 bits");

// Counts retain the original scale: 10 * half-period error in nanoseconds.
// Check before casting: out-of-range/non-finite floating-point casts are unsafe.
inline Sample EncodeCounts(double counts)
{
    const Sample low = (std::numeric_limits<Sample>::min)();
    const Sample high = (std::numeric_limits<Sample>::max)();
    if (std::isnan(counts) || counts <= static_cast<double>(low))
        return low;
    if (counts >= static_cast<double>(high))
        return high;
    return static_cast<Sample>(counts); // Preserve truncation toward zero.
}

inline bool WriteSample(std::FILE* stream, Sample value)
{
    // Encode the byte order explicitly, independently of the host byte order.
    const std::uint32_t bits = static_cast<std::uint32_t>(value);
    const unsigned char bytes[4] = {
        static_cast<unsigned char>(bits),
        static_cast<unsigned char>(bits >> 8),
        static_cast<unsigned char>(bits >> 16),
        static_cast<unsigned char>(bits >> 24)
    };
    return stream != NULL &&
        std::fwrite(bytes, 1, sizeof(bytes), stream) == sizeof(bytes);
}

inline bool WriteMetadata(std::FILE* stream, int referenceHz)
{
    if (stream == NULL || (referenceHz != 3000 && referenceHz != 3150))
        return false;
    const int result = std::fprintf(stream,
        "{\n"
        "  \"format_version\": 2,\n"
        "  \"data_file\": \"WFGUI_wave.dat\",\n"
        "  \"sample_format\": \"signed-int32-le\",\n"
        "  \"bytes_per_sample\": 4,\n"
        "  \"channels\": 1,\n"
        "  \"reference_hz\": %d,\n"
        "  \"nominal_sample_rate_hz\": %d,\n"
        "  \"units\": \"0.1 ns half-period error\",\n"
        "  \"scale_counts_per_ns\": 10,\n"
        "  \"weighting_applied\": false,\n"
        "  \"overflow_policy\": \"saturate\",\n"
        "  \"nan_value\": -2147483648\n"
        "}\n", referenceHz, referenceHz * 2);
    return result >= 0 && std::fflush(stream) == 0;
}
}

#endif // WFGUI_WAVE_CAPTURE_FORMAT_H
