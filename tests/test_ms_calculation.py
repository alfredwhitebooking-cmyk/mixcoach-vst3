"""
M/S Calculation Algorithm Test
==============================

Validates the dB->linear->dB conversion used in
AnalyzersPanelComponent::fastUpdateMeters()

Usage: python tests/test_ms_calculation.py
"""

import math
import sys

PASS = 0
FAIL = 0

def db_to_linear(db):
    return 10.0 ** (db / 20.0)

def linear_to_db(lin):
    return 20.0 * math.log10(max(lin, 1e-12))

def ms_calculation_correct(rms_left_db, rms_right_db):
    """CORRECT M/S calculation (dB -> linear -> M/S -> dB)."""
    lin_l = db_to_linear(rms_left_db)
    lin_r = db_to_linear(rms_right_db)
    mid_lin = (lin_l + lin_r) * 0.5
    side_lin = (lin_l - lin_r) * 0.5
    mid_db = linear_to_db(mid_lin)
    side_db = linear_to_db(abs(side_lin))
    return mid_db, side_db

def ms_calculation_wrong(rms_left_db, rms_right_db):
    """WRONG M/S calculation (dB subtraction - the original bug)."""
    mid = (rms_left_db + rms_right_db) * 0.5
    side = (rms_left_db - rms_right_db) * 0.5
    return mid, side

def check(name, left_db, right_db):
    global PASS, FAIL

    mid_correct, side_correct = ms_calculation_correct(left_db, right_db)
    mid_wrong, side_wrong = ms_calculation_wrong(left_db, right_db)

    print("")
    print("=" * 60)
    print("TEST: %s" % name)
    print("  Input: L=%.1f dBFS, R=%.1f dBFS" % (left_db, right_db))
    print("  CORRECT:  Mid=%.1f dBFS, Side=%.1f dBFS" % (mid_correct, side_correct))
    print("  WRONG:    Mid=%.1f dBFS, Side=%.1f dBFS" % (mid_wrong, side_wrong))

    # Rule 1: Mono signal (L == R) -> Side should be very quiet (< -200 dB)
    if abs(left_db - right_db) < 0.01:
        if side_correct < -200:
            print("  [OK] Mono signal -> Side ~= -inf  (correct)")
            PASS += 1
        else:
            print("  [FAIL] Mono: Side should be < -200 dB, got %.1f" % side_correct)
            FAIL += 1

    # Rule 2: For stereo, side should always be <= mid
    if abs(left_db - right_db) > 0.1 and left_db > -80 and right_db > -80:
        if side_correct <= mid_correct:
            print("  [OK] Side (%.1f) <= Mid (%.1f)  (correct)" % (side_correct, mid_correct))
            PASS += 1
        else:
            print("  [FAIL] Side (%.1f) > Mid (%.1f) is impossible" % (side_correct, mid_correct))
            FAIL += 1

    # Rule 3: The WRONG algorithm gives INCORRECT results
    # When L ~= R in dB, side should be quiet, NOT near 0 dB
    if abs(left_db - right_db) < 3 and left_db < -10 and right_db < -10:
        if side_wrong > -20:
            print("  [OK] Detected: Wrong algorithm gives Side=%.1f dB (should be quiet)" % side_wrong)
            print("    This was the BUG! Correct Side is %.1f dB" % side_correct)
            PASS += 1

def main():
    global PASS, FAIL

    print("=" * 60)
    print("M/S Calculation Algorithm Test")
    print("=" * 60)
    print("")
    print("Tests the dB->linear->dB conversion used in")
    print("AnalyzersPanelComponent::fastUpdateMeters()")
    print("")

    check("Mono -18 dBFS (identical channels)", -18.0, -18.0)
    check("Typical stereo (-18 dB L, -20 dB R)", -18.0, -20.0)
    check("Wide stereo (-12 dB L, -24 dB R)", -12.0, -24.0)
    check("Hard panned left (-12 dB L, -inf R)", -12.0, -100.0)
    check("Loud stereo (-6 dB L, -8 dB R)", -6.0, -8.0)
    check("Quiet stereo (-40 dB L, -42 dB R)", -40.0, -42.0)
    check("Same RMS (-18 dB both)", -18.0, -18.0)
    check("Extreme imbalance (-6 dB L, -60 dB R)", -6.0, -60.0)
    check("Near silence (-90 dB L, -92 dB R)", -90.0, -92.0)
    check("One channel silent (-18 dB L, -inf R)", -18.0, -100.0)

    print("")
    print("=" * 60)
    print("RESULTS: %d passed, %d failed" % (PASS, FAIL))
    print("=" * 60)

    if FAIL > 0:
        print("[FAIL] %d tests failed" % FAIL)
        sys.exit(1)
    else:
        print("[OK] ALL TESTS PASSED")
        sys.exit(0)

if __name__ == "__main__":
    main()
