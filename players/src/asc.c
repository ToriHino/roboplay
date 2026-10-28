/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * asc.c
 *
 * ASC: ASC Sound Master (ZX Spectrum, AY-3-8910 / YM2149) player
 *
 * An ASC module is a bare data block (max. ~15KB), so it always fits in the one
 * data segment. All "pointers" are 16 bit offsets into the file.
 */

#include <string.h>

#include "player_interface.h"
#include "asc.h"

bool load(const char* file_name)
{
    g_roboplay_interface->open(file_name, false);
    g_data = (uint8_t *)DATA_SEGMENT_BASE;

    uint16_t bytes_read = g_roboplay_interface->read(g_data, DATA_SEGMENT_SIZE);
    g_roboplay_interface->close();

    /* A module can never fill the whole segment, so this is not an ASC file (or it is truncated) */
    if(bytes_read >= DATA_SEGMENT_SIZE) return false;

    g_file_size = bytes_read;

    /* ASM 0.xx files have no loop position byte. Try the newer layout first */
    if(parse_header(1)) return true;
    if(parse_header(0)) return true;

    return false;
}

bool parse_header(uint8_t version)
{
    /* Header: tempo, [loop position,] 3 pointers, number of positions, position list */
    uint8_t  p = 1 + version;
    uint16_t positions = p + 7;

    if(g_file_size < positions + 1) return false;

    uint8_t tempo = g_data[0];
    uint8_t loop = version ? g_data[1] : 0;

    uint16_t patterns  = read_word(p);
    uint16_t samples   = read_word(p + 2);
    uint16_t ornaments = read_word(p + 4);
    uint8_t  length    = g_data[p + 6];

    if(tempo < MIN_TEMPO || tempo > MAX_TEMPO) return false;
    if(loop >= MAX_POSITIONS) return false;
    if(length < 1 || length > MAX_POSITIONS) return false;

    /* Order in the file: positions, patterns, samples, ornaments */
    if(positions + length > patterns) return false;
    if(patterns >= samples || samples >= ornaments) return false;
    if(samples + ASC_LIST_SIZE > g_file_size) return false;
    if(ornaments + ASC_LIST_SIZE > g_file_size) return false;

    /* The first sample directly follows the sample list */
    if(read_word(samples) != ASC_LIST_SIZE) return false;
    if(read_word(ornaments) < ASC_LIST_SIZE) return false;

    /* The pattern table has no length, so it is as long as the highest pattern in use */
    uint8_t highest = 0;
    for(uint8_t i = 0; i < length; i++)
    {
        if(g_data[positions + i] > highest) highest = g_data[positions + i];
    }

    if(highest >= MAX_PATTERNS) return false;
    if(patterns + (highest + 1) * 6 > g_file_size) return false;

    g_version       = version;
    g_initial_tempo = tempo;
    g_loop_position = (loop < length) ? loop : 0;
    g_order_size    = length;
    g_order_list    = positions;
    g_patterns      = patterns;

    parse_lines(samples,   ASC_SAMPLE_LINE_SIZE,   MAX_SAMPLE_LINES,   g_samples);
    parse_lines(ornaments, ASC_ORNAMENT_LINE_SIZE, MAX_ORNAMENT_LINES, g_ornaments);

    parse_id_block();

    return true;
}

void parse_lines(uint16_t table, uint8_t line_size, uint8_t max_lines, ASC_LINES *lines)
{
    for(uint8_t i = 0; i < 32; i++)
    {
        uint16_t relative = read_word(table + i * 2);

        lines[i].offset = 0;
        lines[i].size   = 0;
        lines[i].loop   = 0;
        lines[i].limit  = 0;

        if(relative >= g_file_size || table + relative >= g_file_size) continue;

        /* Offsets are relative to the start of the list */
        uint16_t offset    = table + relative;
        uint16_t available = (g_file_size - offset) / line_size;
        uint8_t  count     = 0;

        while(count < max_lines && count < available)
        {
            uint8_t flags = g_data[offset + count * line_size];

            if(flags & LINE_LOOP_BEGIN) lines[i].loop  = count;
            if(flags & LINE_LOOP_END)   lines[i].limit = count;

            count++;

            if(flags & LINE_FINISHED) break;
        }

        lines[i].offset = offset;
        lines[i].size   = count;
    }
}

