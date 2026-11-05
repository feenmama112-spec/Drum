// ===========================================================================
//  test_core.cpp  --  host unit tests for the hardware independent modules
// ===========================================================================
//  Build & run:  tests/host/run_tests.sh
//
//  These tests cover the parts that are expensive to debug on the hardware:
//    * MCP3208 SPI frame bit alignment (velocity depends on it)
//    * u-law round trip
//    * WAV header parsing (incl. odd chunk sizes / extensible format)
//    * resampling and the WAV -> RAM loader
//    * trigger peak detection, velocity mapping, retrigger masking
//    * hi-hat zone hysteresis and foot "chick"
//    * polyphonic mixing, choke groups, voice stealing
// ===========================================================================
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>

#include "config.h"
#include "core/converter.h"
#include "core/hihat.h"
#include "core/kit_defs.h"
#include "core/mcp3208_frame.h"
#include "core/sample_bank.h"
#include "core/sample_loader.h"
#include "core/trigger.h"
#include "core/ulaw.h"
#include "core/voice_pool.h"
#include "core/wav.h"

using namespace drum;

static int g_fail = 0;
static int g_pass = 0;

#define CHECK(cond)                                                       \
  do {                                                                    \
    if (cond) {                                                           \
      ++g_pass;                                                           \
    } else {                                                              \
      ++g_fail;                                                           \
      std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);       \
    }                                                                     \
  } while (0)

#define CHECK_NEAR(a, b, tol)                                                    \
  do {                                                                           \
    const double va = (a), vb = (b);                                             \
    if (std::fabs(va - vb) <= (tol)) {                                           \
      ++g_pass;                                                                  \
    } else {                                                                     \
      ++g_fail;                                                                  \
      std::printf("  FAIL %s:%d  %s=%.4f vs %.4f\n", __FILE__, __LINE__, #a, va, vb); \
    }                                                                            \
  } while (0)

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
static void put32(std::vector<uint8_t>& b, uint32_t v) {
  for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
}
static void put16(std::vector<uint8_t>& b, uint16_t v) {
  for (int i = 0; i < 2; ++i) b.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
}
static void putStr(std::vector<uint8_t>& b, const char* s) {
  for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>(s[i]));
}

static std::vector<uint8_t> makeWav(uint32_t rate, uint16_t ch, uint16_t bits,
                                    const std::vector<int16_t>& pcm, bool withPadChunk = false) {
  std::vector<uint8_t> b;
  const uint32_t dataBytes = static_cast<uint32_t>(pcm.size() * 2);
  const uint32_t extra = withPadChunk ? 9 : 0;  // odd sized "LIST" chunk + pad
  putStr(b, "RIFF");
  put32(b, 36 + dataBytes + extra);
  putStr(b, "WAVE");
  putStr(b, "fmt ");
  put32(b, 16);
  put16(b, 1);
  put16(b, ch);
  put32(b, rate);
  put32(b, rate * ch * 2);
  put16(b, static_cast<uint16_t>(ch * 2));
  put16(b, bits);
  if (withPadChunk) {
    putStr(b, "LIST");
    put32(b, 9);  // odd size -> one pad byte follows
    for (int i = 0; i < 9; ++i) b.push_back(0xAA);
    b.push_back(0x00);
  }
  putStr(b, "data");
  put32(b, dataBytes);
  for (int16_t s : pcm) put16(b, static_cast<uint16_t>(s));
  return b;
}

// Models the DOUT stream of an MCP3208 clocked 24 times in mode 0,0, using the
// clock numbering of datasheet figure 6-1 (five leading zeros):
//   clk 1..10 : high-Z (reads as 0)
//   clk 11    : null bit
//   clk 12..23: B11..B0
//   clk 24    : LSB-first echo (B1)
static void simulateMcp3208(uint16_t code, uint8_t rx[3]) {
  uint8_t bits[24] = {0};
  bits[10] = 0;  // null bit
  for (int k = 0; k < 12; ++k) bits[11 + k] = static_cast<uint8_t>((code >> (11 - k)) & 1u);
  bits[23] = static_cast<uint8_t>((code >> 1) & 1u);
  for (int by = 0; by < 3; ++by) {
    uint8_t v = 0;
    for (int j = 0; j < 8; ++j) v = static_cast<uint8_t>((v << 1) | bits[by * 8 + j]);
    rx[by] = v;
  }
}

