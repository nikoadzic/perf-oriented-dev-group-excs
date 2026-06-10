function fibonacci_naive(n)
    if n < 2 then
        return n
    end
    return fibonacci_naive(n-1) + fibonacci_naive(n-2)
end

function fibonacci_tail(n)
  local function inner(m, a, b)
    if m == 0 then
      return a
    end
    return inner(m-1, b, a+b)
  end
  return inner(n, 0, 1)
end

function fibonacci_iter(n)
    local a, b = 0, 1
    for i = 1, n do
        a, b = b, a + b
    end
    return a
end

function measure(reps, f, ...)
  local start = os.clock()
  local res
  for i = 0, reps do
    res = f(...)
  end
  local time = (os.clock() - start)
  
  return time, res
end

g = 0
baseline = 13.7609
speedup = 0
t, v = measure(100, fibonacci_naive, 30)
g = g + t
print(string.format("100 x fibonacci_naive(30)     time: %8.4f s  --  %s", t, v))
t, v = measure(10000000, fibonacci_tail, 30)
g = g + t
print(string.format("10000000 x fibonacci_tail(30) time: %8.4f s  --  %s", t, v))
t, v = measure(25000000, fibonacci_iter, 30)
g = g + t
print(string.format("25000000 x fibonacci_iter(30) time: %8.4f s  --  %s", t, v))
g = g/3
speedup = 1 - (g/baseline)
print(string.format("AVG: %8.4f (%s percent faster)", g, speedup))