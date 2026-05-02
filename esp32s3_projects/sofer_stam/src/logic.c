#include "logic.h"
#include <stdio.h> // Needed for sprintf
// 1. PREPROCESSOR SECTION
// We define the limit once here. If we need to change it later to 128,
// we only change it in this one spot.

#define MAX_MSG_LEN 64
// This 'buffer' is where we will store the string we want to display



// 2. PERSISTENT MEMORY SECTION
// These live in the Data Segment of the RAM.
static int press_count = 0;
static int last_btn1_state = 0;
static char message_buffer[MAX_MSG_LEN]; // Our fixed-size "parking lot" for text
//Static, but plenty of room.

const char* get_message(int btn1_pressed, int btn2_pressed) {

    // --- EDGE DETECTION ---
    // If button is currently 1 (pressed) AND it was 0 last time...
    if (btn1_pressed == 1 && last_btn1_state == 0) {
        press_count++; // We found a click!
    }

    // --- RESET LOGIC ---
    // Use the second button to zero out the counter
    if (btn2_pressed == 1) {
        press_count = 0;
    }

    // --- STATE UPDATE ---
    // Save the current state to be the 'last' state for the next loop
    last_btn1_state = btn1_pressed;

    // --- OUTPUT FORMATTING ---
    // snprintf is the "Seatbelt" of C.
    // It says: "Write this string, but STOP if you hit 64 characters."
    // The %-5d ensures we always occupy 5 slots to prevent ghosting.
    // We format the string: "Count: {decimal}     "
    snprintf(message_buffer, MAX_MSG_LEN, "Count: %-5d", press_count); // (str_name, max_str_size, format_string, value)
    // Return the pointer to the buffer so the screen can print it
    return message_buffer;
}