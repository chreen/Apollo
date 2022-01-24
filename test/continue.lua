print("Testing contextual continue keyword...")

do
    local buf = ""
    for i = 1, 10 do
        if (i % 2 == 1) then
            continue
        end

        buf = buf .. i
    end

    assert(buf == "246810", "Failed statement test")
end

do
    local buf = ""
    for i = 1, 20 do
        if (i % 2 == 1) then
            continue
        end

        if (i > 10) then
            goto continue
        end

        buf = buf .. i
    end
    ::continue::

    assert(buf == "246810", "Failed label test")
end

do
    local continue = 0
    local fl1 = continue

    continue = 1

    assert(fl1 == 0 and continue == 1, "Failed local variable test")
end

do
    local function continue(x)
        return x + 5
    end

    local did, val = pcall(continue, 5)
    assert(did, "Failed function test with error:\n" .. val)
    assert(val == 10, "Failed function test with invalid return: " .. val .. " (expected 10)")
end

do
    local continue = 0

    local function upvalueTest()
        local fl1 = continue
        continue = 1

        return fl1
    end

    assert(upvalueTest() == 0 and continue == 1, "Failed upvalue test")
end

print()