void parse_id_block(void)
{
    strcpy(g_title, "-");
    strcpy(g_author, "-");

    uint16_t id = g_order_list + g_order_size;

    if(g_patterns < id + ASC_ID_BLOCK_SIZE) return;
    if(memcmp(g_data + id, g_asc_id, ASC_ID_LENGTH)) return;

    if(trim_copy(g_title, g_data + id + ASC_ID_LENGTH, ASC_TITLE_LENGTH) == 0)
        strcpy(g_title, "-");

    /* Only an author when the separator reads ' BY ' (any case) */
    char by[ASC_BY_LENGTH + 1];
    uint8_t by_length = trim_copy(by, g_data + id + ASC_ID_LENGTH + ASC_TITLE_LENGTH, ASC_BY_LENGTH);

    if(by_length == 2 && (by[0] == 'B' || by[0] == 'b') && (by[1] == 'Y' || by[1] == 'y'))
    {
        if(trim_copy(g_author, g_data + id + ASC_ID_LENGTH + ASC_TITLE_LENGTH + ASC_BY_LENGTH, ASC_AUTHOR_LENGTH) == 0)
            strcpy(g_author, "-");
    }
}

/* Copies a space padded field without the padding, returns its length */
uint8_t trim_copy(char *destination, const uint8_t *source, uint8_t length)
{
    uint8_t first = 0;
    uint8_t last  = length;

    while(first < length && (source[first] <= 0x20 || source[first] >= 0x7F)) first++;
    while(last > first && (source[last - 1] <= 0x20 || source[last - 1] >= 0x7F)) last--;

    uint8_t count = 0;
    for(uint8_t i = first; i < last; i++)
    {
        uint8_t c = source[i];
        destination[count++] = (c >= 0x20 && c < 0x7F) ? c : '?';
    }

    destination[count] = 0;

    return count;
}

bool update(void)
{
    /* A new pattern line starts every 'tempo' interrupts */
    if(g_tick == 0) play_line();

    for(uint8_t i = 0; i < MAX_CHANNEL_NUMBER; i++)
        g_frame_volume[i] = 0;

    g_frame_mixer     = 0;
    g_frame_noise_set = false;

    for(uint8_t i = 0; i < MAX_CHANNEL_NUMBER; i++)
        synthesize_channel(i);

    write_psg();

    g_tick++;
    if(g_tick >= g_tempo) g_tick = 0;

    return true;
}

void rewind(const uint8_t subsong)
{
    subsong;

    memset(g_channels, 0, sizeof(g_channels));
    for(uint8_t i = 0; i < MAX_CHANNEL_NUMBER; i++)
    {
        g_channels[i].volume = 15;
        g_channels[i].sliding_target = NO_TARGET_NOTE;

        g_frame_tone[i] = 0;
    }

    g_env_tone         = 0;
    g_env_tone_changed = false;
    g_env_type         = 0;
    g_env_type_changed = false;

    g_tempo = g_initial_tempo;
    g_tick  = 0;

    g_order_position = 0;
    start_pattern();

    silence_psg();
}

void command(const uint8_t id)
{
    id;
}

float get_refresh(void)
{
    /* Written for the 50Hz interrupt of the ZX Spectrum */
    return 50.0;
}

uint8_t get_subsongs(void)
{
    return 0;
}

char* get_player_info(void)
{
    return "ASC Sound Master player V1.0 by RoboSoft Inc.";
}

char* get_title(void)
{
    return g_title;
}

char* get_author(void)
{
    return g_author;
}

char* get_description(void)
{
    return g_version ? "ASC Sound Master 1.xx+" : "ASC Sound Master 0.xx";
}

uint16_t read_word(uint16_t pos)
{
    return (uint16_t)(g_data[pos] | (g_data[pos + 1] << 8));
}

/* ---- Order list and patterns ---- */

