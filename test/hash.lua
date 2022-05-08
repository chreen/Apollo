assert(`` == 0, "invalid empty string hash")
assert(`abc` == -317513893, "invalid 'abc' hash, got " .. `abc` .. ", expected -317513893")