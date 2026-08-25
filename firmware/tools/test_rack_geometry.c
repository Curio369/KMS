/* Host self-check for rack_geometry.h.
 *
 * This is the calculation that decides which key a member walks away with.
 * It is pure C so it can be proved on a laptop, with no Uno, no motor and no
 * rack:
 *
 *   cc -Wall -Wextra -o /tmp/test_rack_geometry \
 *      firmware/tools/test_rack_geometry.c && /tmp/test_rack_geometry
 */

#include <assert.h>
#include <stdio.h>

#include "../KMS_LowLevel_ArduinoUno/rack_geometry.h"

int main(void) {
  int slot;
  long prev;

  printf("rack: %d slots, %d steps/rev, %.4f steps/slot, %.2f deg/slot\n",
         SLOT_COUNT, STEPS_PER_REV,
         (double)STEPS_PER_REV / SLOT_COUNT, 360.0 / SLOT_COUNT);

  /* --- the anchors ------------------------------------------------------- */
  /* Slot 1 is home. If this ever becomes 66 the whole rack is off by one. */
  assert(rack_slot_to_steps(1) == 0);

  /* Slots that land on exact step boundaries — these are the ones the broken
     divide-first formula gets wrong, so they are the regression guard.
     divide-first would give 198, 792 and 1518 respectively. */
  assert(rack_slot_to_steps(4) == 200);
  assert(rack_slot_to_steps(13) == 800);
  assert(rack_slot_to_steps(24) == 1533);   /* (1600*23)/24 = 1533.33 -> 1533 */

  /* --- monotonic and inside one revolution ------------------------------- */
  prev = -1;
  for (slot = 1; slot <= SLOT_COUNT; slot++) {
    const long steps = rack_slot_to_steps(slot);
    assert(steps > prev);                 /* strictly increasing */
    assert(steps >= 0);
    assert(steps < STEPS_PER_REV);        /* never past a full turn */
    prev = steps;
  }

  /* --- accuracy ---------------------------------------------------------- */
  /* Every slot must sit within one step of its ideal position. The
     divide-first bug fails this at slot 4 and is 16 steps out by slot 24. */
  for (slot = 1; slot <= SLOT_COUNT; slot++) {
    const double ideal =
        (double)STEPS_PER_REV * SLOT_TO_INDEX(slot) / SLOT_COUNT;
    const double err = rack_slot_to_steps(slot) - ideal;
    assert(err > -1.0 && err < 1.0);
  }

  /* --- range checking ---------------------------------------------------- */
  assert(rack_slot_valid(1));
  assert(rack_slot_valid(SLOT_COUNT));
  assert(!rack_slot_valid(0));            /* a garbled line, not "slot zero" */
  assert(!rack_slot_valid(-1));
  assert(!rack_slot_valid(SLOT_COUNT + 1));

  printf("rack_geometry.h: all checks passed\n");
  return 0;
}