void start_pattern(void)
{
    uint16_t entry = g_patterns + g_data[g_order_list + g_order_position] * 6;

    for(uint8_t i = 0; i < MAX_CHANNEL_NUMBER; i++)
    {
        /* Channel offsets are relative to the start of the pattern table */
        uint32_t offset = (uint32_t)g_patterns + read_word(entry + i * 2);

        /* Out of the file will end the pattern right away */
        g_parse[i].offset   = (offset < g_file_size) ? (uint16_t)offset : g_file_size;
        g_parse[i].period   = 0;
        g_parse[i].counter  = 0;
        g_parse[i].envelope = false;

        /* The base noise is reset at the start of every pattern */
        g_channels[i].base_noise = 0;
    }

    g_line = 0;
}

void next_position(void)
{
    g_order_position++;

    if(g_order_position >= g_order_size)
        g_order_position = g_loop_position;

    start_pattern();
}

bool pattern_ended(void)
{
    if(g_line >= MAX_PATTERN_LINES) return true;

    for(uint8_t i = 0; i < MAX_CHANNEL_NUMBER; i++)
    {
        if(!g_parse[i].counter && g_parse[i].offset >= g_file_size) return true;
    }

    /* Only channel A carries the end marker */
    if(!g_parse[0].counter && g_data[g_parse[0].offset] == CMD_END_OF_PATTERN) return true;

    return false;
}

void play_line(void)
{
    /* Guard against a module with only empty patterns */
    uint8_t guard = 0;

    while(pattern_ended())
    {
        next_position();

        if(++guard > g_order_size) return;
    }

    ASC_CELL cell;

    for(uint8_t i = 0; i < MAX_CHANNEL_NUMBER; i++)
    {
        if(g_parse[i].counter)
        {
            g_parse[i].counter--;
            continue;
        }

        parse_cell(i, &cell);
        apply_cell(i, &cell);

        g_parse[i].counter = g_parse[i].period;
    }

    g_line++;
}

uint8_t next_data_byte(ASC_PARSE *parse)
{
    if(parse->offset >= g_file_size) return CMD_STOP;

    return g_data[parse->offset++];
}

void parse_cell(uint8_t channel, ASC_CELL *cell)
{
    ASC_PARSE *parse = &g_parse[channel];

    memset(cell, 0, sizeof(ASC_CELL));

    for(;;)
    {
        uint8_t cmd = next_data_byte(parse);

        if(cmd <= CMD_NOTE_LAST)
        {
            cell->enable = true;

            if(cell->has_slide)
            {
                /* A note after a slide command is the target of the slide */
                cell->slide_to_note = true;
                cell->slide_target  = cmd;
            }
            else
            {
                cell->has_note = true;
                cell->note     = cmd;
            }

            if(parse->envelope)
            {
                cell->has_env_tone = true;
                cell->env_tone     = next_data_byte(parse);
            }

            break;
        }
        else if(cmd <= CMD_STOP_LAST)
        {
            break;
        }
        else if(cmd == CMD_BREAK_SAMPLE)
        {
            cell->break_sample = true;
            break;
        }
        else if(cmd == CMD_REST)
        {
            cell->disable = true;
            break;
        }
        else if(cmd <= CMD_SKIP_LAST)
        {
            parse->period = cmd - CMD_SKIP_FIRST;
        }
        else if(cmd <= CMD_SAMPLE_LAST)
        {
            cell->has_sample = true;
            cell->sample     = cmd - CMD_SAMPLE_FIRST;
        }
        else if(cmd <= CMD_ORNAMENT_LAST)
        {
            cell->has_ornament = true;
            cell->ornament     = cmd - CMD_ORNAMENT_FIRST;
        }
        else if(cmd == CMD_ENVELOPE_VOL)
        {
            cell->has_volume = true;
            cell->volume     = 15;
            cell->env_on     = true;
            cell->env_off    = false;
            parse->envelope  = true;
        }
        else if(cmd <= 0xEF)
        {
            cell->has_volume = true;
            cell->volume     = cmd - CMD_ENVELOPE_VOL;
            cell->env_on     = false;
            cell->env_off    = true;
            parse->envelope  = false;
        }
        else if(cmd == CMD_NOISE)
        {
            cell->has_noise = true;
            cell->noise     = next_data_byte(parse);
        }
        else if(cmd <= CMD_CONTINUE_LAST)
        {
            if(cmd & 1) cell->cont_sample = true;
            if(cmd & 2) cell->cont_ornament = true;
        }
        else if(cmd == CMD_TEMPO)
        {
            g_tempo = next_data_byte(parse);
        }
        else if(cmd == CMD_GLISSADE_DOWN || cmd == CMD_GLISSADE_UP)
        {
            int16_t amount = next_data_byte(parse);

            cell->has_gliss = true;
            cell->gliss     = (cmd == CMD_GLISSADE_DOWN) ? -16 * amount : 16 * amount;
        }
        else if(cmd == CMD_SLIDE_TONE || cmd == CMD_SLIDE)
        {
            if(cmd == CMD_SLIDE_TONE) cell->cont_sample = true;

            cell->has_slide   = true;
            cell->slide_tone  = (cmd == CMD_SLIDE_TONE);
            cell->slide_steps = (int8_t)next_data_byte(parse);
        }
        else if((cmd & 0xF9) == 0xF8)
        {
            /* 0xF8, 0xFA, 0xFC, 0xFE: envelope type = value for register 13 */
            cell->has_env_type = true;
            cell->env_type     = cmd & 0x0F;
        }
        else if(cmd == CMD_AMP_DELAY)
        {
            uint8_t step = next_data_byte(parse);

            cell->has_vol_slide    = true;
            cell->vol_slide_period = step & 31;
            cell->vol_slide_delta  = (step & 32) ? -1 : 1;
        }

        /* Anything else (0xFD, 0xFF in the middle of a cell) is ignored */
    }
}

