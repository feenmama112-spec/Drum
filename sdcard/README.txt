ESP32-S3 ELECTRONIC DRUM MODULE - SAMPLE PACK
=============================================

Copy this whole "sdcard" folder onto a FAT32 formatted microSD card
(4 GB .. 32 GB; cards 64 GB and larger normally come as exFAT and will NOT mount).

The card must end up looking like this:

  /DRUM/POP/SNARE.wav
  /DRUM/POP/SNARE_RIM.wav
  /DRUM/POP/TOM_HIGH.wav
  /DRUM/POP/TOM_MID.wav
  /DRUM/POP/TOM_LOW.wav
  /DRUM/POP/FLOOR_TOM.wav
  /DRUM/POP/HH_CLOSED.wav
  /DRUM/POP/HH_HALF.wav
  /DRUM/POP/HH_OPEN.wav
  /DRUM/POP/HH_CHICK.wav
  /DRUM/POP/CRASH.wav
  /DRUM/POP/RIDE.wav
  ... and the same 12 files in ROCK, HIPHOP and JAZZ

File requirements (the firmware rejects anything else and prints
"[kit] invalid WAV ..." on the serial port):

  * RIFF / WAVE, PCM (format 1), 16-bit
  * mono or stereo
  * 8000 .. 192000 Hz (resampled to 22050 Hz on load)
  * file names must be UPPER CASE, exactly as listed above

These files are synthesised placeholders so the module works on day one.
Replace them with your own recordings - use the same names and format,
keep them roughly the same length, and nothing in the firmware needs to change.

Regenerate the pack at any time with:
  python3 tools/make_test_wavs.py
