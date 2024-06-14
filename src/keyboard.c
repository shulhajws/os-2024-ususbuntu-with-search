#include "header/cpu/portio.h"
#include "header/driver/framebuffer.h"
#include "header/driver/keyboard.h"
#include "header/stdlib/string.h"
#include "header/cpu/interrupt.h"
#include "header/cpu/idt.h"

const char keyboard_scancode_1_to_ascii_map[256] = {
      0, 0x1B, '1', '2', '3', '4', '5', '6',  '7', '8', '9',  '0',  '-', '=', '\b', '\t',
    'q',  'w', 'e', 'r', 't', 'y', 'u', 'i',  'o', 'p', '[',  ']', '\n',   0,  'a',  's',
    'd',  'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0, '\\',  'z', 'x',  'c',  'v',
    'b',  'n', 'm', ',', '.', '/',   0, '*',    0, ' ',   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0, '-',    0,    0,   0,  '+',    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,

      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
};

char shift_map[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // a
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // b
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // c
    0, 0, 0, 0, 0, 0, 0, 0, 0, '"',
    0, 0, 0, 0, '<', '_', '>', '?', ')', '!',
    '@', '#', '$', '%', '^', '&', '*', '(', 0, ':',
    0, '+', 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, '{', '|', '}', 0, 0, '~'

};

static struct KeyboardDriverState keyboard_state;

/* -- Driver Interfaces -- */

// Activate keyboard ISR / start listen keyboard & save to buffer
void keyboard_state_activate(void)
{
    // memset(keyboard_state.keyboard_buffer, '\0', sizeof(keyboard_state.keyboard_buffer));
    keyboard_state.buffer_index = 0;
    keyboard_state.keyboard_input_on = true;
    keyboard_state.shift_on = false;
    keyboard_state.capslock_on = false;
    keyboard_state.ctrl_on = false;
    framebuffer_state.start_col = framebuffer_state.cur_col;
    framebuffer_state.start_row = framebuffer_state.cur_row;
    framebuffer_set_cursor(framebuffer_state.cur_row, framebuffer_state.cur_col);
}

// Deactivate keyboard ISR / stop listening keyboard interrupt
void keyboard_state_deactivate(void)
{
    keyboard_state.keyboard_input_on = false;
}

void get_keyboard_buffer(char* buf, int32_t* retcode)
{
    buf[0] = keyboard_state.keyboard_buffer;

    if (keyboard_state.keyboard_buffer == 0)
    {
        *retcode = -1;
    }
    else
    {
        *retcode = 0;
        keyboard_state.keyboard_buffer = 0;
    }
}

// Scroll framebuffer up by 1 row, will clear the last row to empty
void scroll_up() {
    memcpy(FRAMEBUFFER_MEMORY_OFFSET, FRAMEBUFFER_MEMORY_OFFSET + MAX_COLUMN * 2, MAX_COLUMN * 2 * MAX_ROW - MAX_COLUMN * 2);
    framebuffer_state.start_row--;
    framebuffer_state.cur_row--;
    for (int i = 0; i < MAX_COLUMN; i++) {
        framebuffer_write(framebuffer_state.cur_row, i, ' ', 0xF, 0);
    }
}

/* -- Keyboard Interrupt Service Routine -- */

/**
 * Handling keyboard interrupt & process scancodes into ASCII character.
 * Will start listen and process keyboard scancode if keyboard_input_on.
 *
 * Will only print printable character into framebuffer.
 * Stop processing when enter key (line feed) is pressed.
 *
 * Note that, with keyboard interrupt & ISR, keyboard reading is non-blocking.
 * This can be made into blocking input with `while (is_keyboard_blocking());`
 * after calling `keyboard_state_activate();`
 */

uint8_t get_current_buffer_index()
{
    return framebuffer_state.cur_col - framebuffer_state.start_col + (framebuffer_state.cur_row - framebuffer_state.start_row) * MAX_COLUMN;
}

