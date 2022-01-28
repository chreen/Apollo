print("Testing compound operators...")

do
    local x = 1

    x += 2
    assert(x == 3, "Failed compound add test")

    x -= 1
    assert(x == 2, "Failed compound subtract test")

    x *= 3
    assert(x == 6, "Failed compound multiply test")

    x /= 2
    assert(x == 3, "Failed compound divide test")

    x %= 2
    assert(x == 1, "Failed compound mod test")

    x++
    assert(x == 2, "Failed increment test")

    x ^= 2
    assert(x == 4, "Failed compound exponent test")
end

print()