#include <kissStepper.h>
#include <Bounce2.h>
#ifndef UNO_TEST
    #include <Servo.h>
#endif

#include "global.h"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
// Motoren
kissStepper _stepper[6] = {
    kissStepper(J1_MODE, J1_DIR_PIN, J1_STEP_PIN),
    kissStepper(J2_MODE, J2_DIR_PIN, J2_STEP_PIN),
    kissStepper(J3_MODE, J3_DIR_PIN, J3_STEP_PIN),
    kissStepper(J4_MODE, J4_DIR_PIN, J4_STEP_PIN),
    kissStepper(J5_MODE, J5_DIR_PIN, J5_STEP_PIN),
    kissStepper(J6_MODE, J6_DIR_PIN, J6_STEP_PIN)
};
boolean _refOkay[6] = { false, false, false, false, false, false };
uint16_t _stopAcc[6];

#ifndef UNO_TEST
    // Servos
    Servo _servos[3] = {
        Servo(),
        Servo(),
        Servo()
    };
#endif

// Endschalter
const uint8_t _limitswitchPins[6] = { 
    J1_LIMIT_SWITCH_PIN, 
    J2_LIMIT_SWITCH_PIN, 
    J3_LIMIT_SWITCH_PIN, 
    J4_LIMIT_SWITCH_PIN, 
    J5_LIMIT_SWITCH_PIN,
    J6_LIMIT_SWITCH_PIN
};
Bounce _limitswitches[6];
// Not-Halt
Bounce _emergencystop;

void _roboStop() {
    for (uint8_t i = 0; i < 6; i++) {
        _stepper[i].setAccel(_stopAcc[i]);
        _stepper[i].decelerate();
    }
}

void _roboFastStop() {
    for (uint8_t i = 0; i < 6; i++) {
        _stepper[i].stop();
    }
}

bool _checkRoboStop() {
    if (Serial.available() > 0) if (Serial.read() == '!') {
        _roboStop();
        return true;
    }
    return false;
}

bool _checkEstop() {
    // Nothalt prüfen
    _emergencystop.update();
    if (_emergencystop.fell()) {
        _roboFastStop();
        sendRES();
        sendESS();
        for (uint8_t i = 0; i < 6; i++) _refOkay[i] = false;
        return true;
    }
    return false;
}

void _updateLimitSwitches() {
    for (uint8_t i = 0; i < 6; i++) {
        _limitswitches[i].update();
    }
}

void _roboInitMove(kissStepper &mot, int32_t target, uint16_t minSpeed, uint16_t speed, uint16_t acc) {
    mot.setMinSpeed(minSpeed);
    mot.setAccel(acc);
    mot.setMaxSpeed(speed);
    mot.moveTo(target);
}

bool _roboWork() {
    bool working = false;
    for (uint8_t i = 0; i < 6; i++) {
        working |= _stepper[i].work();
    }
    return working;
}

void _setJointLimits() {
    _stepper[0].forwardLimit = J1_POS_STEP_LIMIT;
    _stepper[0].reverseLimit = J1_NEG_STEP_LIMIT;
    _stepper[1].forwardLimit = J2_POS_STEP_LIMIT;
    _stepper[1].reverseLimit = J2_NEG_STEP_LIMIT;
    _stepper[2].forwardLimit = J3_POS_STEP_LIMIT;
    _stepper[2].reverseLimit = J3_NEG_STEP_LIMIT;
    _stepper[3].forwardLimit = J4_POS_STEP_LIMIT;
    _stepper[3].reverseLimit = J4_NEG_STEP_LIMIT;
    _stepper[4].forwardLimit = J5_POS_STEP_LIMIT;
    _stepper[4].reverseLimit = J5_NEG_STEP_LIMIT;
    _stepper[5].forwardLimit = J6_POS_STEP_LIMIT;
    _stepper[5].reverseLimit = J6_NEG_STEP_LIMIT;
}