void keyboard_isr(void)
{
    if (!keyboard_state.keyboard_input_on)
        keyboard_state.buffer_index = 0;
    else
    {
        uint8_t scancode = in(KEYBOARD_DATA_PORT);
        // Handle extended scancode
        switch (scancode)
        {
            // Capslock key
        case 0x3a:
            keyboard_state.capslock_on ^= true;
            break;
            // Shift key released
        case 0x2a:
            keyboard_state.shift_on = true;
            break;
        case 0xaa:
            keyboard_state.shift_on = false;
            break;
        case 0x36:
            keyboard_state.shift_on = true;
            break;
        case 0xb6:
            keyboard_state.shift_on = false;
            break;
            // Ctrl key
        case 0x1d:
            keyboard_state.ctrl_on = true;
            break;
        case 0x9d:
            keyboard_state.ctrl_on = false;
            break;
        case 0x4b:
            // Left arrow
            if (get_current_buffer_index() == 0)
                break;
            framebuffer_state.cur_col--;
            if (framebuffer_state.cur_col < 0)
            {
                framebuffer_state.cur_col = MAX_COLUMN - 1;
                framebuffer_state.cur_row--;
            }
            framebuffer_set_cursor(framebuffer_state.cur_row, framebuffer_state.cur_col);
            break;
        case 0x4d:
            // Right arrow
            framebuffer_state.cur_col++;
            if (framebuffer_state.cur_col == MAX_COLUMN)
            {
                framebuffer_state.cur_col = 0;
                framebuffer_state.cur_row++;
            }
            framebuffer_set_cursor(framebuffer_state.cur_row, framebuffer_state.cur_col);
            break;
        case 0x48:
            // Up arrow
            if (framebuffer_state.cur_row > framebuffer_state.start_row)
            {
                framebuffer_state.cur_row--;
                framebuffer_set_cursor(framebuffer_state.cur_row, framebuffer_state.cur_col);
            }
            break;
        case 0x50:
            // Down arrow
            if (framebuffer_state.cur_row < MAX_ROW - 1)
            {
                framebuffer_state.cur_row++;
                framebuffer_set_cursor(framebuffer_state.cur_row, framebuffer_state.cur_col);
            }
            break;
        default:
            break;
        }
        char ascii_char = keyboard_scancode_1_to_ascii_map[scancode];

        // check char valid
        if (ascii_char == 0)
        {
            pic_ack(IRQ_KEYBOARD);
            return;
        }
        // Ctrl + C
        if (keyboard_state.ctrl_on && (ascii_char == 'c' || ascii_char == 'C'))
        {
            // Kill terminal on Ctrl+C
            keyboard_state_deactivate();
        }
        // Backspace
        else if (ascii_char == '\b')
        {
            if (keyboard_state.buffer_index > 0)
            {
                keyboard_state.buffer_index--;
                keyboard_state.keyboard_buffer = '\b';
                if (framebuffer_state.cur_col == 0)
                {
                    framebuffer_state.cur_row--;
                    framebuffer_state.cur_col = MAX_COLUMN - 1;
                    if (framebuffer_state.cur_row < 0)
                    {
                        framebuffer_state.cur_row = 0;
                        framebuffer_state.cur_col = 0;
                    }
                }
                else
                {
                    framebuffer_state.cur_col--;
                }

                framebuffer_write(framebuffer_state.cur_row, framebuffer_state.cur_col, ' ', 0xFF, 0);
            }
        }
        // Newline Enter
        else if (ascii_char == '\n')
        {
            keyboard_state.keyboard_buffer = '\n';
            keyboard_state_deactivate();
            framebuffer_state.cur_row++;
            framebuffer_state.cur_col = 0;
            // Scroll up if the screen is full
            if (framebuffer_state.cur_row == MAX_ROW) scroll_up();
        }
        else
        {
            // Check if the character is a lowercase letter
            if (ascii_char >= 'a' && ascii_char <= 'z')
            {
                if (keyboard_state.capslock_on ^ keyboard_state.shift_on)
                    ascii_char = 'A' + ascii_char - 'a';
            }
            // Map the character to the shifted character if the shift_on key is pressed
            else if (keyboard_state.shift_on && ascii_char < 97 && shift_map[(uint8_t)ascii_char] != 0)
            {
                ascii_char = shift_map[(uint8_t)ascii_char];
            }
            keyboard_state.keyboard_buffer = ascii_char;
            keyboard_state.buffer_index++;

            // Print the character to the screen
            if (framebuffer_state.cur_col >= MAX_COLUMN)
            {
                framebuffer_state.cur_row++;
                framebuffer_state.cur_col = 0;
            }
            framebuffer_write(framebuffer_state.cur_row, framebuffer_state.cur_col, ascii_char, 0xFF, 0);
            framebuffer_state.cur_col++;
            // Scroll up if the screen is full
            if (framebuffer_state.cur_row == MAX_ROW) scroll_up();
        }
        framebuffer_set_cursor(framebuffer_state.cur_row, framebuffer_state.cur_col);
    }
    pic_ack(IRQ_KEYBOARD);
}

bool is_keyboard_blocking(void) {
    return keyboard_state.keyboard_input_on;
}