// ---------------------------------------------------------------------------
static void test_mcp3208() {
  std::printf("MCP3208 frame codec\n");

  // command layout: START on clock 6, SGL on 7, D2..D0 on 8..10
  for (uint8_t ch = 0; ch < 8; ++ch) {
    uint8_t tx[3];
    mcp3208BuildCommand(ch, tx);
    CHECK((tx[0] & 0x04) != 0);                       // START bit
    CHECK((tx[0] & 0x02) != 0);                       // single ended
    CHECK(((tx[0] >> 0) & 1) == ((ch >> 2) & 1));     // D2
    CHECK(((tx[1] >> 7) & 1) == ((ch >> 1) & 1));     // D1
    CHECK(((tx[1] >> 6) & 1) == (ch & 1));            // D0
  }

  const uint16_t codes[] = {0, 1, 2, 3, 127, 128, 255, 1023, 2047, 2048, 4094, 4095};
  for (uint16_t c : codes) {
    uint8_t rx[3];
    simulateMcp3208(c, rx);
    const uint16_t got = mcp3208Decode(rx);
    if (got != c) std::printf("    code %u -> %u (rx %02X %02X %02X)\n", c, got, rx[0], rx[1], rx[2]);
    CHECK(got == c);
  }
  // every 12-bit code must survive the round trip
  int bad = 0;
  for (uint32_t c = 0; c <= 4095; ++c) {
    uint8_t rx[3];
    simulateMcp3208(static_cast<uint16_t>(c), rx);
    if (mcp3208Decode(rx) != c) ++bad;
  }
  CHECK(bad == 0);
}

// ---------------------------------------------------------------------------
static void test_ulaw() {
  std::printf("u-law codec\n");
  CHECK(ulawDecode(ulawEncode(0)) == 0);
  // round trip error must stay inside the u-law step size for the segment
  for (int32_t v = -32000; v <= 32000; v += 37) {
    const int16_t in = static_cast<int16_t>(v);
    const int16_t out = ulawDecode(ulawEncode(in));
    const int32_t err = std::abs(static_cast<int32_t>(in) - out);
    const double rel = static_cast<double>(err) / 32768.0;
    if (std::abs(v) > 2000) {
      CHECK(rel < 0.03);  // < 3 % of full scale (G.711 is ~38 dB SNR)
    } else {
      // u-law step size doubles per segment, so the absolute error allowed
      // grows with the signal level: one step at 2 000 is 128 counts.
      CHECK(err <= 0.065 * std::abs(v) + 8);
    }
  }
  CHECK(ulawDecode(0xFF) == 0);
  for (int i = 0; i < 256; ++i) {
    const int16_t d = ulawDecode(static_cast<uint8_t>(i));
    CHECK(ulawEncode(d) == static_cast<uint8_t>(i) || std::abs(ulawDecode(ulawEncode(d)) - d) < 64);
  }
}

