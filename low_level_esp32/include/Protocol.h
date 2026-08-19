#ifndef PROTOCOL_H
#define PROTOCOL_H

/*
  Simple ASCII Protocol for High-Low Level ESP32 Communication
  Commands are terminated by a newline '\n'

  High -> Low Commands:
  - GOTO:<position>   -> Move stepper to <position>
  - ACTUATE:<1/0>     -> Engage(1) or Disengage(0) the solenoid
  - BATT:?            -> Request battery percentage
  - STATUS:?          -> Request system status

  Low -> High Responses:
  - ACK:<cmd>         -> Command acknowledged and started
  - DONE:<cmd>        -> Command finished successfully
  - ERR:<msg>         -> Error occurred
  - BATT:<%>          -> Battery level response
  - STATUS:<state>    -> Status response (IDLE, MOVING, ERROR)
*/

enum SystemState { STATE_IDLE, STATE_MOVING, STATE_ERROR };

#endif // PROTOCOL_H
