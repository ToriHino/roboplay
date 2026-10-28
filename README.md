![Image description](https://gitlab.com/torihino/roboplay/-/raw/master/robologo.svg)

# RoboPlay
RoboPlay is a multiformat music replayer for the MSX platform, which originally was started to play as much music file formats as possible on the OPL4 (with run-time conversion when needed). Over time also additional sound-chips are supported.

RoboPlay is using a main executable and a plug-in player is loaded for each supported file format. Note that players named with _ORG use the original hardware (for example MSX-AUDIO or MSX-MUSIC) for output instead of converted to the OPL4.

Currently the following sound chips are supported:

- Yamaha YMF278B OPL4
- MSX-AUDIO
- MSX-MUSIC
- General Instrument AY-3-8910 PSG (primary + secondary)
- Konami K051649 SCC
- Yamaha YM2151 OPM (also known as SFG-01 or SFG-05)
- MIDI-PAC midi out
- MSX-MIDI, MSX-Pico, MSX-VR midi out
- Darky 2x AY-8930 ePSG
- SoundStar SAA1099

The dsk folder contains the MSX binaries based on a build of the most recent sources.

MSX-DOS2 is needed. Note that for timing the OPL4 is used so this is almost always mandatory. For a few formats that don't need an OPL4 and run on 50 or 60 Hz (for example SoundStar) the VDP is used as a timer.

It is written in C using [SDCC](http://sdcc.sourceforge.net/), version 4.2 or higher.
Open the folder in VS Code and build with 'CTRL+SHIFT+B' or use 'make all' from the command line. To build under Windows latest version of Cygwin64 with package for gcc-core needs to be installed.

Created by RoboSoft Inc. in 2026.
This work is free. You can redistribute it and/or modify it under the
terms of the Simplified BSD License.

Currently the following file formats are supported:

For OPL4:
- AMD: AMUSiC Adlib tracker (packed and unpacked modules, stereo enhanced replay)
- D00: EdLib packed module
- DRO: DOSBox Raw OPL v0.1 and v2.0
- MOD: Amiga Module (up to 24 channels)
- HSC: HSC AdLib Composer
- IMF: iD Software Music Format (for .WLF extension 700Hz replay is used)
- IMS: Iyagi Music Sound 
- MBM: MoonBlaster 1.4 (inlcuding PSG drums, ADPCM sample support, USER and EDIT files)
- MID: Standard MIDI file (using WaveTable or FM)
- MUS: FAC SoundTracker 1.0, 2.0 and PRO (stereo replay, ADPCM sample support)
- MWM: MoonBlaster for MoonSound Wave (USER and EDIT files)
- MFM: MoonBlaster for MoonSound FM (USER, EDIT and RAW files)
- PRO: Tyfoon Pro-Tracker (stereo enhanced replay)
- RAD: Reality ADlib Tracker 1.x (stereo enhanced replay) and 2.x
- RAW: Rdos Adlib Capture
- ROL: Adlib Visual Composer
- SA2: Surprise! Adlib Tracker v2 (file version 8 and 9, stereo enhanced replay)
- SOP: Note OPL3 Music Sequencer 
- SSY: Petscii Robots adlib (file extension usually .adl, stereo enhanced replay)
- VGM: Video Game Music

For MSX-AUDIO (using _ORG players):
- MBM: MoonBlaster 1.4
- MUS: FAC SoundTracker 1.0, 2.0 and PRO

For MSX-MUSIC (using _ORG players):
- PRO: Tyfoon Pro-Tracker

For SCC:
- SBM: SCC Blaffer NT (including PSG drums)
- SNG: SCC Musixx

For OPM:
- MDX: MXDRV music format for the Sharp X68000
- TFC: TurboSound FM compiled (original 2xYM2203, stereo enhanced replay)
- ZSM: Commander X16 zsound (VERA psg part played by OPL4)

For PSG:
- ASC: ASC Sound Master for the ZX Spectrum, version 0.xx and 1.xx
- PT3: ProTracker 3.xx / Vortex Tracker II (3 channel stereo enhanced replay, 6 channel)

For Darky:
- Replaces all PSG output, channels distributed over left, middle and right ('ABC'), 6 channel PT3 replay support

For SoundStar
- ETC: E-Tracker compiled (also support for .COP and .SAA files, with our without player code present)

For external MIDI (use mid_ext.ply player):
- MIDI-PAC
- MSX-MIDI, MSX-Pico (use /P option to force to MSX-MIDI or MSX-Pico out when detection fails)

# Installation
RoboPlay consists of the main RoboPlay application and a separate player plugin (.PLY) per supported file format.

The main application together with all player files can be placed on any folder location, just make sure all players are located in the same folder as the main application. Add the location of the main application to the search path to be able to start it from anywhere.

# Usage
To start the player, start the main application followed by optionally the name of the player file and the song file to play.
s
> **ROBOPLAY** [/T &lt;seconds&gt;] [/L loops] [/W] [/P] [player.PLY] song.EXT    or **ROBOPLAY** [/R] [/T &lt;seconds&gt;] [/L loops] name.M3U

The player name is optional, when no player is provided the file extension of the song is used for selecting which player to use
For example:
- **ROBOPLAY SOP.PLY TEST.SOP** will play the song TEST.SOP using SOP.PLY as the player.
- **ROBOPLAY TEST.SOP** will play the song TEST.SOP using SOP.PLY as the player
- **ROBOPLAY SOP.PLY TEST.RAW** will play the song TEST.RAW using SOP.PLY as the player

The /T option indicates the maximum play length per song in seconds, the /L option the number of loops to play per song.
To use MSX-MIDI or MSX-Pico MIDI out, option /P can be used to force using the device.
Using the /W option replaces OPL rhythm drums with OPL4 wavetable drums.
When a M3U file is supplied, the /R option can be used to randomize selection of the songs. 


During replay use **ESC** to stop replaying. When the song contains multiple sub-songs, use **LEFT** and **RIGHT** to go to previous or next subsong. When using a M3U file use **SPACE** to go to the next song.

# Players

## ROBO_PLAYER_INTERFACE
All players use the same header (located in the *players* inc subfolder) in which the ROBO_PLAYER_INTERFACE is declared. This interface is used to plug-in the player into the main application

The following methods need to implemented by a player and are called from the RoboPlay framework:
>* *bool load(char** *fileName)*
>Called when a song file needs to be loaded
>* *bool update(void)*
>Called every interrupt cycle to play the next song event
>* *void rewind(int8_t subsong)*
>Reset the song to it's initial state and select the provided subsong (when applicable)
>* *float get_refresh(void);*
>Provide the frequency (in Hz) of the update calls

>* *uint8_t get_subsongs(void)*
> Return the number of sub-songs. 

>* *char** *get_player_info()*
> Return information about the player
>* *char** *get_title()*
> Return the title of the song
>* *char** *get_author()*
> Return the author of the song
>* *char** *get_description()*
> Return the description of the song

Next to this the interface also provides a number of methods towards the RoboPlay framework which can be used by the player:
>* *open(char** *name)*
>Open a file to read
>* *uint_16_t read(void* *buf, uint16_t nbytes)*
>Read *nbytes* bytes from the opened file. Returns the actual number of bytes read
>* *void close()*
>Close the opened file
>* *boolean exists(char* *name)*
>Check if a specific file exists
>* *void seek(int32_t offset, roboplay_seek whence)*
>Seek in opened file to location *offset*, relative from *whence*


>* *uint8_t get_segment()*
>Get a new allocated memory segment. Segments are always a block of 16KB and always located on Page 2 (*0x8000 - 0xBFFF*). Returns the index number of the segment
>* *void set_segment(uint8_t segment)*
>Set a memory segment active on Page 2

>* *void update_refresh()*
>Force a get_refresh call from the framework to set a new refresh rate.

>* *void set_opl_mode(ROBO_PLAYER_OPL_MODE mode)*
>Select OPL2, OPL3 or OPL4 mode.

>* *opl4_write_fm_1(uint8_t reg, uint8_t value)*
>Write value *value* to register *addr* using the first OPL4 FM port (*0xC4*). Register 2, 3 and 4 will be ignored.
>* *opl4_write_fm_2(uint8_t reg, uint8_t value)*
>Write value *value* to register *reg* using the second OPL4 FM port (*0xC6*). Register 5 will be ignored.

>* *opl4_write_wave(uint8_t reg, uint8_t value)*
>Write value *value* to register *reg* using the OPL4 wave port (*0x7E*)
>* *opl4_write_wave_data(uint8_t value, uint16_t size)*
>Write block of data to the OPL4 wave data port (*0x7F*)

>* *opl4_read_status()*
>Read *value* of the OPL4 status register
>* *opl4_read_wave_register(uint8_t reg)*
>Read *value* from register *reg* using the OPL4 wave port (*0x7E*)
>* *opl4_read_fm_1(uint8_t value)*
>Read value of the OPL4 register *reg* on the first OPL4 FM port
>* *opl4_read_fm_2(uint8_t value)*
>Read value of the OPL4 register *reg* on the second OPL4 FM port
>* *opl4_wait_for_load*
>Wait for the OPL4 when loading a new wave on a channel

>* *psg_write(uint8_t reg, uint8_t value)*
>Write value *value* to register *reg* of the primary PSG
>* *psg_read(uint8_t reg)*
>Read the value of register *reg* of the primary PSG
>* *psg2_write(uint8_t reg, uint8_t value)*
>Write value *value* to register *reg* of the secondary PSG
>* *psg2_read(uint8_t reg)*
>Read the value of register *reg* of the secondary PSG

>* *scc_set_waveform(uint8_t channel, uint8_t \*waveform)*
>Set the waveform pointed by *waveform* to channel *channel* of the SCC
>* *scc_set_frequency(uint8_t channel, uint8_t frequency)*
>Set the *frequency* to channel *channel* of the SCC
>* *scc_set_volume(uint8_t channel, uint8_t volume)*
>Set the volume to *volume* for channel *channel* of the SCC
>* *scc_write register(uint8_t reg, uint8_t value)*
>Write value *value* to register *reg* of the SCC

>* *opm_write(uint8_t value)*
>Write value *value* to register *reg* of the OPM
>* *opm_read_status(uint8_t reg)*
>Read the status register of the OPM

>* *midi_send_data_1(const uint8_t data_1)*
> Write a single byte to MIDI device
>* *midi_send_data_2(const uint8_t data_1, const uint8_t data_2)*
> Write two bytes to MIDI device
>* *midi_send_data_3(const uint8_t data_1, const uint8_t data_2, const uint8_t data_3)*
> Write three bytes to MIDI device

>* *soundstar_write(const uint8_t reg, const uint8_t value)*
>Write value *value* to register *reg* of the SoundStar device

## Makefile
When a new player is added, place it in the *players* src subfolder, the makefile will take it along automatically.

All players are linked together with the same assembly file (*player_base.z80*) which provides the binary player headers as well as the entry points used for the ROBO_PLAYER_INTERFACE. 
When one or more of the mandatory interface methods are not impmented by the player this will result in link errors, acting sort of as a virtual base class.

## Memory layout
* Main application of the framework uses Page 0 (*0x000 - 0x3FFF*)
* Players are located starting from Page 1 (*0x4000 - 0x7FFF*)
* Song data can be freely located on every location in Page 1 or higher not yet in use. When more memory is needed this can be allocated using *get_segment* and set using *set_segment*. This will **always** be on Page 2.
* First part of Page 3 is also used by the main application as a buffer
