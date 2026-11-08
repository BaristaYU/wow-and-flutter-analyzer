# Measurement output formats

This document describes the current BaristaYU fork. The implementation is in
[`wfguiDlg.cpp`](../wfguiDlg.cpp), primarily `OnButton2`, `ProcessHeader`, and
`OnButton1`. Older upstream filenames and WAV formats are different.

## Location and file lifetime

- Select **Folder...** before Start. The initial location is the executable's
  directory; the last selected folder is saved in the application's Windows
  profile settings.
- **Save log** creates `WFGUI_log.txt`; **Save wave** creates `WFGUI_wave.dat`.
  Select either option, or both, before Start.
- Files open in truncate/write mode on Start. **An existing file with that name
  is overwritten**, including when the same session is restarted.
- Stop closes the files. Copy or rename them before the next capture.
- The folder picker is disabled while measuring. A failure to open a requested
  file prevents measurement from starting and displays an error. File creation
  is not transactional: another selected file may already have been truncated.

## CSV: `WFGUI_log.txt`

Despite its `.txt` extension, this is comma-separated text with a comment
preamble. Lines starting with `#` describe the reference frequency, selected
filter, and timing. The first non-comment line is exactly:

```csv
time_s,freq_0p1s_hz,freq_1s_hz,speed_error_pct,rms_1s_pct,max_quasi_peak_10s_pct
```

| Column | Units and meaning | Written precision |
| --- | --- | --- |
| `time_s` | End time of the captured audio block, in seconds since Start. Computed from captured stereo frames / 44100. Includes blocks later rejected for silence or invalid tone. | 2 decimal places |
| `freq_0p1s_hz` | Carrier frequency from accepted demodulated half-periods within this 0.1-second block. | 2 decimal places |
| `freq_1s_hz` | Carrier frequency from the total period and crossing count in the last ten consecutive valid logger blocks. | 2 decimal places |
| `speed_error_pct` | `(freq_1s_hz / reference_hz - 1) * 100`. Positive means fast; negative means slow. Calculated before frequency rounding. | Explicit sign, 4 decimal places |
| `rms_1s_pct` | `sqrt(sum(weighted_error_squared) / crossing_count) * 100` over those ten blocks. Uses the selected DIN, Wow, Flutter, or Unweighted filter. | 4 decimal places |
| `max_quasi_peak_10s_pct` | Largest stored quasi-peak estimate in the last 100 accepted meter blocks. Uses the selected filter and the existing detector calibration. | 4 decimal places |

### Timing and missing values

The input is 44100 Hz / 16-bit / stereo, with **4410 stereo frames per block**:
0.1 seconds, 17640 bytes. Both channels are captured, but only the right sample
of each stereo frame is analyzed.

- A valid uninterrupted signal normally produces ten rows per second. These are
  audio-block times, not scheduled wall-clock or UI refresh times.
- The logger omits its first acquisition block and blocks that fail its validity
  checks. There is no placeholder row for a rejected block. A gap in `time_s`
  can therefore indicate silence, a dropout, or rejected tone.
- The logger requires plausible crossing counts and frequency within 95-105%
  of the selected reference. Bad blocks reset the rolling one-second history.
- Until ten consecutive valid logger blocks have accumulated, the three
  one-second fields are empty. Empty fields are **missing values, not zero**.
- The quasi-peak history is updated on accepted meter blocks. Rejected/silent
  blocks do not advance that history, so its nominal ten-second span can cover
  more than ten seconds of elapsed capture time. It can also retain values from
  before a gap after the logger's one-second history has reset.
- The GUI uses its original, less frequent update path. Treat the CSV as its own
  rolling measurement series rather than a transcription of the visible boxes.

Illustrative rows (not measured results):

```csv
time_s,freq_0p1s_hz,freq_1s_hz,speed_error_pct,rms_1s_pct,max_quasi_peak_10s_pct
0.20,3150.00,,,,0.0300
1.10,3150.00,3150.00,+0.0000,0.0100,0.0300
```

To import into a spreadsheet, remove/skip the leading `#` lines and choose a
comma delimiter. Decimal values use a dot; preserve blank cells as missing.
For Python/pandas, `pandas.read_csv(path, comment="#")` skips the preamble.

## Raw capture: `WFGUI_wave.dat`

This is **not a WAV file** and does not contain the original 44.1 kHz stereo
input. It is a stream of demodulated half-period errors before the selectable
weighting filter. The audio band-pass stage still precedes demodulation.

| Property | Value |
| --- | --- |
| Container | None: no header, metadata, or timestamps |
| Sample type | Signed 16-bit integer, little-endian |
| Channels | One |
| Import offset | 0 bytes |
| Nominal sample rate, 3000 Hz reference | 6000 samples/s |
| Nominal sample rate, 3150 Hz reference | 6300 samples/s |
| Sample unit | 0.1 ns of half-period deviation |
| Sign | Positive when the measured half-period is shorter than nominal (fast) |

The calculation is:

```text
nominal_half_period_ns = 1e9 / (2 * reference_hz)
count = signed_int16(10 * (nominal_half_period_ns - measured_half_period_ns))
half_period_deviation_pct = count / (10 * nominal_half_period_ns) * 100
```

Thus a count divided by approximately **16666.67** (3000 Hz) or **15873.02**
(3150 Hz) gives the unweighted relative half-period deviation in percent.
It is not the filtered RMS or quasi-peak shown on the meter. The exact carrier
speed error is instead `(measured_frequency / reference_hz - 1) * 100`.

There is one output value per accepted demodulated zero crossing, so twice the
reference frequency is a **nominal import rate**, not an independently clocked
sample rate. A steady offset in tape speed changes the number of values per
second. Invalid input blocks are skipped, and the file has no markers to retain
those gaps. Its duration after import need not equal elapsed capture time.

### Importing for diagnosis

Use an audio editor's **Import Raw Data** command with signed 16-bit PCM,
little-endian, mono, offset 0, and the nominal rate above. Renaming `.dat` to
`.wav` does not add a WAV header.

The file includes the average half-period offset. If looking for modulation
components, remove the DC component in a separate analysis copy. The selectable
DIN/Wow/Flutter filter is not baked into this capture.

The current implementation converts directly to a 16-bit value without a
saturation check or clipping flag. Values outside -32768..32767 cannot be
represented reliably. That limit is about +/-1.97% half-period deviation at
3000 Hz, or +/-2.06% at 3150 Hz, less than the input validity range. Large speed
offsets, acquisition transients, and dropouts can therefore make the raw file
unsuitable for quantitative analysis. Preserve the CSV reference/filter preamble
alongside raw files when archiving measurements.
