#ifndef NIMBLE_MIDI_H
#define NIMBLE_MIDI_H

#include <stdint.h>

// Initialize the BLE MIDI Stack
void nimble_midi_init(char *device_name);

// Send a MIDI Message (3 bytes: Status, Data1, Data2)
void nimble_midi_send(uint8_t status, uint8_t data1, uint8_t data2);

#endif