//TODO
//Referenzstatus senden

#include "global.h"

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------
void setup(void) {
    // Kommunikation
    void comSetup();
    // Init
    sendRES();
}

// -----------------------------------------------------------------------------
// Loop
// -----------------------------------------------------------------------------
void loop(void) {
    //Kommunikation
    comLoop();
}