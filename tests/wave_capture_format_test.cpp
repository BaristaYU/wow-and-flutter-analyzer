#include "../WaveCaptureFormat.h"
#include <cstdio>
#include <limits>

static int failures = 0;
static void Check(bool condition, const char* description)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failures;
    }
}

int main(int argc, char** argv)
{
    if (argc != 4)
        return 2;

    using namespace WfguiWave;
    const Sample low = (std::numeric_limits<Sample>::min)();
    const Sample high = (std::numeric_limits<Sample>::max)();
    Check(EncodeCounts(32768.0) == 32768, "positive values beyond int16 do not wrap");
    Check(EncodeCounts(-32769.0) == -32769, "negative values beyond int16 do not wrap");
    Check(EncodeCounts(40000.0) == 40000, "positive half-period error keeps its sign");
    Check(EncodeCounts(-40000.0) == -40000, "negative half-period error keeps its sign");
    Check(EncodeCounts(1.9) == 1 && EncodeCounts(-1.9) == -1, "original truncation is preserved");
    Check(EncodeCounts(0.0) == 0, "nominal period is zero error");
    Check(EncodeCounts(2147483647.0) == high, "int32 upper boundary");
    Check(EncodeCounts(-2147483648.0) == low, "int32 lower boundary");
    Check(EncodeCounts(2147483648.0) == high, "positive overflow saturates");
    Check(EncodeCounts(-2147483649.0) == low, "negative overflow saturates");
    Check(EncodeCounts(1.0e300) == high && EncodeCounts(-1.0e300) == low, "extreme finite values saturate");
    Check(EncodeCounts(std::numeric_limits<double>::infinity()) == high, "positive infinity saturates");
    Check(EncodeCounts(-std::numeric_limits<double>::infinity()) == low, "negative infinity saturates");
    Check(EncodeCounts(std::numeric_limits<double>::quiet_NaN()) == low, "NaN uses the documented marker");
    Check(!WriteSample(NULL, 0), "missing raw stream fails safely");
    Check(!WriteMetadata(NULL, 3150), "missing metadata stream fails safely");

    const Sample values[] = {
        40000, 40001, -40000, -40001, 32767, 32768, -32768, -32769,
        0, 1, -1, high, low, 123456789, -123456789
    };
    std::FILE* raw = std::fopen(argv[1], "wb");
    Check(raw != NULL, "open raw test output");
    if (raw == NULL)
        return 1;
    for (unsigned int i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
        Check(WriteSample(raw, values[i]), "write a complete 4-byte sample");
    Check(std::fclose(raw) == 0, "close raw output");

    raw = std::fopen(argv[1], "rb");
    Check(raw != NULL, "reopen raw test output");
    if (raw == NULL)
        return 1;
    unsigned char bytes[16] = {0};
    Check(std::fread(bytes, 1, sizeof(bytes), raw) == sizeof(bytes), "read serialized bytes");
    const unsigned char expected[] = {
        0x40, 0x9c, 0x00, 0x00, 0x41, 0x9c, 0x00, 0x00,
        0xc0, 0x63, 0xff, 0xff, 0xbf, 0x63, 0xff, 0xff
    };
    for (unsigned int i = 0; i < sizeof(expected); ++i)
        Check(bytes[i] == expected[i], "little-endian bytes preserve adjacent large errors");
    Check(std::fclose(raw) == 0, "close raw input");

    for (int i = 0; i < 2; ++i)
    {
        std::FILE* metadata = std::fopen(argv[i + 2], "w");
        Check(metadata != NULL, "open metadata test output");
        if (metadata == NULL)
            return 1;
        Check(!WriteMetadata(metadata, 0), "reject an unsupported reference");
        Check(WriteMetadata(metadata, i == 0 ? 3150 : 3000), "write metadata for the selected reference");
        Check(std::fclose(metadata) == 0, "close metadata output");
    }

    if (failures)
        return 1;
    std::puts("32-bit capture boundary and serialization tests passed.");
    return 0;
}
