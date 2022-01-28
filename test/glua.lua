print("Testing GLua logic operators...")

do
    assert(true != false, "Failed != test")
    assert(!true == false, "Failed unary ! test")
    assert(false || true, "Failed || test")
    assert(true && true, "Failed && test")
end

print()