void apply_cell(uint8_t channel, const ASC_CELL *cell)
{
    ASC_CHANNEL *ch = &g_channels[channel];

    bool reload = false;

    if(cell->enable)  ch->enabled = true;
    if(cell->disable) ch->enabled = false;

    /* Any new cell stops a running volume slide and slide */
    ch->vol_slide_counter = 0;
    ch->sliding_steps = 0;

    if(cell->has_env_type)
    {
        g_env_type = cell->env_type;
        g_env_type_changed = true;
    }

    if(cell->has_env_tone)
    {
        g_env_tone = cell->env_tone;
        g_env_tone_changed = true;
    }

    if(cell->env_on)  ch->envelope = true;
    if(cell->env_off) ch->envelope = false;

    if(cell->has_noise) ch->base_noise = cell->noise;

    if(cell->has_gliss)
    {
        ch->glissade = cell->gliss;
        ch->sliding_steps = -1;     /* Endless */
    }

    if(cell->has_slide)
    {
        if(!cell->slide_to_note)
        {
            /* Slide back to the note */
            int32_t new_sliding = ch->sliding & ~0x0FL;

            ch->sliding_steps = cell->slide_steps;
            ch->glissade = div_steps(-new_sliding, cell->slide_steps);
            ch->sliding = ch->glissade * cell->slide_steps;
        }
        else
        {
            int32_t difference = (int32_t)g_note_periods[cell->slide_target] - (int32_t)g_note_periods[ch->note];

            ch->sliding_steps  = cell->slide_steps;
            ch->sliding_target = cell->slide_target;

            if(cell->slide_tone) difference -= div_16(ch->sliding);

            ch->glissade = div_steps(16 * difference, cell->slide_steps);

            reload = true;
        }
    }

    if(cell->has_vol_slide)
    {
        ch->vol_slide_counter = cell->vol_slide_period;
        ch->vol_slide_delay   = cell->vol_slide_period;
        ch->vol_slide_addon   = cell->vol_slide_delta;
    }

    if(cell->break_sample) ch->break_sample = true;

    if(cell->has_ornament) ch->ornament_num = cell->ornament;
    if(cell->has_sample)   ch->sample_num = cell->sample;

    if(cell->has_note)
    {
        ch->note = cell->note;
        reload = true;
    }

    if(reload)
    {
        ch->current_noise = ch->base_noise;

        if(ch->sliding_steps <= 0) ch->sliding = 0;

        if(!cell->cont_sample)
        {
            ch->current_sample = ch->sample_num;
            ch->pos_in_sample  = 0;
            ch->volume_addon   = 0;
            ch->tone_deviation = 0;
            ch->break_sample   = false;
        }

        if(!cell->cont_ornament)
        {
            ch->current_ornament = ch->ornament_num;
            ch->pos_in_ornament  = 0;
            ch->note_addon       = 0;
        }
    }

    if(cell->has_volume) ch->volume = cell->volume;
}

