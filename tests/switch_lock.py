

'''
I need to implement something equivalent on the P4-Switch using only

(register ± header/metadata ± constant) >/</≤/≥/=/≠ 0

inside the if-statement.

LOCK & mask is not possible as used in the example implementation.
LOCK |= mask is valid, because it is not in the if-statement.

There the mapping would be LOCK ≡ register  and  mask ≡ header
'''


LOCK = [0, 0]


def try_lock(*mask):
    global LOCK
    if mask[0] + LOCK[0] == 2:
        return False
    elif mask[1] + LOCK[1] == 2:
        return False
    else:
        LOCK[0] += mask[0]
        LOCK[1] += mask[1]
        return True


def unlock(*mask):
    global LOCK
    LOCK[0] -= mask[0]
    LOCK[1] -= mask[1]


def is_locked():
    global LOCK
    if LOCK[0] > 0 or LOCK[1] > 0:
        return True
    return False


LOCK = [0, 0]
assert(not is_locked())
LOCK = [0, 0]
assert(try_lock(0, 1))
assert(not try_lock(0, 1))
assert(not try_lock(1, 1))
assert(is_locked())

LOCK = [0, 0]
assert(try_lock(1, 0))
assert(not try_lock(1, 0))
assert(not try_lock(1, 1))
assert(is_locked())

LOCK = [0, 0]
assert(try_lock(1, 1))
assert(not try_lock(0, 1))
assert(not try_lock(1, 0))
assert(not try_lock(1, 1))

assert(is_locked())


'''
struct pair {
    bit<32> left;
    bit<32> right;
}

Register<pair, bit<1>>(1) switch_lock;
RegisterAction<pair, bit<1>, bit<1>>(switch_lock) try_lock = {
    void apply(inout pair value, out bit<1> rv) {
        if ((hdr.info.left + value.left) == 2) {
            rv = 0;
        } else if ((hdr.info.right + value.right) == 2) {
            rv = 0;
        } else {
            rv = 1;
            value.left  = value.left + hdr.info.left;
            value.right  = value.right + hdr.info.right;
        }
    }
};
RegisterAction<pair, bit<1>, bit<1>>(switch_lock) unlock = {
    void apply(inout pair value, out bit<1> rv) {
        value.left = value.left - hdr.info.left;
        value.right = value.right - hdr.info.right;
    }
};
RegisterAction<pair, bit<1>, bit<1>>(switch_lock) is_locked = {
    void apply(inout pair value, out bit<1> rv) {
        if (value.left > 0 || value.right > 0) {
            rv = 1;
        } else {
            rv = 0;
        }
    }
};
'''
