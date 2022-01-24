print("Testing contextual goto keyword...")

do
    local c = 0
    local f1 = 0

    ::start::
    for i = 1, 10 do
        if (i == 10) then
            if (c == 0) then
                c = 1
                goto start
            else
                goto finish
            end
        end

        f1 = f1 + 1
    end
    ::finish::
    assert(f1 == 18, "Failed statement test")
end

do
    local goto = 0
    local fl1 = goto

    goto = 1

    assert(fl1 == 0 and goto == 1, "Failed local variable test")
end

do
    local function goto(x)
        return x + 5
    end

    local did, val = pcall(goto, 5)
    assert(did, "Failed function test with error:\n" .. val)
    assert(val == 10, "Failed function test with invalid return: " .. val .. " (expected 10)")
end

do
    local goto = 0

    local function upvalueTest()
        local fl1 = goto
        goto = 1

        return fl1
    end

    assert(upvalueTest() == 0 and goto == 1, "Failed upvalue test")
end

print()