/* ---- Sound generation ---- */

void synthesize_channel(uint8_t channel)
{
    ASC_CHANNEL *ch = &g_channels[channel];
    ASC_TICK tick;

    if(!ch->enabled) return;

    load_lines(ch, &tick);

    update_tone(channel, ch, &tick);
    update_volume(channel, ch, &tick);
    update_noise(channel, ch, &tick);
    update_slide(ch);
    next_lines(ch);
}

/* Decodes the sample and ornament line that is current for this channel */
void load_lines(const ASC_CHANNEL *ch, ASC_TICK *tick)
{
    const ASC_LINES *sample   = &g_samples[ch->current_sample];
    const ASC_LINES *ornament = &g_ornaments[ch->current_ornament];

    /* Lines outside the sample / ornament are silent / do nothing */
    tick->adding       = 0;
    tick->deviation    = 0;
    tick->level        = 0;
    tick->command      = SAMPLE_CMD_NONE;
    tick->tone_mask    = true;
    tick->noise_mask   = true;
    tick->note_offset  = 0;
    tick->noise_offset = 0;

    if(ch->pos_in_sample < sample->size)
    {
        const uint8_t *line = g_data + sample->offset + ch->pos_in_sample * ASC_SAMPLE_LINE_SIZE;

        tick->adding     = sign_extend_5(line[0]);
        tick->deviation  = (int8_t)line[1];
        tick->level      = line[2] >> 4;
        tick->noise_mask = (line[2] & 0x08) != 0;
        tick->command    = (line[2] & 0x06) >> 1;
        tick->tone_mask  = (line[2] & 0x01) != 0;
    }

    if(ch->pos_in_ornament < ornament->size)
    {
        const uint8_t *line = g_data + ornament->offset + ch->pos_in_ornament * ASC_ORNAMENT_LINE_SIZE;

        tick->noise_offset = sign_extend_5(line[0]);
        tick->note_offset  = (int8_t)line[1];
    }
}

void update_tone(uint8_t channel, ASC_CHANNEL *ch, const ASC_TICK *tick)
{
    ch->tone_deviation += tick->deviation;
    ch->note_addon += tick->note_offset;

    /* Same 8 bit arithmetic as the original player */
    int8_t half_tone = (int8_t)(ch->note + ch->note_addon);
    if(half_tone < 0) half_tone = 0;
    if(half_tone > CMD_NOTE_LAST) half_tone = CMD_NOTE_LAST;

    uint16_t period = g_note_periods[half_tone];
    period += ch->tone_deviation;
    period += div_16(ch->sliding);

    g_frame_tone[channel] = period & 0x0FFF;

    if(tick->tone_mask) g_frame_mixer |= (1 << channel);
}

void update_volume(uint8_t channel, ASC_CHANNEL *ch, const ASC_TICK *tick)
{
    int16_t volume_addon = ch->volume_addon;

    /* Volume slide set by a pattern command */
    if(ch->vol_slide_counter >= 2)
    {
        ch->vol_slide_counter--;
    }
    else if(ch->vol_slide_counter)
    {
        volume_addon += ch->vol_slide_addon;
        ch->vol_slide_counter = ch->vol_slide_delay;
    }

    /* Volume slide set by the sample */
    if(tick->command == SAMPLE_CMD_DEC_VOL) volume_addon--;
    if(tick->command == SAMPLE_CMD_INC_VOL) volume_addon++;

    if(volume_addon < -15) volume_addon = -15;
    if(volume_addon > 15)  volume_addon = 15;

    ch->volume_addon = (int8_t)volume_addon;

    int16_t level = volume_addon + tick->level;
    if(level < 0)  level = 0;
    if(level > 15) level = 15;

    level = ((ch->volume + 1) * level) >> 4;

    if(ch->envelope && tick->command == SAMPLE_CMD_ENVELOPE)
        level |= PSG_VOLUME_ENVELOPE;

    g_frame_volume[channel] = (uint8_t)level;
}