// -----------------------------------------------------------------------------
// Public
// -----------------------------------------------------------------------------
void roboMOV() {
    if (MsgData.cnt > 0) {
        for (uint8_t i = 0; i < MsgData.cnt; i++) {
            uint8_t nr = MsgData.parSet[i][0] - 1;
            int32_t target = MsgData.parSet[i][1];
            uint16_t minSpeed = MsgData.parSet[i][2];
            uint16_t speed = MsgData.parSet[i][3];
            uint16_t acc = MsgData.parSet[i][4];
            _stopAcc[nr] = MsgData.parSet[i][5];

            // Referenz prüfen
            if (!_refOkay[nr]) {
                sendERR(ERR_NO_REF);
                _roboFastStop();
                return;
            }

            // Daten prüfen
            if (nr > 5 || minSpeed == 0 || speed == 0 || _stopAcc[nr] == 0) {
                sendERR(ERR_PARAMETER_ERR);
                _roboFastStop();
                return;
            }
            // Fahrt initieren
            _roboInitMove(_stepper[nr], target, minSpeed, speed, acc);
        }
        sendACK(); // Telegramm bestätigen
        // Achsen verfahren
        while (_roboWork()) {
            _checkEstop();
            _checkRoboStop();
        }
    }
    // Fertig
    sendPOS();
    sendFIN();
}

