do
    for k, v in {1, 4, 9} do
        assert(k * k == v)
    end
end

do
    local obj = { items = {1, 4, 9} }
    setmetatable(obj, { __iter = function(o) return next, o.items end })

    for k, v in obj do
        assert(k * k == v)
    end
end