// ---------------------------------------------------------------------------
static void test_wav() {
  std::printf("WAV parser\n");
  std::vector<int16_t> pcm(1000);
  for (size_t i = 0; i < pcm.size(); ++i) pcm[i] = static_cast<int16_t>(i * 3);

  {
    auto bytes = makeWav(44100, 1, 16, pcm);
    MemoryByteSource src(bytes.data(), static_cast<uint32_t>(bytes.size()));
    WavInfo info;
    CHECK(wavParse(src, info));
    CHECK(info.valid);
    CHECK(info.channels == 1);
    CHECK(info.sampleRate == 44100);
    CHECK(info.bitsPerSample == 16);
    CHECK(info.frameCount == 1000);
    CHECK(info.dataOffset == 44);
  }
  {
    // odd sized chunk before "data" must be skipped correctly
    auto bytes = makeWav(22050, 2, 16, pcm, true);
    MemoryByteSource src(bytes.data(), static_cast<uint32_t>(bytes.size()));
    WavInfo info;
    CHECK(wavParse(src, info));
    CHECK(info.channels == 2);
    CHECK(info.frameCount == 500);  // 1000 samples = 500 stereo frames
  }
  {
    // garbage must be rejected, not played as noise
    std::vector<uint8_t> junk(100, 0x55);
    MemoryByteSource src(junk.data(), static_cast<uint32_t>(junk.size()));
    WavInfo info;
    CHECK(!wavParse(src, info));
    CHECK(!info.valid);
  }
  {
    // 8-bit must be rejected (documented limitation)
    auto bytes = makeWav(22050, 1, 8, pcm);
    MemoryByteSource src(bytes.data(), static_cast<uint32_t>(bytes.size()));
    WavInfo info;
    CHECK(!wavParse(src, info));
  }
}

// ---------------------------------------------------------------------------
static void test_converter() {
  std::printf("resampler / downmix\n");

  // 44100 -> 22050: exactly half the frames
  {
    std::vector<int16_t> in(4410);
    for (size_t i = 0; i < in.size(); ++i)
      in[i] = static_cast<int16_t>(12000 * std::sin(2.0 * M_PI * 440.0 * i / 44100.0));
    SampleConverter c;
    c.begin(44100, 1, 22050);
    std::vector<int16_t> out(4410);
    size_t n = c.feed(in.data(), in.size(), 0, true, out.data(), out.size());
    CHECK(n >= 2200 && n <= 2206);
    int32_t peak = 0;
    for (size_t i = 0; i < n; ++i) peak = std::max<int32_t>(peak, std::abs(out[i]));
    CHECK(peak > 11000 && peak <= 12100);
  }
  // stereo downmix averages the two channels
  {
    std::vector<int16_t> in = {1000, 3000, 1000, 3000, 1000, 3000};
    SampleConverter c;
    c.begin(22050, 2, 22050);
    std::vector<int16_t> out(8);
    size_t n = c.feed(in.data(), 3, 0, true, out.data(), out.size());
    CHECK(n == 3);
    CHECK(out[0] == 2000);
    CHECK(out[1] == 2000);
  }
  // same rate is a pass-through
  {
    std::vector<int16_t> in = {0, 100, -100, 32767, -32768};
    SampleConverter c;
    c.begin(22050, 1, 22050);
    std::vector<int16_t> out(16);
    size_t n = c.feed(in.data(), in.size(), 0, true, out.data(), out.size());
    CHECK(n == in.size());
    for (size_t i = 0; i < n; ++i) CHECK(out[i] == in[i]);
  }
}