void roboREF() {
    #ifdef UNO_TEST // Simulation
        // Daten prüfen
        for (uint8_t i = 0; i < MsgData.cnt; i++) {
            uint8_t nr = MsgData.parSet[i][0] - 1;
            bool dir = MsgData.parSet[i][1] == 0 ? false : true;
            uint16_t minSpeed = MsgData.parSet[i][2];
            uint16_t speedFast = MsgData.parSet[i][3];
            uint16_t speedSlow = MsgData.parSet[i][4];
            uint16_t acc = MsgData.parSet[i][5];
            uint16_t maxStepsBack = MsgData.parSet[i][6];
            _stopAcc[nr] = MsgData.parSet[i][7];
            // Daten prüfen
            if (nr > 5 || minSpeed == 0 || speedFast == 0 || speedSlow == 0 || acc == 0 || maxStepsBack == 0 || _stopAcc[nr] == 0) {
                sendERR(ERR_PARAMETER_ERR);
                return; // Referenzfahrt abbrechen
            }
        }
        sendACK();
        // Referenzfahrt abschließen
        for (uint8_t i = 0; i < MsgData.cnt; i++) {
            uint8_t nr = MsgData.parSet[i][0] - 1;
            _stepper[nr].setPos(0);
            _refOkay[nr] = true;
        }
        sendPOS();
        sendFIN();
        return;
    #endif
    
    bool stopRef = false;
    // ********************************************
    // Schritt 1: Schnell auf Endschalter fahren
    // ********************************************
    for (uint8_t i = 0; i < MsgData.cnt; i++) {
        uint8_t nr = MsgData.parSet[i][0] - 1;
        bool dir = MsgData.parSet[i][1] == 0 ? false : true;
        uint16_t minSpeed = MsgData.parSet[i][2];
        uint16_t speedFast = MsgData.parSet[i][3];
        uint16_t speedSlow = MsgData.parSet[i][4];
        uint16_t acc = MsgData.parSet[i][5];
        uint16_t maxStepsBack = MsgData.parSet[i][6];
        _stopAcc[nr] = MsgData.parSet[i][7];
        // Daten prüfen
        if (nr > 5 || minSpeed == 0 || speedFast == 0 || speedSlow == 0 || acc == 0 || maxStepsBack == 0 || _stopAcc[nr] == 0) {
            _roboFastStop();
            sendERR(ERR_PARAMETER_ERR);
            return; // Referenzfahrt abbrechen
        }
        // Fahrt initieren
        _stepper[nr].setPos(dir ? _stepper[nr].reverseLimit : _stepper[nr].forwardLimit);
        _roboInitMove(_stepper[nr], dir ? _stepper[nr].forwardLimit : _stepper[nr].reverseLimit, minSpeed, speedFast, acc);
    }
    sendACK(); // Telegramm bestätigen
    // Endschalter schon belegt? => dann gleich "stoppen"
    _updateLimitSwitches();
    for (uint8_t i = 0; i < 6; i++) if (_limitswitches[i].read()) _stepper[i].stop();
    // Achsen verfahren...
    while (_roboWork()) {
        if (_checkEstop()) return;	//bei Nothalt Referenzfahrt abbrechen
        stopRef |= _checkRoboStop();
        _updateLimitSwitches();
        for (uint8_t i = 0; i < 6; i++) if (_limitswitches[i].rose()) {
            _stepper[i].stop();
            sendLSS();
        }
    }
    if (stopRef) {
        sendERR(ERR_REF_CANCELED);
        return; //Referenzfahrt abbrechen
    }
    //Wurden Endschalter wirklich erreicht?
    for (uint8_t i = 0; i < MsgData.cnt; i++) {
        uint8_t nr = MsgData.parSet[i][0];
        if (!_limitswitches[nr - 1].read()) {
            _roboFastStop();
            sendERR(ERR_REF_FAILED_STEP1);
            return; // Referenzfahrt abbrechen
        }
    }

    // 500ms Warten
    delay(500);

    // ********************************************
    // Schritt 2: vom Endschalter runterfahren
    // ********************************************
    for (uint8_t i = 0; i < MsgData.cnt; i++) {
        uint8_t nr = MsgData.parSet[i][0] - 1;
        bool dir = MsgData.parSet[i][1] == 0 ? false : true;
        uint16_t minSpeed = MsgData.parSet[i][2];
        uint16_t speedFast = MsgData.parSet[i][3];
        uint16_t acc = MsgData.parSet[i][5];
        uint16_t maxStepsBack = MsgData.parSet[i][6];
        // Fahrt initieren
        _stepper[nr].setPos(0);
        _roboInitMove(_stepper[nr], dir ? -(int32_t)maxStepsBack : (int32_t)maxStepsBack, minSpeed, speedFast, acc);
    }
    // Endschalter schon frei? => dann gleich "stoppen"
    _updateLimitSwitches();
    for (uint8_t i = 0; i < 6; i++) if (!_limitswitches[i].read()) _stepper[i].stop();
    // Achsen verfahren...
    while (_roboWork()) {
        if (_checkEstop()) return;	//bei Nothalt Referenzfahrt abbrechen
        stopRef |= _checkRoboStop();
        _updateLimitSwitches();
        for (uint8_t i = 0; i < 6; i++) if (_limitswitches[i].fell()) {
            _stepper[i].decelerate();
            sendLSS();
        }
    }
    if (stopRef) {
        sendERR(ERR_REF_CANCELED);
        return; //Referenzfahrt abbrechen
    }
    //Wurden Endschalter wirklich verlassen?
    for (uint8_t i = 0; i < MsgData.cnt; i++) {
        uint8_t nr = MsgData.parSet[i][0];
        if (_limitswitches[nr - 1].read()) {
            _roboFastStop();
            sendERR(ERR_REF_FAILED_STEP2);
            return; // Referenzfahrt abbrechen
        }
    }
    // 250ms Warten
    delay(250);

    // ********************************************
    // Schritt 3: Langsam auf Endschalter fahren
    // ******************************************** 
    for (uint8_t i = 0; i < MsgData.cnt; i++) {
        uint8_t nr = MsgData.parSet[i][0] - 1;
        bool dir = MsgData.parSet[i][1] == 0 ? false : true;
        uint16_t minSpeed = MsgData.parSet[i][2];
        uint16_t speedSlow = MsgData.parSet[i][4];
        uint16_t acc = MsgData.parSet[i][5];
        // Fahrt initieren
        _stepper[nr].setPos(dir ? _stepper[nr].reverseLimit : _stepper[nr].forwardLimit);
        _roboInitMove(_stepper[nr], dir ? _stepper[nr].forwardLimit : _stepper[nr].reverseLimit, minSpeed, speedSlow, acc);
    }
    // Endschalter schon belegt? => dann gleich "stoppen"
    _updateLimitSwitches();
    for (uint8_t i = 0; i < 6; i++) if (_limitswitches[i].read()) _stepper[i].stop();
    // Achsen verfahren...
    while (_roboWork()) {
        if (_checkEstop()) return;	//bei Nothalt Referenzfahrt abbrechen
        stopRef |= _checkRoboStop();
        _updateLimitSwitches();
        for (uint8_t i = 0; i < 6; i++) if (_limitswitches[i].rose()) {
            _stepper[i].stop();
            sendLSS();
        }
    }
    if (stopRef) {
        sendERR(ERR_REF_CANCELED);
        return; //Referenzfahrt abbrechen
    }
    //Wurden Endschalter wirklich erreicht?
    for (uint8_t i = 0; i < MsgData.cnt; i++) {
        uint8_t nr = MsgData.parSet[i][0];
        if (!_limitswitches[nr - 1].read()) {
            _roboFastStop();
            sendERR(ERR_REF_FAILED_STEP3);
            return; // Referenzfahrt abbrechen
        }
    }

    // ********************************************
    // Schritt 4: Referenzfahrt abschließen
    // ******************************************** 
    for (uint8_t i = 0; i < MsgData.cnt; i++) {
        uint8_t nr = MsgData.parSet[i][0] - 1;
        _stepper[nr].setPos(0);
        _refOkay[nr] = true;
    }
    sendPOS();
    sendFIN();
}

