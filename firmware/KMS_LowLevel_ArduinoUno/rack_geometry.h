#ifndef RACK_GEOMETRY_H
#define RACK_GEOMETRY_H

/*
  Slot -> step arithmetic for the rotating rack.

  Pure C, no Arduino types, and in its own header for one reason: this is the
  calculation that decides which physical key a member walks away with, and it
  has a failure mode that a bench test of the first two slots will not catch.
  firmware/tools/test_rack_geometry.c exercises this file on the host, against
  the real numbers, with no board attached.

  The rack is a carousel: SLOT_COUNT keys evenly spaced around one revolution,
  moved by a single stepper. Slot 1 is the home position.
*/

/* --- Motor ---------------------------------------------------------------
   Must match the TB6600 DIP switches. Divisor = pulses_per_rev / 200.
   400->2, 800->4, 1600->8, 3200->16, 6400->32 */
#ifndef MOTOR_FULL_STEPS_PER_REV
#define MOTOR_FULL_STEPS_PER_REV 200   /* NEMA 17, 1.8 deg/step */
#endif
#ifndef MICROSTEP_DIVISOR
#define MICROSTEP_DIVISOR 8            /* TB6600 at 1/8 step */
#endif
#ifndef STEPS_PER_REV
#define STEPS_PER_REV (MOTOR_FULL_STEPS_PER_REV * MICROSTEP_DIVISOR)  /* 1600 */
#endif

/* --- Rack ----------------------------------------------------------------- */
#ifndef SLOT_COUNT
#define SLOT_COUNT 24
#endif

/* Slot 1 sits at step 0. If slot 1 is one position out on the real rack, this
   is the line to change — not the backend, and not the ESP32. */
#define SLOT_TO_INDEX(slot) ((slot) - 1)

/* Absolute step target for a 1-based slot number.

   MULTIPLY FIRST, THEN DIVIDE. The obvious spelling —

       (STEPS_PER_REV / SLOT_COUNT) * index

   is wrong in C, and wrong in a way that looks correct on the first few slots.
   1600 / 24 is integer division: it gives 66, not 66.67. Multiplying that
   truncated value back up loses a third of a step per slot and the error walks
   around the platter:

       slot  3 ->  198 steps, want  200   (0.45 deg out)
       slot 12 ->  792 steps, want  800   (1.8 deg out)
       slot 24 -> 1584 steps, want 1600   (3.6 deg out — the revolution
                                           never closes)

   Multiplying first leaves only the final division to truncate, so the worst
   case is under a single step (0.15 deg).

   The long cast is not decoration either: 1600 * 24 is 38400, which overflows
   the AVR's 16-bit int. */
static long rack_slot_to_steps(int slot) {
  return ((long)STEPS_PER_REV * SLOT_TO_INDEX(slot)) / SLOT_COUNT;
}

/* 1 when `slot` names a real slot. Slots are 1-based, so 0 is not "the first
   one" — it is a garbled line, and treating it as a position would send the
   platter home and hand out the wrong key. */
static int rack_slot_valid(long slot) {
  return slot >= 1 && slot <= SLOT_COUNT;
}

#endif /* RACK_GEOMETRY_H */