void update_noise(uint8_t channel, ASC_CHANNEL *ch, const ASC_TICK *tick)
{
    ch->current_noise += tick->noise_offset;

    if(tick->noise_mask && tick->command == SAMPLE_CMD_ENVELOPE)
    {
        /* Without noise the adding modifies the envelope period instead */
        g_env_tone += tick->adding;
        g_env_tone_changed = true;
    }
    else
    {
        ch->current_noise += tick->adding;
    }

    if(!tick->noise_mask)
    {
        g_frame_noise = (uint8_t)(ch->current_noise + div_256(ch->sliding)) & 0x1F;
        g_frame_noise_set = true;
    }
    else
    {
        g_frame_mixer |= (8 << channel);
    }
}

void update_slide(ASC_CHANNEL *ch)
{
    if(ch->sliding_steps == 0) return;

    if(ch->sliding_steps > 0)
    {
        ch->sliding_steps--;

        if(!ch->sliding_steps && ch->sliding_target != NO_TARGET_NOTE)
        {
            /* Slide to note is finished */
            ch->note = ch->sliding_target;
            ch->sliding_target = NO_TARGET_NOTE;
            ch->sliding = 0;
            ch->glissade = 0;
        }
    }

    ch->sliding += ch->glissade;
}

void next_lines(ASC_CHANNEL *ch)
{
    const ASC_LINES *sample   = &g_samples[ch->current_sample];
    const ASC_LINES *ornament = &g_ornaments[ch->current_ornament];

    if(ch->pos_in_sample++ >= sample->limit)
    {
        if(!ch->break_sample)
            ch->pos_in_sample = sample->loop;
        else if(ch->pos_in_sample >= sample->size)
            ch->enabled = false;
    }

    if(ch->pos_in_ornament++ >= ornament->limit)
        ch->pos_in_ornament = ornament->loop;
}

void write_psg(void)
{
    for(uint8_t i = 0; i < MAX_CHANNEL_NUMBER; i++)
    {
        g_roboplay_interface->psg_write(i * 2,     g_frame_tone[i] & 0xFF);
        g_roboplay_interface->psg_write(i * 2 + 1, g_frame_tone[i] >> 8);
    }

    if(g_frame_noise_set) g_roboplay_interface->psg_write(6, g_frame_noise);

    /* The two top bits are the I/O port directions on MSX, so they must stay as the BIOS sets them */
    g_roboplay_interface->psg_write(7, PSG_MIXER_PORT_BITS | (g_frame_mixer & 0x3F));

    for(uint8_t i = 0; i < MAX_CHANNEL_NUMBER; i++)
        g_roboplay_interface->psg_write(8 + i, g_frame_volume[i]);

    if(g_env_tone_changed)
    {
        g_roboplay_interface->psg_write(11, g_env_tone & 0xFF);
        g_roboplay_interface->psg_write(12, g_env_tone >> 8);
        g_env_tone_changed = false;
    }

    /* Writing register 13 restarts the envelope, so only do this when the module asks for it */
    if(g_env_type_changed)
    {
        g_roboplay_interface->psg_write(13, g_env_type);
        g_env_type_changed = false;
    }
}

void silence_psg(void)
{
    for(uint8_t i = 0; i < 7; i++)
        g_roboplay_interface->psg_write(i, 0);

    g_roboplay_interface->psg_write(7, PSG_MIXER_PORT_BITS | 0x3F);

    for(uint8_t i = 8; i < 13; i++)
        g_roboplay_interface->psg_write(i, 0);
}

/* ---- Helpers ---- */

int8_t sign_extend_5(uint8_t value)
{
    int8_t result = value & 0x1F;

    if(result & 0x10) result -= 32;

    return result;
}

/* Divisions that truncate towards zero, without a call to the library division */
int16_t div_16(int32_t value)
{
    return (int16_t)((value + ((value >> 31) & 15)) >> 4);
}

int16_t div_256(int32_t value)
{
    return (int16_t)((value + ((value >> 31) & 255)) >> 8);
}

int32_t div_steps(int32_t value, int8_t steps)
{
    return value / (steps ? steps : 1);
}