// ---------------------------------------------------------------------------
static void test_loader() {
  std::printf("WAV -> RAM loader\n");
  const uint32_t rate = 22050;
  std::vector<int16_t> pcm(2205);  // 100 ms
  for (size_t i = 0; i < pcm.size(); ++i)
    pcm[i] = static_cast<int16_t>(16000 * std::sin(2.0 * M_PI * 200.0 * i / rate));

  auto bytes = makeWav(rate, 1, 16, pcm);
  MemoryByteSource src(bytes.data(), static_cast<uint32_t>(bytes.size()));
  WavInfo info;
  CHECK(wavParse(src, info));

  std::vector<uint8_t> buf(pcmBytesFor(2205, false));
  LoadResult r = loadWavToBuffer(src, info, rate, 0, false, buf.data(), buf.size());
  CHECK(r.ok);
  CHECK(r.frames == 2205);
  CHECK(!r.truncated);
  // the decoded signal must still be a 200 Hz sine of the right amplitude
  int32_t peak = 0;
  for (uint32_t i = 0; i < r.frames; ++i) peak = std::max<int32_t>(peak, std::abs(ulawDecode(buf[i])));
  CHECK(peak > 15000 && peak < 16500);
  const int16_t q15 = computePeakNormaliseQ15(buf.data(), r.frames, false);
  CHECK(q15 > 32000 && q15 <= 32768);  // already close to full scale

  // a cap must truncate, not overflow the destination
  src.seek(info.dataOffset);
  LoadResult r2 = loadWavToBuffer(src, info, rate, 500, false, buf.data(), buf.size());
  CHECK(r2.ok);
  CHECK(r2.frames == 500);
  CHECK(r2.truncated);

  // 48 kHz stereo file resampled to the engine rate
  std::vector<int16_t> st(4800 * 2);
  for (size_t i = 0; i < 4800; ++i) {
    st[2 * i] = static_cast<int16_t>(8000 * std::sin(2.0 * M_PI * 300.0 * i / 48000.0));
    st[2 * i + 1] = st[2 * i];
  }
  auto bytes2 = makeWav(48000, 2, 16, st);
  MemoryByteSource src2(bytes2.data(), static_cast<uint32_t>(bytes2.size()));
  WavInfo info2;
  CHECK(wavParse(src2, info2));
  std::vector<uint8_t> buf2(pcmBytesFor(3000, true));
  LoadResult r3 = loadWavToBuffer(src2, info2, rate, 0, true, buf2.data(), buf2.size());
  CHECK(r3.ok);
  CHECK(r3.frames > 2100 && r3.frames < 2400);  // 100 ms at 22050
}

// ---------------------------------------------------------------------------
static void test_trigger() {
  std::printf("trigger detector\n");
  TriggerDetector d;
  TriggerParams p;
  p.threshold = 100;
  p.maskMs = 25;
  p.gamma = 0.65f;
  d.configure(p);

  TriggerEvent ev;
  int events = 0;

  // idle below threshold
  for (uint32_t t = 0; t < 20; ++t) CHECK(!d.update(50, t, &ev));

  // a hit: rises fast, decays
  const uint16_t pulse[] = {50, 3000, 2900, 2600, 400, 120, 60, 50};
  for (uint32_t i = 0; i < sizeof(pulse) / sizeof(pulse[0]); ++i) {
    if (d.update(pulse[i], 20 + i, &ev)) {
      ++events;
      CHECK(ev.peak == 3000);
      CHECK(ev.velocity >= 95 && ev.velocity <= 105);
    }
  }
  CHECK(events == 1);

  // second hit inside the mask window is swallowed
  for (uint32_t i = 0; i < sizeof(pulse) / sizeof(pulse[0]); ++i)
    if (d.update(pulse[i], 30 + i, &ev)) ++events;
  CHECK(events == 1);

  // ... but a hit after the mask window is accepted
  for (uint32_t i = 0; i < sizeof(pulse) / sizeof(pulse[0]); ++i)
    if (d.update(pulse[i], 80 + i, &ev)) ++events;
  CHECK(events == 2);

  // a long quiet tail below the threshold produces nothing
  for (uint32_t t = 200; t < 400; ++t) CHECK(!d.update(80, t, &ev));

  // velocity mapping
  CHECK(peakToVelocity(100, 100, 0.65f) == 1);
  CHECK(peakToVelocity(4095, 100, 0.65f) == 127);
  CHECK(peakToVelocity(200, 100, 0.65f) > 1);
  CHECK(peakToVelocity(200, 100, 0.65f) < peakToVelocity(800, 100, 0.65f));
  CHECK(peakToVelocity(800, 100, 0.65f) < peakToVelocity(3000, 100, 0.65f));

  // a very short spike that never falls inside the window still fires
  TriggerDetector d2;
  d2.configure(p);
  events = 0;
  for (uint32_t t = 0; t < 50; ++t)
    if (d2.update(4000, t + 500, &ev)) ++events;
  CHECK(events >= 1);  // window expiry guarantees the note is not lost
}