void roboSRV() {
    if (MsgData.parSet[0][0] > 3 || MsgData.parSet[0][0] < 1 || MsgData.parSet[0][2] < 1 || MsgData.parSet[0][2] > 100) {
        sendERR(ERR_PARAMETER_ERR);
        return;
    }
    sendACK();
    #ifndef UNO_TEST
    int srvIndex = MsgData.parSet[0][0] - 1;
    int srvTarget = MsgData.parSet[0][1];
    unsigned int srvDelay = trunc(-0.8F * (float)MsgData.parSet[0][2] + 100.0F);
    if (srvDelay == 0) {
        _servos[srvIndex].write(srvTarget);
    } else {
        int srvPos = _servos[srvIndex].read();        
        int interval = srvPos < srvTarget ? 1 : -1;
        for (int i = srvPos + interval; srvPos < srvTarget ? i <= srvTarget : i >= srvTarget; i += interval) {
            _servos[srvIndex].write(i);
            if (i != srvTarget) {
                for (unsigned int j = 1; j <= srvDelay; j++) delayMicroseconds(100);
            }
        }
    }
    sendSRV();
    #else
    sendSRV(MsgData.parSet[0][0], MsgData.parSet[0][1]);
    #endif
    sendFIN();
}

void roboWAI() {
    if (MsgData.parSet[0][0] <= 0) {
        sendERR(ERR_PARAMETER_ERR);
        return;
    }
    sendACK();
    //warten
    delay(MsgData.parSet[0][0]);
    sendFIN();
}

bool roboGetRefOkay(int i) {
    return _refOkay[i];
}

int32_t roboGetStepperPos(int i) {
    return _stepper[i].getPos();
}

int roboGetServoPos(int i) {
    #ifndef UNO_TEST
        return _servos[i].read();
    #endif
}

bool roboGetEstop() {
    return _emergencystop.read();
}

bool roboGetLimitSwitch(int i) {
    return _limitswitches[i].read();
}

// -----------------------------------------------------------------------------
// Loop
// -----------------------------------------------------------------------------
void roboLoop() {
    // Endschalter einlesen
    _updateLimitSwitches();
    for (int i = 0; i < 6; i++) {
        if (_limitswitches[i].rose() || _limitswitches[i].fell()) sendLSS();
    }

    // Nothalt prüfen
    _emergencystop.update();
    if (_emergencystop.rose() || _emergencystop.fell()) {
        sendRES();
        sendESS();
        if (!_emergencystop.read()) for (uint8_t i = 0; i < 6; i++) _refOkay[i] = false;
    }
}

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------
void roboCtrlSetup() {
    // Enschalter
    for (uint8_t i = 0; i < 6; i++) {
        _limitswitches[i].attach(_limitswitchPins[i], INPUT_PULLUP);
        _limitswitches[i].interval(3);
    }
    // Nothalt
    _emergencystop.attach(EMERGENCY_STOP_PIN, INPUT_PULLUP);
    _emergencystop.interval(3);
    // Schrittmotoren initialisieren
    for (uint8_t i = 0; i < 6; i++) _stepper[i].begin();
    // Achslimits setzen
    _setJointLimits();

    // Servos initialisieren
#ifndef UNO_TEST
    _servos[0].attach(SERVO1_PIN);
    _servos[1].attach(SERVO2_PIN);
    _servos[2].attach(SERVO3_PIN);
#endif // UNO_TES
}