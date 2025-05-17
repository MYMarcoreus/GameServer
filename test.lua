-- 全局变量x, y
X = 300
Y = 100

-- pos x : 300 pos y :100  table: 0x60c1c4e5cc50   table: 0x60c1c4e5cc50
print("pos x : " .. X .." pos y :" .. Y, _G, _ENV)

local function setEnv()
    local new_env = {X = 800 , Y = 500, print = print }
    _ENV = new_env
    -- pos x : 800 pos y :500  nil     table: 0x60c1c4e62e10
    print("pos x : " .. X .." pos y :" .. Y , _G, _ENV)
    --! 说明对于x, y, print这些自由变量（没有aa.bb这些.运算的变量），是通过，_ENV.x、_ENV.y、_ENV.print的形式来引用的
end

setEnv()

-- pos x : 800 pos y :500  nil     table: 0x60c1c4e62e10
print("pos x : " .. X .." pos y :" .. Y, _G , _ENV)