// ---------------------------------------------------------------------------
static void test_hihat() {
  std::printf("hi-hat pedal\n");
  HiHatPedal p;
  p.begin();
  uint8_t vel = 0;

  // open pedal
  for (int i = 0; i < 20; ++i) p.update(3900, 0 + i, &vel);
  CHECK(p.zone() == HH_ZONE_OPEN);
  CHECK(p.sampleForZone() == S_HH_OPEN);

  // half open
  for (int i = 0; i < 20; ++i) p.update(2048, 100 + i, &vel);
  CHECK(p.zone() == HH_ZONE_HALF);
  CHECK(p.sampleForZone() == S_HH_HALF);

  // close it fast -> foot chick
  bool chick = false;
  for (int i = 0; i < 6; ++i) chick |= p.update(300, 200 + i * 8, &vel);
  CHECK(chick);
  CHECK(p.zone() == HH_ZONE_CLOSED);
  CHECK(p.sampleForZone() == S_HH_CLOSED);

  // staying closed must not fire again
  chick = false;
  for (int i = 0; i < 20; ++i) chick |= p.update(300, 300 + i, &vel);
  CHECK(!chick);

  // release and step down again -> chick again
  for (int i = 0; i < 10; ++i) p.update(3900, 400 + i, &vel);
  chick = false;
  for (int i = 0; i < 6; ++i) chick |= p.update(300, 500 + i * 8, &vel);
  CHECK(chick);

  // a genuinely slow pedal movement (2.4 s of travel) is NOT a chick
  HiHatPedal slow;
  slow.begin();
  uint16_t raw = 3900;
  for (int i = 0; i < 10; ++i) slow.update(3900, static_cast<uint32_t>(i), &vel);
  chick = false;
  for (int i = 0; i < 120; ++i) {  // 120 x 20 ms = 2.4 s
    raw = static_cast<uint16_t>(3900 - (3600 * i) / 119);
    chick |= slow.update(raw, static_cast<uint32_t>(600 + i * 20), &vel);
  }
  CHECK(!chick);

  // hysteresis: sitting on the open/half border must not flap
  HiHatPedal h;
  h.begin();
  for (int i = 0; i < 10; ++i) h.update(3000, i, &vel);  // 73 % -> open
  CHECK(h.zone() == HH_ZONE_OPEN);
  int changes = 0;
  HiHatZone last = h.zone();
  for (int i = 0; i < 40; ++i) {
    h.update(static_cast<uint16_t>(i & 1 ? 2810 : 2830), 100 + i, &vel);  // ~69 %
    if (h.zone() != last) {
      ++changes;
      last = h.zone();
    }
  }
  CHECK(changes == 0);
}

// ---------------------------------------------------------------------------
static void test_voice_pool() {
  std::printf("voice pool / mixer\n");

  static uint8_t pool[200000];
  SampleBank bank;
  bank.begin(pool, sizeof(pool));

  // 11 of the 12 samples are loaded; S_CRASH is left out on purpose
  const uint32_t frames = 500;
  for (uint8_t s = 0; s < S_COUNT; ++s) {
    if (s == S_CRASH) continue;
    uint8_t* d = bank.allocate(frames);
    CHECK(d != nullptr);
    for (uint32_t i = 0; i < frames; ++i)
      d[i] = ulawEncode(static_cast<int16_t>(12000 * std::sin(2.0 * M_PI * 100.0 * i / AUDIO_SAMPLE_RATE)));
    bank.set(static_cast<SampleId>(s), d, frames, false);
  }
  CHECK(bank.loadedCount() == S_COUNT - 1);

  VoicePool vp;
  vp.begin(&bank);
  vp.setMasterVolumeQ15(32768);

  // a sample that is not loaded must not start
  CHECK(!vp.start(S_CRASH, 100));

  vp.stopAll();
  CHECK(vp.start(S_SNARE, 127));
  CHECK(vp.activeCount() == 1);

  int16_t block[AUDIO_BLOCK_FRAMES];
  vp.render(block, AUDIO_BLOCK_FRAMES);
  int32_t peak = 0;
  for (int i = 0; i < AUDIO_BLOCK_FRAMES; ++i) peak = std::max<int32_t>(peak, std::abs(block[i]));
  CHECK(peak > 9000);

  // the voice must end exactly after `frames` frames
  int total = AUDIO_BLOCK_FRAMES;
  int guard = 0;
  while (vp.activeCount() > 0 && guard++ < 100) {
    vp.render(block, AUDIO_BLOCK_FRAMES);
    total += AUDIO_BLOCK_FRAMES;
  }
  CHECK(total >= static_cast<int>(frames));
  CHECK(total < static_cast<int>(frames) + 2 * AUDIO_BLOCK_FRAMES);

  // silence when nothing is playing
  vp.render(block, AUDIO_BLOCK_FRAMES);
  bool silent = true;
  for (int i = 0; i < AUDIO_BLOCK_FRAMES; ++i)
    if (block[i] != 0) silent = false;
  CHECK(silent);

  // a sample that is not in RAM must be ignored, not played as noise
  vp.stopAll();
  CHECK(!vp.start(S_CRASH, 100));
  CHECK(vp.activeCount() == 0);

  // per-pad instance limit: a pad allows 2 voices, the third re-strike reuses
  // the oldest one instead of stealing a voice from another pad
  vp.start(S_SNARE, 100);
  vp.start(S_SNARE, 100);
  vp.start(S_SNARE, 100);  // pad allows 2 instances -> oldest is reused
  CHECK(vp.activeCount() == 2);

  // choke group: starting a closed hi-hat after choking must keep only the new
  // voice alive, and the choke must fade rather than click
  vp.stopAll();
  CHECK(vp.start(S_HH_OPEN, 127));
  CHECK(vp.activeCount() == 1);
  vp.chokeGroup(1);
  vp.start(S_HH_CLOSED, 127);
  vp.render(block, AUDIO_BLOCK_FRAMES);
  CHECK(vp.activeCount() == 1);  // the choked open hat finished fading

  // master volume 0 must mute
  vp.stopAll();
  vp.setMasterVolumeQ15(0);
  vp.start(S_SNARE, 127);
  vp.render(block, AUDIO_BLOCK_FRAMES);
  bool muted = true;
  for (int i = 0; i < AUDIO_BLOCK_FRAMES; ++i)
    if (block[i] != 0) muted = false;
  CHECK(muted);

  // voice stealing: 8 distinct pads x 2 instances = 16 voices needed but the
  // pool only holds AUDIO_MAX_VOICES, so the mixer must steal and stay sane
  vp.setMasterVolumeQ15(32768);
  vp.stopAll();
  const uint8_t padSamples[] = {S_SNARE, S_SNARE_RIM, S_TOM_HIGH, S_TOM_MID,
                                S_TOM_LOW, S_FLOOR_TOM, S_HH_CLOSED, S_RIDE};
  for (int round = 0; round < 3; ++round)
    for (uint8_t s : padSamples) vp.start(static_cast<SampleId>(s), 90);
  CHECK(vp.activeCount() <= AUDIO_MAX_VOICES);
  vp.render(block, AUDIO_BLOCK_FRAMES);
  CHECK(vp.stolenCount() > 0);
}

// ---------------------------------------------------------------------------
int main() {
  std::printf("=== ESP32-S3 drum module : core unit tests ===\n");
  test_mcp3208();
  test_ulaw();
  test_wav();
  test_converter();
  test_loader();
  test_trigger();
  test_hihat();
  test_voice_pool();
  std::printf("=== %d checks passed, %d failed ===\n", g_pass, g_fail);
  return g_fail == 0 ? 0 : 1;
}
